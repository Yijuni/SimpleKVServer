set(SRC_LIST1 KVservice.cpp ${PROJECT_SOURCE_DIR}/src/ServiceMod/KVService.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp)
set(SRC_LIST2 client.cpp ${PROJECT_SOURCE_DIR}/src/ProtoMod/KVService.pb.cc)

add_executable(KVservice ${SRC_LIST1})
target_link_libraries(KVservice kvrpc protobuf)

add_executable(KVclient ${SRC_LIST2})
target_link_libraries(KVclient kvrpc protobuf)