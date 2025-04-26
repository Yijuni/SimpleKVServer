#include "Raft.hpp"
#include "Logger.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <sstream>
#include "KVRpcController.hpp"

KVRaft::KVRaft(std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> &peers, std::string &name, std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyChan)
    : peers_myj(peers), name_myj(name), persister_myj(persister), applyChan_myj(applyChan)
{
    currentTerm_myj = 0;
    voterFor_myj = -1;
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
    heartbeatsTimer_myj = new AfterTimer(heartbeatsTime_myj, 0, [&]()
                                         {
        LOG_INFO("server[%s]>>发送心跳",name.c_str());
        std::unique_lock<std::mutex> lock(sourceMutex_myj);
        if(status_myj==LEADER){
            std::thread td(std::bind(&KVRaft::appendEntriesToFollower,this,currentTerm_myj,commitIndex_myj,peers_myj.size()));
            td.detach();
            heartbeatsTimer_myj->Reset();
        } });
    electionTimer_myj = new AfterTimer(electionTimeout_myj, 0, [&]()
                                       {
        LOG_INFO("server[%s]>> 开始leader选举",name_myj.c_str());
        std::unique_lock<std::mutex> lock(sourceMutex_myj);
        if(status_myj==FOLLOWER || status_myj==CANDIDATE){
            //成为候选者
            status_myj == CANDIDATE;
            voterFor_myj = name;
            currentTerm_myj++;
            //持久化数据
            std::string snapshotdata = persister_myj->ReadSnapshot();
            persist(snapshotdata);
            
            long long lastLogIndex = -1;
            long long lastLogTerm = -1;
            if(logEntries_myj.size()!=0 || lastSnapshotIndex_myj!=-1){
                if(logEntries_myj.size()!=0){
                    lastLogIndex = logEntries_myj.size() + lastSnapshotIndex_myj+1 -1;
                    lastLogTerm = logEntries_myj[logEntries_myj.size()-1].term();
                }else{
                    lastLogIndex = lastSnapshotIndex_myj;
                    lastLogTerm = lastSnapshotTerm_myj;
                 }
            }
            std::thread td(std::bind(&KVRaft::electStart,this,currentTerm_myj,lastLogIndex,lastLogTerm,peers_myj.size()));
            td.detach();
            electionTimer_myj->Reset();
        } });

    // 读取持久化数据恢复
    std::string raftstatedata = persister_myj->ReadRaftState();
    readPersist(raftstatedata);

    // 启动定时上传日志线程
    std::thread td(std::bind(&KVRaft::applyEntries, this, 20));
    td.detach();
    // 启动选举超时定时器
    electionTimer_myj->Reset();
}

KVRaft::KVRaft()
{
}

void KVRaft::AppendEntries(google::protobuf::RpcController *controller, const ::kvraft::AppendEntriesRequest *request, ::kvraft::AppendEntriesResponse *response, ::google::protobuf::Closure *done)
{
}

void KVRaft::RequestVote(google::protobuf::RpcController *controller, const ::kvraft::RequestVoteRequest *request, ::kvraft::RequestVoteResponse *response, ::google::protobuf::Closure *done)
{
}

void KVRaft::InstallSnapshot(google::protobuf::RpcController *controller, const ::kvraft::InstallSnapshotRequest *request, ::kvraft::InstallSnapshotResponse *response, ::google::protobuf::Closure *done)
{
}

bool KVRaft::GetState(long long &term)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
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
    LOG_INFO("server[%s]>>收到快照请求", name_myj.c_str());
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
    LOG_INFO("server[%s]>>生成快照:lastSnapshotindex:%lld,lastSnapshotTerm:%lld", lastSnapshotIndex_myj, lastSnapshotTerm_myj);
}

bool KVRaft::Start(kvraft::Command command, long long &index, long long &term)
{
    return false;
}

void KVRaft::persist(std::string &snapshot)
{
    std::ostringstream ops;
    boost::archive::binary_oarchive bo(ops);

    bo << currentTerm_myj;
    bo << voterFor_myj;
    bo << lastSnapshotIndex_myj;
    bo << lastSnapshotTerm_myj;
    serializeLogEntriesVector(bo);
    std::string ostring = ops.str();
    persister_myj->Save(ostring, snapshot);

    LOG_INFO("server[%s]>>持久化数据[%s]", name_myj.c_str(), ostring.c_str());
}

void KVRaft::readPersist(std::string &data)
{
    std::istringstream ips(data);
    boost::archive::binary_iarchive bi(ips);

    bi >> currentTerm_myj;
    bi >> voterFor_myj;
    bi >> lastSnapshotIndex_myj;
    bi >> lastSnapshotTerm_myj;
    deserializeLogEntriesVector(bi);

    LOG_INFO("server[%s]>>重启，term:%lld,voteFor:%lld,lastSnapshotIndex:%lld,lastSnapshotTerm:%lld", name_myj.c_str(), currentTerm_myj, voterFor_myj, lastSnapshotIndex_myj, lastSnapshotTerm_myj);
}

void KVRaft::electStart(std::string name, long long curterm, long long lastLogIndex, long long lastLogTerm, long long peerscount)
{
    int voteCount = 1;
    LOG_INFO("server[%s]>>开始leader选举", name.c_str());
    std::vector<std::thread> waitGroup;
    for (int i = 0; i < peerscount; i++)
    {
        // 注意这里 i必须通过参数传入，如果直接通过引用获得i，可能会导致发送时i的数值不确定，因为新开的线程与此线程并发执行
        waitGroup.emplace_back([&](int sendindex){  
            kvraft::RequestVoteRequest voteRequest;
            kvraft::RequestVoteResponse voteResponse;

            voteRequest.set_candidateid(name);
            voteRequest.set_term(curterm);
            voteRequest.set_lastlogindex(lastLogIndex);
            voteRequest.set_lastlogterm(lastLogTerm);

            KVRpcController controller;
            peers_myj[sendindex]->RequestVote(&controller,&voteRequest,&voteResponse,nullptr);
            if(!controller.Failed()){
                std::unique_lock<std::mutex> lock(sourceMutex_myj);
                //状态没变
                if(status_myj==CANDIDATE && currentTerm_myj==curterm){
                    if(voteResponse.votegranted()){
                        voteCount++;
                        //投票数超过半数
                        if(voteCount>peerscount/2+1){
                            //变为leader，投票对象设为-1以便失去leader身份时可以给其他人投票
                            status_myj = LEADER;
                            voterFor_myj = -1;

                            //持久化
                            std::string snapshotdata = persister_myj->ReadSnapshot();
                            persist(snapshotdata);

                            //nextIndex数组和matchIndex数组重置(最后一个日志的下一个日志的index(原因看论文)，-1)
                            for(int index=0;index<peerscount;index++){
                                nextIndex_myj[index] = logEntries_myj.size() + lastSnapshotIndex_myj + 1;
                                matchIndex_myj[index] = -1; 
                            }

                            //立即发送心跳并启动心跳计时器
                            std::thread td(std::bind(&KVRaft::appendEntriesToFollower,this,curterm,commitIndex_myj,peerscount));
                            td.detach();
                            heartbeatsTimer_myj->Reset();
                        }
                    }else if(voteResponse.term()>currentTerm_myj){
                        currentTerm_myj = voteResponse.term();
                        status_myj = FOLLOWER;
                        voterFor_myj = -1;
                        electionTimer_myj->Reset();
                        std::string snapshotdata = persister_myj->ReadSnapshot();
                        persist(snapshotdata);
                    }
                }
            }else{
                LOG_INFO("server[%s]>>发送选举消息失败,pos=%d,errormsg:%s",name_myj.c_str(),sendindex,controller.ErrorText().c_str());
            } }, i);
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
        voterFor_myj = -1;
        electionTimer_myj->Reset();
        std::string snapshotdata = persister_myj->ReadSnapshot();
        persist(snapshotdata);
        LOG_INFO("server[%s]>>没收到足够选票", name.c_str());
    }
}

void KVRaft::appendEntriesToFollower(long long curterm, long long leaderCommit, long long peerscount)
{
    std::unique_lock lock(status_myj);
    std::vector<kvraft::LogEntry> logstmp(logEntries_myj.begin(), logEntries_myj.end());
    lock.unlock();

    LOG_INFO("server[%s]>>开始发送日志/心跳/快照",name_myj.c_str());
    for (int i = 0; i < peerscount; i++)
    {
        std::thread td([&](int sendindex){
            LOG_INFO("server[%s]>>开始发送心跳/日志/快照，pos=%d",name_myj.c_str(),sendindex);
            std::unique_lock<std::mutex> td_lock(sourceMutex_myj);
            if(currentTerm_myj!=curterm){
                td_lock.unlock();
                return;
            }
            long long nextIndex = nextIndex_myj[sendindex];
            if(nextIndex<=lastSnapshotIndex_myj){ //已经没这条日志，发送快照
                kvraft::InstallSnapshotRequest installSnapshotRequest;
                kvraft::InstallSnapshotResponse installSnapshotResponse;

                long long curSnapshotIndex = lastSnapshotIndex_myj;

                installSnapshotRequest.set_data(persister_myj->ReadSnapshot());
                installSnapshotRequest.set_lastincludeindex(lastSnapshotIndex_myj);
                installSnapshotRequest.set_lastincludeterm(lastSnapshotTerm_myj);
                installSnapshotRequest.set_leaderid(name_myj);
                installSnapshotRequest.set_term(curterm);
                td_lock.unlock();

                KVRpcController controller;
                peers_myj[sendindex]->InstallSnapshot(&controller,&installSnapshotRequest,&installSnapshotResponse,nullptr);

                if(!controller.Failed()){
                    td_lock.lock();
                    LOG_INFO("server[%s]>>成功收到发送快照的回复,pos=%d",name_myj.c_str(),sendindex);
                    if(currentTerm_myj==curterm){ //状态没变
                        if(installSnapshotResponse.term()>currentTerm_myj){
                            currentTerm_myj = installSnapshotResponse.term();
                            status_myj = FOLLOWER;
                            voterFor_myj = -1;
                            electionTimer_myj->Reset();

                            std::string snapshotdata = persister_myj->ReadSnapshot();
                            persist(snapshotdata);

                            return;
                        }

                        //nextIndex没变，更新
                        if(nextIndex_myj[sendindex]==nextIndex){
                            nextIndex_myj[sendindex] = curSnapshotIndex+1;
                        }
                        matchIndex_myj[sendindex] = nextIndex_myj[sendindex]-1;
                        updateCommitIndex();
                    }
                }

            }else{//发送日志
                //判断要发送日志是否产生改变，如果产生改变就不发送了，因为剩下的日志可能不是
                if(logstmp.size()!=logEntries_myj.size()){
                    td_lock.unlock();
                    return;
                }
                for(int index=0;index<logstmp.size();index++){
                    if(!compareEntry(logEntries_myj[index],logstmp[index])){
                        td_lock.unlock();
                        return;
                    }
                }
                kvraft::AppendEntriesRequest appendEntriesRequest;
                kvraft::AppendEntriesResponse appendEntriesResponse;

                appendEntriesRequest.set_leaderid(name_myj);
                appendEntriesRequest.set_term(curterm);
                appendEntriesRequest.set_leadercommit(leaderCommit);

                //前一条日志的index和term，有可能前一条日志为快照中的最后一个日志的index
                appendEntriesRequest.set_prelogindex(nextIndex-1);
                if((nextIndex-lastSnapshotIndex_myj-1-1) >=0){
                    appendEntriesRequest.set_prelogterm(logstmp[nextIndex-lastSnapshotIndex_myj-1-1].term());
                }else{
                    appendEntriesRequest.set_prelogterm(lastSnapshotIndex_myj);
                }

                //将发送的日志装入request
                for(int index = nextIndex-lastSnapshotIndex_myj-1;index<logstmp.size();index++){
                    kvraft::LogEntry* logtmp = appendEntriesRequest.add_logentries();
                    *logtmp = logstmp[index];
                }

                td_lock.unlock();

                KVRpcController controller;
                peers_myj[sendindex]->AppendEntries(&controller,&appendEntriesRequest,&appendEntriesResponse,nullptr);

                if(!controller.Failed()){
                    td_lock.lock();
                    LOG_INFO("server[%s]>>成功收到发送心跳/日志的回复,pos=%d",name_myj.c_str(),sendindex);
                    if(currentTerm_myj==curterm){
                        if(!appendEntriesResponse.success()){ 
                            if(currentTerm_myj<appendEntriesResponse.term()){//term落后
                                currentTerm_myj = appendEntriesResponse.term();
                                status_myj = FOLLOWER;
                                voterFor_myj = -1;
                                electionTimer_myj->Reset();

                                std::string snapshotdata = persister_myj->ReadSnapshot();
                                persist(snapshotdata);
                            }else{ //没匹配上日志
                                nextIndex_myj[sendindex] = appendEntriesResponse.fastback();
                            }
                        }
                        if(appendEntriesRequest.logentries_size()==0 || !appendEntriesResponse.success()){
                            return;
                        }
                        //更新下次发送的nextIndex
                        if(nextIndex==nextIndex_myj[sendindex]){
                            nextIndex_myj[sendindex] = nextIndex + appendEntriesRequest.logentries_size();
                        }
                        matchIndex_myj[sendindex] = nextIndex_myj[sendindex]-1;
                        updateCommitIndex();
                    }
                }else{
                    LOG_INFO("server[%s]>>发送心跳/心跳失败,pos=%d,errormsg:%s",name_myj.c_str(),sendindex,controller.ErrorText().c_str());
                }

            } }, i);
        td.detach();
    }
}

bool KVRaft::matchNewEntries(std::vector<kvraft::LogEntry> &entries, long long preLogIndex, long long preLogTerm, kvraft::AppendEntriesResponse *resposne)
{
    if(preLogIndex==-1||preLogIndex<=lastSnapshotIndex_myj){
        if(preLogIndex<lastSnapshotIndex_myj){
            resposne->set_fastback(commitIndex_myj+1);
            resposne->set_success(false);
            return false;
        }
    }else if(logEntries_myj.size()<=preLogIndex-lastSnapshotIndex_myj-1){ 
        LOG_INFO("server[%s]>>当前节点没有%lld位置处的日志",name_myj.c_str(),preLogIndex);
        resposne->set_fastback(logEntries_myj.size()+lastSnapshotIndex_myj+1);
        resposne->set_success(false);
        return false;
    }else if(logEntries_myj[preLogIndex-lastSnapshotIndex_myj-1].term()!=preLogTerm){ 
        LOG_INFO("server[%s]>>当前节点在%lld位置处的日志与leader的不匹配",name_myj.c_str(),preLogIndex);
        resposne->set_fastback(commitIndex_myj+1);
        resposne->set_success(false);
        return false;
    }
    int index = 0;
    for(;index<entries.size();index++){
        //追加日志数组中的日志对应到当前节点日志数组的日志位置，超过当前节点日志数组的大小就跳出循环
        if((index+preLogIndex+1-lastSnapshotIndex_myj-1)>=logEntries_myj.size()){
            break;
        }
        //两个数组对应日志的term不同
        if(logEntries_myj[index+preLogIndex+1-lastSnapshotIndex_myj-1].term()!=entries[index].term()){
            //只留下匹配的，不匹配的全部删除
            std::vector<kvraft::LogEntry> logstmp(logEntries_myj.begin(),logEntries_myj.begin()+index+preLogIndex+1-lastSnapshotIndex_myj-1);
            logEntries_myj.clear();
            logEntries_myj.swap(logstmp);
            break;
        }
    }
    //如果有剩余就把剩余的日志追加到当前节点
    for(;index<entries.size();index++){
        logEntries_myj.push_back(entries[index]);
    }
    return true;
}

void KVRaft::applyEntries(long long sleep)
{
    std::unique_lock<std::mutex> td_lock(sourceMutex_myj);
    while(1){
        
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
    }
}

void KVRaft::updateCommitIndex()
{
}

void KVRaft::serializeLogEntriesVector(boost::archive::binary_oarchive &bo)
{
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
