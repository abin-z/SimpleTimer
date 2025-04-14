add_executable(main main.cpp)
target_link_libraries(main PRIVATE simple_timer)

add_executable(timer_basic timer_basic.cpp)
target_link_libraries(timer_basic PRIVATE simple_timer)

add_executable(timer_pause timer_pause.cpp)
target_link_libraries(timer_pause PRIVATE simple_timer)