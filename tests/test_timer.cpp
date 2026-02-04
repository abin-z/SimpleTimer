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

  std::this_thread::sleep_for(milliseconds(220));  // 应该触发两次
  timer.set_interval(milliseconds(30));            // 修改为30ms

  std::this_thread::sleep_for(milliseconds(110));  // 应该再触发至少3次
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

TEST_CASE("SimpleTimer runs repeatedly", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(170));
  timer.stop();
  REQUIRE(counter >= 3);
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

TEST_CASE("SimpleTimer pause and resume works", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(60));
  timer.pause();
  int paused_count = counter.load();
  std::this_thread::sleep_for(milliseconds(100));  // paused期间不应增加
  REQUIRE(counter == paused_count);

  timer.resume();
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();
  REQUIRE(counter > paused_count);
}

TEST_CASE("SimpleTimer restart after stop works", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();
  int first_run = counter.load();

  timer.restart([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();

  REQUIRE(counter > first_run);
}

TEST_CASE("SimpleTimer restart in one-shot mode works", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == 1);
  REQUIRE(timer.is_stopped());

  timer.restart([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(100));
  REQUIRE(counter == 2);
}

TEST_CASE("SimpleTimer set_interval affects next cycle", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(100));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(120));
  timer.set_interval(milliseconds(30));
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();

  REQUIRE(counter >= 3);
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
  REQUIRE(timer.is_stopped());
}

TEST_CASE("Multiple stop calls do not crash", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(110));
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

TEST_CASE("Multiple pause and resume toggles", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(80));
  timer.pause();                                  // 第一次暂停
  std::this_thread::sleep_for(milliseconds(50));  // 确保任务暂停
  REQUIRE(counter == 1);                          // 仅执行一次

  timer.resume();  // 第一次恢复
  std::this_thread::sleep_for(milliseconds(30));
  timer.pause();                                  // 第二次暂停
  std::this_thread::sleep_for(milliseconds(50));  // 确保任务暂停
  REQUIRE(counter == 1);                          // 仍然只执行了一次

  timer.resume();  // 第二次恢复
  std::this_thread::sleep_for(milliseconds(80));
  timer.stop();  // 停止定时器

  REQUIRE(counter == 2);  // 确保任务被执行了两次
}

TEST_CASE("Multiple stop calls in one-shot mode", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50), true);  // one-shot 模式
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(100));
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

TEST_CASE("Multiple start calls with stop between", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();                       // 第一次 stop
  timer.start([&]() { counter++; });  // 重新 start
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();  // 第二次 stop

  REQUIRE(counter >= 2);  // 至少执行了两次
}

TEST_CASE("Multiple restart calls after stop", "[SimpleTimer]")
{
  std::atomic<int> counter(0);
  SimpleTimer timer(milliseconds(50));

  timer.start([&]() { counter++; });
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();                         // 第一次 stop
  timer.restart([&]() { counter++; });  // 第一次重启
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();                         // 第二次 stop
  timer.restart([&]() { counter++; });  // 第二次重启
  std::this_thread::sleep_for(milliseconds(100));
  timer.stop();  // 第三次 stop

  REQUIRE(counter >= 3);  // 至少执行了三次
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

  std::this_thread::sleep_for(milliseconds(120));
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

  std::this_thread::sleep_for(milliseconds(120));
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

  std::this_thread::sleep_for(milliseconds(80));
  timer.stop();

  timer.stop();  // 连续 stop
  timer.start([&]() { counter++; });

  std::this_thread::sleep_for(milliseconds(80));
  timer.stop();

  REQUIRE(counter >= 2);
}
