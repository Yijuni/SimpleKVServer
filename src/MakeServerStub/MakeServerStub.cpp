#include "MakeServerStub.hpp"
#include "Logger.hpp"
MakeServerStub::MakeServerStub(std::string zkip, uint16_t zkport):zkip_myj(zkip),zkport_myj(zkport)
{
    path_prefix_myj = "/kvserver/replica_group";
    zkConnptr_myj = std::make_shared<ZKClient>(zkip,zkport);
    zkConnptr_myj->Connect();
    // 注册复制组的事件响应函数
    zkConnptr_myj->initChildWatcher(path_prefix_myj,std::bind(&MakeServerStub::groupsWatcher,this));

    std::vector<std::string> groups;
    zkConnptr_myj->registerChildWatcher(path_prefix_myj,groups);
    for(int i=0;i<groups.size();i++){
        std::string path = path_prefix_myj+"/"+groups[i];
        std::vector<std::string> servers;
        //给每个组注册事件回调函数,必须传入变化节点的路径，能针对性的修改连接
        zkConnptr_myj->initChildWatcher(path,std::bind(&MakeServerStub::childWatcher,this,path));
        zkConnptr_myj->registerChildWatcher(path,servers);
        //给该组的所有服务器创建Stub
        connectPeers(servers,path);
    }
}

std::shared_ptr<kvservice::KVServiceRPC_Stub> MakeServerStub::GetServerStub(const long long &gid, const std::string &name,bool &flag)
{
    auto iter = all_stubs_myj.find(gid);
    if(iter==all_stubs_myj.end()){
        flag=false;
        return nullptr;
    }
    auto iter1 = all_stubs_myj[gid].find(name);
    if(iter1==all_stubs_myj[gid].end()){
        flag==false;
        return nullptr;
    }
    flag = true;
    return iter1->second;
}

void MakeServerStub::connectPeers(std::vector<std::string> &info,std::string path)
{
    // +4是因为 /gid长度为4
    long long gid = std::stoll(path.substr(path_prefix_myj.size()+4));
    auto iter = all_stubs_myj.find(gid);
    if(iter==all_stubs_myj.end()){
        all_stubs_myj[gid] = std::unordered_map<std::string,std::shared_ptr<kvservice::KVServiceRPC_Stub>>();
    }
    all_stubs_myj[gid].clear();
    for(int i=0;i<info.size();i++){
        std::string peerinfo;
        zkConnptr_myj->getPathData(path+"/"+info[i],peerinfo);
        int pos = peerinfo.find(":");
        std::string peerip = peerinfo.substr(0, pos);
        uint16_t port = std::stoi(peerinfo.substr(pos + 1));
        LOG_INFO("MAKESERVERSTUB>>获取对端信息:ip[%s],port[%d]", peerip.c_str(), port);
        all_stubs_myj[gid][info[i]] = std::make_shared<kvservice::KVServiceRPC_Stub>(new KVRpcChannel(peerip, port), Service::STUB_OWNS_CHANNEL);
        LOG_INFO("MAKESERVERSTUB>>成功连接对端:ip[%s],port[%d]", peerip.c_str(), port);
    }
    LOG_INFO("MAKESERVERSTUB>>gid[%lld],所有服务器连接完成", gid);
}

void MakeServerStub::groupsWatcher()
{
    LOG_INFO("MAKESERVERSTUB>>复制组[/kvserver/replica_group]的成员发生变化,重新获取组别信息并重新连接");
    std::vector<std::string> groups;
    zkConnptr_myj->registerChildWatcher(path_prefix_myj,groups);
    all_stubs_myj.clear();
    for(int i=0;i<groups.size();i++){
        std::string path = path_prefix_myj+"/"+groups[i];
        std::vector<std::string> servers;
        //给每个组注册事件回调函数,必须传入变化节点的路径，能针对性的修改连接
        zkConnptr_myj->initChildWatcher(path,std::bind(&MakeServerStub::childWatcher,this,path));
        zkConnptr_myj->registerChildWatcher(path,servers);
        //给该组的所有服务器创建Stub
        connectPeers(servers,path);
    }
}

void MakeServerStub::childWatcher(std::string path)
{
    LOG_INFO("MAKESERVERSTUB>>复制组[%s],成员节点发生变化，重新连接",path.c_str());
    std::vector<std::string> serverinfo;
    zkConnptr_myj->getChildInfo(path,serverinfo);
    connectPeers(serverinfo,path);
}
