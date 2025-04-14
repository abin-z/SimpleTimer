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

TEST_CASE("SimpleTimer stops properly", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  timer.stop();
  int value_after_stop = counter;

  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == value_after_stop);  // 停止后计数不再增长
}

TEST_CASE("SimpleTimer one-shot mode triggers only once", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);  // one-shot 模式
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(200));
  timer.stop();
  REQUIRE(counter == 1);
}

TEST_CASE("SimpleTimer can pause and resume", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  timer.pause();
  int paused_value = counter;

  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == paused_value);  // 暂停期间不应递增

  timer.resume();
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();

  REQUIRE(counter > paused_value);  // 恢复后应继续计数
}

TEST_CASE("SimpleTimer updates interval immediately", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(100));  // 初始100ms
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(210));  // 应该触发两次
  timer.set_interval(milliseconds(30));           // 修改为30ms

  std::this_thread::sleep_for(milliseconds(100));  // 应该再触发至少3次
  timer.stop();

  REQUIRE(counter >= 5);  // 总次数不少于两种间隔加起来
}

TEST_CASE("SimpleTimer restart works correctly", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(110));
  timer.restart([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();

  REQUIRE(counter >= 3);  // restart 后应继续触发
}

TEST_CASE("SimpleTimer multiple start does not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(80));

  // 再次 start 应该不会崩溃或引发未定义行为
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(80));
  timer.stop();

  REQUIRE(counter >= 2);
}
