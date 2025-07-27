set(SRC_LIST RocksDB.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp ${PROJECT_SOURCE_DIR}/src/RocksDBMod/rocksdbapi.cpp)

add_executable(rocksdbtest ${SRC_LIST})

target_link_libraries(rocksdbtest rocksdb)