set(SRC_LISTS zkclient.cpp ${PROJECT_SOURCE_DIR}/src/ZookeeperMod/ZKClient.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp)
add_executable(zkclienttest ${SRC_LISTS})
target_link_libraries(zkclienttest zookeeper_mt)