#ifndef MAKESERVERSTUB_HPP
#define MAKESERVERSTUB_HPP
/***
 * 2025-10-20
 * 主要根据组ID和服务器的名称(ip：port)来获取连接
 * 主要用于与其他组的服务器通信
 */

 #include <string>
 #include <memory>
 #include "KVService.pb.h"
 #include "KVRpcChannel.hpp"
 #include <unordered_map>
 #include "ZKClient.hpp"
class MakeServerStub{
public:
    /// @brief 
    /// @param zkip zookeeper的服务器IP地址
    /// @param port zookeeper的服务器端口还
    MakeServerStub(std::string zkip="127.0.0.1",uint16_t zkport=2181);
    std::shared_ptr<kvservice::KVServiceRPC_Stub> GetServerStub(const long long &gid,const std::string &name,bool &flag);
private:
    MakeServerStub();
    //用来创建某复制组下的所有服务器的stub
    void connectPeers(std::vector<std::string> &info,std::string path);
    // 用来观复制组的变化
    void groupsWatcher();
    // 用来观测复制组下的所有服务器节点的变化
    void childWatcher(std::string path);
    std::string zkip_myj;
    uint16_t zkport_myj;
    // 连接zookeeper服务器用的
    std::shared_ptr<ZKClient> zkConnptr_myj;
    //保存所有复制组服务器的连接
    std::unordered_map<long long,std::unordered_map<std::string,std::shared_ptr<kvservice::KVServiceRPC_Stub>>> all_stubs_myj;
    //所有复制组的zk路径前缀
    std::string path_prefix_myj;
 };
#endif