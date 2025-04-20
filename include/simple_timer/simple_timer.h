/**
 * @file: simple_timer.h
 * @version: v0.9.0
 * @description: A simple and easy-to-use timer, suitable for scenarios where periodic task execution is required.
 *               Supports pause/resume/modifying the interval and more.
 *
 * - Features:
 *    - Cross-platform: Works on multiple platforms (Windows, Linux, macOS) using the C++11 standard library.
 *    - Thread-safe: Built with `std::thread` and `std::condition_variable`.
 *    - Flexible Intervals: Uses `std::chrono::duration` for time intervals, supporting any time unit (minutes, seconds,
 *      milliseconds, etc.).
 *    - Execution Modes: Supports both one-shot (single execution) and periodic execution modes.
 *    - Control: Provides capabilities to pause, resume, restart, and dynamically modify the interval of the timer.
 *    - Timer Precision: The timer’s precision is dependent on the system clock, typically millisecond precision.
 *    - Automatic Cleanup: `SimpleTimer` objects automatically stop the timer on destruction, ensuring proper resource
 *      cleanup even if `stop` is not explicitly called.
 *    - Minimal Dependencies: No third-party libraries required, except for POSIX systems, where the `pthread` library
 *      must be linked.
 *
 * @author: abin
 * @date: 2025-04-12
 * @license: MIT
 * @repository: https://github.com/abin-z/SimpleTimer
 */

#ifndef SIMPLE_TIMER_H
#define SIMPLE_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

/**
 * @brief 使用 std::condition_variable 的 wait_until 方法 (也可以使用wait_for方法, 但是会累计误差)
 * 函数原型:
 * cv.wait_until(lock, time_point, predicate);
 *
 * wait_until 的行为:
 * - wait_until 方法的作用是让当前线程在指定的时间点之前等待, 直到条件变量被通知或者超时
 * - 进入等待前会先检查 predicate() 的值.
 * - 如果 predicate() 在进入等待前立即为 true, 则直接返回 true, 不会进入等待状态
 * - 如果 predicate() 为 false, 则进入等待状态, 直到 time_point 到达
 * - 如果在 time_point 到达前 `notify_*` 被调用, 并且此时 predicate() 为 true, 则唤醒并返回 true
 * - 如果在 time_point 到达前 `notify_*` 被调用, 但此时 predicate() 为 false, 则继续等待(可能是虚假唤醒)
 * - 如果直到 time_point 到达时 predicate() 仍为 false, 返回 false(表示超时)
 *
 * 简要来说:
 * - `wait_until` 返回 true 表示条件变量被唤醒并且条件成立
 * - `wait_until` 返回 false 表示已超时, 且条件仍未满足(即超时触发任务)
 */

/// @brief 简单定时器类
class SimpleTimer
{
  using clock = std::chrono::steady_clock;  // 单调时钟, 不受系统时间变化影响
 public:
  /// @brief 定时器状态
  enum class State : unsigned char
  {
    Stopped = 0,  // 停止
    Running = 1,  // 运行中
    Paused = 2,   // 暂停
  };

  /// @brief 简单定时器
  /// @param interval std::chrono::duration 定时器间隔
  /// @param one_shot 是否只触发一次
  template <typename Rep, typename Period>
  SimpleTimer(std::chrono::duration<Rep, Period> interval, bool one_shot = false) :
    interval_(interval), one_shot_(one_shot), state_(State::Stopped)
  {
  }

  /// @brief 简单定时器
  /// @param milliseconds 定时器间隔(毫秒)
  /// @param one_shot 是否只触发一次
  explicit SimpleTimer(long long milliseconds, bool one_shot = false) :
    SimpleTimer(std::chrono::milliseconds(milliseconds), one_shot)  // 代理到主构造函数
  {
  }

  /// @brief 简单定时器
  /// @param one_shot 是否只触发一次
  SimpleTimer(bool one_shot = false) : SimpleTimer(std::chrono::seconds(10), one_shot)  // 默认间隔为10秒
  {
  }

  /// @brief 析构函数
  ~SimpleTimer()
  {
    stop();
  }

  // 禁用拷贝构造函数和拷贝赋值运算符
  SimpleTimer(const SimpleTimer&) = delete;
  SimpleTimer& operator=(const SimpleTimer&) = delete;

  // 禁用移动构造函数和移动赋值运算符
  SimpleTimer(SimpleTimer&&) = delete;
  SimpleTimer& operator=(SimpleTimer&&) = delete;

  /// @brief 启动定时器
  /// @tparam Func 可调用对象类型
  /// @param f 可调用对象, 定时器到期后执行的任务
  /// @note 定时器会在新的线程中执行可调用对象 f
  template <typename Func>
  void start(Func&& f)
  {
    stop();                                        // 确保没有其他线程在运行(替换旧任务)
    state_ = State::Running;                       // 设置状态为运行中
    auto task = std::move(std::forward<Func>(f));  // 完美转发后再 move, 提高效率
    // 使用 std::thread 创建一个新的线程来执行定时器任务
    thread_ = std::thread([this, task]() mutable {
      std::unique_lock<std::mutex> lock(mutex_);
      auto next_time = clock::now() + interval_;
      while (true)
      {
        if (state_ == State::Stopped)
        {
          break;
        }

        while (state_ == State::Paused)
        {
          cv_.wait(lock, [this]() { return state_ != State::Paused; });
          next_time = clock::now() + interval_;  // 重新计算下一次触发时间
        }

        if (cv_.wait_until(lock, next_time, [this]() { return state_ != State::Running || interval_changed_; }))
        {
          if (interval_changed_)  // interval_修改后立即使用新间隔
          {
            next_time = clock::now() + interval_;
            interval_changed_ = false;
          }
          continue;  // 若状态不是 Running, 继续循环判断; 若是 interval_ 被修改, 则更新 next_time 并立即跳过等待
        }

        lock.unlock();
        // Timer 内部处理异常, 执行task遇到异常后直接停止timer
        try
        {
          task();  // 执行任务
        }
        catch (const std::exception& e)
        {
          state_ = State::Stopped;  // 出现异常时停止定时器 (不能调用stop()会死锁)
          std::fprintf(stderr, "[SimpleTimer] Exception: %s\n", e.what());
        }
        catch (...)
        {
          state_ = State::Stopped;  // 出现异常时停止定时器
          std::fprintf(stderr, "[SimpleTimer] Unknown exception occurred.\n");
        }
        lock.lock();

        if (one_shot_)
        {
          state_ = State::Stopped;
          break;
        }

        next_time += interval_;  // 精确推进时间点, 避免偏差
      }
    });
  }

  /// @brief 重启定时器
  /// @tparam Func 可调用对象类型
  /// @param f 可调用对象, 定时器到期后执行的任务
  template <typename Func>
  void restart(Func&& f)
  {
    stop();
    start(std::forward<Func>(f));
  }

  /// @brief 停止定时器, 定时器会等待当前任务执行完成后停止
  void stop()
  {
    state_ = State::Stopped;
    cv_.notify_all();  // 唤醒等待的线程
    if (thread_.joinable())
    {
      thread_.join();  // 等待线程结束
    }
  }

  /// @brief 暂停任务
  void pause()
  {
    if (state_ == State::Running)
    {
      state_ = State::Paused;
    }
  }

  /// @brief 恢复任务
  void resume()
  {
    if (state_ == State::Paused)
    {
      state_ = State::Running;
      cv_.notify_all();  // 唤醒正在等待的线程
    }
  }

  /// @brief 获取定时器的当前间隔
  /// @return std::chrono::milliseconds 定时器间隔
  std::chrono::milliseconds interval() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(interval_);
  }

  /// @brief 设置新的定时器间隔, 立即生效
  /// @param new_interval std::chrono::duration 定时器间隔
  template <typename Rep, typename Period>
  void set_interval(std::chrono::duration<Rep, Period> new_interval)
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      interval_ = new_interval;
      interval_changed_ = true;  // 标记为已改变
    }
    cv_.notify_all();  // 确保线程能获取到新的时间间隔
  }

  /// @brief 设置新的定时器间隔, 立即生效
  /// @param milliseconds 定时器间隔(毫秒)
  void set_interval(long long milliseconds)
  {
    set_interval(std::chrono::milliseconds(milliseconds));
  }

  /// @brief 获取定时器的当前状态
  /// @return State 定时器状态
  State state() const
  {
    return state_;
  }
  /// @brief 返回定时器是否在运行
  bool is_running() const
  {
    return state_ == State::Running;
  }
  /// @brief 返回定时器是否暂停
  bool is_paused() const
  {
    return state_ == State::Paused;
  }
  /// @brief 返回定时器是否停止
  bool is_stopped() const
  {
    return state_ == State::Stopped;
  }

 private:
  // 定时器间隔, 默认10秒
  clock::duration interval_ = std::chrono::seconds(10);
  bool interval_changed_ = false;  // 时间间隔是否被修改过
  bool one_shot_ = false;          // 是否只触发一次
  std::atomic<State> state_;       // 定时器状态
  std::thread thread_;             // 定时器线程
  std::mutex mutex_;               // 互斥锁, 确保线程安全
  std::condition_variable cv_;     // 条件变量, 用于暂停和恢复
};

#endif  // SIMPLE_TIMER_H