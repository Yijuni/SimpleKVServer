#ifndef RAFT_HPP
#define RAFT_HPP
/**
 * 2024-4-24 moyoj
 * Raft算法实现类
 **/
#include "KVRaft.pb.h"
#include "AfterTimer.hpp"
#include "LockQueue.hpp"
#include "Persister.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <mutex>
#include <vector>
#include <memory>
enum STATUS
{
    FOLLOWER = 0,
    LEADER = 1,
    CANDIDATE = 2
};

struct ApplyMsg
{
    bool commandValid;
    kvraft::Command command;
    long long commandIndex;
    long long commandTerm;

    bool snapshotValid;
    std::string data;
    long long snapshotTerm;
    long long snapshotIndex;
};

class KVRaft : public kvraft::KVRaftRPC
{
public:
    KVRaft();
    // 追加日志，其他服务器远程调用
    void AppendEntries(google::protobuf::RpcController *controller,
                       const ::kvraft::AppendEntriesRequest *request,
                       ::kvraft::AppendEntriesResponse *response,
                       ::google::protobuf::Closure *done);
    // 请求投票，其他服务器远程调用
    void RequestVote(google::protobuf::RpcController *controller,
                     const ::kvraft::RequestVoteRequest *request,
                     ::kvraft::RequestVoteResponse *response,
                     ::google::protobuf::Closure *done);
    // 下载快照，leader远程调用
    void InstallSnapshot(google::protobuf::RpcController *controller,
                         const ::kvraft::InstallSnapshotRequest *request,
                         ::kvraft::InstallSnapshotResponse *response,
                         ::google::protobuf::Closure *done);
    // 服务层获取当前raft节点状态的函数
    bool GetState(long long &term);
    // 上层调用，生成快照
    void Snapshot(long long index, std::string &snapshot);
    // 上层追加命令用 参数1：命令 参数2：如果这条命令达成共识他的index是多少 参数3：这条命令追加时服务器的任期
    bool Start(kvraft::Command command, long long &index, long long &term);
    // 启动Raft，参数1：与其他对端通信用的stub 参数2：该服务器名字（唯一标识） 参数3：用来持久化 参数4：往上层提交达成共识的命令、快照
    void Make(std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> &peers, std::string &name, std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyChan);
private:
    // 共享资源的锁
    std::mutex sourceMutex_myj;
    // 快照正在上传时不能追加日志，需要等待
    std::mutex snapshotInstallingMutex_myj;

    // 由上层传入(KVServer)，保存对端的stub
    std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> peers_myj;
    // 本服务器名称，我设置为 ip:port 等效于mit6.824的me
    std::string name_myj;
    // 持久化用的对象
    std::shared_ptr<Persister> persister_myj;

    // 心跳定时器和选举定时器
    AfterTimer *heartbeatsTimer_myj;
    AfterTimer *electionTimer_myj;
    // 心跳间隔、选举超时时间
    double heartbeatsTime_myj;
    double electionTimeout_myj;

    // leader的名字，唯一标识(ip:port)
    std::string leaderid_myj;
    // 当前节点状态
    STATUS status_myj;
    // 往服务层提交数据的channel类似于go的chan，用队列实现的
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan_myj;

    // 达成共识日志的最大下标
    long long commitIndex_myj;
    long long lastApplied_myj;

    // 下一个要发送到某服务器日志的下标和已经匹配的最后一个日志的下标
    std::vector<long long> nextIndex_myj;
    std::vector<long long> matchIndex_myj;

    // 快照最后一个一次操作对应日志的index和term
    long long lastSnapshotIndex_myj;
    long long lastSnapshotTerm_myj;

    // 当前term
    long long currentTerm_myj;
    // 投票对象 空字符串代表没投票
    std::string voterFor_myj;
    // 保存在内存的日志
    std::vector<kvraft::LogEntry> logEntries_myj;
    
    //是否已经初始化完成,只有执行完Make才能响应其他服务器请求
    std::atomic<bool> ready_myj;

    // 持久化函数，参数1：序列化的快照数据
    void persist(std::string &snapshot);
    // 读取持久化信息并反序列化出来 参数1：持久化的数据
    void readPersist(std::string &data);

    // leader选举
    void electStart(std::string name, long long curterm, long long lastLogIndex, long long lastLogTerm, long long peerscount);
    // 发送心跳、追加日志
    void appendEntriesToFollower(long long curterm, long long leaderCommit, long long peerscount);
    // 匹配leader发来的日志
    bool matchNewEntries(const std::vector<kvraft::LogEntry> &entries, long long preLogIndex, long long preLogTerm, kvraft::AppendEntriesResponse *resposne);
    // 往服务层提交达成共识的日志 参数为多少毫秒提交一次
    void applyEntries(long long sleep);
    // 更新大多数节点上达成共识的最大index
    void updateCommitIndex();
    // 将logEntries_myj预处理并序列化到输出流中
    void serializeLogEntriesVector(boost::archive::binary_oarchive &bo);
    // 从输入流反序列化到logEntries_myj
    void deserializeLogEntriesVector(boost::archive::binary_iarchive &bi);
    // 比较两个日志是否一致
    bool compareEntry(kvraft::LogEntry &a, kvraft::LogEntry &b);
};

#endif