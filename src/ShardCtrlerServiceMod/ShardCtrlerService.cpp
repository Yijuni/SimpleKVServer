#include "ShardCtrlerService.h"

ShardCtrlerService::ShardCtrlerService(std::string name, std::shared_ptr<KVRaft> raft,
                                       std::shared_ptr<LockQueue<ApplyMsg>> applyChan, int timeout) : name_myj(name), raft_myj(raft),
                                                                                                      applyChan_myj(applyChan), timeout_myj(timeout)
{
    ready_myj = true;
    std::thread td(std::bind(applyLogs, this));
    td.detach();
}

void ShardCtrlerService::applyLogs()
{
    while (ready_myj)
    {
        ApplyMsg applymsg = applyChan_myj->pop();
        if (applymsg.commandValid)
        {
            commandApplyHandler(applymsg);
        }
    }
}

void ShardCtrlerService::commandApplyHandler(ApplyMsg applymsg)
{
    long long logterm = applymsg.commandTerm;
    long long logindex = applymsg.commandIndex;
    std::string clientid = applymsg.command.clientid();
    long long requestid = applymsg.command.requestid();
    std::string optype = applymsg.command.type();
    LOG_INFO("server[%s]>> 开始提交的命令,receive commit command index:%lld,clientid[%s],requestid[%lld],optype[%s]", name_myj.c_str(), logindex, clientid.c_str(), requestid, optype.c_str());
    std::unique_lock<std::mutex> lock(sourceMutex_myj);

    if (logindex <= maxCommitIndex_myj)
    {
        LOG_INFO("server[%s]>>maxCommitIndex[%lld],这次操作的index[%lld]已经执行过", name_myj.c_str(), maxCommitIndex_myj, logindex);
        return;
    }

    // 更新一下提交日志的最大下标
    maxCommitIndex_myj = logindex;

    bool existFlag = false;
    clientLastReply lastReply = clientLastReply{};
    shardctrler::Config config = shardctrler::Config{};

    auto iter = clientLastRequest_myj.find(clientid);
    existFlag = iter != clientLastRequest_myj.end();

    if (existFlag)
    {
        lastReply = iter->second;
        config = lastReply.replyMsg;
    }

    // 已经执行过
    if (existFlag && lastReply.requestid >= requestid)
    {
        LOG_INFO("server[%s]>>clent[%s],的请求requestid[%lld]已经执行过", name_myj.c_str(), clientid.c_str(), requestid);
        return;
    }
    else
    {
        if (optype == "Join")
        {
            std::unordered_map<long long, std::vector<std::string>> groups;
            for (const auto &kv : applymsg.command.groups())
            {
                long long gid = kv.first;
                groups[gid] = std::vector<std::string>();
                auto servers = kv.second;
                for (int i = 0; i < servers.serversname_size(); i++)
                {
                    groups[gid].push_back(servers.serversname(i));
                }
            }
            joinHandler(groups);
        }
        if (optype == "Leave")
        {
            std::vector<long long> gids;
            for (int i = 0; i < applymsg.command.gids_size(); i++)
            {
                gids.push_back(applymsg.command.gids(i));
            }
            leaveHandler(gids);
        }
        if (optype == "Move")
        {
            long long gid = applymsg.command.gid();
            long long shard = applymsg.command.shard();
            moveHandler(gid, shard);
        }
        if (optype == "Query")
        {
            long long num = applymsg.command.num();
            config = queryHandler(num);
        }
        clientLastRequest_myj[clientid] = clientLastReply{requestid, config};
    }

    auto notifyChanIter = notifyChan_myj.find(logindex);
    if (notifyChanIter != notifyChan_myj.end())
    {
        notifyChanMsg msg;
        msg.config = config;
        msg.errid = OK;
        std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChanIter->second;
        lock.unlock();

        long long term;
        bool isleader = raft_myj->GetState(term);

        if (!isleader)
        {
            msg.errid = ErrWrongLeader;
        }
        if (term == logterm)
        {
            std::thread td([&]()
                           { notifychan->push(msg); });
            td.detach();
        }
    }
}

void ShardCtrlerService::waitRequestCommit(ERRORID &err, bool &wrongleader, shardctrler::Config &config, std::shared_ptr<LockQueue<notifyChanMsg>> notifychan)
{
    AfterTimer waittimeout(500, 0,
                           std::bind(
                               [](std::shared_ptr<LockQueue<notifyChanMsg>> notifychantmp)
                               {
                                   notifyChanMsg notifymsg;
                                   notifymsg.errid = ErrTimeOut;
                                   notifychantmp->push(notifymsg);
                                   // LOG_INFO("TEST WAIT TIME");
                               },
                               notifychan));
    waittimeout.Reset();
    notifyChanMsg notifymsg = notifychan->pop();
    if (notifymsg.errid == ErrWrongLeader || notifymsg.errid == ErrTimeOut)
    {
        wrongleader = true;
    }
    else
    {
        wrongleader = false;
        config = notifymsg.config;
    }
    err = notifymsg.errid;
}

void ShardCtrlerService::joinHandler(std::unordered_map<long long, std::vector<std::string>> &groups)
{
    
}

void ShardCtrlerService::leaveHandler(std::vector<long long> &gids)
{
}

void ShardCtrlerService::moveHandler(long long &gid, long long &shrad)
{
}

shardctrler::Config ShardCtrlerService::queryHandler(long long num)
{
    return shardctrler::Config();
}
