#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <string>
class RocksDBAPI{
public:
    RocksDBAPI& GetInstance(std::string db_path="../../db");
    // 存放Raft层持久化的元数据 term、voteFor、logs、snapshot等
    bool RaftMetaPut(const std::string& key,const std::string& value);
    // Raft层获取源数据
    bool RaftMetaGet(const std::string& key,std::string& value);
    // Raft层删除数据
    bool RaftMetaDelete(const std::string& key);
    
    // 存放KV数据
    bool KVPut(const std::string& key,const std::string& value);
    // 获取KV数据
    bool KVGet(const std::string& key);
    // 删除KV数据
    bool KVDelete(const std::string& key);
private:
    RocksDBAPI();
    ~RocksDBAPI();
    RocksDBAPI(std::string db_path);
    RocksDBAPI(const RocksDBAPI&) = delete;
    RocksDBAPI& operator=(const RocksDBAPI&) = delete;

    std::string db_path_myj;
    rocksdb::DB* db_myj;
    rocksdb::ColumnFamilyHandle 
};