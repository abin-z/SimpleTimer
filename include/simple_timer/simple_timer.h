#ifndef SIMPLE_TIMER_H
#define SIMPLE_TIMER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

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
 * - `wait_until` 返回 true 表示条件变量被唤醒并且条件成立
 * - `wait_until` 返回 false 表示已超时，且条件仍未满足（即超时触发任务）
 */

class SimpleTimer
{
  using clock = std::chrono::steady_clock;  // 单调时钟
 public:
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

  template <typename Func>
  void start(Func&& f)
  {
    stop();                                        // 确保没有其他线程在运行(替换旧任务)
    state_ = State::Running;                       // 设置状态为运行中
    auto task = std::move(std::forward<Func>(f));  // 完美转发后再 move，提高效率
    // 使用 std::thread 创建一个新的线程来执行定时器任务
    thread_ = std::thread([this, task]() mutable {
      std::unique_lock<std::mutex> lock(mutex_);
      auto next_time = clock::now() + interval_;
      while (true)
      {
        while (state_ == State::Paused)
        {
          cv_.wait(lock, [this]() { return state_ != State::Paused; });
          next_time = clock::now() + interval_;  // 重新计算下一次触发时间
        }

        if (state_ == State::Stopped)
        {
          break;
        }

        if (cv_.wait_until(lock, next_time, [this]() { return state_ != State::Running; }))
        {
          continue;  // 被唤醒或状态变了，不执行任务
        }

        lock.unlock();
        task();  // 执行任务
        lock.lock();

        if (one_shot_)
        {
          state_ = State::Stopped;
          break;
        }

        next_time += interval_;  // 精确推进时间点，避免偏差
      }
    });
  }

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
    std::lock_guard<std::mutex> lock(mutex_);  // 虽然state_已经是atomic了, 但这里还是建议加锁保护
    if (state_ == State::Running)
    {
      state_ = State::Paused;
    }
  }

  /// @brief 恢复任务
  void resume()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::Paused)
    {
      state_ = State::Running;
      cv_.notify_all();  // 唤醒正在等待的线程
    }
  }

  /// @brief 获取定时器的当前状态
  /// @return State 定时器状态
  State state() const
  {
    return state_;  // 返回定时器状态
  }
  /// @brief 返回定时器是否在运行
  bool is_running() const
  {
    return state_ == State::Running;  // 返回定时器是否在运行
  }
  /// @brief 获取定时器是否在暂停
  bool is_paused() const
  {
    return state_ == State::Paused;  // 返回定时器是否在暂停
  }
  /// @brief 获取定时器是否在停止
  bool is_stopped() const
  {
    return state_ == State::Stopped;  // 返回定时器是否在停止
  }

  /// @brief 获取定时器的当前间隔
  /// @return std::chrono::milliseconds 定时器间隔
  std::chrono::milliseconds interval() const
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(interval_);
  }

  /// @brief 设置新的定时器间隔
  /// @param new_interval std::chrono::duration 定时器间隔
  template <typename Rep, typename Period>
  void set_interval(std::chrono::duration<Rep, Period> new_interval)
  {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      interval_ = new_interval;
    }
    cv_.notify_all();  // 确保等待时间更新
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
  std::atomic<State> state_;    // 定时器状态
  std::thread thread_;          // 定时器线程
  std::mutex mutex_;            // 互斥锁，确保线程安全
  std::condition_variable cv_;  // 条件变量，用于暂停和恢复
};

#endif  // SIMPLE_TIMER_H