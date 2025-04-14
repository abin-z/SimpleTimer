#include <simple_timer/simple_timer.h>

#include <iostream>

int64_t get_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int main()
{
  SimpleTimer timer(std::chrono::seconds(1));

  timer.start([]() { std::cout << get_ms() << ": Timer task executed!" << std::endl; });

  // 等待 3 秒后暂停定时器
  std::this_thread::sleep_for(std::chrono::seconds(4));
  timer.pause();
  std::cout << "Pausing timer..." << std::endl;

  // 等待 2 秒后恢复定时器
  std::this_thread::sleep_for(std::chrono::seconds(2));
  timer.resume();
  std::cout << "Resuming timer..." << std::endl;

  // 等待定时器执行完成
  std::this_thread::sleep_for(std::chrono::seconds(5));
}