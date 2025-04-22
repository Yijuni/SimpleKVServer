#当前目录所有文件放入RC_LIST
set(SRC_LIST HostNetTest.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/HostNet.cpp )
add_executable(HostNetTest ${SRC_LIST})

