set(SRC_LIST1 kvserver.cpp ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc 
    ${PROJECT_SOURCE_DIR}/src/ServiceMod/KVService.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/KVServer/KVServer.cpp
    ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp)

set(SRC_LIST2 kvclient.cpp ${PROJECT_SOURCE_DIR}/src/KVClient/KVClient.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc 
    ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp )

add_executable(kvserver ${SRC_LIST1})
target_link_libraries(kvserver kvrpc protobuf zookeeper_mt boost_serialization)

add_executable(kvclient ${SRC_LIST2})
target_link_libraries(kvclient kvrpc protobuf zookeeper_mt)

# 为KVServerTest设置专门的输出目录
set_target_properties(kvserver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/kvserver_test
)

set_target_properties(kvclient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/kvserver_test
)