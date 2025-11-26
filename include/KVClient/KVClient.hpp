#ifndef KVCLIENT_HPP
#define KVCLIENT_HPP
/**
 * 2025-5-12 moyoj
 * 提供Put、Append、Get的API，远程调用KVserver提供的服务
 */
#include <string>
#include <ZKClient.hpp>
#include "KVRpcChannel.hpp"
#include "KVRpcController.hpp"
#include "KVService.pb.h"
#include "MakeServerStub.hpp"
#include "ShardCtrlerClient.hpp"
#include "ShardCtrler.pb.h"
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <unordered_map>
namespace kvclient{
enum ERRORID
{
    OK = 1,
    ErrNoKey = 2,
    ErrWrongLeader = 3,
    ErrTimeOut = 4,
    ErrWrongGroup = 5,
    WaitAndRetry = 6,
};
};

class KVClient{
public:
    /// @brief 
    /// @param zkip zookeeper服务器ip
    /// @param zkport zookeeper服务器端口
    /// @param clientid 客户端唯一标识
    KVClient(std::string zkip="127.0.0.1",uint16_t zkport=2181,std::string clientid="",int shardlen=10);
    bool Get(std::string key,std::string& value);
    bool Put(std::string key,std::string value);
    bool Append(std::string key,std::string value);
private:
    //2025.11.17下面两个函数没用了
    void serverWatcher();
    void getServerStubs();
    //2025/11/17下方成员变量失效
    std::vector<std::shared_ptr<kvservice::KVServiceRPC_Stub>> server_myj; 


    bool PutAppend(std::string key,std::string value,std::string op);
    std::string generateId();
    void getNewConfig();
    long long key2shard(std::string& key);
    std::string clientid_myj;
    std::atomic<long long> requestid_myj;
    std::atomic<int> leaderindex_myj;
    ZKClient zkclient_myj;
    std::mutex sourceMutex_myj;
    //获取连接用的stub
    std::shared_ptr<MakeServerStub> make_stubd_myj;
    //与ShardServer通信的客户端
    std::shared_ptr<ShardCtrlerClient> shard_client_myj;
    //最新的配置的分片分组信息
    std::vector<long long> shard2gid_myj;
    //最新配置服务器的信息
    std::unordered_map<long long,std::vector<std::string>> groups_myj;
    //分片数目
    int shard_len_myj;

};

#endif