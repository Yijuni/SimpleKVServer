#ifndef ROCKSDBAPI_H
#define ROCKSDBAPI_H
#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/options.h>
#include <string>
#include <Logger.hpp>
#include <vector>
#include <unordered_map>
class RocksDBAPI{
public:
    ~RocksDBAPI();
    RocksDBAPI(const std::string& db_path="./db");
    // 可以自己选择db路径
    void SetPath(const std::string& db_path);
    // 存放Raft层持久化的元数据 term、voteFor、logs、snapshot等
    bool RaftMetaPut(const std::string& key,const std::string& value);
    // Raft层获取源数据
    bool RaftMetaGet(const std::string& key,std::string& value);
    // Raft层删除数据
    bool RaftMetaDelete(const std::string& key);

    //key的数据都以SHARD_id_key的形式存储，例如"Hello world"对应的shardid为2，则key为SHARD_00002_Hello world,并且id是5位左补0
    // 存放KV数据
    bool KVPut(const std::string& key,const std::string& value);
    // 获取KV数据
    bool KVGet(const std::string& key, std::string &value);
    // 删除KV数据
    bool KVDelete(const std::string& key);

    // 存放client请求信息
    bool ClientRequestPut(const std::string& key,const std::string& value);
    // 获取client请求信息
    bool ClientRequestGet(const std::string& key, std::string &value);
    // 删除client请求信息
    bool ClientRequestDelete(const std::string& key);

    // 配置的key都以CONFIG_id的形式存储，value都是序列化后的Config，例如配置1的key就是CONFIG_1,value就是序列化后的Config
    // shard的存活信息的key都以EXIST_SHARD_id的形式存储，value只有"false"或者"true"，例如EXIST_SHARD_1    false
    // shard的状态信息的key都以STATE_SHARD_id的形式存储，value都是序列化后的分片状态信息，例如STATE_SHARD_1   data
    //获取config_cf下的数据
    bool ConfigMetaGet(const std::string& key,std::string& value);
    //设置config_cf下的数据
    bool ConfigMetaPut(const std::string& key,const std::string& value);
    //删除config_cf下的数据
    bool ConfigMetaDelete(const std::string& key);

    // 开启数据库
    bool DBOpen();
    // 获取某个时间点rocksdb的所有kv数据
    std::unordered_map<std::string,std::string> GenerateKVSnapshot();
    // 下载KV数据到当前rocksdb
    bool InstallKVSnapshot(std::unordered_map<std::string,std::string>&);
    // 获取某个时间点rocksdb的所有客户端请求信息
    std::unordered_map<std::string,std::string> GenerateClientRequestSnapshot();
    // 下载客户端请求信息数据到当前rocksdb
    bool InstallClientRequestSnapshot(std::unordered_map<std::string,std::string>&);
    //删除某个分片下的所有KV数据
    bool DeleteShardKV(long long shardid);

private:
    // raft层操作时临界区锁
    std::mutex db_raft_mutex_myj;
    // service层操作时的临界区锁
    std::mutex db_service_mutex_myj;
    // client请求信息的锁
    std::mutex db_client_request_mutex_myj;
    // config信息的锁
    std::mutex db_config_mutex_myj;
    // 数据库保存路径
    std::string db_path_myj;
    // 数据库实例指针
    rocksdb::DB* db_myj;
    // 数据库选项配置
    rocksdb::Options options_myj;
    // ColumnFamilyHandle是RocksDB 中用于操作列族（Column Family）的句柄，
    // 列族是 RocksDB 中用于对数据进行逻辑分组的机制，类似数据库中的 “表”。
    // 列族句柄，用来操作的
    std::vector<rocksdb::ColumnFamilyHandle*> cf_handles_myj;
    // 列族描述符
    std::vector<rocksdb::ColumnFamilyDescriptor> cf_desc_myj;

    //存储raft元数据用的列族
    rocksdb::ColumnFamilyHandle* raft_cf_myj;
    //存储kv数据的列族
    rocksdb::ColumnFamilyHandle* kv_cf_myj;
    //存储kv操作客户端请求的列族
    rocksdb::ColumnFamilyHandle* client_request_cf_myj;
    //用来存储，分片状态、所有分片配置和用来判断当前分片是否存在的map
    rocksdb::ColumnFamilyHandle* config_cf_myj;
};

#endif