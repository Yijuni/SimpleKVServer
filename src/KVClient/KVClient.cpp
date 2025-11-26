#include "KVClient.hpp"
#include <random>
#include <Logger.hpp>
KVClient::KVClient(std::string zkip, uint16_t zkport, std::string clientid,int shardlen)
    : zkclient_myj(zkip, zkport), requestid_myj(0), leaderindex_myj(0), clientid_myj(clientid),shard_len_myj(shardlen)
{
    
    if (clientid == "")
    {
        clientid_myj = generateId();
    }
    // getServerStubs();
    shard_client_myj = std::make_shared<ShardCtrlerClient>(zkip,zkport,clientid_myj);
    make_stubd_myj = std::make_shared<MakeServerStub>(zkip,zkport);
    //初始化一下配置信息
    getNewConfig();
}

bool KVClient::Get(std::string key, std::string &value)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int count = 0;
    requestid_myj++;
    // 最多循环发送10次
    while (count < 10)
    {
        kvservice::GetRequest request;
        kvservice::GetResponse response;
        KVRpcController controller;
        request.set_clientid(clientid_myj);
        request.set_requestid(requestid_myj);
        request.set_key(key);

        long long shardid = key2shard(key);
        long long gid = shard2gid_myj[shardid];
        LOG_INFO("key：%s，对应复制组：%lld",key.c_str(),gid);
        std::vector<std::string> serversname = groups_myj[gid];

        bool flag = false;
        auto stub = make_stubd_myj->GetServerStub(gid,serversname[leaderindex_myj],flag);
        if(!flag){
            LOG_ERROR("获取通信stub失败，不包含gid:%lld,servername:%s，的stub",gid,serversname[leaderindex_myj].c_str());
            //更新下配置
            getNewConfig();
            leaderindex_myj=0;
            count++;
            continue;
        }

        LOG_INFO("发送Get请求");
        stub->Get(&controller, &request, &response, nullptr);
        if (controller.Failed())
        {
            LOG_INFO("client[%s]>>requestid[%lld],发送消息时失败，网路可能出现了错误，send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            count++;
            continue;
        }

        if (response.resultcode().errorcode() ==  kvclient::ErrWrongLeader)
        {
            leaderindex_myj = (leaderindex_myj + 1) % serversname.size();
            LOG_INFO("client[%s]>>requestid[%lld], leader选择错误，对端不是leader,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrWrongGroup){
            LOG_INFO("client[%s]>>requestid[%lld], group选择错误，对端不是处理key:%s的复制组,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), key.c_str(),response.resultcode().errormsg().c_str());
            //更新配置
            getNewConfig();
            leaderindex_myj = 0;
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld], 超时需要等待0,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrNoKey){
            LOG_INFO("client[%s]>>requestid[%lld], 不存在key:%s,error msg:%s", clientid_myj.c_str(), requestid_myj.load(),key.c_str(),response.resultcode().errormsg().c_str());
            value = "";
            flag = false;
        }else if(response.resultcode().errorcode()==kvclient::OK){
            LOG_INFO("client[%s]>>requestid[%lld], Get请求成功,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            value = response.value();
            flag = true;
        }

        return flag;
    }

    value = "";
    return false;
}

bool KVClient::Put(std::string key, std::string value)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return PutAppend(key, value, "Put");
}

bool KVClient::Append(std::string key, std::string value)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return PutAppend(key, value, "Append");
}

bool KVClient::PutAppend(std::string key, std::string value, std::string op)
{
    int count = 0;
    requestid_myj++;
    // 最多循环发送两次
    while (count < 10)
    {
        KVRpcController controller;
        kvservice::PutAppendRequest request;
        kvservice::PutAppendResponse response;

        request.set_clientid(clientid_myj);
        request.set_requestid(requestid_myj);
        request.set_key(key);
        request.set_value(value);

        long long shardid = key2shard(key);
        long long gid = shard2gid_myj[shardid];
        LOG_INFO("key：%s，对应复制组：%lld",key.c_str(),gid);
        std::vector<std::string> serversname = groups_myj[gid];

        bool flag = false;
        auto stub = make_stubd_myj->GetServerStub(gid,serversname[leaderindex_myj],flag);
        if(!flag){
            LOG_ERROR("获取通信stub失败，不包含gid:%lld,servername:%s，的stub",gid,serversname[leaderindex_myj].c_str());
            //更新下配置
            getNewConfig();
            count++;
            continue;
        }

        if (op == "Put")
        {
            LOG_INFO("发送Put请求")
            stub->Put(&controller, &request, &response, nullptr);
        }
        else
        {
            LOG_INFO("发送Append请求");
            stub->Append(&controller, &request, &response, nullptr);
        }

        if (controller.Failed())
        {
            LOG_INFO("client[%s]>>requestid[%lld],发送消息时失败，网路可能出现了错误，send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            //更新下配置
            getNewConfig();
            leaderindex_myj=0;
            count++;
            continue;
        }

        if (response.resultcode().errorcode() ==  kvclient::ErrWrongLeader)
        {
            leaderindex_myj = (leaderindex_myj + 1) % serversname.size();
            LOG_INFO("client[%s]>>requestid[%lld], leader选择错误，对端不是leader,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrWrongGroup){
            LOG_INFO("client[%s]>>requestid[%lld], group选择错误，对端不是处理key:%s的复制组,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), key.c_str(),response.resultcode().errormsg().c_str());
            //更新配置
            getNewConfig();
            leaderindex_myj = 0;
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld], 超时需要等待0,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            count++;
            continue;
        }else if(response.resultcode().errorcode()==kvclient::ErrNoKey){
            LOG_INFO("client[%s]>>requestid[%lld], 不存在key:%s,error msg:%s", clientid_myj.c_str(), requestid_myj.load(),key.c_str(),response.resultcode().errormsg().c_str());
            flag = false;
        }else if(response.resultcode().errorcode()==kvclient::OK){
            LOG_INFO("client[%s]>>requestid[%lld], Put请求成功,error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            flag = true;
        }

        return flag;
    }

    return false;
}

std::string KVClient::generateId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string clientid;
    for (int i = 0; i < 10; i++)
    {
        std::uniform_int_distribution<size_t> dis(0, chars.size() - 1);
        clientid += chars[dis(gen)];
    }
    LOG_INFO("client[%s]>>成功生成id", clientid.c_str());
    return clientid;
}

void KVClient::getNewConfig()
{
    LOG_INFO("Client[%s]>>开始获取新的配置",clientid_myj.c_str());
    shard2gid_myj.clear();
    groups_myj.clear();
    shardctrler::Config config;
    shard_client_myj->Query(-1,config);
    for(int i =0 ; i <config.shards_size();i++){
        shard2gid_myj.push_back(config.shards(i));
    }

    for(auto & kv : config.groups()){
        std::vector<std::string> servers;
        for(int i=0;i<kv.second.serversname_size();i++){
            servers.push_back(kv.second.serversname(i));
        }
        groups_myj[kv.first] = servers;
    }
}

long long KVClient::key2shard(std::string& key)
{
    return std::hash<std::string>{}(key) % shard_len_myj;
}

void KVClient::serverWatcher()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);

    std::vector<std::string> info;
    zkclient_myj.getChildInfo("/kvserver/servers", info);

    server_myj.clear();
    for (int i = 0; i < info.size(); i++)
    {
        std::string peerinfo;
        zkclient_myj.getPathData("/kvserver/servers/" + info[i], peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        LOG_INFO("client[%s]>>获取对端信息:ip[%s],port[%d]", clientid_myj.c_str(), peerip.c_str(), port);
        server_myj.emplace_back(std::make_shared<kvservice::KVServiceRPC_Stub>(new KVRpcChannel(peerip, port), Service::STUB_OWNS_CHANNEL));
        LOG_INFO("client[%s]>>成功连接对端:ip[%s],port[%d]", clientid_myj.c_str(), peerip.c_str(), port);
    }
    LOG_INFO("client[%s]>>/kvserver/servers的子节点发生改变，重新连接完成", clientid_myj.c_str());
}

void KVClient::getServerStubs()
{
    std::vector<std::string> info;
    zkclient_myj.Connect();
    zkclient_myj.initChildWatcher("/kvserver/servers", std::bind(&KVClient::serverWatcher, this));
    zkclient_myj.registerChildWatcher("/kvserver/servers", info);
    for (int i = 0; i < info.size(); i++)
    {
        std::string peerinfo;
        zkclient_myj.getPathData("/kvserver/servers/" + info[i], peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        server_myj.emplace_back(std::make_shared<kvservice::KVServiceRPC_Stub>(new KVRpcChannel(peerip, port), Service::STUB_OWNS_CHANNEL));
    }
    LOG_INFO("client[%s]>>servers连接完成", clientid_myj.c_str());
}
