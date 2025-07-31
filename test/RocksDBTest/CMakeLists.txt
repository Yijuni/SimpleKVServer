set(SRC_LIST RocksDB.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp)
add_executable(rocksdbtest ${SRC_LIST})
target_link_libraries(rocksdbtest rocksdb)

# 为RocksDBTest设置专门的输出目录
set_target_properties(rocksdbtest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/rocksdb_test
)