set(SRC_LIST makeserverstubtest.cpp ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp
    ${PROJECT_SOURCE_DIR}/src/ServiceMod/KVService.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc
    ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp
    ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp
    ${PROJECT_SOURCE_DIR}/src/MakeServerStub/MakeServerStub.cpp ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp
    ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp)

add_executable(makeserverstubtest ${SRC_LIST})
target_link_libraries(makeserverstubtest kvrpc protobuf zookeeper_mt rocksdb boost_serialization)
set_target_properties(makeserverstubtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/makeserverstub_test
)