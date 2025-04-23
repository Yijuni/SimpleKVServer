# 设置cmake最低版本要求和项目名
cmake_minimum_required(VERSION 3.0)
project(KVServer)

#设置项目可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

##指定头文件搜索路径，就算在编写代码过程中，也会影响不同文件夹下的头文件引用是否可以搜索到
include_directories(${PROJECT_SOURCE_DIR}/include/ShareMod)
include_directories(${PROJECT_SOURCE_DIR}/include/RpcMod)
include_directories(${PROJECT_SOURCE_DIR}/include/ProtoMod)
include_directories(${PROJECT_SOURCE_DIR}/include/ServiceMod)
#设置项目库文件输出路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#设置项目库文件搜索路径 -L <目录路径>
link_directories(${PROJECT_SOURCE_DIR}/lib)

#指定子目录
add_subdirectory(src)
add_subdirectory(test)
