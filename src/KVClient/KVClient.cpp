#include "KVClient.hpp"
#include <random>
#include <Logger.hpp>
KVClient::KVClient(std::string zkip, uint16_t zkport, std::string clientid)
    : zkclient_myj(zkip, zkport), requestid_myj(0), leaderindex_myj(0), clientid_myj(clientid)
{
    if (clientid == "")
    {
        clientid_myj = generateId();
    }
    getServerStubs();
}

bool KVClient::Get(std::string key, std::string &value)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    int count = 0;
    // 最多循环发送两次
    while (count < 2 * server_myj.size())
    {
        kvservice::GetRequest request;
        kvservice::GetResponse response;
        KVRpcController controller;
        request.set_clientid(clientid_myj);
        request.set_requestid(requestid_myj);
        request.set_key(key);

        server_myj[leaderindex_myj]->Get(&controller, &request, &response, nullptr);

        if (controller.Failed())
        {
            leaderindex_myj = (leaderindex_myj + 1) % server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            count++;
            continue;
        }
        if (response.resultcode().errorcode() != OK)
        {
            leaderindex_myj = (leaderindex_myj + 1) % server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld],response error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            count++;
            continue;
        }

        requestid_myj++;
        value = response.value();
        return true;
    }

    requestid_myj++;
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
    // 最多循环发送两次
    while (count < 2 * server_myj.size())
    {
        KVRpcController controller;
        kvservice::PutAppendRequest request;
        kvservice::PutAppendResponse response;

        request.set_clientid(clientid_myj);
        request.set_requestid(requestid_myj);
        request.set_key(key);
        request.set_value(value);

        if (op == "Put")
        {
            server_myj[leaderindex_myj]->Put(&controller, &request, &response, nullptr);
        }
        else
        {
            server_myj[leaderindex_myj]->Append(&controller, &request, &response, nullptr);
        }

        if (controller.Failed())
        {
            leaderindex_myj = (leaderindex_myj + 1) % server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld],send error msg:%s", clientid_myj.c_str(), requestid_myj.load(), controller.ErrorText().c_str());
            count++;
            continue;
        }

        if (response.resultcode().errorcode() != OK)
        {
            leaderindex_myj = (leaderindex_myj + 1) % server_myj.size();
            LOG_INFO("client[%s]>>requestid[%lld],response error msg:%s", clientid_myj.c_str(), requestid_myj.load(), response.resultcode().errormsg().c_str());
            count++;
            continue;
        }

        requestid_myj++;
        return true;
    }

    requestid_myj++;
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
