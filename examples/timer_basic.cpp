#include <simple_timer/simple_timer.h>

#include <iostream>

int64_t get_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int main()
{
  SimpleTimer timer(std::chrono::seconds(1));  // 1秒后执行一次
  timer.start([]() { std::cout << "Triggered in thread: " << std::this_thread::get_id() << ", ms:" << get_ms() % 100000 << '\n'; });
  std::this_thread::sleep_for(std::chrono::seconds(10));
}