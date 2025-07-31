#include "Persister.hpp"
#include "Logger.hpp"
Persister::Persister(std::string raftstate_path, std::string snapshot_path)
    : raftstate_path_myj(raftstate_path), snapshot_path_myj(snapshot_path),
      // 必须用二进制方式打开文件，因为boost库序列化的数据是二进制数据，会包含特殊字符'\0'之类的，输入流要app方式打开，防止清除原内容
      raftstate_inputFile_myj(raftstate_path_myj, std::ios::in | std::ios::app | std::ios::binary),
      snapshot_inputFile_myj(snapshot_path_myj, std::ios::in | std::ios::app | std::ios::binary)
{
    if (!raftstate_inputFile_myj.is_open())
    {
        LOG_ERROR("raftstate：%s 持久化文件无法打开", raftstate_path_myj.c_str());
    }
    if (!snapshot_inputFile_myj.is_open())
    {
        LOG_ERROR("snapshot: %s 持久化文件无法打开", raftstate_path_myj.c_str());
    }
}

Persister::~Persister()
{
    raftstate_inputFile_myj.close();
    snapshot_inputFile_myj.close();
}

std::string Persister::ReadRaftState()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return ReadData(raftstate_inputFile_myj);
}

long long Persister::RaftStateSize()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return ReadData(raftstate_inputFile_myj).size();
}

std::string Persister::ReadSnapshot()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return ReadData(snapshot_inputFile_myj);
}

long long Persister::SnapshotSize()
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    return ReadData(snapshot_inputFile_myj).size();
}

void Persister::Save(std::string &raftstate, std::string &snapshot)
{
    std::unique_lock<std::mutex> lock(sourceMutex_myj);
    raftstate_outputFile_myj = std::ofstream(raftstate_path_myj, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!raftstate_outputFile_myj.is_open())
    {
        LOG_ERROR("raftstate：%s 持久化文件无法打开", raftstate_path_myj.c_str());
        return;
    }
    WriteData(raftstate_outputFile_myj, raftstate);
    raftstate_outputFile_myj.close();

    // 这次没传入快照信息，说明快照没变，不需要写入
    if(snapshot.size()!=0){
        snapshot_outputFile_myj = std::ofstream(snapshot_path_myj, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!snapshot_outputFile_myj.is_open())
        {
            LOG_ERROR("snapshot: %s 持久化文件无法打开", raftstate_path_myj.c_str());
            return;
        }
        WriteData(snapshot_outputFile_myj, snapshot);
        snapshot_outputFile_myj.close();
    }
    
}

std::string Persister::ReadData(std::ifstream &stream)
{
    std::string data;
    // 清除流的错误状态标志
    stream.clear();
    // 文件指针调整到末尾
    stream.seekg(0, std::ios::end);
    // string大小调整为文件中字符数,tellg返回文件指针指向的位置相对文件开始位置的偏移
    data.resize(stream.tellg());
    // 文件指针指向文件开头
    stream.seekg(0, std::ios::beg);
    // 读数据，这里不能用>> <<输入输出，因为遇到\0,\n,空格可能会暂停输入，不适合输入输出二进制数据
    stream.read(data.data(), data.size());
    return data;
}

void Persister::WriteData(std::ofstream &stream, std::string data)
{
    // 清除流的错误状态标志
    stream.clear();
    // 写指针位置转移到文件头部
    stream.seekp(0, std::ios::beg);
    // 先清除文件原本内容
    stream.write(data.data(), data.size());
    stream.flush();
}
