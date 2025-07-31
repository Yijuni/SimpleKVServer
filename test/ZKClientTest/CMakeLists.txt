set(SRC_LISTS zkclient.cpp ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp)
add_executable(zkclienttest ${SRC_LISTS})
target_link_libraries(zkclienttest zookeeper_mt)

# 为ZKClientTest设置专门的输出目录
set_target_properties(zkclienttest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/zkclient_test
)