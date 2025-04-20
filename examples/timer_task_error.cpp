#include <simple_timer/simple_timer.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

int64_t get_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

void task_error()
{
  static thread_local int num = 1;
  int64_t ms = get_ms() % 100000;
  std::cout << ms << ": Task executed " << num << " times." << std::endl;
  if (num == 11)
  {
    throw std::runtime_error("Boom! Error occurred in task.");
  }
  num++;
}

int main()
{
  SimpleTimer timer(std::chrono::milliseconds(200));  // 200ms执行一次

  // 启动定时器，任务每200ms执行一次
  timer.start(task_error);
  std::cout << "Timer started, task will execute every 200ms." << std::endl;

  // 让定时器跑一段时间，让任务有机会执行多次
  std::this_thread::sleep_for(std::chrono::seconds(5));  // 等待5秒钟

  // 停止定时器
  timer.stop();
  std::cout << "Timer stopped after 5 seconds." << std::endl;

  return 0;
}
