set(SRC_LIST serialize.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVRaft.pb.cc)
add_executable(serializetest ${SRC_LIST})
target_link_libraries(serializetest boost_serialization protobuf)

# 为BoostSerializeTest设置专门的输出目录
set_target_properties(serializetest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/boost_serialize_test
)