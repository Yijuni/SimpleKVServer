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
    // 开启数据库
    bool DBOpen();
    // 获取某个时间点rocksdb的所有kv数据
    std::unordered_map<std::string,std::string> GenerateKVSnapshot();
    // 下载数据到当前rocksdb
    void InstallKVSnapshot(std::unordered_map<std::string,std::string>&);
private:
    // raft层操作时临界区锁
    std::mutex db_raft_mutex_myj;
    // service层操作时的临界区锁
    std::mutex db_service_mutex_myj;
    // client请求信息的锁
    std::mutex db_client_request_mutex_myj;
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
    rocksdb::ColumnFamilyHandle* raft_cf_myj;
    rocksdb::ColumnFamilyHandle* kv_cf_myj;
    rocksdb::ColumnFamilyHandle* client_request_cf_myj;

};