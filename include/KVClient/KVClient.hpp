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
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
enum ERRORID
{
    OK = 1,
    ErrNoKey = 2,
    ErrWrongLeader = 3,
    ErrTimeOut = 4,
};

class KVClient{
public:
    /// @brief 
    /// @param zkip zookeeper服务器ip
    /// @param zkport zookeeper服务器端口
    /// @param name 服务器唯一标识
    KVClient(std::string zkip="127.0.0.1",uint16_t zkport=2181,std::string clientid="");
    bool Get(std::string key,std::string& value);
    bool Put(std::string key,std::string value);
    bool Append(std::string key,std::string value);
private:
    void serverWatcher();
    void getServerStubs();
    bool PutAppend(std::string key,std::string value,std::string op);
    std::string generateId();
    std::string clientid_myj;
    std::atomic<long long> requestid_myj;
    std::atomic<int> leaderindex_myj;
    ZKClient zkclient_myj;
    std::vector<std::shared_ptr<kvservice::KVServiceRPC_Stub>> server_myj; 
    std::mutex sourceMutex_myj;
};

#endif