#include "ShardCtrlerService.h"

ShardCtrlerService::ShardCtrlerService(std::string name, std::shared_ptr<KVRaft> raft,
                                       std::shared_ptr<LockQueue<ApplyMsg>> applyChan, int timeout) : name_myj(name), raft_myj(raft),
                                                                                                      applyChan_myj(applyChan), timeout_myj(timeout)
{
    ready_myj = true;
    std::thread td(std::bind(applyLogs, this));
    td.detach();
}

void ShardCtrlerService::Join(google::protobuf::RpcController *controller, const ::shardctrler::JoinRequest *request, ::shardctrler::JoinResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    std::string clientid = request->clientid();
    long long reqeustid = request->requestid();
    auto groups = request->groups();
    auto iter = clientLastRequest_myj.find(clientid);
    if (iter != clientLastRequest_myj.end() && iter->second.requestid >= reqeustid)
    {
        response->set_err(OK);
        response->set_wrongleader(false);
        done->Run();
        return;
    }
    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_type("Join");
    command.set_clientid(clientid);
    command.set_requestid(reqeustid);
    auto mgroups = command.mutable_groups();
    for (const auto &kv : groups)
    {
        long long gid = kv.first;
        kvraft::Servers servers;
        for (int i = 0; i < kv.second.serversname_size(); i++)
        {
            servers.add_serversname(kv.second.serversname(i));
        }
        mgroups->insert({gid, servers});
    }
    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->set_err(ErrWrongLeader);
        response->set_wrongleader(true);
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    ERRORID errid;
    bool wrongleader;
    shardctrler::Config config;
    waitRequestCommit(errid, wrongleader, config, notifychan);

    response->set_err(errid);
    response->set_wrongleader(wrongleader);

    std::thread td([&]()
                   {
        std::unique_lock<std::mutex> llock(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();
    done->Run();
}

void ShardCtrlerService::Leave(google::protobuf::RpcController *controller, const ::shardctrler::LeaveRequest *request, ::shardctrler::LeaveResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    std::string clientid = request->clientid();
    long long reqeustid = request->requestid();
    auto gids = request->gids();
    auto iter = clientLastRequest_myj.find(clientid);
    if (iter != clientLastRequest_myj.end() && iter->second.requestid >= reqeustid)
    {
        response->set_err(OK);
        response->set_wrongleader(false);
        done->Run();
        return;
    }
    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_type("Leave");
    command.set_clientid(clientid);
    command.set_requestid(reqeustid);
    for(int i =0;i<gids.size();i++){
        command.add_gids(gids.Get(i));
    }
    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->set_err(ErrWrongLeader);
        response->set_wrongleader(true);
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    ERRORID errid;
    bool wrongleader;
    shardctrler::Config config;
    waitRequestCommit(errid, wrongleader, config, notifychan);

    response->set_err(errid);
    response->set_wrongleader(wrongleader);

    std::thread td([&]()
                   {
        std::unique_lock<std::mutex> llock(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();
    done->Run();
}

void ShardCtrlerService::Move(google::protobuf::RpcController *controller, const ::shardctrler::MoveRequest *request, ::shardctrler::MoveResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    std::string clientid = request->clientid();
    long long reqeustid = request->requestid();
    long long shardid = request->shard();
    long long gid = request->gid();
    auto iter = clientLastRequest_myj.find(clientid);
    if (iter != clientLastRequest_myj.end() && iter->second.requestid >= reqeustid)
    {
        response->set_err(OK);
        response->set_wrongleader(false);
        done->Run();
        return;
    }
    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_type("Move");
    command.set_clientid(clientid);
    command.set_requestid(reqeustid);
    command.set_shard(shardid);
    command.set_gid(gid);
    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->set_err(ErrWrongLeader);
        response->set_wrongleader(true);
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    ERRORID errid;
    bool wrongleader;
    shardctrler::Config config;
    waitRequestCommit(errid, wrongleader, config, notifychan);

    response->set_err(errid);
    response->set_wrongleader(wrongleader);

    std::thread td([&]()
                   {
        std::unique_lock<std::mutex> llock(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();
    done->Run();
}

void ShardCtrlerService::Query(google::protobuf::RpcController *controller, const ::shardctrler::QueryRequest *request, ::shardctrler::QueryResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    std::string clientid = request->clientid();
    long long reqeustid = request->requestid();
    long long num = request->num();
    auto iter = clientLastRequest_myj.find(clientid);
    if (iter != clientLastRequest_myj.end() && iter->second.requestid >= reqeustid)
    {
        response->set_err(OK);
        response->set_wrongleader(false);
        auto mconfig = response->mutable_config();
        shardctrler::Config config = iter->second.replyMsg;
        mconfig->set_num(config.num());
        for(int i=0;i<config.shards_size();i++){
            mconfig->add_shards(config.shards(i));
        }
        for(const auto &kv : config.groups()){
            mconfig->mutable_groups()->insert({kv.first,kv.second});
        }
        done->Run();
        return;
    }
    lock.unlock();

    long long logindex;
    long long logterm;
    kvraft::Command command;
    command.set_type("Query");
    command.set_clientid(clientid);
    command.set_requestid(reqeustid);
    command.set_num(request->num());
    bool isleader = raft_myj->Start(command, logindex, logterm);
    if (!isleader)
    {
        response->set_err(ErrWrongLeader);
        response->set_wrongleader(true);
        done->Run();
        return;
    }

    lock.lock();
    notifyChan_myj[logindex] = std::make_shared<LockQueue<notifyChanMsg>>(2);
    std::shared_ptr<LockQueue<notifyChanMsg>> notifychan = notifyChan_myj[logindex];
    lock.unlock();

    ERRORID errid;
    bool wrongleader;
    shardctrler::Config config;
    waitRequestCommit(errid, wrongleader, config, notifychan);

    response->set_err(errid);
    response->set_wrongleader(wrongleader);

    std::thread td([&]()
                   {
        std::unique_lock<std::mutex> llock(sourceMutex_myj);
        notifyChan_myj.erase(logindex); });
    td.detach();
    done->Run();
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

void ShardCtrlerService::joinHandler(const std::unordered_map<long long, std::vector<std::string>> &groups)
{
    shardctrler::Config oldconfig = configs_myj[configs_myj.size() - 1];
    shardctrler::Config newconfig;
    // 更新日志新的版本号，将旧shard配置复制到新配置
    newconfig.set_num(oldconfig.num() + 1);
    for (int i = 0; i < oldconfig.shards_size(); i++)
    {
        newconfig.add_shards(oldconfig.shards(i));
    }
    // 将旧的groups复制到新的配置
    auto newgroups = newconfig.mutable_groups();
    for (const auto &kv : oldconfig.groups())
    {
        newgroups->insert({kv.first, kv.second});
    }
    // 新加入的组加入到新的配置组
    for (const auto &kv : groups)
    {
        shardctrler::Servers newservers;
        for (int i = 0; i < kv.second.size(); i++)
        {
            newservers.add_serversname(kv.second[i]);
        }
        newgroups->insert({kv.first, newservers});
    }

    int shardlen = newconfig.shards_size();
    int groupslen = newconfig.groups_size();
    int peerGroupShardNum = groupslen / shardlen;
    int remain = groupslen % shardlen;

    // C++的map遍历是有序的，但是protobuf的map不是有序的，要保存key的顺序遍历不管在哪个线程都是一致的
    std::vector<long long> gidVec;
    for (const auto &kv : newconfig.groups())
    {
        gidVec.push_back(kv.first);
    }
    std::sort(gidVec.begin(), gidVec.end());

    // 保存每个复制组能管理的分片数
    std::unordered_map<long long, int> groupShardNum;
    for (int i = 0; i < gidVec.size(); i++)
    {
        // 获取GID
        int gid = gidVec[i];
        // 设置能保存的分片数目
        groupShardNum[gid] = peerGroupShardNum;
        if (remain > 0)
        {
            groupShardNum[gid]++;
            remain--;
        }
    }
    // 给复制组分配shard
    for (int shardindex = 0; shardindex < newconfig.shards_size(); shardindex++)
    {
        int curgid = newconfig.shards(shardindex);
        if (groupShardNum[curgid] > 0)
        {
            groupShardNum[curgid]--;
            continue;
        }
        // 如果原本所在的复制组不能接受更多分片，那就找到一个新加入的复制组
        for (int i = 0; i < gidVec.size(); i++)
        {
            long long gid = gidVec[i];
            // 确保是新加入的组
            auto iter = groups.find(gid);
            if (iter != groups.end() && groupShardNum[gid] > 0)
            {
                newconfig.set_shards(shardindex, gid);
                groupShardNum[gid]--;
                break;
            }
        }
    }
    configs_myj.push_back(newconfig);
}

void ShardCtrlerService::leaveHandler(const std::vector<long long> &gids)
{
    shardctrler::Config oldconfig = configs_myj[configs_myj.size() - 1];
    shardctrler::Config newconfig;
    // 更新日志新的版本号，将旧shard配置复制到新配置
    newconfig.set_num(oldconfig.num() + 1);
    for (int i = 0; i < oldconfig.shards_size(); i++)
    {
        newconfig.add_shards(oldconfig.shards(i));
    }
    // 将旧的groups复制到新的配置
    auto newgroups = newconfig.mutable_groups();
    for (const auto &kv : oldconfig.groups())
    {
        newgroups->insert({kv.first, kv.second});
    }
    // 根据移除的组id，删除掉对应组
    for (int i = 0; i < gids.size(); i++)
    {
        newgroups->erase(gids[i]);
    }
    if (newgroups->size() == 0)
    {
        for (int i = 0; i < newconfig.shards_size(); i++)
        {
            newconfig.set_shards(i, 0);
        }
        configs_myj.push_back(newconfig);
        return;
    }
    int shardlen = newconfig.shards_size();
    int groupslen = newconfig.groups_size();
    int peerGroupShardNum = groupslen / shardlen;
    int remain = groupslen % shardlen;

    // C++的map遍历是有序的，但是protobuf的map不是有序的，要保存key的顺序遍历不管在哪个线程都是一致的
    std::vector<long long> gidVec;
    for (const auto &kv : newconfig.groups())
    {
        gidVec.push_back(kv.first);
    }
    std::sort(gidVec.begin(), gidVec.end());

    // 保存每个复制组能管理的分片数
    std::unordered_map<long long, int> groupShardNum;
    for (int i = 0; i < gidVec.size(); i++)
    {
        // 获取GID
        int gid = gidVec[i];
        // 设置能保存的分片数目
        groupShardNum[gid] = peerGroupShardNum;
        if (remain > 0)
        {
            groupShardNum[gid]++;
            remain--;
        }
    }
    int nextGroupIndex = 0;
    for (int shardindex = 0; shardindex < newconfig.shards_size(); shardindex++)
    {
        long long curgid = newconfig.shards(shardindex);
        auto iter = newconfig.groups().find(curgid);
        // 自己所在的复制组没有被移除，继续留在这里
        if (iter != newconfig.groups().end())
        {
            groupShardNum[curgid]--;
        }
        else
        {
            for (;;)
            {
                // 原先的复制组被移除，加入现有的复制组
                long long nextgid = gidVec[nextGroupIndex];
                if (groupShardNum[nextgid] > 0)
                {
                    groupShardNum[nextgid]--;
                    newconfig.set_shards(shardindex, nextgid);
                    nextGroupIndex = (nextGroupIndex + 1) % gidVec.size();
                    break;
                }
                else
                {
                    // 直到能找到一个有空余位置的复制组
                    nextGroupIndex = (nextGroupIndex + 1) % gidVec.size();
                }
            }
        }
    }
    configs_myj.push_back(newconfig);
}

void ShardCtrlerService::moveHandler(const long long &gid, const long long &shrad)
{
    shardctrler::Config oldconfig = configs_myj[configs_myj.size() - 1];
    shardctrler::Config newconfig;
    // 更新日志新的版本号，将旧shard配置复制到新配置
    newconfig.set_num(oldconfig.num() + 1);
    for (int i = 0; i < oldconfig.shards_size(); i++)
    {
        newconfig.add_shards(oldconfig.shards(i));
    }
    // 将旧的groups复制到新的配置
    auto newgroups = newconfig.mutable_groups();
    for (const auto &kv : oldconfig.groups())
    {
        newgroups->insert({kv.first, kv.second});
    }
    newconfig.set_shards(shrad, gid);
    configs_myj.push_back(newconfig);
}

shardctrler::Config ShardCtrlerService::queryHandler(long long num)
{
    shardctrler::Config config;
    if (num < 0 || num >= configs_myj.size())
    {
        config = configs_myj[configs_myj.size() - 1];
    }
    else
    {
        config = configs_myj[num];
    }
    return config;
}
