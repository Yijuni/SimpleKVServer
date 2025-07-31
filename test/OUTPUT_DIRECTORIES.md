# 测试输出目录设置

本文档说明了所有测试的可执行文件输出目录设置。

## 输出目录结构

```
bin/
├── raft_test/              # Raft相关测试
│   └── simplerafttest
├── rocksdb_test/           # RocksDB相关测试
│   └── rocksdbtest
├── hostnet_test/           # 网络相关测试
│   └── HostNetTest
├── kvserver_test/          # KV服务器测试
│   ├── kvserver
│   └── kvclient
├── zkclient_test/          # ZooKeeper客户端测试
│   └── zkclienttest
├── boost_serialize_test/   # Boost序列化测试
│   └── serializetest
├── persister_test/         # 持久化测试
│   └── PersisterTest
├── after_timer_test/       # 定时器测试
│   └── AfterTimerTest
├── lock_queue_test/        # 锁队列测试
│   └── LockQueueTest
└── kvservice_test/         # KV服务测试
    ├── KVservice
    └── KVclient
```

## 各测试目录设置

### 1. RaftTest
- **输出目录**: `bin/raft_test/`
- **可执行文件**: `simplerafttest`
- **设置文件**: `test/RaftTest/CMakeLists.txt`

### 2. RocksDBTest
- **输出目录**: `bin/rocksdb_test/`
- **可执行文件**: `rocksdbtest`
- **设置文件**: `test/RocksDBTest/CMakeLists.txt`

### 3. HostNetTest
- **输出目录**: `bin/hostnet_test/`
- **可执行文件**: `HostNetTest`
- **设置文件**: `test/HostNetTest/CMakeLists.txt`

### 4. KVServerTest
- **输出目录**: `bin/kvserver_test/`
- **可执行文件**: `kvserver`, `kvclient`
- **设置文件**: `test/KVServerTest/CMakeLists.txt`

### 5. ZKClientTest
- **输出目录**: `bin/zkclient_test/`
- **可执行文件**: `zkclienttest`
- **设置文件**: `test/ZKClientTest/CMakeLists.txt`

### 6. BoostSerializeTest
- **输出目录**: `bin/boost_serialize_test/`
- **可执行文件**: `serializetest`
- **设置文件**: `test/BoostSerializeTest/CMakeLists.txt`

### 7. PersisterTest
- **输出目录**: `bin/persister_test/`
- **可执行文件**: `PersisterTest`
- **设置文件**: `test/PersisterTest/CMakeLists.txt`

### 8. AfterTimerTest
- **输出目录**: `bin/after_timer_test/`
- **可执行文件**: `AfterTimerTest`
- **设置文件**: `test/AfterTimerTest/CMakeLists.txt`

### 9. LockQueueTest
- **输出目录**: `bin/lock_queue_test/`
- **可执行文件**: `LockQueueTest`
- **设置文件**: `test/LockQueueTest/CMakeLists.txt`

### 10. KVServiceTest
- **输出目录**: `bin/kvservice_test/`
- **可执行文件**: `KVservice`, `KVclient`
- **设置文件**: `test/KVServiceTest/CMakeLists.txt`

## CMake设置方法

每个测试目录的CMakeLists.txt都使用以下模式：

```cmake
# 添加可执行文件
add_executable(target_name source_files)
target_link_libraries(target_name libraries)

# 设置专门的输出目录
set_target_properties(target_name PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/test_name
)
```

