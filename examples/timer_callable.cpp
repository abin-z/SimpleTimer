#include <simple_timer/simple_timer.h>

#include <iostream>

/// 普通无参函数
void func()
{
  std::cout << "Timer task executed!(via function)" << std::endl;
}

/// 带参数的函数
void func2(int a, int b, int c)
{
  std::cout << "Timer task executed!(via function with args)" << std::endl;
  std::cout << "a = " << a << ", b = " << b << ", c = " << c << std::endl;
}

/// 测试类
class MyClass
{
 public:
  int num = 999;
  // 成员函数
  void func()
  {
    std::cout << "MyClass::func() called! num = " << num << std::endl;
  }
  // 成员函数带参数
  void func2(int a, int b, int c)
  {
    std::cout << "MyClass::func2() called! a = " << a << ", b = " << b << ", c = " << c << std::endl;
  }
};

/// 测试函数对象
struct Functor
{
  void operator()()
  {
    std::cout << "Timer task executed!(via Functor)" << std::endl;
  }
};

class MyClass2
{
 private:
  SimpleTimer timer_;  // 定时器对象作为成员变量
 public:
  MyClass2() : timer_(std::chrono::milliseconds(210))  // 定时器间隔210ms
  {
    timerStrat();
  }

  void timerStrat()
  {
    timer_.start([this]() { this->heartbeat(); });
  }

  void heartbeat()  // 模拟发送心跳
  {
    std::cout << "MyClass2::heartbeat() called!" << std::endl;
  }
};

//////////////////////////////////////// 测试函数 //////////////////////////////////////////
void test_func()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  timer.start([]() { std::cout << "Timer task executed!(via lambda)" << std::endl; });
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func2()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  timer.start(func);
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func3()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  timer.start([]() { func2(1, 2, 3); });  // 使用 lambda 包装调用带参数的函数
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func4()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  MyClass obj;
  timer.start([&obj]() { obj.func(); });  // 使用 lambda 包装调用成员函数
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func5()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  MyClass obj;
  timer.start([&obj]() { obj.func2(1, 2, 3); });  // 使用 lambda 包装调用成员函数带参数
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func6()
{
  SimpleTimer timer(std::chrono::milliseconds(210));
  Functor f;
  timer.start(f);  // 使用 Functor 对象
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

void test_func7()
{
  MyClass2 obj;  // 创建 MyClass2 对象, 构造函数中启动定时器
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main()
{
  test_func();   // 测试lambda
  test_func2();  // 测试普通函数
  test_func3();  // 测试带参数的函数
  test_func4();  // 测试成员函数
  test_func5();  // 测试成员函数带参数
  test_func6();  // 测试函数对象
  test_func7();  // 测试Timer作为成员变量,构造函数中调用heartbeat
  return 0;
}