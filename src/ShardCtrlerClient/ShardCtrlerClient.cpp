#include "ShardCtrlerClient.hpp"
#include "Logger.hpp"
#include <random>
#include <thread>
#include <chrono>
#include <string>
ShardCtrlerClient::ShardCtrlerClient(std::string zkip, uint16_t zkport, std::string clientid):
    zkclient_myj(zkip,zkport),leaderindex_myj(0),requestid_myj(0),clientid_myj(clientid)
{
    if(clientid==""){
        clientid_myj = generateId();
    }
    getServerStubs();
}

bool ShardCtrlerClient::Join(std::unordered_map<long long, std::vector<std::string>> groups)
{
    shardctrler::JoinRequest request;
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int send_count=0;//只最多遍历两次服务器列表发送请求
    request.set_clientid(clientid_myj);
    request.set_requestid(requestid_myj);
    auto mgroup =  request.mutable_groups();
    for(auto &kv : groups){
        long long gid = kv.first;
        shardctrler::Servers servers;
        std::cout<<"新加的gid"<<gid<<"的服务器数目:"<<kv.second.size()<<std::endl;
        for(int i=0;i<kv.second.size();i++){
            servers.add_serversname(kv.second[i]);
        }
        std::cout<<"servers类新加的gid"<<gid<<"的服务器数目:"<<servers.serversname_size()<<std::endl;
        mgroup->insert({gid,servers});
    }

    while (send_count < server_myj.size()*2)
    {
        send_count++;
        shardctrler::JoinResponse response;
        KVRpcController controller;
        
        server_myj[leaderindex_myj]->Join(&controller,&request,&response,nullptr);
        if(controller.Failed()){
            LOG_INFO("client[%s]>>requestid[%lld][JOIN],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            send_count++;
            continue;
        }
        if(response.err()==ErrWrongLeader || response.wrongleader()){
            leaderindex_myj = (leaderindex_myj+1)%server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld][JOIN],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"目标服务器不是leader节点");
            send_count++;
            continue;
        }
        if(response.err()==ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld][JOIN],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"等待服务器完成请求");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_count++;
            continue;
        }
        requestid_myj++;
        LOG_INFO("Join请求成功");
        return true;
    }
    
    return false;
}

bool ShardCtrlerClient::Leave(std::vector<long long> gids)
{
    shardctrler::LeaveRequest request;
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int send_count=0;//只最多遍历两次服务器列表发送请求
    request.set_clientid(clientid_myj);
    request.set_requestid(requestid_myj);
    for(int i=0;i<gids.size();i++){
        request.add_gids(gids[i]);
    }
    while (send_count < server_myj.size()*2)
    {
        send_count++;
        shardctrler::LeaveResponse response;
        KVRpcController controller;
        
        server_myj[leaderindex_myj]->Leave(&controller,&request,&response,nullptr);
        if(controller.Failed()){
            LOG_INFO("client[%s]>>requestid[%lld][LEAVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            send_count++;
            continue;
        }
        if(response.err()==ErrWrongLeader || response.wrongleader()){
            leaderindex_myj = (leaderindex_myj+1)%server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld][LEAVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"目标服务器不是leader节点");
            send_count++;
            continue;
        }
        if(response.err()==ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld][LEAVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"等待服务器完成请求");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_count++;
            continue;
        }
        requestid_myj++;
        return true;
    }
    
    return false;
}

bool ShardCtrlerClient::Move(long long gid, long long shard)
{
    shardctrler::MoveRequest request;
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int send_count=0;//只最多遍历两次服务器列表发送请求
    request.set_clientid(clientid_myj);
    request.set_requestid(requestid_myj);
    request.set_gid(gid);
    request.set_shard(shard);
    while (send_count < server_myj.size()*2)
    {
        send_count++;
        shardctrler::MoveResponse response;
        KVRpcController controller;
        
        server_myj[leaderindex_myj]->Move(&controller,&request,&response,nullptr);
        if(controller.Failed()){
            LOG_INFO("client[%s]>>requestid[%lld][MOVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            send_count++;
            continue;
        }
        if(response.err()==ErrWrongLeader || response.wrongleader()){
            leaderindex_myj = (leaderindex_myj+1)%server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld][MOVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"目标服务器不是leader节点");
            send_count++;
            continue;
        }
        if(response.err()==ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld][MOVE],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"等待服务器完成请求");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_count++;
            continue;
        }
        requestid_myj++;
        return true;
    }
    
    return false;;
}

bool ShardCtrlerClient::Query(long long num, shardctrler::Config &config)
{
    shardctrler::QueryRequest request;
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int send_count=0;//只最多遍历两次服务器列表发送请求
    request.set_clientid(clientid_myj);
    request.set_requestid(requestid_myj);
    request.set_num(num);
    while (send_count < server_myj.size()*2)
    {
        send_count++;
        shardctrler::QueryResponse response;
        KVRpcController controller;
        
        server_myj[leaderindex_myj]->Query(&controller,&request,&response,nullptr);
        if(controller.Failed()){
            LOG_INFO("client[%s]>>requestid[%lld][QUERY],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            send_count++;
            continue;
        }
        if(response.err()==ErrWrongLeader || response.wrongleader()){
            leaderindex_myj = (leaderindex_myj+1)%server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld][QUERY],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"目标服务器不是leader节点");
            send_count++;
            continue;
        }
        if(response.err()==ErrTimeOut){
            LOG_INFO("client[%s]>>requestid[%lld][QUERY],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(),"等待服务器完成请求");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            send_count++;
            continue;
        }
        requestid_myj++;
        config = response.config();
        return true;
    }
    
    return false;
}

std::string ShardCtrlerClient::generateId()
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

void ShardCtrlerClient::serverWatcher()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);

    std::vector<std::string> info;
    zkclient_myj.getChildInfo("/kvserver/shard_config_group/servers", info);

    server_myj.clear();
    for (int i = 0; i < info.size(); i++)
    {
        std::string peerinfo;
        zkclient_myj.getPathData("/kvserver/shard_config_group/servers/" + info[i], peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        LOG_INFO("client[%s]>>获取对端信息:ip[%s],port[%d]", clientid_myj.c_str(), peerip.c_str(), port);
        server_myj.emplace_back(std::make_shared<shardctrler::ShardCtrlerRPC_Stub>(new KVRpcChannel(peerip, port), Service::STUB_OWNS_CHANNEL));
        LOG_INFO("client[%s]>>成功连接对端:ip[%s],port[%d]", clientid_myj.c_str(), peerip.c_str(), port);
    }
    LOG_INFO("client[%s]>>/kvserver/shard_config_group/servers的子节点发生改变，重新连接完成", clientid_myj.c_str());
}

void ShardCtrlerClient::getServerStubs()
{
    std::vector<std::string> info;
    zkclient_myj.Connect();
    zkclient_myj.initChildWatcher("/kvserver/shard_config_group/servers", std::bind(&ShardCtrlerClient::serverWatcher, this));
    zkclient_myj.registerChildWatcher("/kvserver/shard_config_group/servers", info);
    for (int i = 0; i < info.size(); i++)
    {
        std::string peerinfo;
        zkclient_myj.getPathData("/kvserver/shard_config_group/servers/" + info[i], peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        server_myj.emplace_back(std::make_shared<shardctrler::ShardCtrlerRPC_Stub>(new KVRpcChannel(peerip, port), Service::STUB_OWNS_CHANNEL));
    }
    LOG_INFO("client[%s]>>shardctrlerservers连接完成", clientid_myj.c_str());
}
