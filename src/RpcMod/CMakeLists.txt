#当前目录所有文件放入RC_LIST
set(SRC_LIST KVRpcChannel.cpp KVRpcController.cpp KVRpcProvider.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/RpcHeader.pb.cc ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/HostNet.cpp)

#生成动态库
add_library(kvrpc ${SRC_LIST})
target_link_libraries(kvrpc muduo_net muduo_base pthread)