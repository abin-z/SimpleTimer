#define CATCH_CONFIG_MAIN
#include <simple_timer/simple_timer.h>

#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono;

namespace
{
template <typename Predicate>
bool wait_for_condition(Predicate predicate, milliseconds timeout = milliseconds(1000))
{
  const auto deadline = steady_clock::now() + timeout;
  while (!predicate() && steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(milliseconds(1));
  }
  return predicate();
}

struct MoveOnlyTask
{
  MoveOnlyTask(std::unique_ptr<int> value, std::atomic<int> &result) : value_(std::move(value)), result_(result) {}
  MoveOnlyTask(MoveOnlyTask &&) = default;
  MoveOnlyTask &operator=(MoveOnlyTask &&) = delete;
  MoveOnlyTask(const MoveOnlyTask &) = delete;
  MoveOnlyTask &operator=(const MoveOnlyTask &) = delete;

  void operator()()
  {
    result_ += *value_;
  }

  std::unique_ptr<int> value_;
  std::atomic<int> &result_;
};
}  // namespace

TEST_CASE("SimpleTimer triggers task at interval", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(100));  // 100ms
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(350));
  timer.stop();

  REQUIRE(counter >= 3);
  REQUIRE(counter <= 4);  // 容许调度误差
}

TEST_CASE("Manual trigger executes a running timer before its interval", "[SimpleTimer][trigger]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(seconds(5));
  timer.start([&]() { counter++; });

  timer.trigger();

  const auto deadline = steady_clock::now() + milliseconds(500);
  while (counter.load() == 0 && steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(milliseconds(1));
  }

  timer.stop();
  REQUIRE(counter == 1);
}

TEST_CASE("Manual trigger is ignored unless the timer is running", "[SimpleTimer][trigger]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(seconds(5));

  timer.trigger();
  REQUIRE_FALSE(wait_for_condition([&]() { return counter.load() != 0; }, milliseconds(30)));

  timer.start([&]() { counter++; });
  timer.pause();
  timer.trigger();
  REQUIRE_FALSE(wait_for_condition([&]() { return counter.load() != 0; }, milliseconds(30)));

  timer.resume();
  timer.trigger();
  REQUIRE(wait_for_condition([&]() { return counter.load() == 1; }));

  timer.stop();
  timer.trigger();
  REQUIRE_FALSE(wait_for_condition([&]() { return counter.load() != 1; }, milliseconds(30)));
}

TEST_CASE("Manual trigger runs the callback on the timer thread", "[SimpleTimer][trigger]")
{
  const std::thread::id caller_id = std::this_thread::get_id();
  std::thread::id callback_id;
  std::mutex callback_id_mutex;
  std::atomic<bool> called{false};
  SimpleTimer timer(seconds(5));
  timer.start([&]() {
    {
      std::lock_guard<std::mutex> lock(callback_id_mutex);
      callback_id = std::this_thread::get_id();
    }
    called = true;
  });

  timer.trigger();
  REQUIRE(wait_for_condition([&]() { return called.load(); }));
  timer.stop();

  std::lock_guard<std::mutex> lock(callback_id_mutex);
  REQUIRE(callback_id != caller_id);
}

TEST_CASE("Manual trigger preserves every sequential and concurrent request", "[SimpleTimer][trigger]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(seconds(5));
  timer.start([&]() { counter++; });

  for (int i = 0; i < 5; ++i)
  {
    timer.trigger();
  }

  std::vector<std::thread> callers;
  for (int thread_index = 0; thread_index < 4; ++thread_index)
  {
    callers.emplace_back([&]() {
      for (int i = 0; i < 25; ++i)
      {
        timer.trigger();
      }
    });
  }
  for (std::size_t i = 0; i < callers.size(); ++i)
  {
    callers[i].join();
  }

  REQUIRE(wait_for_condition([&]() { return counter.load() == 105; }, milliseconds(2000)));
  timer.stop();
  REQUIRE(counter == 105);
}

TEST_CASE("Manual triggers queue while a callback is active and never overlap", "[SimpleTimer][trigger]")
{
  std::atomic<int> calls{0};
  std::atomic<int> active{0};
  std::atomic<int> maximum_active{0};
  std::atomic<bool> release_first{false};
  SimpleTimer timer(seconds(5));
  timer.start([&]() {
    const int now_active = ++active;
    int observed = maximum_active.load();
    while (observed < now_active && !maximum_active.compare_exchange_weak(observed, now_active))
    {
    }

    const int call_number = ++calls;
    if (call_number == 1)
    {
      while (!release_first.load())
      {
        std::this_thread::yield();
      }
    }
    --active;
  });

  timer.trigger();
  const bool first_call_started = wait_for_condition([&]() { return calls.load() == 1; });
  if (!first_call_started)
  {
    release_first = true;
    timer.stop();
  }
  REQUIRE(first_call_started);
  timer.trigger();
  timer.trigger();
  timer.trigger();
  release_first = true;

  REQUIRE(wait_for_condition([&]() { return calls.load() == 4; }));
  timer.stop();
  REQUIRE(maximum_active == 1);
}

TEST_CASE("Manual trigger does not reset the scheduled deadline", "[SimpleTimer][trigger]")
{
  std::atomic<int> counter{0};
  std::mutex times_mutex;
  std::vector<steady_clock::time_point> callback_times;
  SimpleTimer timer(milliseconds(2000));
  timer.start([&]() {
    {
      std::lock_guard<std::mutex> lock(times_mutex);
      callback_times.push_back(steady_clock::now());
    }
    counter++;
  });

  timer.trigger();
  REQUIRE(wait_for_condition([&]() { return counter.load() == 1; }, milliseconds(500)));
  const auto worker_ready_at = steady_clock::now();

  std::this_thread::sleep_until(worker_ready_at + milliseconds(500));
  REQUIRE(counter == 1);
  const auto second_triggered_at = steady_clock::now();
  timer.trigger();
  REQUIRE(wait_for_condition([&]() { return counter.load() == 2; }, milliseconds(500)));

  const bool scheduled_call_arrived = wait_for_condition([&]() { return counter.load() >= 3; }, milliseconds(2000));
  timer.stop();
  REQUIRE(scheduled_call_arrived);

  std::lock_guard<std::mutex> lock(times_mutex);
  REQUIRE(callback_times.size() >= 3);
  REQUIRE(callback_times[2] - second_triggered_at < milliseconds(1800));
}

TEST_CASE("Stop discards pending manual triggers and restart begins with an empty queue", "[SimpleTimer][trigger]")
{
  std::atomic<int> calls{0};
  std::atomic<bool> release_first{false};
  SimpleTimer timer(seconds(5));
  timer.start([&]() {
    const int call_number = ++calls;
    if (call_number == 1)
    {
      while (!release_first.load())
      {
        std::this_thread::yield();
      }
    }
  });

  timer.trigger();
  const bool first_call_started = wait_for_condition([&]() { return calls.load() == 1; });
  if (!first_call_started)
  {
    release_first = true;
    timer.stop();
  }
  REQUIRE(first_call_started);
  for (int i = 0; i < 5; ++i)
  {
    timer.trigger();
  }

  std::thread stopper([&]() { timer.stop(); });
  const bool stop_started = wait_for_condition([&]() { return timer.is_stopped(); });
  release_first = true;
  stopper.join();
  REQUIRE(stop_started);
  REQUIRE(calls == 1);

  timer.restart([&]() { calls++; });
  REQUIRE_FALSE(wait_for_condition([&]() { return calls.load() != 1; }, milliseconds(30)));
  timer.trigger();
  REQUIRE(wait_for_condition([&]() { return calls.load() == 2; }));
  timer.stop();
}

TEST_CASE("Manual trigger consumes a one-shot timer", "[SimpleTimer][trigger]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(seconds(5), true);
  timer.start([&]() { counter++; });

  timer.trigger();
  timer.trigger();
  timer.trigger();

  REQUIRE(wait_for_condition([&]() { return timer.is_stopped(); }));
  timer.trigger();
  std::this_thread::sleep_for(milliseconds(30));
  timer.stop();
  REQUIRE(counter == 1);
}

TEST_CASE("Exception from a manually triggered callback stops the timer", "[SimpleTimer][trigger]")
{
  SimpleTimer timer(seconds(5));
  timer.start([]() { throw std::runtime_error("manual trigger failure"); });

  timer.trigger();

  REQUIRE(wait_for_condition([&]() { return timer.is_stopped(); }));
  timer.stop();
}

TEST_CASE("Move-only tasks support manual and scheduled execution", "[SimpleTimer][trigger]")
{
  std::atomic<int> manual_result{0};
  SimpleTimer manual_timer(seconds(5));
  manual_timer.start(MoveOnlyTask(std::unique_ptr<int>(new int(7)), manual_result));
  manual_timer.trigger();
  REQUIRE(wait_for_condition([&]() { return manual_result.load() == 7; }));
  manual_timer.stop();

  std::atomic<int> scheduled_result{0};
  SimpleTimer scheduled_timer(milliseconds(20), true);
  scheduled_timer.start(MoveOnlyTask(std::unique_ptr<int>(new int(9)), scheduled_result));
  REQUIRE(wait_for_condition([&]() { return scheduled_result.load() == 9; }));
  scheduled_timer.stop();
}

TEST_CASE("Stop prevents further execution", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(std::chrono::milliseconds(40));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  timer.stop();

  int stopped_value = counter.load();

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  REQUIRE(counter == stopped_value);
}

TEST_CASE("Pause and resume works", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(std::chrono::milliseconds(40));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  timer.pause();
  int paused = counter.load();

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  // 允许 pause 时触发一个多余回调
  REQUIRE(counter >= paused);
  REQUIRE(counter <= paused + 1);  // 容许 race condition

  timer.resume();

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  timer.stop();

  REQUIRE(counter > paused);
}

TEST_CASE("SimpleTimer restart works correctly", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(40));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  REQUIRE(counter >= 1);  // 应该触发两次
  timer.restart([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();

  REQUIRE(counter >= 2);  // restart 后应继续触发
}

TEST_CASE("SimpleTimer multiple start does not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));

  // 再次 start 应该不会崩溃或引发未定义行为
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();

  REQUIRE(counter >= 2);
}

TEST_CASE("SimpleTimer one-shot mode fires only once", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  REQUIRE(counter == 1);
  REQUIRE(timer.is_stopped());
}

TEST_CASE("SimpleTimer restart after stop works", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(150));
  timer.stop();
  int first_run = counter.load();

  timer.restart([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  REQUIRE(counter > first_run);
}

TEST_CASE("SimpleTimer restart in one-shot mode works", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(300));

  REQUIRE(counter == 1);
  REQUIRE(timer.is_stopped());

  timer.restart([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(300));

  REQUIRE(counter == 2);
  REQUIRE(timer.is_stopped());
}

TEST_CASE("SimpleTimer handles exception and stops", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() {
    counter++;
    throw std::runtime_error("test exception");
  });

  std::this_thread::sleep_for(milliseconds(500));
  REQUIRE(counter == 1);  // one-shot due to exception
}

TEST_CASE("Multiple stop calls do not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(300));
  timer.stop();          // 第一次 stop
  timer.stop();          // (不应崩溃)
  timer.stop();          // (不应崩溃)
  timer.stop();          // (不应崩溃)
  timer.resume();        // (不应崩溃)
  timer.resume();        // (不应崩溃)
  timer.resume();        // (不应崩溃)
  timer.resume();        // (不应崩溃)
  timer.pause();         // (不应崩溃)
  timer.pause();         // (不应崩溃)
  timer.pause();         // (不应崩溃)
  REQUIRE(counter > 0);  // 确保任务已运行
}

TEST_CASE("Multiple pause/resume calls do not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
  timer.pause();  // 第一次暂停
  timer.pause();  // 第二次暂停 (不应崩溃)

  std::this_thread::sleep_for(milliseconds(50));  // 确保不增加

  timer.resume();  // 第一次恢复
  timer.resume();  // 第二次恢复 (不应崩溃)

  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();

  REQUIRE(counter > 1);  // 确保定时器任务已经执行
}

TEST_CASE("Destructor stops timer safely", "[SimpleTimer]")
{
  std::atomic<int> counter{0};

  {
    SimpleTimer timer(std::chrono::milliseconds(30));
    timer.start([&] { counter++; });

    std::this_thread::sleep_for(std::chrono::milliseconds(60));
  }

  int value = counter.load();

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  REQUIRE(counter == value);
}

TEST_CASE("Multiple pause and resume toggles", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(40));

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  timer.pause();  // 第一次暂停
  std::this_thread::sleep_for(milliseconds(120));

  int paused1 = counter.load();

  std::this_thread::sleep_for(milliseconds(120));
  REQUIRE(counter == paused1);  // pause期间不增长

  timer.resume();
  std::this_thread::sleep_for(milliseconds(120));
  timer.pause();  // 第二次暂停
  std::this_thread::sleep_for(milliseconds(120));

  int paused2 = counter.load();

  std::this_thread::sleep_for(milliseconds(120));
  REQUIRE(counter == paused2);  // 仍然不增长

  timer.resume();
  std::this_thread::sleep_for(milliseconds(500));
  timer.stop();

  REQUIRE(counter > paused2);  // resume后继续执行
}

TEST_CASE("Multiple start replaces task", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(std::chrono::milliseconds(40));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(std::chrono::milliseconds(80));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  timer.stop();

  REQUIRE(counter >= 2);
}

TEST_CASE("Multiple stop calls in one-shot mode", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);  // one-shot 模式
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();           // 第一次 stop
  timer.stop();           // 第二次 stop (不应崩溃)
  REQUIRE(counter == 1);  // 应该只触发一次
}

TEST_CASE("Multiple stop, pause, resume calls in one-shot mode", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);  // one-shot 模式
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(400));
  timer.pause();   // 暂停
  timer.resume();  // 恢复
  timer.stop();    // 停止
  timer.stop();    // 再次 stop (不应崩溃)

  REQUIRE(counter == 1);  // one-shot 模式只触发一次
}

TEST_CASE("Interval change while running", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(100));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(milliseconds(150));

  timer.set_interval(milliseconds(20));

  std::this_thread::sleep_for(milliseconds(100));

  timer.stop();

  REQUIRE(counter >= 2);
}

TEST_CASE("Long running callback does not overlap", "[SimpleTimer]")
{
  std::atomic<int> counter{0};

  SimpleTimer timer(milliseconds(50));

  timer.start([&]() {
    counter++;
    std::this_thread::sleep_for(milliseconds(80));
  });

  std::this_thread::sleep_for(milliseconds(500));
  timer.stop();

  REQUIRE(counter >= 2);
}
TEST_CASE("Timer can be started multiple times", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(30));

  // first run
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  int first = counter.load();
  REQUIRE(first > 0);

  // second run
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  int second = counter.load();
  REQUIRE(second > first);

  // third run (restart)
  timer.restart([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  REQUIRE(counter > second);
}

TEST_CASE("Stop before start then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(20));

  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop before resume then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(300));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop before pause then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(20));

  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop before stop pause resume then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(20));

  timer.stop();    // 尚未 start 就 stop，应当是安全的
  timer.pause();   // 尚未 start 就 pause，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.stop();    // 尚未 start 就 stop，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(300));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop before stop pause resume then restart", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.stop();    // 尚未 start 就 stop，应当是安全的
  timer.pause();   // 尚未 start 就 pause，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.stop();    // 尚未 start 就 stop，应当是安全的

  timer.restart([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(500));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop then start repeatedly", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(30));

  timer.stop();
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();
  int first = counter.load();
  REQUIRE(first >= 1);
  timer.stop();  // 连续 stop
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();

  REQUIRE(counter >= first + 1);
}

TEST_CASE("test one-shot", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(10), true);  // one-shot 模式

  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));

  // 由于立即调用了多次 start, 但 one-shot 模式只应触发第一次(start会先调用stop停止旧任务), 后续的 start 应该被 stop
  // 直接覆盖掉, 因此只会触发一次
  REQUIRE(counter == 1);
}

TEST_CASE("test one-shot 2", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(10), true);  // one-shot 模式

  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(100));
  timer.start([&]() { counter++; });
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == 2);
}

TEST_CASE("test one-shot 3", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(10), true);  // one-shot 模式

  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(200));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(1000));

  REQUIRE(counter == 5);  // 每次 start 都应触发一次，且 one-shot 不应影响后续 start 的行为
}

TEST_CASE("Stop while paused")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(40));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(milliseconds(80));

  timer.pause();

  std::this_thread::sleep_for(milliseconds(80));

  timer.stop();

  REQUIRE(timer.is_stopped());
}

TEST_CASE("Restart while paused")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(40));

  timer.start([&] { counter++; });

  std::this_thread::sleep_for(milliseconds(80));

  timer.pause();

  timer.restart([&] { counter++; });

  std::this_thread::sleep_for(milliseconds(120));

  timer.stop();

  REQUIRE(counter >= 1);
}

// TODO 在回调中调用 stop/restart 等情况的测试, 以确保不会死锁或崩溃(待修复)

TEST_CASE("Callback calls stop()", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(30));

  timer.start([&]() {
    counter++;
    timer.stop();  // 在回调线程里 stop 自己
  });

  std::this_thread::sleep_for(milliseconds(500));

  REQUIRE(counter == 1);  // 只能执行一次，且不能死锁
}

// TEST_CASE("Callback calls restart()", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   SimpleTimer timer(milliseconds(30));

//   timer.start([&]() {
//     counter++;
//     if (counter == 1) timer.restart([&]() { counter++; });
//   });

//   std::this_thread::sleep_for(milliseconds(120));
//   timer.stop();

//   REQUIRE(counter >= 2);
// }

// TEST_CASE("Concurrent stop calls", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   SimpleTimer timer(milliseconds(30));

//   timer.start([&]() { counter++; });
//   std::this_thread::sleep_for(milliseconds(50));

//   std::thread t1([&]() { timer.stop(); });
//   std::thread t2([&]() { timer.stop(); });
//   std::thread t3([&]() { timer.stop(); });

//   t1.join();
//   t2.join();
//   t3.join();

//   REQUIRE(counter >= 1);
// }

// TEST_CASE("Concurrent start and stop", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   SimpleTimer timer(milliseconds(30));

//   std::thread t1([&]() {
//     for (int i = 0; i < 5; ++i) timer.start([&]() { counter++; });
//   });

//   std::thread t2([&]() {
//     for (int i = 0; i < 5; ++i) timer.stop();
//   });

//   t1.join();
//   t2.join();

//   std::this_thread::sleep_for(milliseconds(100));
//   timer.stop();

//   REQUIRE(counter >= 0);  // 重点是“不崩”
// }

// TEST_CASE("Destructor stops timer safely", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);

//   {
//     SimpleTimer timer(milliseconds(30));
//     timer.start([&]() { counter++; });
//     std::this_thread::sleep_for(milliseconds(50));
//   }  // 析构

//   int value = counter.load();
//   std::this_thread::sleep_for(milliseconds(100));

//   REQUIRE(counter == value);  // 析构后不再增长
// }

// TEST_CASE("Destroy timer during callback", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   std::unique_ptr<SimpleTimer> timer;

//   timer.reset(new SimpleTimer(milliseconds(30)));
//   timer->start([&]() {
//     counter++;
//     timer.reset();  // 回调中析构自己
//   });

//   std::this_thread::sleep_for(milliseconds(100));

//   REQUIRE(counter == 1);
// }

// TEST_CASE("Pause prevents callback execution", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   SimpleTimer timer(milliseconds(30));

//   timer.start([&]() { counter++; });
//   std::this_thread::sleep_for(milliseconds(60));
//   timer.pause();

//   int paused_value = counter.load();
//   std::this_thread::sleep_for(milliseconds(100));

//   timer.stop();

//   REQUIRE(counter == paused_value);
// }

// TEST_CASE("Resume does not catch up missed ticks", "[SimpleTimer]")
// {
//   std::atomic<int> counter(0);
//   SimpleTimer timer(milliseconds(30));

//   timer.start([&]() { counter++; });
//   std::this_thread::sleep_for(milliseconds(60));

//   timer.pause();
//   std::this_thread::sleep_for(milliseconds(120));

//   timer.resume();
//   std::this_thread::sleep_for(milliseconds(60));
//   timer.stop();

//   REQUIRE(counter <= 4);  // 不应疯狂补跑
// }

// TEST_CASE("Callback throws exception", "[SimpleTimer]")
// {
//   SimpleTimer timer(milliseconds(30));

//   timer.start([]() { throw std::runtime_error("boom"); });

//   std::this_thread::sleep_for(milliseconds(60));
//   timer.stop();

//   SUCCEED();  // 不崩就行
// }
