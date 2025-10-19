set(SRC_LIST1 shardctlserver.cpp ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc 
    ${PROJECT_SOURCE_DIR}/src/ShardCtrlerServiceMod/ShardCtrlerService.cpp  ${PROJECT_SOURCE_DIR}/src/ServiceMod/KVService.cpp
    ${PROJECT_SOURCE_DIR}/src/ProtoMod/ShardCtrler.pb.cc  ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc
    ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/KVServer/KVServer.cpp
    ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp)

set(SRC_LIST2 sharfctlclient.cpp ${PROJECT_SOURCE_DIR}/src/ShardCtrlerClient/ShardCtrlerClient.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/ShardCtrler.pb.cc
    ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp )

add_executable(shardctlserver ${SRC_LIST1})
target_link_libraries(shardctlserver kvrpc protobuf zookeeper_mt boost_serialization rocksdb)

add_executable(shardctlclient ${SRC_LIST2})
target_link_libraries(shardctlclient kvrpc protobuf zookeeper_mt)

# 为KVServerTest设置专门的输出目录
set_target_properties(shardctlserver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/shard_ctler_server_test
)

set_target_properties(shardctlclient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/shard_ctler_server_test
)