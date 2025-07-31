set(SRC_LIST1 KVservice.cpp ${PROJECT_SOURCE_DIR}/src/ServiceMod/KVService.cpp ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc
    ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp 
    ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp)
set(SRC_LIST2 client.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc)

add_executable(KVservice ${SRC_LIST1})
target_link_libraries(KVservice kvrpc protobuf boost_serialization rocksdb)

add_executable(KVclient ${SRC_LIST2})
target_link_libraries(KVclient kvrpc protobuf)

# 为KVServiceTest设置专门的输出目录
set_target_properties(KVservice PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/kvservice_test
)

set_target_properties(KVclient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/kvservice_test
)