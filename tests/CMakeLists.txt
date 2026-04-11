# 如果是 MSVC 编译器，设置源文件为 UTF-8 编码
if (MSVC)
  add_compile_options("/utf-8")
endif()

# 添加测试可执行文件，将 test_timer.cpp 作为源文件
add_executable(timertest test_timer.cpp)

# 链接被测库 simple_timer
target_link_libraries(timertest PRIVATE simple_timer)
target_link_libraries(timertest PRIVATE Catch2_v2)  # 链接 Catch2 测试框架

# 允许 add_test() 添加测试
enable_testing()

# 注册 timertest 作为 CTest 可识别的测试用例
# 当执行 `ctest` 时，会运行 timertest 并检查其返回值
add_test(NAME timertest COMMAND timertest)