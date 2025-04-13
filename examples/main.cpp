#include <simple_timer/simple_timer.h>

#include <iostream>

int main()
{
  SimpleTimer timer(std::chrono::seconds(1));  // 1秒后执行一次
  timer.start([]() {
    std::cout << "Triggered in thread: " << std::this_thread::get_id() << '\n';
  });

  // 等待定时器执行
  std::this_thread::sleep_for(std::chrono::seconds(4));

  return 0;
}