set(SOURCE_LIST AfterTimer.cpp ${PROJECT_SOURCE_DIR}/src/ShareMod/AfterTimer.cpp)
add_executable(AfterTimerTest ${SOURCE_LIST})

# 为AfterTimerTest设置专门的输出目录
set_target_properties(AfterTimerTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/after_timer_test
)