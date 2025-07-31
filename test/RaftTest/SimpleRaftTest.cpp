#include "KVRpcChannel.hpp"
#include "KVRpcProvider.hpp"
#include "Persister.hpp"
#include "Raft.hpp"
#include "LockQueue.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <string>
#include <atomic>

// 简化的Raft节点启动函数
void startSimpleRaftNode(const std::string& ip, uint16_t port, int nodeId) {
    std::cout << "启动Raft节点 " << nodeId << " (端口: " << port << ")" << std::endl;
    
    // 创建Raft实例
    auto raft = std::make_shared<KVRaft>();
    

    
    // 在新线程中运行Provider
    std::thread provider_thread([&]() {
            // 创建RPC Provider
        KVRpcProvider provider(ip, port);
        provider.NotifyService(raft.get());
        provider.Run();
    });
    
    // 创建持久化器
    std::string name = ip + ":" + std::to_string(port);
    auto persister = std::make_shared<Persister>(
        "./" + name + "_state", 
        "./" + name + "_snapshot"
    );
    
    // 创建应用消息通道
    auto applChan = std::make_shared<LockQueue<ApplyMsg>>();
    
    // 创建到其他节点的stub连接
    std::vector<std::shared_ptr<kvraft::KVRaftRPC_Stub>> stubs;
    for (int i = 0; i < 3; i++) {
        uint16_t targetPort = 8000 + i;
        if (targetPort != port) {
            std::cout << "节点 " << nodeId << " 连接到节点 " << i << " (端口: " << targetPort << ")" << std::endl;
            stubs.emplace_back(std::make_shared<kvraft::KVRaftRPC_Stub>(
                new KVRpcChannel("127.0.0.1", targetPort)
            ));
        }
    }
    // 创建rocksdb api
    std::shared_ptr<RocksDBAPI> db = std::make_shared<RocksDBAPI>();
    db->DBOpen();
    // 启动Raft共识算法
    std::cout << "节点 " << nodeId << " 启动共识算法..." << std::endl;
    raft->Make(stubs, name, persister, applChan,db);
    
    // 监听应用消息
    std::thread apply_thread([&applChan, nodeId]() {
        while (true) {
            try {
                ApplyMsg msg = applChan->pop();
                std::cout << "节点 " << nodeId << " 收到应用消息: " 
                          << "CommandValid=" << msg.commandValid 
                          << ", Command=" << msg.command.key()<<" :"<<msg.command.value() 
                          << ", CommandIndex=" << msg.commandIndex << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "节点 " << nodeId << " 处理应用消息异常: " << e.what() << std::endl;
                break;
            }
        }
    });
    
    std::cout << "节点 " << nodeId << " 启动完成" << std::endl;
    
    // 等待线程完成
    provider_thread.join();
    apply_thread.join();
}

int main(int argc, char** argv) {
    std::cout << "=== 简化Raft测试程序 ===" << std::endl;
    
    if (argc != 2) {
        std::cout << "用法: " << argv[0] << " <节点ID>" << std::endl;
        std::cout << "节点ID: 0, 1, 或 2" << std::endl;
        return 1;
    }
    
    int nodeId = std::stoi(argv[1]);
    if (nodeId < 0 || nodeId > 2) {
        std::cout << "节点ID必须在0-2之间" << std::endl;
        return 1;
    }
    
    uint16_t port = 8000 + nodeId;
    std::string ip = "127.0.0.1";
    
    std::cout << "启动节点 " << nodeId << " 在端口 " << port << std::endl;
    
    try {
        startSimpleRaftNode(ip, port, nodeId);
    } catch (const std::exception& e) {
        std::cerr << "启动节点时发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 