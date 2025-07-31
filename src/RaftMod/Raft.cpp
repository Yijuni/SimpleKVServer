#include "Raft.hpp"
#include "Logger.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <sstream>
#include <cstdio>
#include "KVRpcController.hpp"

KVRaft::KVRaft() : ready_myj(false),dbptr_myj(nullptr)
{
}

KVRaft::~KVRaft()
{
    delete heartbeatsTimer_myj;
    delete electionTimer_myj;
}

void KVRaft::AppendEntries(google::protobuf::RpcController *controller, const ::kvraft::AppendEntriesRequest *request, ::kvraft::AppendEntriesResponse *response, ::google::protobuf::Closure *done)
{
    // 当前是否有快照正在提交，如果有则先等待,防止快照后的日志到快照之前
    std::unique_lock<std::mutex> slock(snapshotInstallingMutex_myj);
    slock.unlock();
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (!ready_myj)
    {
        response->set_term(request->term());
        response->set_success(false);
        response->set_fastback(request->prelogindex() + 1); // 就让请求服务器下次还发这个位置的数据
        done->Run();
        return;
    }

    LOG_INFO("server[%s]>>收到leader[%s]的追加日志请求", name_myj.c_str(), request->leaderid().c_str());
    if (request->term() < currentTerm_myj)
    {
        response->set_success(false);
        response->set_term(currentTerm_myj);
        lock.unlock();
        done->Run();
        return;
    }
    currentTerm_myj = request->term();
    status_myj = FOLLOWER;
    leaderid_myj = request->leaderid();
    response->set_term(request->term());
    response->set_success(true);

    std::vector<kvraft::LogEntry> entries;
    for (int i = 0; i < request->logentries_size(); i++)
    {
        entries.push_back(request->logentries(i));
    }
    if (matchNewEntries(entries, request->prelogindex(), request->prelogterm(), response))
    {
        LOG_INFO("server[%s]>>leader coomitindex:%ld,self commitindex:%lld", name_myj.c_str(), request->leadercommit(), commitIndex_myj);
        if (request->leadercommit() >= commitIndex_myj)
        {
            if (logEntries_myj.size() - 1 < request->leadercommit() - lastSnapshotIndex_myj - 1)
            {
                commitIndex_myj = logEntries_myj.size() + lastSnapshotIndex_myj + 1 - 1;
            }
            else
            {
                commitIndex_myj = request->leadercommit();
            }
        }
    }
    // 有时候服务器重启会直接收到现有节点的追加日志请求，就没法在投票里面更新voterFor，
    // 有可能导致目前的leader和自己数据库存储的voteFor不一样
    // 虽然对我的Raft投票方法还没发现有什么不良影响，但是为了一致，还是修改一下voteFor比较好
    voterFor_myj = request->leaderid();
    electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
    persist();
    lock.unlock();
    done->Run();
}

void KVRaft::RequestVote(google::protobuf::RpcController *controller, const ::kvraft::RequestVoteRequest *request, ::kvraft::RequestVoteResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (!ready_myj)
    {
        response->set_term(request->term());
        response->set_votegranted(false); // 就装作已经投票了
        done->Run();
        return;
    }
    printf("server[%s]>>收到server[%s]请求投票\n", name_myj.c_str(), request->candidateid().c_str());
    LOG_INFO("server[%s]>>收到server[%s]请求投票", name_myj.c_str(), request->candidateid().c_str());
    if (request->term() < currentTerm_myj)
    {
        response->set_votegranted(false);
        response->set_term(currentTerm_myj);
        lock.unlock();
        done->Run();
        return;
    }
    if (request->term() == currentTerm_myj)
    {
        if (status_myj == LEADER || voterFor_myj != "")
        {
            response->set_votegranted(false);
            response->set_term(currentTerm_myj);
            lock.unlock();
            done->Run();
            return;
        }
    }
    response->set_votegranted(true);
    if (logEntries_myj.size() != 0 && logEntries_myj[logEntries_myj.size() - 1].term() > request->lastlogterm())
    {
        response->set_votegranted(false);
        response->set_term(currentTerm_myj);
    }
    if (logEntries_myj.size() != 0 && logEntries_myj[logEntries_myj.size() - 1].term() == request->lastlogterm() && logEntries_myj.size() + lastSnapshotIndex_myj + 1 - 1 > request->lastlogindex())
    {
        response->set_votegranted(false);
        response->set_term(currentTerm_myj);
    }
    if (lastSnapshotIndex_myj != -1 && lastSnapshotTerm_myj > request->lastlogterm())
    {
        response->set_votegranted(false);
        response->set_term(currentTerm_myj);
    }
    if (lastSnapshotIndex_myj != -1 && lastSnapshotTerm_myj == request->lastlogterm() && lastSnapshotIndex_myj > request->lastlogindex())
    {
        response->set_votegranted(false);
        response->set_term(currentTerm_myj);
    }
    if (response->votegranted())
    {
        voterFor_myj = request->candidateid();
        electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
        LOG_INFO("server[%s]>>给server[%s]投票", name_myj.c_str(), request->candidateid().c_str());
    }
    currentTerm_myj = request->term();
    status_myj = FOLLOWER;
    response->set_term(currentTerm_myj);

    persist();

    lock.unlock();
    done->Run();
}

void KVRaft::InstallSnapshot(google::protobuf::RpcController *controller, const ::kvraft::InstallSnapshotRequest *request, ::kvraft::InstallSnapshotResponse *response, ::google::protobuf::Closure *done)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (!ready_myj)
    { // 就装作成功安装快照，反正等本服务器真正启动，如果并没有这部分快照还会让leader回退的
        response->set_term(request->term());
        done->Run();
        return;
    }
    response->set_term(request->term());
    LOG_INFO("server[%s]>>收到leader[%s],下载快照请求", name_myj.c_str(), request->leaderid().c_str());
    if (request->term() < currentTerm_myj)
    {
        response->set_term(currentTerm_myj);
        lock.unlock();
        done->Run();
        return;
    }
    if (request->lastincludeindex() <= commitIndex_myj)
    {
        lock.unlock();
        done->Run();
        return;
    }
    long long curindex = request->lastincludeindex() - lastSnapshotIndex_myj - 1;
    if (curindex < logEntries_myj.size())
    {
        if (logEntries_myj[curindex].term() != request->term())
        { // 如果对应位置日志term不同，当前节点该日志及后面的日志全部删除
            logEntries_myj.clear();
        }
        else
        {
            std::vector<kvraft::LogEntry> logstmp(logEntries_myj.begin() + curindex + 1, logEntries_myj.end());
            logEntries_myj.clear();
            logEntries_myj.swap(logstmp);
        }
    }
    else
    {
        logEntries_myj.clear();
    }
    lastSnapshotIndex_myj = request->lastincludeindex();
    lastSnapshotTerm_myj = request->lastincludeterm();
    lastApplied_myj = lastSnapshotIndex_myj;
    commitIndex_myj = lastSnapshotIndex_myj;

    electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
    currentTerm_myj = request->term();
    status_myj = FOLLOWER;
    leaderid_myj = request->leaderid();

    persist(request->data());

    ApplyMsg sendApplyMsg;
    sendApplyMsg.commandValid = false;
    sendApplyMsg.snapshotValid = true;
    sendApplyMsg.snapshotIndex = request->lastincludeindex() + 1;
    sendApplyMsg.snapshotTerm = request->lastincludeterm();
    sendApplyMsg.data = request->data();

    snapshotInstallingMutex_myj.lock();
    std::thread td(
        [](std::shared_ptr<KVRaft> raft, ApplyMsg sendApplyMsg)
        {
            raft->applyChan_myj->push(sendApplyMsg);
            raft->snapshotInstallingMutex_myj.unlock();
        },
        shared_from_this(), sendApplyMsg);
    td.detach();

    lock.unlock();
    done->Run();
}

bool KVRaft::GetState(long long &term)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (!ready_myj)
    {
        return false;
    }
    bool isLeader = false;
    if (status_myj == LEADER)
    {
        isLeader = true;
    }
    term = currentTerm_myj;
    return isLeader;
}

void KVRaft::Snapshot(long long index, std::string &snapshot)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    if (!ready_myj)
    {
        return;
    }
    LOG_INFO("server[%s]>>收到服务层生成快照请求", name_myj.c_str());
    if (index - 1 <= lastSnapshotIndex_myj)
    {
        LOG_INFO("server[%s]>>这部分快照已经存在", name_myj.c_str());
        return;
    }
    if (index - 1 > commitIndex_myj)
    {
        LOG_INFO("server[%s]>>请求的快照最后一条日志index大于当前节点可提交日志的最大index，无法生成", name_myj.c_str());
        return;
    }

    // index之内的日志生成快照，index之后的留下
    lastSnapshotTerm_myj = logEntries_myj[index - 1 - lastSnapshotIndex_myj - 1].term();
    std::vector<kvraft::LogEntry> tmp(logEntries_myj.begin() + index - lastSnapshotIndex_myj - 1, logEntries_myj.end());
    lastSnapshotIndex_myj = index - 1;
    logEntries_myj.swap(tmp);
    persist(snapshot);
    LOG_INFO("server[%s]>>生成快照:lastSnapshotindex:%lld,lastSnapshotTerm:%lld", name_myj.c_str(), lastSnapshotIndex_myj, lastSnapshotTerm_myj);
}

bool KVRaft::Start(kvraft::Command command, long long &index, long long &term)
{
    index = -1;
    term = -1;

    std::unique_lock<std::mutex> lock(sourceMutex_myj);

    if (!ready_myj || status_myj != LEADER)
    {
        return false;
    }
    else
    {
        LOG_INFO("server[%s]>>追加新的命令，key:%s,value:%s,clientid:%s,requestid:%ld", name_myj.c_str(), command.key().c_str(), command.value().c_str(), command.clientid().c_str(), command.requestid());
        kvraft::LogEntry log;
        log.set_term(currentTerm_myj);
        *log.mutable_command() = command;
        logEntries_myj.emplace_back(log);

        index = logEntries_myj.size() + lastSnapshotIndex_myj + 1;
        term = currentTerm_myj;

        persist();

        std::thread td(std::bind(&KVRaft::appendEntriesToFollower, this, currentTerm_myj, commitIndex_myj, peers_myj.size()));
        td.detach();
        heartbeatsTimer_myj->Reset();
        return true;
    }

    return false;
}

void KVRaft::Make(std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> &peers, std::string &name, 
    std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyChan,
    std::shared_ptr<RocksDBAPI> dbptr)
{
    peers_myj = peers;
    name_myj = name;
    persister_myj = persister;
    dbptr_myj = dbptr;

    currentTerm_myj = 0;
    voterFor_myj = "";
    status_myj = STATUS::FOLLOWER;
    logEntries_myj = std::vector<kvraft::LogEntry>(0);

    heartbeatsTime_myj = 100;
    electionTimeout_myj = 1000;

    commitIndex_myj = -1;
    lastApplied_myj = -1;
    nextIndex_myj.resize(peers_myj.size(), 0);
    matchIndex_myj.resize(peers_myj.size(), 0);
    applyChan_myj = applyChan;

    lastSnapshotIndex_myj = -1;
    lastSnapshotTerm_myj = -1;

    // 心跳定时器和选举超时定时器启动
    heartbeatsTimer_myj = new AfterTimer(heartbeatsTime_myj, 0,
                                         [this]()
                                         {
                                             std::unique_lock<std::mutex> lock(sourceMutex_myj);

                                             if (!ready_myj)
                                             {
                                                 return;
                                             }
                                             if (status_myj == LEADER)
                                             {
                                                 LOG_INFO("server[%s]>>发送心跳", name_myj.c_str());
                                                 std::thread td([](std::shared_ptr<KVRaft> raft)
                                                                { raft->appendEntriesToFollower(raft->currentTerm_myj, raft->commitIndex_myj, raft->peers_myj.size()); }, shared_from_this());
                                                 td.detach();
                                                 heartbeatsTimer_myj->Reset();
                                             }
                                         });
    electionTimer_myj = new AfterTimer(electionTimeout_myj, 0,
                                       [this]()
                                       {
                                           std::unique_lock<std::mutex> lock(sourceMutex_myj);

                                           if (!ready_myj)
                                           {
                                               return;
                                           }

                                           if (status_myj == FOLLOWER || status_myj == CANDIDATE)
                                           {
                                               // 成为候选者
                                               status_myj = CANDIDATE;
                                               voterFor_myj = name_myj;
                                               currentTerm_myj++;
                                               // 持久化数据
                                               persist();

                                               long long lastLogIndex = -1;
                                               long long lastLogTerm = -1;
                                               if (logEntries_myj.size() != 0 || lastSnapshotIndex_myj != -1)
                                               {
                                                   if (logEntries_myj.size() != 0)
                                                   {
                                                       lastLogIndex = logEntries_myj.size() + lastSnapshotIndex_myj + 1 - 1;
                                                       lastLogTerm = logEntries_myj[logEntries_myj.size() - 1].term();
                                                   }
                                                   else
                                                   {
                                                       lastLogIndex = lastSnapshotIndex_myj;
                                                       lastLogTerm = lastSnapshotTerm_myj;
                                                   }
                                               }
                                               std::thread td(std::bind(&KVRaft::electStart, this, name_myj.c_str(), currentTerm_myj, lastLogIndex, lastLogTerm, peers_myj.size()));
                                               td.detach();
                                               electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
                                           }
                                       });

    // 读取持久化数据恢复
    readPersist();

    ready_myj = true; // 这个一定要在下面这些函数启动前，否则下面这个函数会直接退出
    // 启动定时上传日志线程
    std::thread td([](std::shared_ptr<KVRaft> raft)
                   { raft->applyEntries(20); }, shared_from_this()); // 传入自身指针防止，上传过程中访问已释放变量
    td.detach();

    // 启动选举超时定时器
    electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
}

void KVRaft::Close()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    ready_myj = false;
}

void KVRaft::ChangePeer(std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> &stubs)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    status_myj = FOLLOWER;
    voterFor_myj = "";
    leaderid_myj = "";
    peers_myj.clear();
    peers_myj = stubs;
    electionTimer_myj->Reset();
    LOG_INFO("server[%s]>>对端信息改变", name_myj.c_str());
}

void KVRaft::persist(std::string snapshot)
{

    // 序列化log数组
    std::ostringstream ops;
    boost::archive::binary_oarchive bo(ops);
    serializeLogEntriesVector(bo);
    std::string logsostring = ops.str();

    // 这种元数据直接存在rocksdb里面
    dbptr_myj->RaftMetaPut("currentTerm",std::to_string(currentTerm_myj));
    dbptr_myj->RaftMetaPut("voteFor",voterFor_myj);
    dbptr_myj->RaftMetaPut("lastSnapshotIndex",std::to_string(lastSnapshotIndex_myj));
    dbptr_myj->RaftMetaPut("lastSnapshotTerm",std::to_string(lastSnapshotTerm_myj));
    // bo << currentTerm_myj;
    // bo << voterFor_myj;
    // bo << lastSnapshotIndex_myj;
    // bo << lastSnapshotTerm_myj;
    // 只需要序列化日志数组
    
    persister_myj->Save(logsostring, snapshot);
    printf("server[%s]>>logs序列化数据[%s]\n", name_myj.c_str(), logsostring.c_str());
    LOG_INFO("server[%s]>>logs序列化数据[%s]", name_myj.c_str(), logsostring.c_str());
}

void KVRaft::readPersist()
{
    // 只解析log数组
    std::string data = persister_myj->ReadRaftState();
    if(data.size()!=0){
        std::istringstream ips(data);
        boost::archive::binary_iarchive bi(ips);
        deserializeLogEntriesVector(bi);
    }

    std::string currentTerm="";
    std::string lastSnapshotIndex="";
    std::string lastSnapshotTerm="";
    if(dbptr_myj->RaftMetaGet("currentTerm",currentTerm)){
        currentTerm_myj = std::stoll(currentTerm);
    }
    if(dbptr_myj->RaftMetaGet("lastSnapshotIndex",lastSnapshotIndex)){
        lastSnapshotIndex_myj = std::stoll(lastSnapshotIndex);
    }
    if(dbptr_myj->RaftMetaGet("lastSnapshotTerm",lastSnapshotTerm)){
        lastSnapshotTerm_myj  =std::stoll(lastSnapshotTerm);
    }
    dbptr_myj->RaftMetaGet("voteFor",voterFor_myj);
    // bi >> currentTerm_myj;
    // bi >> voterFor_myj;
    // bi >> lastSnapshotIndex_myj;
    // bi >> lastSnapshotTerm_myj;
    LOG_INFO("server[%s]>>重启，term:%lld,voteFor:%s,lastSnapshotIndex:%lld,lastSnapshotTerm:%lld", name_myj.c_str(), currentTerm_myj, voterFor_myj.c_str(), lastSnapshotIndex_myj, lastSnapshotTerm_myj);
    
}

void KVRaft::electStart(std::string name, long long curterm, long long lastLogIndex, long long lastLogTerm, long long peerscount)
{
    std::atomic<int> voteCount = 1;
    LOG_INFO("server[%s]>>开始leader选举,term=%lld", name.c_str(), curterm);
    std::vector<std::thread> waitGroup;
    for (int i = 0; i < peerscount; i++)
    {
        // 注意这里 i必须通过参数传入，如果直接通过引用获得i，可能会导致发送时i的数值不确定，因为新开的线程与此线程并发执行
        waitGroup.emplace_back(
            [&](int sendindex)
            {
                kvraft::RequestVoteRequest voteRequest;
                kvraft::RequestVoteResponse voteResponse;

                voteRequest.set_candidateid(name);
                voteRequest.set_term(curterm);
                voteRequest.set_lastlogindex(lastLogIndex);
                voteRequest.set_lastlogterm(lastLogTerm);

                std::unique_lock<std::mutex> lock(sourceMutex_myj);
                std::shared_ptr<kvraft::KVRaftRPC_Stub> sendstub = peers_myj[sendindex];
                lock.unlock();

                KVRpcController controller;
                sendstub->RequestVote(&controller, &voteRequest, &voteResponse, nullptr);

                if (!controller.Failed())
                {
                    LOG_INFO("server[%s]收到来自%d投票回复,granted:%d", name_myj.c_str(), sendindex, voteResponse.votegranted());
                    lock.lock();
                    // 状态没变
                    if (status_myj == CANDIDATE && currentTerm_myj == curterm)
                    {
                        if (voteResponse.votegranted())
                        {
                            LOG_INFO("server[%s]>>收到server[%d]的投票", name_myj.c_str(), sendindex);
                            printf("server[%s]>>收到server[%d]的投票\n", name_myj.c_str(), sendindex);
                            voteCount++;
                            // 投票数超过半数
                            if (voteCount >= (peerscount + 1) / 2 + 1)
                            {
                                LOG_INFO("server[%s]>>votrCount:%d ,成为leader！term:%lld", name_myj.c_str(), voteCount.load(), curterm);
                                // 变为leader，投票对象设为-1以便失去leader身份时可以给其他人投票
                                status_myj = LEADER;
                                voterFor_myj = "";

                                // nextIndex数组和matchIndex数组重置(最后一个日志的下一个日志的index(原因看论文)，-1)
                                for (int index = 0; index < peerscount; index++)
                                {
                                    nextIndex_myj[index] = logEntries_myj.size() + lastSnapshotIndex_myj + 1;
                                    matchIndex_myj[index] = -1;
                                }

                                // 持久化
                                persist();

                                // 立即发送心跳并启动心跳计时器
                                std::thread td(std::bind(&KVRaft::appendEntriesToFollower, this, curterm, commitIndex_myj, peerscount));
                                td.detach();
                                heartbeatsTimer_myj->Reset();
                            }
                        }
                        else if (voteResponse.term() > currentTerm_myj)
                        {
                            currentTerm_myj = voteResponse.term();
                            status_myj = FOLLOWER;
                            voterFor_myj = "";
                            electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
                            persist();
                        }
                    }
                }
                else
                {
                    LOG_INFO("server[%s]>>发送选举消息失败,pos=%d,errormsg:%s", name_myj.c_str(), sendindex, controller.ErrorText().c_str());
                }
            },
            i);
    }
    // 等待所有请求发送完成
    for (int i = 0; i < waitGroup.size(); i++)
    {
        waitGroup[i].join();
    }
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    // 没收到足够选票的情况
    if (status_myj == CANDIDATE && currentTerm_myj == curterm)
    {
        status_myj = FOLLOWER;
        voterFor_myj = "";
        electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);
        persist();
        LOG_INFO("server[%s]>>没收到足够选票", name.c_str());
    }
}

void KVRaft::appendEntriesToFollower(long long curterm, long long leaderCommit, long long peerscount)
{
    std::unique_lock lock(sourceMutex_myj);
    std::vector<kvraft::LogEntry> logstmp(logEntries_myj.begin(), logEntries_myj.end());
    lock.unlock();

    LOG_INFO("server[%s]>>开始发送日志/心跳/快照", name_myj.c_str());
    std::vector<std::thread> waitGroup;
    for (int i = 0; i < peerscount; i++)
    {
        waitGroup.emplace_back([&](int sendindex)
                               {
                                   LOG_INFO("server[%s]>>开始发送心跳/日志/快照，sendindex=%d", name_myj.c_str(), sendindex);
                                   std::unique_lock<std::mutex> td_lock(sourceMutex_myj);

                                   if (currentTerm_myj != curterm)
                                   {
                                       td_lock.unlock();
                                       return;
                                   }
                                   long long nextIndex = nextIndex_myj[sendindex];
                                   if (nextIndex <= lastSnapshotIndex_myj)
                                   { // 已经没这条日志，发送快照
                                       kvraft::InstallSnapshotRequest installSnapshotRequest;
                                       kvraft::InstallSnapshotResponse installSnapshotResponse;

                                       long long curSnapshotIndex = lastSnapshotIndex_myj;

                                       installSnapshotRequest.set_data(persister_myj->ReadSnapshot());
                                       installSnapshotRequest.set_lastincludeindex(lastSnapshotIndex_myj);
                                       installSnapshotRequest.set_lastincludeterm(lastSnapshotTerm_myj);
                                       installSnapshotRequest.set_leaderid(name_myj);
                                       installSnapshotRequest.set_term(curterm);

                                       std::shared_ptr<kvraft::KVRaftRPC_Stub> sendstub = peers_myj[sendindex];
                                       td_lock.unlock();

                                       KVRpcController controller;
                                       sendstub->InstallSnapshot(&controller, &installSnapshotRequest, &installSnapshotResponse, nullptr);

                                       if (!controller.Failed())
                                       {
                                           td_lock.lock();
                                           LOG_INFO("server[%s]>>成功收到发送快照的回复,pos=%d", name_myj.c_str(), sendindex);
                                           if (currentTerm_myj == curterm && status_myj == LEADER)
                                           { // 状态没变
                                               if (installSnapshotResponse.term() > currentTerm_myj)
                                               {
                                                   currentTerm_myj = installSnapshotResponse.term();
                                                   status_myj = FOLLOWER;
                                                   voterFor_myj = "";
                                                   electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);

                                                   persist();

                                                   return;
                                               }

                                               // nextIndex没变，更新
                                               if (nextIndex_myj[sendindex] == nextIndex)
                                               {
                                                   nextIndex_myj[sendindex] = curSnapshotIndex + 1;
                                               }
                                               matchIndex_myj[sendindex] = nextIndex_myj[sendindex] - 1;
                                               updateCommitIndex();
                                           }
                                       }
                                   }
                                   else
                                   { // 发送日志
                                       // 判断要发送日志是否产生改变，如果产生改变就不发送了，因为剩下的日志可能不是
                                       if (logstmp.size() != logEntries_myj.size())
                                       {
                                           td_lock.unlock();
                                           return;
                                       }
                                       for (int index = 0; index < logstmp.size(); index++)
                                       {
                                           if (!compareEntry(logEntries_myj[index], logstmp[index]))
                                           {
                                               td_lock.unlock();
                                               return;
                                           }
                                       }
                                       kvraft::AppendEntriesRequest appendEntriesRequest;
                                       kvraft::AppendEntriesResponse appendEntriesResponse;

                                       appendEntriesRequest.set_leaderid(name_myj);
                                       appendEntriesRequest.set_term(curterm);
                                       appendEntriesRequest.set_leadercommit(leaderCommit);

                                       // 前一条日志的index和term，有可能前一条日志为快照中的最后一个日志的index
                                       appendEntriesRequest.set_prelogindex(nextIndex - 1);
                                       if ((nextIndex - lastSnapshotIndex_myj - 1 - 1) >= 0)
                                       {
                                           appendEntriesRequest.set_prelogterm(logstmp[nextIndex - lastSnapshotIndex_myj - 1 - 1].term());
                                       }
                                       else
                                       {
                                           appendEntriesRequest.set_prelogterm(lastSnapshotIndex_myj);
                                       }

                                       // 将发送的日志装入request
                                       for (int index = nextIndex - lastSnapshotIndex_myj - 1; index < logstmp.size(); index++)
                                       {
                                           kvraft::LogEntry *logtmp = appendEntriesRequest.add_logentries();
                                           *logtmp = logstmp[index];
                                       }

                                       std::shared_ptr<kvraft::KVRaftRPC_Stub> sendstub = peers_myj[sendindex];
                                       td_lock.unlock();

                                       KVRpcController controller;
                                       sendstub->AppendEntries(&controller, &appendEntriesRequest, &appendEntriesResponse, nullptr);

                                       if (!controller.Failed())
                                       {
                                           td_lock.lock();
                                           LOG_INFO("server[%s]>>成功收到发送心跳/日志的回复,pos=%d", name_myj.c_str(), sendindex);
                                           if (currentTerm_myj == curterm && status_myj == LEADER)
                                           {
                                               if (!appendEntriesResponse.success())
                                               {
                                                   if (currentTerm_myj < appendEntriesResponse.term())
                                                   { // term落后
                                                       currentTerm_myj = appendEntriesResponse.term();
                                                       status_myj = FOLLOWER;
                                                       voterFor_myj = "";
                                                       electionTimer_myj->RandomReset(electionTimeout_myj, electionTimeout_myj * 2);

                                                       persist();
                                                   }
                                                   else
                                                   { // 没匹配上日志
                                                       nextIndex_myj[sendindex] = appendEntriesResponse.fastback();
                                                   }
                                               }
                                               if (appendEntriesRequest.logentries_size() == 0 || !appendEntriesResponse.success())
                                               {

                                                   return;
                                               }
                                               // 更新下次发送的nextIndex
                                               if (nextIndex == nextIndex_myj[sendindex])
                                               {
                                                   nextIndex_myj[sendindex] = nextIndex + appendEntriesRequest.logentries_size();
                                               }
                                               matchIndex_myj[sendindex] = nextIndex_myj[sendindex] - 1;
                                               updateCommitIndex();
                                           }
                                       }
                                       else
                                       {
                                           LOG_INFO("server[%s]>>发送心跳/心跳失败,pos=%d,errormsg:%s", name_myj.c_str(), sendindex, controller.ErrorText().c_str());
                                       }
                                   }
                               },
                               i);
    }
    // 等待所有请求发送完成
    for (int i = 0; i < waitGroup.size(); i++)
    {
        waitGroup[i].join();
    }
}

bool KVRaft::matchNewEntries(const std::vector<kvraft::LogEntry> &entries, long long preLogIndex, long long preLogTerm, kvraft::AppendEntriesResponse *resposne)
{
    if (preLogIndex == -1 || preLogIndex <= lastSnapshotIndex_myj)
    {
        if (preLogIndex < lastSnapshotIndex_myj)
        {
            resposne->set_fastback(commitIndex_myj + 1);
            resposne->set_success(false);
            return false;
        }
    }
    else if (logEntries_myj.size() <= preLogIndex - lastSnapshotIndex_myj - 1)
    {
        LOG_INFO("server[%s]>>当前节点没有%lld位置处的日志", name_myj.c_str(), preLogIndex);
        resposne->set_fastback(logEntries_myj.size() + lastSnapshotIndex_myj + 1);
        resposne->set_success(false);
        return false;
    }
    else if (logEntries_myj[preLogIndex - lastSnapshotIndex_myj - 1].term() != preLogTerm)
    {
        LOG_INFO("server[%s]>>当前节点在%lld位置处的日志与leader的不匹配", name_myj.c_str(), preLogIndex);
        resposne->set_fastback(commitIndex_myj + 1);
        resposne->set_success(false);
        return false;
    }
    int index = 0;
    for (; index < entries.size(); index++)
    {
        // 追加日志数组中的日志对应到当前节点日志数组的日志位置，超过当前节点日志数组的大小就跳出循环
        if ((index + preLogIndex + 1 - lastSnapshotIndex_myj - 1) >= logEntries_myj.size())
        {
            break;
        }
        // 两个数组对应日志的term不同
        if (logEntries_myj[index + preLogIndex + 1 - lastSnapshotIndex_myj - 1].term() != entries[index].term())
        {
            // 只留下匹配的，不匹配的全部删除
            std::vector<kvraft::LogEntry> logstmp(logEntries_myj.begin(), logEntries_myj.begin() + index + preLogIndex + 1 - lastSnapshotIndex_myj - 1);
            logEntries_myj.clear();
            logEntries_myj.swap(logstmp);
            break;
        }
    }
    // 如果有剩余就把剩余的日志追加到当前节点
    for (; index < entries.size(); index++)
    {
        logEntries_myj.push_back(entries[index]);
    }
    return true;
}

void KVRaft::applyEntries(long long sleep)
{
    while (ready_myj)
    {
        std::unique_lock<std::mutex> td_lock(sourceMutex_myj);
        for (; lastApplied_myj < commitIndex_myj;)
        {
            LOG_INFO("server[%s]>>开始apply日志", name_myj.c_str());
            long long nextApplied = lastApplied_myj + 1;
            ApplyMsg sendApplyMsg;
            sendApplyMsg.command = logEntries_myj[lastApplied_myj + 1 - lastSnapshotIndex_myj - 1].command();
            sendApplyMsg.commandIndex = lastApplied_myj + 2;
            sendApplyMsg.commandValid = true;
            sendApplyMsg.commandTerm = logEntries_myj[lastApplied_myj + 1 - lastSnapshotIndex_myj - 1].term();
            td_lock.unlock();
            applyChan_myj->push(sendApplyMsg);
            td_lock.lock();
            // 如果发送过程中lastApplied_myj变了就不用在这修改了，说明其他线程修改了
            if (lastApplied_myj + 1 == nextApplied)
            {
                lastApplied_myj = nextApplied;
                LOG_INFO("server[%s]>>lastApplied[%lld],commitIndex[%lld]", name_myj.c_str(), lastApplied_myj, commitIndex_myj);
            }
        }
        td_lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }
}

void KVRaft::updateCommitIndex()
{
    for (long long maxMatchIndex = commitIndex_myj + 1; maxMatchIndex - lastSnapshotIndex_myj - 1 < logEntries_myj.size(); maxMatchIndex++)
    {
        // 不对非当前任期的日志提交
        if (logEntries_myj[maxMatchIndex - lastSnapshotIndex_myj - 1].term() != currentTerm_myj)
        {
            continue;
        }
        int count = 1;
        for (int peerindex = 0; peerindex < peers_myj.size(); peerindex++)
        {
            if (maxMatchIndex <= matchIndex_myj[peerindex])
            {
                count++;
            }
        }
        if (count >= peers_myj.size() / 2 + 1)
        {
            commitIndex_myj = maxMatchIndex;
        }
    }
}

void KVRaft::serializeLogEntriesVector(boost::archive::binary_oarchive &bo)
{
    printf("序列化数据开始\n");
    std::vector<std::string> strvec;
    for (int i = 0; i < logEntries_myj.size(); i++)
    {
        std::string tmp;
        logEntries_myj[i].SerializeToString(&tmp);
        strvec.emplace_back(tmp);
    }
    bo << strvec;
}

void KVRaft::deserializeLogEntriesVector(boost::archive::binary_iarchive &bi)
{
    std::vector<std::string> strvec;
    bi >> strvec;
    // 反序列化到日志数组
    logEntries_myj.clear();
    for (int i = 0; i < strvec.size(); i++)
    {
        kvraft::LogEntry tmp;
        tmp.ParseFromString(strvec[i]);
        logEntries_myj.emplace_back(tmp);
    }
}

bool KVRaft::compareEntry(kvraft::LogEntry &a, kvraft::LogEntry &b)
{
    if (a.term() != b.term())
    {
        return false;
    }
    kvraft::Command acm = a.command();
    kvraft::Command bcm = b.command();
    if (acm.clientid() != bcm.clientid() || acm.requestid() != bcm.requestid())
    {
        return false;
    }
    return true;
}
