#include "KVService.hpp"
#include "Logger.hpp"
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <sstream>
KVService::KVService()
{
}
KVService::KVService(std::string name, std::shared_ptr<Persister> persister, std::shared_ptr<KVRaft> raft,
                     std::shared_ptr<LockQueue<ApplyMsg>> applyChan, int maxraftstate, std::shared_ptr<RocksDBAPI> db,
                     std::shared_ptr<ShardCtrlerClient> shard_client, std::shared_ptr<MakeServerStub> make_server_stub,
                     int timeout, long long gid, long long shard_len)
    : name_myj(name), persister_myj(persister), raft_myj(raft), applyChan_myj(applyChan), ready_myj(false),
      timeout_myj(timeout), maxraftstate_myj(maxraftstate), snapshoting_myj(false),
      maxCommitIndex_myj(-1), gid_myj(gid), requestid_myj(0),
      shard_len_myj(shard_len), shard_client_myj(shard_client), make_server_stub_myj(make_server_stub)
{
    db_myj = db;
    // readPersist(persister_myj->ReadSnapshot());
    // 为了避免重启之后，重复执行已经在db执行过的操作，每次重启后读取最大已提交日志的下标
    std::string max_commit;
    std::string request_id;
    std::string config_data;

    // 获取服务器最后执行的命令对应的index
    if (db_myj->ConfigMetaGet("MAX_COMMIT_INDEX", max_commit))
    {
        maxCommitIndex_myj = std::stoll(max_commit);
    }
    // 获取服务器的最后一次请求ID
    if (db_myj->ConfigMetaGet("REQUESTID", request_id))
    {
        requestid_myj = std::stoll(request_id);
    }
    // 初始化当前最新配置
    if (db_myj->ConfigMetaGet("CURRENT_CONFIG", config_data))
    {
        curConfig_myj.ParseFromString(config_data);
        // 存在CURRENT_CONFIG,数据库就一定存在CONFIG列表信息
        for (long long confignum = 0; confignum <= curConfig_myj.num(); confignum++)
        {
            std::string config_key = "CONFIG_" + std::to_string(confignum);
            db_myj->ConfigMetaGet(config_key, config_data);
            kvraft::Config config;
            config.ParseFromString(config_data);
            configList_myj.push_back(config);
        }
    }
    else
    {
        curConfig_myj = kvraft::Config();
        curConfig_myj.set_num(-1);
    }
    // 初始化分片状态和存活状态
    for (long long shardid = 0; shardid < shard_len; shardid++)
    {
        std::string shard_key = "STATE_SHARD_" + std::to_string(shardid);
        std::string shard_info_data;
        // 如果没有该SHARD信息，则创建新的
        if (!db_myj->ConfigMetaGet(shard_key, shard_info_data))
        {
            shardStateMap_myj[shardid] = kvserviceclass::ShardStateInfo{0, kvserviceclass::ShardState::Invalid, gid, false};
            std::ostringstream oss;
            boost::archive::binary_oarchive bos(oss);
            bos << shardStateMap_myj[shardid];
            db_myj->ConfigMetaPut(shard_key, oss.str());
        }
        else
        {
            kvserviceclass::ShardStateInfo shard_info;
            std::istringstream iss(shard_info_data);
            boost::archive::binary_iarchive bis(iss);
            bis >> shard_info;
            shardStateMap_myj[shardid] = shard_info;
        }

        // 获取分片存活信息
        shard_key = "EXIST_SHARD_" + std::to_string(shardid);
        if (!db_myj->ConfigMetaGet(shard_key, shard_info_data))
        {
            shardKeysExist_myj[shardid] = false;
            db_myj->ConfigMetaPut(shard_key, "false");
        }
        else
        {
            if (shard_info_data == "true")
            {
                shardKeysExist_myj[shardid] = true;
            }
            if (shard_info_data == "false")
            {
                shardKeysExist_myj[shardid] = false;
            }
        }
    }

    ready_myj = true;
    std::thread td(std::bind(&KVService::applyLogs, this));
    td.detach();
}

void KVService::Get(google::protobuf::RpcController *controller, const ::kvservice::GetRequest *request, ::kvservice::GetResponse *response, ::google::protobuf::Closure *done)
{
    std::string clientid = request->clientid();
    long long requestid = request->requestid();

    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    LOG_INFO("server[%s]>>收到Get请求,clientid[%s],requestid[%lld]", name_myj.c_str(), clientid.c_str(), requestid);

    std::string requestinfo;
    // 获取请求信息并反序列化出来
    if (db_myj->ClientRequestGet(clientid, requestinfo))
    {
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        kvserviceclass::clientLastReply clr;
        bis >> clr;
        if (clr.requestid >= request->requestid())
        {
            response->mutable_resultcode()->set_errorcode(kvserviceclass::OK);
            response->set_value(clr.replyMsg);
            done->Run();
            return;
        }
    }

    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_clientid(clientid);
    command.set_requestid(requestid);
    command.set_key(request->key());
    command.set_type(CommandType::Get.data());

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(kvserviceclass::ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<kvserviceclass::notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;
    response->set_value(value);

    std::thread td([&, logindex]()
                   {
        std::unique_lock<std::mutex> locktmp(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();

    done->Run();
}

void KVService::Put(google::protobuf::RpcController *controller, const ::kvservice::PutAppendRequest *request, ::kvservice::PutAppendResponse *response, ::google::protobuf::Closure *done)
{
    std::string clientid = request->clientid();
    long long requestid = request->requestid();

    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    LOG_INFO("server[%s]>>收到Put请求,clientid[%s],requestid[%lld]", name_myj.c_str(), clientid.c_str(), requestid);

    std::string requestinfo;
    // 获取请求信息并反序列化出来
    if (db_myj->ClientRequestGet(clientid, requestinfo))
    {
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        kvserviceclass::clientLastReply clr;
        bis >> clr;
        if (clr.requestid >= request->requestid())
        {
            response->mutable_resultcode()->set_errorcode(kvserviceclass::OK);
            done->Run();
            return;
        }
    }

    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_clientid(clientid);
    command.set_requestid(requestid);
    command.set_key(request->key());
    command.set_value(request->value());
    command.set_type(CommandType::Put.data());

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(kvserviceclass::ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<kvserviceclass::notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;

    std::thread td([&, logindex]()
                   {
        std::unique_lock<std::mutex> locktmp(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();
    done->Run();
}

void KVService::Append(google::protobuf::RpcController *controller, const ::kvservice::PutAppendRequest *request, ::kvservice::PutAppendResponse *response, ::google::protobuf::Closure *done)
{
    std::string clientid = request->clientid();
    long long requestid = request->requestid();

    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    LOG_INFO("server[%s]>>收到Append请求,clientid[%s],requestid[%lld]", name_myj.c_str(), clientid.c_str(), requestid);

    std::string requestinfo;
    // 获取请求信息并反序列化出来
    if (db_myj->ClientRequestGet(clientid, requestinfo))
    {
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        kvserviceclass::clientLastReply clr;
        bis >> clr;
        if (clr.requestid >= request->requestid())
        {
            response->mutable_resultcode()->set_errorcode(kvserviceclass::OK);
            done->Run();
            return;
        }
    }

    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_clientid(clientid);
    command.set_requestid(requestid);
    command.set_key(request->key());
    command.set_value(request->value());
    command.set_type(CommandType::Append.data());

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(kvserviceclass::ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<kvserviceclass::notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;

    std::thread td([&, logindex]()
                   {
        std::unique_lock<std::mutex> locktmp(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();

    done->Run();
}

void KVService::PullShard(google::protobuf::RpcController *controller, const kvservice::PullShardRequest *request, kvservice::PullShardResponse *response, google::protobuf::Closure *done)
{
}

void KVService::DeleteShard(google::protobuf::RpcController *controller, const kvservice::DeleteShardRequest *request, kvservice::DeleteShardResponse *response, google::protobuf::Closure *done)
{
}

void KVService::applyLogs()
{
    while (ready_myj)
    {
        ApplyMsg applymsg = applyChan_myj->pop();
        if (applymsg.commandValid)
        {
            std::string type = applymsg.command.type();
            if (type == CommandType::ApplyNewConfig)
            {
                newConfigHandler(applymsg);
                continue;
            }
            if (type == CommandType::InstallShard)
            {
                installShardHandler(applymsg);
                continue;
            }
            if (type == CommandType::DeleteShard)
            {
                deleteShardHandler(applymsg);
                continue;
            }
            if (type == CommandType::StateChange)
            {
                stateChangeHandler(applymsg);
                continue;
            }
            if (type == CommandType::ConfigIncrease)
            {
                configIncreaseHandler(applymsg);
                continue;
            }
            if (type == CommandType::InitConfig)
            {
                initConfig(applymsg);
                continue;
            }
            commandApplyHandler(applymsg);
        }
        else if (applymsg.snapshotValid)
        {
            snapshotHandler(applymsg);
        }
    }
}

void KVService::snapshot(long long logindex)
{
    snapshoting_myj = true;
    double datalen = persister_myj->RaftStateSize();
    if (datalen / (1.0 * maxraftstate_myj) >= 0.9)
    {
        LOG_INFO("server[%s]>>开始生成快照，快照最后命令的index = %lld,datalen[%f],maxraftsize[%lld]", name_myj.c_str(), logindex, datalen, maxraftstate_myj);
        std::ostringstream oss;
        boost::archive::binary_oarchive bos(oss);
        // 生成KV快照
        std::unordered_map<std::string, std::string> kvmap = db_myj->GenerateKVSnapshot();
        // 生成client_request快照
        std::unordered_map<std::string, std::string> client_request = db_myj->GenerateClientRequestSnapshot();
        // 记录当前执行的最后一条命令的index
        bos << maxCommitIndex_myj;
        // 当前kv数据
        bos << kvmap;
        // 记录客户端回应结果
        bos << client_request;

        // 分片活性表快照化
        bos << shardKeysExist_myj;
        // 分片状态表快照化
        std::unordered_map<long long, std::string> shardstatemap;
        for (auto &iter : shardStateMap_myj)
        {
            kvserviceclass::ShardStateInfo info = iter.second;
            std::ostringstream oss_tmp;
            boost::archive::binary_oarchive bos_tmp(oss_tmp);
            bos_tmp << info;
            shardstatemap[iter.first] = oss_tmp.str();
        }
        bos << shardstatemap;
        // 配置列表持久化
        std::vector<std::string> config_str_list;
        for (int i = 0; i < configList_myj.size(); i++)
        {
            std::string config_str;
            configList_myj[i].SerializeToString(&config_str);
            config_str_list.emplace_back(config_str);
        }
        bos << config_str_list;
        // 当前最新配置持久化
        std::string curconfig_str;
        curConfig_myj.SerializeToString(&curconfig_str);
        bos << curconfig_str;

        std::string data = oss.str();
        
        std::thread([&](std::string data_tmp,long long logindex_tmp){
            raft_myj->Snapshot(logindex_tmp, data_tmp);
            std::unique_lock<std::mutex> lock(sourceMutex_myj);
            snapshoting_myj = false;
        },data,logindex);
    }else{
        snapshoting_myj = false;
    }
}

// 2025.8.4 这个函数没有存在的必要了
void KVService::readPersist(std::string data)
{
    if (data.size() == 0)
    {
        return;
    }
    LOG_INFO("server[%s]启动！", name_myj.c_str());
    std::istringstream iss(data);
    boost::archive::binary_iarchive bis(iss);
    std::unordered_map<std::string, std::string> kvmap;
    std::unordered_map<std::string, std::string> client_request;
    bis >> maxCommitIndex_myj;
    bis >> kvmap;
    bis >> client_request;
}

void KVService::commandApplyHandler(ApplyMsg applymsg)
{
    long long logterm = applymsg.commandTerm;
    long long logindex = applymsg.commandIndex;
    std::string clientid = applymsg.command.clientid();
    long long requestid = applymsg.command.requestid();
    std::string optype = applymsg.command.type();
    std::string key = applymsg.command.key();
    std::string value = applymsg.command.value();

    LOG_INFO("server[%s]>> 开始提交的命令,receive commit command index:%lld,clientid[%s],requestid[%lld],optype[%s],key[%s],value[%s]", name_myj.c_str(), logindex, clientid.c_str(), requestid, optype.c_str(), key.c_str(), value.c_str());

    std::unique_lock<std::mutex> lock(sourceMutex_myj);

    if (logindex <= maxCommitIndex_myj)
    {
        LOG_INFO("server[%s]>>maxCommitIndex[%lld],current log index[%lld]已经执行过", name_myj.c_str(), maxCommitIndex_myj, logindex);
        return;
    }

    // 获取当前指令的客户端最后一个请求的requestid
    std::string requestinfo;
    kvserviceclass::clientLastReply lastReply;
    bool existFlag = false;
    if ((existFlag = db_myj->ClientRequestGet(clientid, requestinfo)))
    {
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        bis >> lastReply;
    }

    // 当前key对应的value
    std::string curValue;
    char skey[key.size() + 15];
    sprintf(skey, "SHARD_%05lld_%s", key2shard(key), key.c_str());
    std::string shard_key(skey);
    db_myj->KVGet(shard_key, curValue);
    // 当前指令已经执行过
    if (existFlag && lastReply.requestid >= requestid)
    {
        maxCommitIndex_myj = logindex;
        // 为了避免重启之后，重复执行已经在db执行过的操作，需要保存最大的已执行日志的下标
        db_myj->ConfigMetaPut("MAX_COMMIT_INDEX", std::to_string(maxCommitIndex_myj));
        if (maxraftstate_myj != -1)
        {
            if (!snapshoting_myj)
            {
                // 判断是否生成快照
                snapshot(logindex);
            }
        }

        return;
    }
    else
    {
        if (optype == "Append")
        {
            // 获取原始数据，然后拼接
            curValue += value;
            db_myj->KVPut(shard_key, curValue);
            LOG_INFO("server[%s]>>KEY[%s],VALUE[%s]", name_myj.c_str(), shard_key.c_str(), curValue.c_str());
        }
        if (optype == "Put")
        {
            curValue = value;
            db_myj->KVPut(shard_key, curValue);
            LOG_INFO("server[%s]>>KEY[%s],VALUE[%s]", name_myj.c_str(), shard_key.c_str(), curValue.c_str());
        }
        // 更新客户端最后请求信息
        std::ostringstream oss;
        boost::archive::binary_oarchive obs(oss);
        lastReply = kvserviceclass::clientLastReply(requestid, curValue);
        obs << lastReply;
        std::string data = oss.str();
        db_myj->ClientRequestPut(clientid, data);
    }
    maxCommitIndex_myj = logindex;
    // 为了避免重启之后，重复执行已经在db执行过的操作，需要保存最大的已执行日志的下标
    db_myj->ConfigMetaPut("MAX_COMMIT_INDEX", std::to_string(maxCommitIndex_myj));
    if (maxraftstate_myj != -1)
    {
        if (!snapshoting_myj)
        {
            // 判断是否生成快照
            snapshot(logindex);
        }
    }

    auto notifyChanIter = notifyChan_myj.find(logindex);

    if (notifyChanIter != notifyChan_myj.end())
    {
        kvserviceclass::notifyChanMsg notifymsg;
        notifymsg.result = curValue;
        notifymsg.errid = kvserviceclass::OK;

        std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan = notifyChanIter->second;
        lock.unlock();

        long long term;
        bool isleader = raft_myj->GetState(term);
        if (!isleader)
        {
            notifymsg.errid = kvserviceclass::ErrWrongLeader;
            notifymsg.result = "当前服务器不是leader";
        }

        // 如果term不同不能提交，因为该命令的结果，可能不是当前正在等待的请求的结果
        if (term == logterm)
        {
            std::thread td(
                [notifychan, notifymsg]()
                {
                    notifychan->push(notifymsg);
                });
            td.detach();
        }
    }
}

void KVService::snapshotHandler(ApplyMsg applymsg)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (maxCommitIndex_myj >= applymsg.snapshotIndex)
    {
        return;
    }
    LOG_INFO("server[%s]>>开始处理提交的快照,lastLogIndex[%lld]", name_myj.c_str(), applymsg.snapshotIndex);
    std::string data = applymsg.data;
    if (data == "")
    {
        return;
    }
    std::unordered_map<std::string, std::string> kvmap;
    std::unordered_map<std::string, std::string> client_request;
    std::istringstream iss(data);
    boost::archive::binary_iarchive bis(iss);
    bis >> maxCommitIndex_myj;
    bis >> kvmap;
    bis >> client_request;

    // 下载leader传来的快照，更新到本地数据库
    db_myj->InstallKVSnapshot(kvmap);
    db_myj->InstallClientRequestSnapshot(client_request);
    // 更新已提交日志的最大下标
    db_myj->ConfigMetaPut("MAX_COMMIT_INDEX", std::to_string(maxCommitIndex_myj));

    // 反序列化分片状态表
    bis >> shardKeysExist_myj;
    // 分片状态表反序列化
    std::unordered_map<long long, std::string> shardstatemap;
    bis >> shardstatemap;
    shardStateMap_myj.clear();
    for (auto &iter : shardstatemap)
    {
        // 本地数据库更新
        std::string shard_state_key = "STATE_SHARD_" + std::to_string(iter.first);
        db_myj->ConfigMetaPut(shard_state_key, iter.second);

        kvserviceclass::ShardStateInfo info;
        std::istringstream iss_tmp(iter.second);
        boost::archive::binary_iarchive bis_tmp(iss_tmp);
        bis_tmp >> info;
        shardStateMap_myj[iter.first] = info;
    }
    // 配置列表反序列化
    std::vector<std::string> config_str_list;
    bis >> config_str_list;
    configList_myj.clear();
    for (int i = 0; i < config_str_list.size(); i++)
    {
        // 本地数据库更新
        std::string config_key = "CONFIG_" + std::to_string(i);
        db_myj->ConfigMetaPut(config_key, config_str_list[i]);

        kvraft::Config config;
        config.ParseFromString(config_str_list[i]);
        configList_myj.emplace_back(config);
    }
    // 当前最新配置反序列化
    std::string cur_config_str;
    bis >> cur_config_str;
    std::string cur_config_key = "CURRENT_CONFIG";
    db_myj->ConfigMetaPut(cur_config_key,cur_config_str);
    curConfig_myj.ParseFromString(cur_config_str);
}

void KVService::waitRequestCommit(std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychan, kvservice::ResultCode &resultcode, std::string &value)
{

    AfterTimer waittimeout(500, 0,
                           std::bind(
                               [](std::shared_ptr<LockQueue<kvserviceclass::notifyChanMsg>> notifychantmp)
                               {
                                   kvserviceclass::notifyChanMsg notifymsg;
                                   notifymsg.errid = kvserviceclass::ErrTimeOut;
                                   notifymsg.result = "等待时间超时";
                                   notifychantmp->push(notifymsg);
                                   LOG_INFO("TEST WAIT TIME");
                               },
                               notifychan));
    waittimeout.Reset();

    kvserviceclass::notifyChanMsg notifymsg = notifychan->pop();

    resultcode.set_errorcode(notifymsg.errid);
    if (notifymsg.errid == kvserviceclass::OK)
    {
        value = notifymsg.result;
    }
    else
    {
        resultcode.set_errormsg(notifymsg.result);
    }
}

void KVService::updateConfig()
{
    long long leaderid = 0;
    while(ready_myj){
        long long term;
        bool isleader = raft_myj->GetState(term);
        while(!isleader){
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            isleader = raft_myj->GetState(term);
        }
        kvraft::Config newConfig = getNewConfig(leaderid,curConfig_myj.num()+1);
        std::unique_lock<std::mutex> lock(sourceMutex_myj);
        bool flag = false;
        if(newConfig.num()==curConfig_myj.num()+1){
            flag = true;
        }else{
            flag = false;
        }
        if(flag){
            LOG_INFO("server{%lld,%s}>>获取新的配置ConfigNum:%lld",gid_myj,name_myj.c_str(),newConfig.num());
            lock.unlock();
            kvraft::Command command;
            command.set_type(std::string(CommandType::ApplyNewConfig));
            auto mconfig = command.mutable_newconfig();
            mconfig->set_num(newConfig.num());
            for(int i=0;i<newConfig.shards_size();i++){
                mconfig->add_shards(newConfig.shards(i));
                LOG_INFO("server{%lld,%s}>>shardid:%d,gid:%lld",gid_myj,name_myj.c_str(),i,newConfig.shards(i));
            }
            auto groups = mconfig->mutable_groups();
            for(auto kv:newConfig.groups()){
                kvraft::Servers servers;
                long long groupid = kv.first;
                LOG_INFO("server{%lld,%s}>>group id:%lld",gid_myj,name_myj.c_str(),groupid);
                for(int i=0;i<kv.second.serversname_size();i++){
                    servers.add_serversname(kv.second.serversname(i));
                    LOG_INFO("server{%lld,%s}>>server name:%s",gid_myj,name_myj.c_str(),kv.second.serversname(i).c_str());
                }
                groups->insert({groupid,servers});
            }
            long long logindex;
            long long logterm;
            raft_myj->Start(command,logindex,logterm);
        }else{
            LOG_INFO("server{%lld,%s}>>获取新的配置ConfigNum:%lld,不是更新的配置",gid_myj,name_myj.c_str(),newConfig.num());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void KVService::updateShardState()
{
}

kvraft::Config KVService::getNewConfig(long long &leadid, long long confignum)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    shardctrler::Config newConfig; 
    kvraft::Config config;
    if(shard_client_myj->Query(confignum,newConfig)){
        LOG_INFO("成功获取新日志,num::%lld",confignum);
        config.set_num(newConfig.num());
        for(int i=0;i<newConfig.shards_size();i++){
            config.add_shards(newConfig.shards(i));
        }
        auto groups = config.mutable_groups();
        for(auto kv:newConfig.groups()){
            kvraft::Servers servers;
            long long groupid = kv.first;
            for(int i=0;i<kv.second.serversname_size();i++){
                servers.add_serversname(kv.second.serversname(i));
            }
            groups->insert({groupid,servers});
        }
    }else{
        LOG_ERROR("获取日志失败,num:%lld",confignum);
    }
    return config;
}

bool KVService::isValidKey(const std::string &key)
{
    long long shardid = key2shard(key);
    bool flag = shardStateMap_myj[shardid].state == kvserviceclass::ShardState::Serving;
    if(flag){
        //更新一下存在信息
        std::string exist_key = "EXIST_SHARD_"+std::to_string(shardid);
        std::string value = "true";
        db_myj->ConfigMetaPut(exist_key,value);
        shardKeysExist_myj[shardid] = true;
    }
    return flag;   
}

long long KVService::key2shard(std::string key)
{
    return std::hash<std::string>{}(key) % shard_len_myj;
}

void KVService::initConfig(ApplyMsg applymsg)
{
    kvraft::Command command = applymsg.command;
    long long shardid = command.shardid();
    long long newgid = command.gid();
    int state = command.newstate();
    long long confignum = command.confignum();
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    kvserviceclass::ShardStateInfo stateinfo = shardStateMap_myj[shardid];
    if(stateinfo.confignum==0){
        LOG_INFO("server{%lld,%s}>>shard:%lld，初始化状态，状态为：%d,组号：%lld",gid_myj,name_myj.c_str(),state,newgid);
        stateinfo.confignum = confignum;
        stateinfo.gid = newgid;
        stateinfo.state = kvserviceclass::ShardState(state);
        stateinfo.waitingcommit = false;
        if(stateinfo.state==kvserviceclass::ShardState::Serving){
            //更新分片详细状态
            shardStateMap_myj[shardid] = stateinfo;
            std::ostringstream oss;
            boost::archive::binary_oarchive bos(oss);
            bos << stateinfo;
            std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
            db_myj->ConfigMetaPut(state_key,oss.str());

            //更新分片存活状态
            shardKeysExist_myj[shardid] = true;
            std::string exist_key = "EXIST_SHARD_"+std::to_string(shardid);
            db_myj->ConfigMetaPut(exist_key,"true");
        }
    }
    if(maxraftstate_myj!=-1){
        if(!snapshoting_myj){
            snapshot(applymsg.commandIndex);
        }
    }
}

void KVService::configIncreaseHandler(ApplyMsg applymsg)
{
    kvraft::Command command = applymsg.command;
    long long shardid = command.shardid();
    long long confignum = command.confignum();
    long long newgid = command.gid();

    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    kvserviceclass::ShardStateInfo stateinfo = shardStateMap_myj[shardid];
    if(stateinfo.confignum+1 == confignum){
        LOG_INFO("server{%lld,%s}>>shard[%lld]的config版本提升至%lld,状态仍然为%d",gid_myj,name_myj.c_str(),confignum,int(stateinfo.state));
        stateinfo.confignum = confignum;
        stateinfo.gid = newgid;
        stateinfo.waitingcommit = false;
        shardStateMap_myj[shardid] = stateinfo;

        //更新数据库
        std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
        std::ostringstream oss;
        boost::archive::binary_oarchive bos(oss);
        bos << stateinfo;
        db_myj->ConfigMetaPut(state_key,oss.str());
    }else{
        LOG_INFO("server{%lld,%s}>>shard[%lld]的config版本提升至%lld失败,当前config num %lld",gid_myj,name_myj.c_str(),confignum,curConfig_myj.num());
    }
    if(maxraftstate_myj!=-1){
        if(!snapshoting_myj){
            snapshot(applymsg.commandIndex);
        }
    }   
}

void KVService::stateChangeHandler(ApplyMsg applymsg)
{
    kvraft::Command command = applymsg.command;
    long long newgid = command.gid();
    kvserviceclass::ShardState newstate = kvserviceclass::ShardState(command.newstate());
    long long confignum = command.confignum();
    long long shardid = command.shardid();

    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if(confignum==shardStateMap_myj[shardid].confignum+1){
        // 新的配置需要处理某分片，由leader下发从Invalid转为Pulling
        if(newstate == kvserviceclass::ShardState::Pulling){
            if(shardStateMap_myj[shardid].state == kvserviceclass::ShardState::Invalid){
                kvserviceclass::ShardStateInfo stateinfo(confignum,newstate,newgid,false);
                shardStateMap_myj[shardid] = stateinfo;
                LOG_INFO("server{%lld,%s}>>shard[%lld]状态从Invalid->Pulling,ConfigNum[%lld]",gid_myj,name_myj.c_str(),shardid,confignum);

                std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
                std::ostringstream oss;
                boost::archive::binary_oarchive bos(oss);
                bos << stateinfo;
                db_myj->ConfigMetaPut(state_key,oss.str());
            }
        }

        // 新的配置不再需要处理某分片，由leader下发从Serving转为WaitingDelete
        if(newstate==kvserviceclass::ShardState::WaitingDelete){
            if(shardStateMap_myj[shardid].state == kvserviceclass::ShardState::Serving){
                kvserviceclass::ShardStateInfo stateinfo(confignum,newstate,newgid,false);
                shardStateMap_myj[shardid] = stateinfo;
                LOG_INFO("server{gid[%lld],me[%s]}>>shard[%lld]状态从Serving->WaitingDelete,ConfigNum[%lld]", gid_myj, name_myj.c_str(), shardid, confignum)

                std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
                std::ostringstream oss;
                boost::archive::binary_oarchive bos(oss);
                bos << stateinfo;
                db_myj->ConfigMetaPut(state_key,oss.str());
            }
        }
    }else if(confignum==shardStateMap_myj[shardid].confignum){
        if(newstate==kvserviceclass::Serving){
            kvserviceclass::ShardStateInfo stateinfo(confignum,newstate,newgid,false);
            shardStateMap_myj[shardid] = stateinfo;
            LOG_INFO("server{gid[%lld],me[%s]}>>shard[%lld]状态从Pulling->Serving,ConfigNum[%lld]", gid_myj, name_myj.c_str(), shardid, confignum);

            std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
            std::ostringstream oss;
            boost::archive::binary_oarchive bos(oss);
            bos << stateinfo;
            db_myj->ConfigMetaPut(state_key,oss.str());
        }
    }
    if(maxraftstate_myj!=-1){
        if(!snapshoting_myj){
            snapshot(applymsg.commandIndex);
        }
    }

}

void KVService::deleteShardHandler(ApplyMsg applymsg)
{
}

void KVService::installShardHandler(ApplyMsg applymsg)
{
    kvraft::Command command = applymsg.command;
    long long newgid = command.gid();
    long long confignum = command.confignum();
    long long shardid = command.shardid();
    
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if(confignum==shardStateMap_myj[shardid].confignum && shardStateMap_myj[shardid].state == kvserviceclass::ShardState::Pulling){
        LOG_INFO("server{gid[%lld],me[%s]}>>开始下载shard[%lld],ConfigNum[%lld]", gid_myj, name_myj.c_str(), shardid, confignum);
        
        //保存数据到数据库
        for(auto &kv : command.sharddata()){
            db_myj->KVPut(kv.first,kv.second);
        }
        for(auto &kv : command.clientlastreply()){
            std::string clientinfo;
            if(!db_myj->ClientRequestGet(kv.first,clientinfo) || clientinfo.size()==0){
                db_myj->ClientRequestPut(kv.first,kv.second);
            }else{
                std::istringstream newiss(kv.second);
                boost::archive::binary_iarchive newbis(newiss);
                kvserviceclass::clientLastReply newlastreply;
                newbis>>newlastreply;

                std::istringstream curiss(clientinfo);
                boost::archive::binary_iarchive curbis(curiss);
                kvserviceclass::clientLastReply curlastreply;
                curbis>>curlastreply;

                if(curlastreply.requestid < newlastreply.requestid){
                    db_myj->ClientRequestPut(kv.first,kv.second);
                }
            }
        }
        // 这里还不能把状态设置为Serving，要等leader成功让另一复制组删除掉分片后，等待leader下发状态改变的命令
		// 状态更改：未正在提交
        kvserviceclass::ShardStateInfo stateinfo(confignum,kvserviceclass::ShardState::Pulling,newgid,false);
        shardStateMap_myj[shardid] = stateinfo;
        LOG_INFO("server{gid[%lld],me[%s]}>>成功下载shard[%lld],ConfigNum[%lld]", gid_myj, name_myj.c_str(), shardid, confignum);

        std::string state_key = "STATE_SHARD_"+std::to_string(shardid);
        std::ostringstream oss;
        boost::archive::binary_oarchive bos(oss);
        bos << stateinfo;
        db_myj->ConfigMetaPut(state_key,oss.str());

        //更新存活状态,方便分片状态更新函数判断是否需要删除对端的对应分片
        shardKeysExist_myj[shardid] = true;
        std::string exist_key = "EXIST_SHARD_"+std::to_string(shardid);
        db_myj->ConfigMetaPut(exist_key,"true");
    }
    if(maxraftstate_myj!=-1){
        if(!snapshoting_myj){
            snapshot(applymsg.commandIndex);
        }
    }
}

void KVService::newConfigHandler(ApplyMsg applymsg)
{
    kvraft::Command command = applymsg.command;
    kvraft::Config config = command.newconfig();
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if(config.num()==curConfig_myj.num()+1){
        configList_myj.push_back(config);
        curConfig_myj = config;
        LOG_INFO("server{%lld,%s}>>成功应用新的配置,num:%lld",gid_myj,name_myj.c_str(),config.num());

        //更新数据库
        std::string config_key = "CONFIG_"+std::to_string(config.num());
        std::string curconfig_key = "CURRENT_CONFIG";
        std::string config_data;
        config.SerializeToString(&config_data);
        db_myj->ConfigMetaPut(config_key,config_data);
        db_myj->ConfigMetaPut(curconfig_key,config_data);
    }
    if(maxraftstate_myj!=-1){
        if(!snapshoting_myj){
            snapshot(applymsg.commandIndex);
        }
    }
}
