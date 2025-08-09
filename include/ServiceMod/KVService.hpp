#ifndef KVSERVICE_HPP
#define KVSERVICE_HPP
/**
 * 2025-4-23 moyoj
 * KV服务器服务层的service，供客户端调用，完成了get put append 操作
 */
#include "KVService.pb.h"
#include "rocksdbapi.hpp"
#include "Persister.hpp"
#include "LockQueue.hpp"
#include "Raft.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

struct clientLastReply{
    long long requestid;
    std::string replyMsg;
    clientLastReply(long long id,std::string replymsg):requestid(id),replyMsg(replymsg)
    {}
    clientLastReply(){}

    //为了能让boost库的序列化对该结构体生效
    template<typename Archive>
    void serialize(Archive& archive, const unsigned int version)
    {
      archive & BOOST_SERIALIZATION_NVP(requestid);
      archive & BOOST_SERIALIZATION_NVP(replyMsg);
    }
};
struct notifyChanMsg{
    long long errid;
    std::string result;
};
//客户端和服务器共用
enum ERRORID{
    OK = 1,
    ErrNoKey = 2,
    ErrWrongLeader = 3,
    ErrTimeOut = 4, 
};
enum REQUESTID{
    Get = 1,
    Append = 2,
    Put = 3   
};
class KVService:public kvservice::KVServiceRPC{
public:
    KVService();
    
    /// @brief 
    /// @param name 服务器名称
    /// @param persister 持久化类
    /// @param raft raft层的类
    /// @param applyChan raft层往服务层提交共识日志的channel
    /// @param timeout 客户端请求等待超时时间
    /// @param maxraftstate raftstate持久化信息大小阈值
    KVService(std::string name,std::shared_ptr<Persister> persister,std::shared_ptr<KVRaft> raft,
        std::shared_ptr<LockQueue<ApplyMsg>> applyChan,int timeout,int maxraftstate,std::shared_ptr<RocksDBAPI> db);
    void Get(google::protobuf::RpcController* controller,const ::kvservice::GetRequest* request,
        ::kvservice::GetResponse* response,
        ::google::protobuf::Closure* done);
    void Put(google::protobuf::RpcController* controller,
        const ::kvservice::PutAppendRequest* request,
        ::kvservice::PutAppendResponse* response,
        ::google::protobuf::Closure* done);
    void Append(google::protobuf::RpcController* controller,
        const ::kvservice::PutAppendRequest* request,
        ::kvservice::PutAppendResponse* response,
        ::google::protobuf::Closure* done);
private:
    //从raft层接受数据
    void applyLogs();
    //生成快照
    void snapshot(long long logindex);
    //读取持久化数据
    void readPersist(std::string data);
    //处理命令
    void commandApplyHandler(ApplyMsg applymsg);
    //处理快照
    void snapshotHandler(ApplyMsg applymsg);
    //等待请求执行提交
    void waitRequestCommit(std::shared_ptr<LockQueue<notifyChanMsg>> notifychan,kvservice::ResultCode& resultcode,std::string &value);
    std::string name_myj;
    //持久化
    std::shared_ptr<Persister> persister_myj;
    //raft层给服务层上交日志用
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan_myj;
    //追加日志、生成快照用
    std::shared_ptr<KVRaft> raft_myj; 
    //临界资源锁
    std::mutex sourceMutex_myj;
    //起否初始化完成
    std::atomic<bool> ready_myj;
    //是否正在生成快照
    std::atomic<bool> snapshoting_myj;
    //raftstate大小阈值，当超过这个阈值就要生成快照
    long long maxraftstate_myj;
    //当前提交日志的最高下标，就算某条日志的命令没执行也要记录（可能重复命令）
    long long maxCommitIndex_myj;
    
    // 数据库指针
    std::shared_ptr<RocksDBAPI> db_myj;

    //记录键值对
    std::unordered_map<std::string,std::string> keyvalue_myj;
    //记录某客户端最后一条请求结果
    std::unordered_map<std::string,clientLastReply> clientLastRequest_myj;
    //给正在等待结果的请求返回结果
    std::unordered_map<long long,std::shared_ptr<LockQueue<notifyChanMsg>>> notifyChan_myj;
    //等待结果超时时间
    int timeout_myj;

};

#endif