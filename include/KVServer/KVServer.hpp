#ifndef KVSERVER_HPP
#define KVSERVER_HPP

/**
 * 2025-5-7 moyoj
 * KVServer包含KVservice和KVRaft，
 * 启动RPC服务监听来自客户端或者其他服务器的请求
 */

#include "KVService.hpp"
#include "Raft.hpp"
#include "LockQueue.hpp"
#include "KVRaft.pb.h"
#include "Persister.hpp"
#include "KVRpcProvider.hpp"
#include "KVRpcChannel.hpp"
#include <vector>
#include "ZKClient.hpp"
#include <memory>

class KVServer
{
public:
    /// @brief
    /// @param ip 当前服务器ip
    /// @param port 当前服务器端口
    /// @param zkip zookeeper服务器ip
    /// @param zkport zookeeper服务器端口
    /// @param maxraftsize 持久化raftstate数据最大大小
    KVServer(std::string ip = "127.0.0.1", uint16_t port = 8009, std::string zkip = "127.0.0.1", uint16_t zkport = 2181, long long maxraftsize = -1);

private:
    std::string ip_myj;
    uint16_t port_myj;
    std::string zkip_myj;
    uint16_t zkport_myj;
    std::string name_myj;

    // 提供服务用
    std::shared_ptr<KVRpcProvider> provider_myj;
    // 与对端通信用的stubs
    std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> peersConnPtrs_myj;
    // 持久化方法
    std::shared_ptr<Persister> persister_myj;

    // 连接zookeeper服务器用的
    std::shared_ptr<ZKClient> zkConnptr_myj;
    // 客户服务
    std::shared_ptr<KVService> service_myj;
    // raft服务
    std::shared_ptr<KVRaft> raft_myj;
    // raft往service提交日志用
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan_myj;

    void connectPeers(std::vector<std::string> &info);
    void childWatcher();
};

#endif