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

struct clientLastReply
{
    long long requestid;
    shardctrler::Config replyMsg;
    clientLastReply(long long id, shardctrler::Config replymsg) : requestid(id), replyMsg(replymsg)
    {
    }
    clientLastReply() {}

    // 分片配置控制器没快照，也不需要序列化客户最有一次请求信息。
    // template<typename Archive>
    // void serialize(Archive& archive, const unsigned int version)
    // {
    //   archive & BOOST_SERIALIZATION_NVP(requestid);
    //   archive & BOOST_SERIALIZATION_NVP(replyMsg);
    // }
};
// 客户端和服务器共用
enum ERRORID
{
    OK = 1,
    ErrWrongLeader = 2,
    ErrTimeOut = 3,
};
struct notifyChanMsg
{
    ERRORID errid;
    shardctrler::Config config;
};
class ShardCtrlerService : public shardctrler::ShardCtrlerRPC
{
public:
    /// @brief
    /// @param name 服务器名称，ip:port
    /// @param raft raft层实例
    /// @param applyChan raft层往服务层提交共识日志的channel
    /// @param timeout 客户端请求超时时间
    ShardCtrlerService(std::string name, std::shared_ptr<KVRaft> raft, std::shared_ptr<LockQueue<ApplyMsg>> applyChan, int timeout);
    void Join(google::protobuf::RpcController *controller, const ::shardctrler::JoinRequest *request,
              ::shardctrler::JoinResponse *response, ::google::protobuf::Closure *done);
    void Leave(google::protobuf::RpcController *controller, const ::shardctrler::LeaveRequest *request,
               ::shardctrler::LeaveResponse *response, ::google::protobuf::Closure *done);
    void Move(google::protobuf::RpcController *controller, const ::shardctrler::MoveRequest *request,
              ::shardctrler::MoveResponse *response, ::google::protobuf::Closure *done);
    void Query(google::protobuf::RpcController *controller, const ::shardctrler::QueryRequest *request,
               ::shardctrler::QueryResponse *response, ::google::protobuf::Closure *done);

private:
    // 从raft层接受数据
    void applyLogs();
    // 处理命令
    void commandApplyHandler(ApplyMsg applymsg);
    // 等待请求执行提交
    void waitRequestCommit(ERRORID &err, bool &wrongleader, shardctrler::Config &config, std::shared_ptr<LockQueue<notifyChanMsg>> notifychan);
    // 处理join请求
    void joinHandler(std::unordered_map<long long, std::vector<std::string>> &groups);
    // 处理leave请求
    void leaveHandler(std::vector<long long> &gids);
    //  处理move请求
    void moveHandler(long long &gid, long long &shrad);
    // 处理Query请求
    shardctrler::Config queryHandler(long long num);
    std::string name_myj;
    // raft层给服务层上交日志用
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan_myj;
    // 追加日志、生成快照用
    std::shared_ptr<KVRaft> raft_myj;
    // 临界资源锁
    std::mutex sourceMutex_myj;
    // 起否初始化完成,是否还在运行
    std::atomic<bool> ready_myj;
    // 当前提交日志的最高下标，就算某条日志的命令没执行也要记录（可能重复命令）
    long long maxCommitIndex_myj;
    // 等待结果超时时间
    int timeout_myj;
    // 记录某客户端最后一条请求结果
    std::unordered_map<std::string, clientLastReply> clientLastRequest_myj;
    // 给正在等待结果的请求返回结果
    std::unordered_map<long long, std::shared_ptr<LockQueue<notifyChanMsg>>> notifyChan_myj;
};
