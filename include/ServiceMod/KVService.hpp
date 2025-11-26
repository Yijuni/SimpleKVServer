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
#include "ShardCtrlerClient.hpp"
#include "MakeServerStub.hpp"
#include "ShardCtrler.pb.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string_view>
namespace kvserviceclass{
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
    ErrWrongGroup = 5,
    WaitAndRetry = 6,
};
enum ShardState{
    Serving = 1,
    Pulling = 2,
    WaitingDelete = 3,
    Invalid = 4,
};
//分片的状态信息结构体
struct ShardStateInfo{
    long long confignum;
    ShardState state;
    long long gid;
    bool waitingcommit;
    ShardStateInfo(long long confignum_,ShardState state_,long long gid_,bool waitingcommit_):confignum(confignum_),
        state(state_),gid(gid_),waitingcommit(waitingcommit_)
    {}
    ShardStateInfo(){}
    //为了能让boost库的序列化对该结构体生效
    template<typename Archive>
    void serialize(Archive& archive, const unsigned int version)
    {
      archive & BOOST_SERIALIZATION_NVP(confignum);
      archive & BOOST_SERIALIZATION_NVP(state);
      archive & BOOST_SERIALIZATION_NVP(gid);
      archive & BOOST_SERIALIZATION_NVP(waitingcommit);
    }
};
}
namespace CommandType{
constexpr std::string_view Get = "Get";
constexpr std::string_view Append = "Append";
constexpr std::string_view Put = "Put";

constexpr std::string_view ApplyNewConfig = "ApplyNewConfig";
constexpr std::string_view InstallShard = "InstallShard";
constexpr std::string_view DeleteShard = "DeleteShard";
constexpr std::string_view StateChange = "StateChange";
constexpr std::string_view ConfigIncrease = "ConfigIncrease";
constexpr std::string_view InitConfig = "InitConfig";
}

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
        std::shared_ptr<LockQueue<ApplyMsg>> applyChan,int maxraftstate,std::shared_ptr<RocksDBAPI> db,
        std::shared_ptr<ShardCtrlerClient> shard_client, std::shared_ptr<MakeServerStub> make_server_stub,
        int timeout=500,long long gid=0,long long shard_len=10);
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
    void PullShard(google::protobuf::RpcController* controller,
        const kvservice::PullShardRequest* request,
        kvservice::PullShardResponse* response,
        google::protobuf::Closure* done);
    void DeleteShard(google::protobuf::RpcController* controller,
        const kvservice::DeleteShardRequest* request,
        kvservice::DeleteShardResponse* response,
        google::protobuf::Closure* done);
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
    void waitRequestCommit(std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan,kvservice::ResultCode& resultcode,std::string &value);
    
    //持续获取最新配置
    void updateConfig();
    //为每个shard启动一个状态检测器
    void updateShardState();
    //向配置组获取最新配置的函数
    kvraft::Config getNewConfig(long long &leadid,long long confignum);
    //判断某个分片是否由当前服务器处理
    bool isValidKey(const std::string &key);
    // 将字符串的关键字映射到分片号
    long long key2shard(std::string key);

    //初始化配置的命令处理函数
    void initConfig(ApplyMsg applymsg);
    //更新分片的配置号的处理函数，用来增长分片配置号
    void configIncreaseHandler(ApplyMsg applymsg);
    //更新分片的状态处理函数
    void stateChangeHandler(ApplyMsg applymsg);
    //删除分片的处理函数
    void deleteShardHandler(ApplyMsg applymsg);
    //下载分片的处理函数
    void installShardHandler(ApplyMsg applymsg);
    //追加新配置的处理函数
    void newConfigHandler(ApplyMsg applymsg);

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

    //当前提交日志的最高下标，就算某条日志的命令没执行也要记录（可能重复命令），
    // 保存到了rocksdb的raft_cf,来记录该数据库最后执行的日志命令的index，内存一份，数据库一份
    //MAX_COMMIT_INDEX
    long long maxCommitIndex_myj;
    
    // 数据库指针
    std::shared_ptr<RocksDBAPI> db_myj;

    // 2025.8.9 以下内容失效了,直接存储到了rocksdb数据库
    //记录键值对,保存到了rocksdb的kv_cf
    std::unordered_map<std::string,std::string> keyvalue_myj;

    //记录某客户端最后一条请求结果,保存到了rocksdb的client_request_cf
    std::unordered_map<std::string,kvserviceclass::clientLastReply> clientLastRequest_myj;


    //给正在等待结果的请求返回结果
    std::unordered_map<long long,std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>>> notifyChan_myj;
    //等待结果超时时间
    int timeout_myj;
    //当前服务器所处复制组id
    long long gid_myj;
    //记录总共有多少分片
    long long shard_len_myj;

    //以下数据也放进rocksdb数据库的config_cf，内存一份，数据库一份
    // 记录当前最新配置，key:CURRENT_CONFIG
    kvraft::Config curConfig_myj;
    //记录当前服务器的请求号的增长,key:REQUESTID
    long long requestid_myj;
    // 保存所有历史Config,key:CONFIG_NUM
    std::vector<kvraft::Config> configList_myj;
    //记录某分片所处的状态,key:STATE_SHARD_ID
    std::unordered_map<long long,kvserviceclass::ShardStateInfo> shardStateMap_myj;
    //记录某个shard对应的分片是否还存在于当前服务器的数据库,EXIST_SHARD_ID
    std::unordered_map<long long,bool> shardKeysExist_myj;

    //分片服务器客户端，可用来获取配置
    std::shared_ptr<ShardCtrlerClient> shard_client_myj;
    //用来获取某个复制组的某个服务器的连接stub
    std::shared_ptr<MakeServerStub> make_server_stub_myj;
};

#endif