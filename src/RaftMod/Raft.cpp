#include "Raft.hpp"
#include <Logger.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <sstream>
KVRaft::KVRaft(std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> &peers, std::string &name, std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyChan)
    :peers_myj(peers),name_myj(name),persister_myj(persister),applyChan_myj(applyChan)
{
    currentTerm_myj = 0;
    voterFor_myj = -1;
    status_myj = STATUS::FOLLOWER;
    logEntries_myj = std::vector<kvraft::LogEntry>(0);

    heartbeatsTime_myj = 100;
    electionTimeout_myj = 1000;

    commitIndex_myj = -1;
    lastApplied_myj = -1;
    nextIndex_myj.resize(peers_myj.size(),0);
    matchIndex_myj.resize(peers_myj.size(),0);    
    applyChan_myj = applyChan;

    lastSnapshotIndex_myj = -1;
    lastSnapshotTerm_myj = -1;

    //心跳定时器和选举超时定时器启动
    heartbeatsTimer_myj = new AfterTimer(heartbeatsTime_myj,0,[&](){
        LOG_INFO("server[%s]>>发送心跳",name.c_str());
        std::unique_lock<std::mutex> lock(sourceMutex_myj);
        if(status_myj==LEADER){
            std::thread td(std::bind(&KVRaft::appendEntriesToFollower,this,currentTerm_myj,commitIndex_myj,peers_myj.size()));
            td.detach();
            heartbeatsTimer_myj->Reset();
        }
    });
    electionTimer_myj = new AfterTimer(electionTimeout_myj,0,[&](){
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
        }
    });

    //读取持久化数据恢复
    std::string raftstatedata = persister_myj->ReadRaftState();
    readPersist(raftstatedata);

    //启动定时上传日志线程
    std::thread td(std::bind(&KVRaft::applyEntries,this,20));
    td.detach();
    //启动选举超时定时器
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
    if(status_myj==LEADER){
        isLeader = true;
    }
    term = currentTerm_myj;
    return isLeader;
}

void KVRaft::Snapshot(long long index, std::string &snapshot)
{

}

bool KVRaft::Start(kvraft::Command command, long long &index, long long &term)
{
    return false;
}

void KVRaft::persist(std::string &snapshot)
{

}

void KVRaft::readPersist(std::string &data)
{
}

void KVRaft::electStart(long long curterm, long long lastLogIndex, long long lastLogTerm, long long peerscount)
{
}

void KVRaft::appendEntriesToFollower(long long curterm, long long leaderCommit, long long peerscount)
{
}

bool KVRaft::matchNewEntries(std::vector<kvraft::LogEntry> &entries, long long preLogIndex, long long preLogTerm, kvraft::AppendEntriesResponse *resposne)
{
    return false;
}

void KVRaft::applyEntries(long long sleep)
{
}

void KVRaft::updateCommitIndex()
{
}

std::string KVRaft::serializeLogEntriesVector()
{
    return std::string();
}

void KVRaft::deserializeLogEntriesVector()
{
}
