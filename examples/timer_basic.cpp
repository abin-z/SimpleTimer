#include <simple_timer/simple_timer.h>

#include <iostream>

int64_t get_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

int main()
{
  std::cout << "[main start]: " << "main-thread-id = " << std::this_thread::get_id() << std::endl;

  SimpleTimer timer(std::chrono::seconds(1));  // 创建一个定时器, 每隔1秒执行一次
  // 启动定时器
  timer.start([]() {
    std::cout << "timer task thread id = " << std::this_thread::get_id() << ", current ms:" << get_ms() % 100000 << '\n';
  });
  std::cout << "===== timer.start() =====" << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(10));
  timer.stop();  // 停止定时器
  std::cout << "===== timer.stop() =====" << std::endl;

  std::cout << "[main end]: " << "main-thread-id = " << std::this_thread::get_id() << std::endl;
}