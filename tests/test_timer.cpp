#define CATCH_CONFIG_MAIN
#include <simple_timer/simple_timer.h>

#include <atomic>
#include <catch.hpp>
#include <chrono>
#include <thread>

using namespace std::chrono;

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

  std::this_thread::sleep_for(milliseconds(150));

  REQUIRE(counter == 1);
  REQUIRE(timer.is_stopped());

  timer.restart([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(150));

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

  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == 1);  // one-shot due to exception
  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(timer.is_stopped());
}

TEST_CASE("Multiple stop calls do not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(140));
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
  REQUIRE(counter > 1);  // 确保任务已运行
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
  std::this_thread::sleep_for(milliseconds(120));
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

  std::this_thread::sleep_for(milliseconds(100));
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

  std::this_thread::sleep_for(milliseconds(220));
  timer.stop();

  REQUIRE(counter >= 2);
}
TEST_CASE("Timer can be started multiple times", "[SimpleTimer]")
{
  std::atomic<int> counter{0};
  SimpleTimer timer(milliseconds(50));

  // first run
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();

  int first = counter.load();
  REQUIRE(first > 0);

  // second run
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();

  int second = counter.load();
  REQUIRE(second > first);

  // third run (restart)
  timer.restart([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();

  REQUIRE(counter > second);
}

TEST_CASE("Stop before start then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的
  timer.stop();  // 尚未 start 就 stop，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
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
  SimpleTimer timer(milliseconds(50));

  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的
  timer.pause();  // 尚未 start 就 pause，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(140));
  timer.stop();

  REQUIRE(counter >= 1);  // start 后应至少触发一次
}

TEST_CASE("Stop before stop pause resume then start", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.stop();    // 尚未 start 就 stop，应当是安全的
  timer.pause();   // 尚未 start 就 pause，应当是安全的
  timer.resume();  // 尚未 start 就 resume，应当是安全的
  timer.stop();    // 尚未 start 就 stop，应当是安全的

  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
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

  std::this_thread::sleep_for(milliseconds(120));
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

  std::this_thread::sleep_for(milliseconds(100));

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
