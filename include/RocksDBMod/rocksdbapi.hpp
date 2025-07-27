#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/options.h>
#include <string>
#include <Logger.hpp>
#include <vector>
class RocksDBAPI{
public:
    static RocksDBAPI& GetInstance(std::string db_path="./db");

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
    // 开启数据库
    bool DBOpen();
    
private:
    RocksDBAPI();
    ~RocksDBAPI();
    RocksDBAPI(std::string& db_path);
    RocksDBAPI(const RocksDBAPI&) = delete;
    RocksDBAPI& operator=(const RocksDBAPI&) = delete;

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

};