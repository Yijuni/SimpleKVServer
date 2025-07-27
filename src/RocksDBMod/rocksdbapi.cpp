#include "rocksdbapi.hpp"
#include <iostream>
RocksDBAPI &RocksDBAPI::GetInstance(std::string db_path)
{
    static RocksDBAPI dbapi(db_path);
    return dbapi;
}

bool RocksDBAPI::RaftMetaPut(const std::string &key, const std::string &value)
{
    if(!db_myj || !raft_cf_myj){
        LOG_ERROR("raft层元数据写入失败，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Put(rocksdb::WriteOptions(),raft_cf_myj,key,value);
    if(!s.ok()){
        LOG_ERROR("raft层原数据写入失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::RaftMetaGet(const std::string &key, std::string &value)
{
    if(!db_myj || !raft_cf_myj){
        LOG_ERROR("raft层元数据读取失败,列族不存在或者数据库没初始化，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Get(rocksdb::ReadOptions(),raft_cf_myj,key,&value);
    if(!s.ok()){
        LOG_ERROR("raft层原数据读取失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::RaftMetaDelete(const std::string &key)
{
    if(!db_myj || !raft_cf_myj){
        LOG_ERROR("raft层元数据删除失败,列族不存在或者数据库没初始化，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Delete(rocksdb::WriteOptions(),raft_cf_myj,key);
    if(!s.ok()){
        LOG_ERROR("raft层原数据删除失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::KVPut(const std::string &key, const std::string &value)
{
    if(!db_myj || !kv_cf_myj){        
        LOG_ERROR("kv层元数据写入失败，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Put(rocksdb::WriteOptions(),kv_cf_myj,key,value);
    if(!s.ok()){
        LOG_ERROR("kv层原数据写入失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::KVGet(const std::string &key, std::string &value)
{
    if(!db_myj || !kv_cf_myj){
        LOG_ERROR("kv层元数据读取失败,列族不存在或者数据库没初始化，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Get(rocksdb::ReadOptions(),kv_cf_myj,key,&value);
    if(!s.ok()){
        LOG_ERROR("kv层原数据读取失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::KVDelete(const std::string &key)
{
    if(!db_myj || !kv_cf_myj){
        LOG_ERROR("kv层元数据删除失败,列族不存在或者数据库没初始化，%s>>%s>>%d",__FILE__,__FUNCTION__,__LINE__);
        return false;
    }
    rocksdb::Status s = db_myj->Delete(rocksdb::WriteOptions(),kv_cf_myj,key);
    if(!s.ok()){
        LOG_ERROR("kv层原数据删除失败，信息:%s",s.ToString().c_str());
        return false;
    }
    return true;
}

bool RocksDBAPI::DBOpen()
{
    LOG_INFO("打开数据库");
    // 打开数据库,传入数据库基础选项,传入列族的描述符，初始化列族句柄
    rocksdb::Status s = rocksdb::DB::Open(options_myj,db_path_myj,cf_desc_myj,&cf_handles_myj,&db_myj);
    if(!s.ok()){
        LOG_ERROR("rocksdb打开失败，失败原因:%s",s.ToString().c_str());
        db_myj = nullptr;
        return false;
    }
    
    // 把创建的列族句柄单独保存下来
    for(int i=0;i<cf_handles_myj.size();i++){
        if(cf_handles_myj[i]->GetName()=="raft_cf"){
            raft_cf_myj = cf_handles_myj[i];
        }
        if(cf_handles_myj[i]->GetName()=="kv_cf"){
            kv_cf_myj = cf_handles_myj[i];
        }
    }
    return true;
}

RocksDBAPI::~RocksDBAPI()
{
    LOG_INFO("rocksdb关闭");
    if(db_myj)
    {
        for(auto* cfh:cf_handles_myj){
            if(cfh){
                db_myj->DestroyColumnFamilyHandle(cfh);
            }
        }
        cf_handles_myj.clear();
        delete db_myj;
        db_myj = nullptr;
    }
}
RocksDBAPI::RocksDBAPI(std::string &db_path) : db_myj(nullptr), raft_cf_myj(nullptr), kv_cf_myj(nullptr)
{
    LOG_INFO("开始初始化Rocksdb");
    db_path_myj = db_path;
    
    // 没有数据库就创建
    options_myj.create_if_missing = true;
    // 没有对应的列族就创建
    options_myj.create_missing_column_families = true;
    // 列族名称
    std::vector<std::string> cf_names = {"default","raft_cf","kv_cf"}; 
    // 构建列族的描述符，包括列名和对应的选项
    for(auto& name : cf_names){
        cf_desc_myj.emplace_back(name,rocksdb::ColumnFamilyOptions());
    }

}
