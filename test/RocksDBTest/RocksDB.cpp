#include <rocksdbapi.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

void testRaftMetaOperations(RocksDBAPI& db) {
    std::cout << "\n=== 测试Raft元数据操作 ===" << std::endl;
    
    // 测试写入
    std::string key1 = "raft_term";
    std::string value1 = "5";
    std::cout << "写入Raft元数据: " << key1 << " = " << value1 << std::endl;
    if (db.RaftMetaPut(key1, value1)) {
        std::cout << "✓ Raft元数据写入成功" << std::endl;
    } else {
        std::cout << "✗ Raft元数据写入失败" << std::endl;
        return;
    }
    
    // 测试读取
    std::string retrieved_value1;
    std::cout << "读取Raft元数据: " << key1 << std::endl;
    if (db.RaftMetaGet(key1, retrieved_value1)) {
        std::cout << "✓ Raft元数据读取成功: " << key1 << " = " << retrieved_value1 << std::endl;
        if (retrieved_value1 == value1) {
            std::cout << "✓ 数据一致性验证通过" << std::endl;
        } else {
            std::cout << "✗ 数据不一致: 期望=" << value1 << ", 实际=" << retrieved_value1 << std::endl;
        }
    } else {
        std::cout << "✗ Raft元数据读取失败" << std::endl;
        return;
    }
    
    // 测试删除
    std::cout << "删除Raft元数据: " << key1 << std::endl;
    if (db.RaftMetaDelete(key1)) {
        std::cout << "✓ Raft元数据删除成功" << std::endl;
        
        // 验证删除
        std::string check_value;
        if (!db.RaftMetaGet(key1, check_value)) {
            std::cout << "✓ 删除验证成功（数据已不存在）" << std::endl;
        } else {
            std::cout << "✗ 删除验证失败（数据仍然存在）" << std::endl;
        }
    } else {
        std::cout << "✗ Raft元数据删除失败" << std::endl;
    }
}

void testKVOperations(RocksDBAPI& db) {
    std::cout << "\n=== 测试KV数据操作 ===" << std::endl;
    
    // 测试写入
    std::string key1 = "user:1001";
    std::string value1 = "{\"name\":\"张三\",\"age\":25}";
    std::cout << "写入KV数据: " << key1 << " = " << value1 << std::endl;
    if (db.KVPut(key1, value1)) {
        std::cout << "✓ KV数据写入成功" << std::endl;
    } else {
        std::cout << "✗ KV数据写入失败" << std::endl;
        return;
    }
    
    // 测试读取
    std::string retrieved_value1;
    std::cout << "读取KV数据: " << key1 << std::endl;
    if (db.KVGet(key1, retrieved_value1)) {
        std::cout << "✓ KV数据读取成功: " << key1 << " = " << retrieved_value1 << std::endl;
        if (retrieved_value1 == value1) {
            std::cout << "✓ 数据一致性验证通过" << std::endl;
        } else {
            std::cout << "✗ 数据不一致: 期望=" << value1 << ", 实际=" << retrieved_value1 << std::endl;
        }
    } else {
        std::cout << "✗ KV数据读取失败" << std::endl;
        return;
    }
    
    // 测试多个KV数据
    std::string key2 = "user:1002";
    std::string value2 = "{\"name\":\"李四\",\"age\":30}";
    std::cout << "写入第二个KV数据: " << key2 << " = " << value2 << std::endl;
    if (db.KVPut(key2, value2)) {
        std::cout << "✓ 第二个KV数据写入成功" << std::endl;
    } else {
        std::cout << "✗ 第二个KV数据写入失败" << std::endl;
    }
    
    // 测试删除
    std::cout << "删除KV数据: " << key1 << std::endl;
    if (db.KVDelete(key1)) {
        std::cout << "✓ KV数据删除成功" << std::endl;
        
        // 验证删除
        std::string check_value;
        if (!db.KVGet(key1, check_value)) {
            std::cout << "✓ 删除验证成功（数据已不存在）" << std::endl;
        } else {
            std::cout << "✗ 删除验证失败（数据仍然存在）" << std::endl;
        }
    } else {
        std::cout << "✗ KV数据删除失败" << std::endl;
    }
}

void testInteractiveMode(RocksDBAPI& db) {
    std::cout << "\n=== 交互式测试模式 ===" << std::endl;
    std::cout << "请输入要测试的操作类型 (1:Raft元数据, 2:KV数据, 0:退出): ";
    
    int choice;
    std::cin >> choice;
    
    if (choice == 0) {
        std::cout << "退出交互式测试" << std::endl;
        return;
    }
    
    std::string key, value;
    std::cout << "请输入key: ";
    std::cin >> key;
    
    if (choice == 1) {
        // Raft元数据测试
        std::cout << "请输入value: ";
        std::cin >> value;
        
        std::cout << "执行RaftMetaPut..." << std::endl;
        if (db.RaftMetaPut(key, value)) {
            std::cout << "✓ 写入成功" << std::endl;
        } else {
            std::cout << "✗ 写入失败" << std::endl;
            return;
        }
        
        std::string retrieved_value;
        std::cout << "执行RaftMetaGet..." << std::endl;
        if (db.RaftMetaGet(key, retrieved_value)) {
            std::cout << "✓ 读取成功: " << key << " = " << retrieved_value << std::endl;
        } else {
            std::cout << "✗ 读取失败" << std::endl;
        }
        
    } else if (choice == 2) {
        // KV数据测试
        std::cout << "请输入value: ";
        std::cin >> value;
        
        std::cout << "执行KVPut..." << std::endl;
        if (db.KVPut(key, value)) {
            std::cout << "✓ 写入成功" << std::endl;
        } else {
            std::cout << "✗ 写入失败" << std::endl;
            return;
        }
        
        std::string retrieved_value;
        std::cout << "执行KVGet..." << std::endl;
        if (db.KVGet(key, retrieved_value)) {
            std::cout << "✓ 读取成功: " << key << " = " << retrieved_value << std::endl;
        } else {
            std::cout << "✗ 读取失败" << std::endl;
        }
    }
}

int main() {
    std::cout << "=== RocksDBAPI 完整功能测试 ===" << std::endl;
    
    try {
        // 获取RocksDBAPI实例
        std::cout << "初始化RocksDBAPI..." << std::endl;
        RocksDBAPI& db = RocksDBAPI::GetInstance("./test_db");
        
        // 打开数据库
        std::cout << "打开数据库..." << std::endl;
        if (!db.DBOpen()) {
            std::cerr << "✗ 数据库打开失败" << std::endl;
            return 1;
        }
        std::cout << "✓ 数据库打开成功" << std::endl;
        
        // 执行自动化测试
        testRaftMetaOperations(db);
        testKVOperations(db);
        
        // 交互式测试
        char continue_test;
        std::cout << "\n是否进行交互式测试? (y/n): ";
        std::cin >> continue_test;
        
        if (continue_test == 'y' || continue_test == 'Y') {
            testInteractiveMode(db);
        }
        
        std::cout << "\n=== 测试完成 ===" << std::endl;
        std::cout << "程序即将退出，等待资源清理..." << std::endl;
        
        // 等待一段时间确保资源正确清理
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
    } catch (const std::exception& e) {
        std::cerr << "✗ 发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}