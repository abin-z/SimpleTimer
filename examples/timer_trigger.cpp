#include <simple_timer/simple_timer.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

std::int64_t current_time_ms()
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
    .count();
}

int main()
{
  // 回调函数运行在定时器线程中，使用原子计数器统计执行次数。
  std::atomic<int> executions{0};

  // 创建一个周期为 2 秒的定时器。
  SimpleTimer timer(std::chrono::seconds(2));

  // 启动定时器。任务会在定时到期，或调用 trigger() 时执行。
  timer.start([&executions]() {
    const int execution = ++executions;
    std::cout << "[" << current_time_ms() << " ms] Task executed #" << execution << " on thread "
              << std::this_thread::get_id() << '\n';
  });

  std::cout << "Timer started; the scheduled interval is 2 seconds.\n";

  // 等待一小段时间；还没到 2 秒的周期，不会有定时执行。
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::cout << "Calling trigger() once: task should run immediately.\n";
  // 手动请求执行一次。trigger() 只负责排队，实际任务仍由定时器线程异步执行。
  timer.trigger();

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  std::cout << "Calling trigger() twice: both requests are queued.\n";
  // 连续调用两次会产生两个待执行请求，不会合并成一次。
  timer.trigger();
  timer.trigger();

  // 继续等待，以便观察手动触发和后续周期触发的输出。
  std::this_thread::sleep_for(std::chrono::seconds(7));
  // stop() 会等待正在执行的任务结束，并停止定时器线程。
  timer.stop();
  std::cout << "Timer stopped after " << executions.load() << " executions.\n";

  return 0;
}
