set(SRC_LIST LockQueueTest.cpp)
add_executable(LockQueueTest ${SRC_LIST})

# 为LockQueueTest设置专门的输出目录
set_target_properties(LockQueueTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/lock_queue_test
)