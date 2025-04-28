set(SRC_LISTS RaftServer.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp 
    ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp ${PROJECT_SOURCE_DIR}/src/RaftMod/Raft.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc)
add_executable(raftservertest ${SRC_LISTS})
target_link_libraries(raftservertest kvrpc protobuf boost_serialization)