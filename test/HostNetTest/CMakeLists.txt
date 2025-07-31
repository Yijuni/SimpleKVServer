#当前目录所有文件放入RC_LIST
set(SRC_LIST HostNetTest.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/HostNet.cpp )
add_executable(HostNetTest ${SRC_LIST})

# 为HostNetTest设置专门的输出目录
set_target_properties(HostNetTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/hostnet_test
)

