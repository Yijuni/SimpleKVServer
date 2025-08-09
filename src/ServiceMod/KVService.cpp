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
    std::shared_ptr<LockQueue<ApplyMsg>> applyChan, int timeout, int maxraftstate,std::shared_ptr<RocksDBAPI> db)
    : name_myj(name), persister_myj(persister), raft_myj(raft), applyChan_myj(applyChan), ready_myj(false),
      timeout_myj(timeout), maxraftstate_myj(maxraftstate), snapshoting_myj(false), maxCommitIndex_myj(-1)
{
    db_myj = db;
    // readPersist(persister_myj->ReadSnapshot());
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
    if(db_myj->ClientRequestGet(clientid,requestinfo)){
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        clientLastReply clr;
        bis >> clr;
        if(clr.requestid >= request->requestid()){
            response->mutable_resultcode()->set_errorcode(OK);
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
    command.set_type("Get");

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;
    response->set_value(value);

    std::thread td([&]()
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
    if(db_myj->ClientRequestGet(clientid,requestinfo)){
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        clientLastReply clr;
        bis >> clr;
        if(clr.requestid >= request->requestid()){
            response->mutable_resultcode()->set_errorcode(OK);
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
    command.set_type("Put");

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;

    std::thread td([&]()
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
    if(db_myj->ClientRequestGet(clientid,requestinfo)){
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        clientLastReply clr;
        bis >> clr;
        if(clr.requestid >= request->requestid()){
            response->mutable_resultcode()->set_errorcode(OK);
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
    command.set_type("Append");

    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->mutable_resultcode()->set_errorcode(ErrWrongLeader);
        response->mutable_resultcode()->set_errormsg("leader节点选择错误");
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    std::string value;
    kvservice::ResultCode resultcode;
    waitRequestCommit(notifychan, resultcode, value);

    *response->mutable_resultcode() = resultcode;

    std::thread td([&]()
                   {
        std::unique_lock<std::mutex> locktmp(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();

    done->Run();
}

void KVService::applyLogs()
{
    while (ready_myj)
    {
        ApplyMsg applymsg = applyChan_myj->pop();
        if (applymsg.commandValid)
        {
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
    double datalen = persister_myj->RaftStateSize();
    if (datalen / (1.0 * maxraftstate_myj) >= 0.9)
    {
        LOG_INFO("server[%s]>>开始生成快照，快照最后命令的index = %lld,datalen[%f],maxraftsize[%lld]", name_myj.c_str(), logindex, datalen, maxraftstate_myj);
        std::ostringstream oss;
        boost::archive::binary_oarchive bos(oss);
        // 生成KV快照
        std::unordered_map<std::string,std::string> kvmap=db_myj->GenerateKVSnapshot();
        // 生成client_request快照
        std::unordered_map<std::string,std::string> client_request = db_myj->GenerateClientRequestSnapshot();
        // 记录当前执行的最后一条命令的index
        bos << maxCommitIndex_myj;
        // 当前kv数据
        bos << kvmap;
        // 记录客户端回应结果
        bos << client_request;
        std::string data = oss.str();
        raft_myj->Snapshot(logindex, data);
    }
    snapshoting_myj = false;
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
    std::unordered_map<std::string,std::string> kvmap;
    bis >> maxCommitIndex_myj;
    bis >> kvmap;
    bis >> clientLastRequest_myj;

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
    // 更新最大的提交日志的index
    maxCommitIndex_myj = logindex;

    // 获取当前指令的客户端最后一个请求的requestid
    std::string clientid;
    std::string requestinfo;
    clientLastReply lastReply;
    bool existFlag=false;
    if((existFlag = db_myj->ClientRequestGet(clientid,requestinfo))){
        std::istringstream iss(requestinfo);
        boost::archive::binary_iarchive bis(iss);
        bis >> lastReply;
    }

    // 当前key对应的value
    std::string curValue;
    db_myj->KVGet(key,curValue);
    // 当前指令已经执行过
    if (optype != "Get" && existFlag && lastReply.requestid >= requestid)
    {

        if (maxraftstate_myj != -1)
        {
            if (!snapshoting_myj)
            {
                snapshoting_myj = true;
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
            db_myj->KVPut(key,curValue);
            LOG_INFO("server[%s]>>KEY[%s],VALUE[%s]", name_myj.c_str(), key.c_str(), curValue.c_str());
        }
        if (optype == "Put")
        {
            curValue = value;
            db_myj->KVPut(key,curValue);
            LOG_INFO("server[%s]>>KEY[%s],VALUE[%s]", name_myj.c_str(), key.c_str(), curValue.c_str());
        }
        // 更新客户端最后请求信息
        std::ostringstream oss;
        boost::archive::binary_oarchive obs(oss);
        lastReply = clientLastReply(requestid, curValue);
        obs << lastReply;
        std::string data = oss.str();
        db_myj->ClientRequestPut(clientid,data); 
    }

    

    if (maxraftstate_myj != -1)
    {
        if (!snapshoting_myj)
        {
            snapshoting_myj = true;
            // 判断是否生成快照
            snapshot(logindex);
        }
    }

    auto notifyChanIter = notifyChan_myj.find(logindex);

    if (notifyChanIter != notifyChan_myj.end())
    {
        notifyChanMsg notifymsg;
        notifymsg.result = curValue;
        notifymsg.errid = OK;

        std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChanIter->second;
        lock.unlock();

        long long term;
        bool isleader = raft_myj->GetState(term);
        if (!isleader)
        {
            notifymsg.errid = ErrWrongLeader;
            notifymsg.result = "当前服务器不是leader";
        }

        // 如果term不同不能提交，因为该命令的结果，可能不是当前正在等待的请求的结果
        if (term == logterm)
        {
            std::thread td(
                [&]()
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
    std::unordered_map<std::string,std::string> kvmap;
    std::unordered_map<std::string,std::string> client_request;
    std::istringstream iss(data);
    boost::archive::binary_iarchive bis(iss);
    bis >> maxCommitIndex_myj;
    bis >> kvmap;
    bis >> client_request;
    // 下载leader传来的快照，更新到本地数据库
    db_myj->InstallKVSnapshot(kvmap);
    db_myj->InstallClientRequestSnapshot(client_request);
}

void KVService::waitRequestCommit(std::shared_ptr<LockQueue<notifyChanMsg>> notifychan, kvservice::ResultCode &resultcode, std::string &value)
{

    AfterTimer waittimeout(500, 0,
                           std::bind(
                               [](std::shared_ptr<LockQueue<notifyChanMsg>> notifychantmp)
                               {
                                   notifyChanMsg notifymsg;
                                   notifymsg.errid = ErrTimeOut;
                                   notifymsg.result = "等待时间超时";
                                   notifychantmp->push(notifymsg);
                                   LOG_INFO("TEST WAIT TIME");
                               },
                               notifychan));
    waittimeout.Reset();

    notifyChanMsg notifymsg = notifychan->pop();

    resultcode.set_errorcode(notifymsg.errid);
    if (notifymsg.errid == OK)
    {
        value = notifymsg.result;
    }
    else
    {
        resultcode.set_errormsg(notifymsg.result);
    }
}
