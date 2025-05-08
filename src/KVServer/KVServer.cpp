#include "KVServer.hpp"
#include "Logger.hpp"
#include <string>
KVServer::KVServer(std::string ip, uint16_t port, std::string zkip, uint16_t zkport,long long maxraftsize)
    : ip_myj(ip), port_myj(port), zkip_myj(zkip), zkport_myj(zkport), 
    name_myj(ip + ":" + std::to_string(port))
{
    persister_myj = std::make_shared<Persister>(name_myj + "raftstate", name_myj + "snapshot");
    applyChan_myj = std::make_shared<LockQueue<ApplyMsg>>(0);
    zkConnptr_myj = std::make_shared<ZKClient>(zkip, zkport);
    raft_myj = std::make_shared<KVRaft>();
    service_myj = std::make_shared<KVService>(name_myj,persister_myj,raft_myj,applyChan_myj,500,maxraftsize);

    //连接zookeeper
    zkConnptr_myj->Connect();
    zkConnptr_myj->initChildWatcher("/kvserver/servers",std::bind(&KVServer::childWatcher,this));
    std::vector<std::string> server;
    zkConnptr_myj->registerChildWatcher("/kvserver/servers",server);

    std::thread td([&](){
        provider_myj = std::make_shared<KVRpcProvider>(ip,port);
        //发布服务
        provider_myj->NotifyService(raft_myj.get());
        provider_myj->NotifyService(service_myj.get());
        provider_myj->Run();
    });
    td.detach();
    
    //连接到对端
    connectPeers(server);

    //初始化raft
    raft_myj->Make(peersConnPtrs_myj, name_myj, persister_myj, applyChan_myj);

}


void KVServer::connectPeers(std::vector<std::string> &server)
{
    for (int i = 0; i < server.size(); i++)
    {
        std::string peerinfo;
        zkConnptr_myj->getPathData("/kvserver/servers/"+server[i],peerinfo);
        if (peerinfo == name_myj)
        {
            continue;
        }
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        LOG_INFO("server[%s]>>获取对端信息:ip[%s],port[%d]", name_myj.c_str(), peerip.c_str(), port);
        peersChannels_myj.emplace_back(std::make_shared<KVRpcChannel>(peerip, port));
        peersConnPtrs_myj.emplace_back(std::make_shared<kvraft::KVRaftRPC_Stub>(peersChannels_myj[peersChannels_myj.size()-1].get()));
        LOG_INFO("server[%s]>>成功连接对端:ip[%s],port[%d]", name_myj.c_str(), peerip.c_str(), port);
    }
    LOG_INFO("server[%s]>>所有对端连接完成", name_myj.c_str());
}

void KVServer::childWatcher()
{

}
