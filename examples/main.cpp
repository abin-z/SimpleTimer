#include <simple_timer/simple_timer.h>

#include <iostream>

int main()
{
  SimpleTimer timer(std::chrono::seconds(2));  // 2秒后执行一次
  timer.start([]() {
    std::cout
      << "Triggered in thread: " << std::this_thread::get_id() << ": "
      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 100000
      << '\n';
  });

  std::this_thread::sleep_for(std::chrono::seconds(7));
  timer.set_interval(std::chrono::milliseconds(200)); // 立即采用新的interval
  std::cout << "== timer.set_interval(std::chrono::milliseconds(200))" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(3));

  timer.set_interval(std::chrono::seconds(1));
  std::cout << "== timer.set_interval(std::chrono::seconds(1))" << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));
  return 0;
}