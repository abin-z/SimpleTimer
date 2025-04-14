/**
 * @file: simple_timer.h
 * @version: v0.9.0
 * @description: 一个简单易用的定时器, 适合于需要定时执行任务的场景, 支持暂停/恢复/修改时间间隔等功能.
 * - 特性:
 *    - 该定时器使用 std::thread 和 std::condition_variable 实现, 线程安全
 *    - 默认 std::chrono::duration 作为时间间隔, 可以使用任意时间单位(秒/毫秒/微秒/纳秒)
 *    - 定时器启动后, 可调用对象会在新的线程中执行, 不会阻塞主线程
 *    - 允许单次和重复执行, 支持暂停/恢复/重启定时器, 支持修改定时器间隔
 *    - 该定时器的时间精度取决于系统时钟的精度,通常为毫秒级别
 *    - SimpleTimer 类的析构函数会自动停止定时器, 确保资源的正确释放
 *    - 该定时器不依赖于任何第三方库, 只使用了 C++11 标准库
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
#include <mutex>
#include <thread>

/**
 * @brief 使用 std::condition_variable 的 wait_until 方法 (也可以使用wait_for方法, 但是会累计误差)
 * 函数原型:
 * cv.wait_until(lock, time_point, predicate);
 *
 * wait_until 的行为：
 * - wait_until 方法的作用是让当前线程在指定的时间点之前等待, 直到条件变量被通知或者超时
 * - 进入等待前会先检查 predicate() 的值.
 * - 如果 predicate() 在进入等待前立即为 true, 则直接返回 true, 不会进入等待状态
 * - 如果 predicate() 为 false, 则进入等待状态, 直到 time_point 到达
 * - 如果在 time_point 到达前 `notify_*` 被调用, 并且此时 predicate() 为 true, 则唤醒并返回 true
 * - 如果在 time_point 到达前 `notify_*` 被调用, 但此时 predicate() 为 false, 则继续等待(可能是虚假唤醒)
 * - 如果直到 time_point 到达时 predicate() 仍为 false, 返回 false(表示超时)
 *
 * 简要来说：
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
        task();  // 执行任务
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