#ifndef SHARDCTRLERCLIENT_HPP
#define SHARDCTRLERCLIENT_HPP

#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "ShardCtrler.pb.h"
#include <mutex>
#include "ZKClient.hpp"
#include <unordered_map>
#include <memory>
#include <vector>
enum ERRORID
{
    OK = 1,
    ErrWrongLeader = 2,
    ErrTimeOut = 3,
};

class ShardCtrlerClient{
public:
    /// @brief
    /// @param zkip zookeeper服务器的ip
    /// @param zkport zookeeper服务器的端口
    /// @param clientid 客户端唯一标识
    ShardCtrlerClient(std::string zkip="127.0.0.1",uint16_t zkport=2181,std::string clientid="");
    bool Join(std::unordered_map<long long,std::vector<std::string>> groups);
    bool Leave(std::vector<long long> gids);
    bool Move(long long gid,long long shard);
    bool Query(long long num,shardctrler::Config &config);
private:
    std::string generateId();
    void serverWatcher();
    void getServerStubs();
    std::string clientid_myj;
    std::atomic<long long> requestid_myj;
    std::atomic<int> leaderindex_myj;
    ZKClient zkclient_myj;
    std::vector<std::shared_ptr<shardctrler::ShardCtrlerRPC_Stub>> server_myj; 
    std::mutex sourceMutex_myj;
};

#endif