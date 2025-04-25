set(SRC_LIST serialize.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc)
add_executable(serializetest ${SRC_LIST})
target_link_libraries(serializetest boost_serialization protobuf)