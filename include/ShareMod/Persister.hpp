#ifndef PERSISTER_HPP
#define PERSISTER_HPP
/**
 * 2025-4-24 moyoj
 * 用来持久化数据,raft状态和快照数据存入不同文件
 */
#include <string>
#include <mutex>
#include <fstream>
class Persister
{
public:
    //参数：持久化文件路径
    Persister(std::string raftstate_path="./raft_tmp",std::string snapshot_path="./snapshot_tmp");
    ~Persister();
    std::string ReadRaftState();
    long long RaftStateSize();
    std::string ReadSnapshot();
    long long SnapshotSize();
    void Save(std::string &raftstate,std::string &snapshot);

private:
    std::mutex sourceMutex_myj;
    std::string raftstate_path_myj;
    std::string snapshot_path_myj;

    std::ofstream raftstate_outputFile_myj;
    std::ifstream raftstate_inputFile_myj;
    std::ofstream snapshot_outputFile_myj;
    std::ifstream snapshot_inputFile_myj;

    std::string ReadData(std::ifstream& stream);
    void WriteData(std::ofstream& stream,std::string data);
};



#endif