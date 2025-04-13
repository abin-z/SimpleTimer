#ifndef SIMPLE_TIMER_H
#define SIMPLE_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class simple_timer
{
  using clock = std::chrono::steady_clock;  // 单调时钟
 public:
  /// @brief 简单定时器
  /// @param interval std::chrono::duration 定时器间隔
  /// @param one_shot 是否只触发一次
  template <typename Rep, typename Period>
  simple_timer(std::chrono::duration<Rep, Period> interval, bool one_shot = false) :
    interval_(interval), one_shot_(one_shot), running_(false)
  {
  }

  /// @brief 简单定时器
  /// @param milliseconds 定时器间隔(毫秒)
  /// @param one_shot 是否只触发一次
  explicit simple_timer(long long milliseconds, bool one_shot = false) :
    simple_timer(std::chrono::milliseconds(milliseconds), one_shot)  // 代理到主构造函数
  {
  }

  /// @brief 简单定时器
  /// @param one_shot 是否只触发一次
  simple_timer(bool one_shot = false) : simple_timer(std::chrono::seconds(10), one_shot)  // 默认间隔为10秒
  {
  }

  /// @brief 析构函数
  ~simple_timer()
  {
    stop();
  }

  template <typename Func>
  void start(Func&& f)
  {
    stop();  // 确保没有其他线程在运行(替换旧任务)
    running_ = true;
    auto task = std::move(std::forward<Func>(f));  // 完美转发后再 move，提高效率
    // 使用 std::thread 创建一个新的线程来执行定时器任务
    thread_ = std::thread([this, task]() mutable {
      std::unique_lock<std::mutex> lock(mutex_);
      auto next_time = clock::now() + interval_;  // 计算下次执行时间
      while (running_)                            // 主循环，只要定时器处于运行状态就持续循环
      {
        /**
         * cv.wait_until(lock, time_point, predicate);
         *
         * wait_until 的行为：
         * - 进入等待前会先检查 predicate() 的值.
         * - 如果 predicate() 在进入等待前立即为 true，则直接返回 true，不会进入等待状态
         * - 如果 predicate() 为 false，则进入等待状态，直到 time_point 到达
         * - 如果在 time_point 到达前 `notify_*` 被调用，并且此时 predicate() 为 true，则唤醒并返回 true
         * - 如果在 time_point 到达前 `notify_*` 被调用，但此时 predicate() 为 false，则继续等待(可能是虚假唤醒)
         * - 如果直到 time_point 到达时 predicate() 仍为 false，返回 false（表示超时）
         *
         * 简要来说：
         * - `wait_until` 返回 true 表示条件变量被唤醒并且条件成立（即 `running_ == false`）
         * - `wait_until` 返回 false 表示已超时，且条件仍未满足（即超时触发任务）
         *
         * 在定时器场景下：
         * - 若 `stop()` 被调用，`running_` 被设置为 false，`notify_all()` 唤醒所有等待的线程，
         *   但只有当 `running_ == false` 时，lambda 会返回 true，触发退出循环。
         */
        if (cv_.wait_until(lock, next_time, [this]() { return !running_; }))
        {
          break;  // 条件变量被唤醒，且检查到 `running_ == false`，退出循环
        }
        task();  // 执行任务
        if (one_shot_)
        {
          running_ = false;  // 如果是单次定时器，停止运行
          break;
        }
        next_time += interval_;  // 更新下次执行时间
      }
    });
  }

  void stop()
  {
    running_ = false;
    cv_.notify_all();  // 唤醒等待的线程
    if (thread_.joinable())
    {
      thread_.join();  // 等待线程结束
    }
  }

  bool is_running() const
  {
    return running_;  // 返回定时器是否在运行
  }

  // 获取定时器的当前间隔
  std::chrono::milliseconds get_interval() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(interval_);
  }

  /// @brief 设置新的定时器间隔
  /// @param new_interval std::chrono::duration 定时器间隔
  template <typename Rep, typename Period>
  void set_interval(std::chrono::duration<Rep, Period> new_interval)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    interval_ = new_interval;  // 更新定时器间隔
  }

  /// @brief 设置新的定时器间隔
  /// @param milliseconds 定时器间隔(毫秒)
  void set_interval(long long milliseconds)
  {
    set_interval(std::chrono::milliseconds(milliseconds));  // 代理到主函数
  }

 private:
  // 定时器间隔, 默认10秒
  clock::duration interval_ = std::chrono::seconds(10);
  bool one_shot_ = false;       // 是否只触发一次
  std::atomic<bool> running_;   // 定时器是否运行
  std::thread thread_;          // 定时器线程
  std::mutex mutex_;            // 互斥锁，确保线程安全
  std::condition_variable cv_;  // 条件变量，用于暂停和恢复
};

#endif  // SIMPLE_TIMER_H