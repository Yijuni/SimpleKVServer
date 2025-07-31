set(SRC_LIST Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Persister.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/Logger.cpp)
add_executable(PersisterTest ${SRC_LIST})

# 为PersisterTest设置专门的输出目录
set_target_properties(PersisterTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/persister_test
)