#include <simple_timer/simple_timer.h>

#include <iostream>

// void test_pause_resume()
// {
//   // 定义一个 2 秒钟的定时器，并且只触发一次
//   SimpleTimer timer(std::chrono::seconds(1));

//   // 启动定时器并传入任务
//   timer.start([]() { std::cout << "Task executed!" << std::endl; });

//   // 等待 1 秒后暂停定时器
//   std::this_thread::sleep_for(std::chrono::seconds(1));
//   std::cout << "Pausing timer..." << std::endl;
//   timer.pause();

//   // 等待 2 秒后恢复定时器
//   std::this_thread::sleep_for(std::chrono::seconds(2));
//   std::cout << "Resuming timer..." << std::endl;
//   timer.resume();

//   // 等待定时器执行完成
//   std::this_thread::sleep_for(std::chrono::seconds(10));
//   // 启动定时器并传入任务
//   timer.start([]() { std::cout << "Task executed2!" << std::endl; });
// }

int main()
{
  SimpleTimer timer(std::chrono::seconds(1));  // 1秒后执行一次
  timer.start([]() { std::cout << "Triggered in thread: " << std::this_thread::get_id() << '\n'; });

  // 等待定时器执行
  std::this_thread::sleep_for(std::chrono::seconds(4));
  timer.stop();  // 停止定时器
  std::cout << "Timer stopped." << std::endl;
  // test_pause_resume();

  timer.start([]() {
    std::cout
      << "Triggered in thread: " << std::this_thread::get_id() << ": "
      << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 1000
      << '\n';
  });
  std::this_thread::sleep_for(std::chrono::seconds(60));
  return 0;
}