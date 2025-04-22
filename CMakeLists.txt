# 设置cmake最低版本要求和项目名
cmake_minimum_required(VERSION 3.0)
project(KVServer)

#设置项目可执行文件输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

##指定头文件搜索路径，就算在编写代码过程中，也会影响不同文件夹下的头文件引用是否可以搜索到
include_directories(${PROJECT_SOURCE_DIR}/include/ShareMod)

#指定子目录
add_subdirectory(test)