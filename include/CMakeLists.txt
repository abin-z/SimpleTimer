# inifile.h 头文件库（Header-Only） INTERFACE 类型
# INTERFACE 目标不会生成 .a 或 .so 文件，但可以被 target_link_libraries 依赖。
add_library(simple_timer INTERFACE)
target_include_directories(simple_timer INTERFACE ${CMAKE_CURRENT_LIST_DIR})

# POSIX系统下使用 pthread 库
find_package(Threads REQUIRED)
target_link_libraries(simple_timer INTERFACE Threads::Threads)