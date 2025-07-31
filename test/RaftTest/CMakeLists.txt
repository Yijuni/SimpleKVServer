# SimpleRaftTest
set(SRC_LISTS_SIMPLE SimpleRaftTest.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp 
    ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp)
add_executable(simplerafttest ${SRC_LISTS_SIMPLE})
target_link_libraries(simplerafttest kvrpc protobuf boost_serialization rocksdb)

# 为SimpleRaftTest设置专门的输出目录
set_target_properties(simplerafttest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/raft_test
)