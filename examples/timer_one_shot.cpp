#include <simple_timer/simple_timer.h>

#include <iostream>

int main()
{
  std::cout << "[main start] please wait..." << std::endl;
  // 2s后执行一次, one_shot = true
  SimpleTimer timer(std::chrono::seconds(2), true);
  // 启动定时器
  timer.start([]() { std::cout << "Timer one slot task executed!" << std::endl; });

  // 等待 7 秒后观察定时器的行为
  std::this_thread::sleep_for(std::chrono::seconds(7));
  std::cout << "[main end]" << std::endl;
  return 0;
}