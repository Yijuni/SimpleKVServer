/**
 * 2025-9-30
 * ShardCtrler服务器服务层的service，供相应的客户端调用join leave move query。
 */

#include "ShardCtrler.pb.h"
#include "rocksdbapi.hpp"
#include "Persister.hpp"
#include "LockQueue.hpp"
#include "Raft.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
struct notifyChanMsg{
    long long errid;
    std::string result;
};
//客户端和服务器共用
enum ERRORID{
    OK = 1,
    ErrWrongLeader = 2,
    ErrTimeOut = 3, 
};
class ShardCtrlerService: public shardctrler::ShardCtrlerRPC{
public:
    ShardCtrlerService(std::string name,std::shared_ptr<Persister> persister,std::shared_ptr<KVRaft> raft,
        std::shared_ptr<LockQueue<ApplyMsg>> applyChan,int timeout);
    void Join(google::protobuf::RpcController* controller,const ::shardctrler::JoinRequest* request,
        ::shardctrler::JoinResponse* response,::google::protobuf::Closure* done);    
    void Leave(google::protobuf::RpcController* controller,const ::shardctrler::LeaveRequest* request,
        ::shardctrler::LeaveResponse* response,::google::protobuf::Closure* done);
    void Move(google::protobuf::RpcController* controller,const ::shardctrler::MoveRequest* request,
        ::shardctrler::MoveResponse* response,::google::protobuf::Closure* done);
    void Query(google::protobuf::RpcController* controller,const ::shardctrler::QueryRequest* request,
        ::shardctrler::QueryResponse* response,::google::protobuf::Closure* done);    
private:
//从raft层接受数据
    void applyLogs();
    //处理命令
    void commandApplyHandler(ApplyMsg applymsg);
    //等待请求执行提交
    void waitRequestCommit(std::string& err,bool& wrongleader,shardctrler::Config& config,std::shared_ptr<LockQueue<notifyChanMsg>> notifychan);
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
    //当前提交日志的最高下标，就算某条日志的命令没执行也要记录（可能重复命令）
    long long maxCommitIndex_myj;

};

