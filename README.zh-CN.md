# SimpleTimer  一个简单的定时器

[![Timer](https://img.shields.io/badge/SimpleTimer-8A2BE2)](https://github.com/abin-z/SimpleTimer) [![headeronly](https://img.shields.io/badge/Header_Only-green)](include/simple_timer/simple_timer.h) [![moderncpp](https://img.shields.io/badge/Modern_C%2B%2B-218c73)](https://learn.microsoft.com/en-us/cpp/cpp/welcome-back-to-cpp-modern-cpp?view=msvc-170) [![licenseMIT](https://img.shields.io/badge/License-MIT-green)](https://opensource.org/license/MIT) [![version](https://img.shields.io/badge/version-0.9.0-green)](https://github.com/abin-z/SimpleTimer/releases)

🌍 Languages/语言:  [English](README.md)  |  [简体中文](README.zh-CN.md)

`SimpleTimer` 是一个简易的定时器类，支持在后台线程中定期执行任务，适用于需要定时执行任务的场景。它支持暂停、恢复、修改时间间隔等功能，且不依赖于任何第三方库，仅依赖 C++11 标准库。

## 特性

- 使用 `std::thread` 和 `std::condition_variable` 实现，线程安全。
- 默认使用 `std::chrono::duration` 作为时间间隔，可以支持任意时间单位（分钟、秒、毫秒、微秒、纳秒等）。
- 支持单次执行（one-shot）和重复执行（周期性）。
- 支持暂停、恢复、重启定时器，支持修改时间间隔。
- 定时器的时间精度取决于系统时钟的精度，通常为毫秒级别。
- `SimpleTimer`类对象在析构时会自动停止定时器，忘记`stop`也确保资源的正确释放。
- 不依赖任何第三方库，除非是 POSIX 系统下需要链接 `pthread` 库。

## 使用方式

将 [`simple_timer.h`](include/simple_timer/simple_timer.h) 文件复制到你的项目目录中。然后在源码文件中引入即可使用:

```cpp
#include "simple_timer.h"
```

> 因为`std::thread`在 POSIX 系统下使用`pthread`实现的, 所以在POSIX 系统下需要链接 `pthread` 库(例如 `-lpthread`)。

## 基础接口

### 创建定时器

定时器可以使用不同的构造函数来创建，支持多种间隔和触发方式。

```cpp
// 默认构造函数，默认间隔为10秒
SimpleTimer timer;

// 使用 std::chrono::duration 设置定时器间隔
SimpleTimer timer(std::chrono::seconds(1));          // 1秒执行一次
SimpleTimer timer(std::chrono::seconds(5), true);    // 5秒，单次执行

// 使用毫秒为单位设置定时器间隔
SimpleTimer timer(1000LL);  // 间隔1000毫秒
```

### 启动定时器

调用 `start` 方法并传入一个可调用对象，定时器将在新的线程中定期执行该任务。

```cpp
timer.start([]() {
    std::cout << "定时器任务执行！" << std::endl;
});
```

### 暂停与恢复定时器

定时器支持在运行时暂停与恢复。

```cpp
timer.pause();  // 暂停定时器
timer.resume(); // 恢复定时器
```

### 设置定时器间隔

可以动态修改定时器的间隔。

```cpp
timer.set_interval(std::chrono::seconds(2));  // 设置新间隔为2秒
```

### 停止定时器

调用 `stop` 方法可以停止定时器。定时器会等当前任务执行完成后停止(阻塞)。

```cpp
timer.stop();  // 停止定时器
```

### 重启定时器

调用 `restart` 方法可以重启定时器并设置新的任务。

```cpp
timer.restart([]() {
    std::cout << "定时器重启，任务执行！" << std::endl;
});
```

### 获取定时器状态

可以查询定时器当前的状态。

```cpp
if (timer.is_running()) {
    std::cout << "定时器正在运行中！" << std::endl;
}
```

## 定时器状态

`SimpleTimer` 类定义了三个状态：

- `Stopped`：定时器已停止。
- `Running`：定时器正在运行。
- `Paused`：定时器已暂停。

可以通过 `state()` 方法查询当前状态。

```cpp
if (timer.state() == SimpleTimer::State::Running) {
    std::cout << "定时器正在运行！" << std::endl;
}
```

## 示例代码

以下是一个完整的示例代码，展示了如何使用 `SimpleTimer` 类：

```cpp
#include "simple_timer.h"
#include <iostream>

int main() {
    SimpleTimer timer(std::chrono::seconds(1));  // 定时器间隔为1秒，重复执行任务

    timer.start([]() {
        std::cout << "定时器任务执行！" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));  // 等待5秒

    timer.pause();  // 暂停定时器
    std::cout << "定时器已暂停..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));  // 等待3秒

    timer.resume();  // 恢复定时器
    std::cout << "定时器已恢复..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));  // 等待5秒

    timer.stop();  // 停止定时器
    std::cout << "定时器已停止" << std::endl;

    return 0;
}
```

## 注意事项

- 定时器的精度取决于系统时钟的精度，通常为毫秒级别。
- 在使用 `std::thread` 创建新线程时，在传入的可调用对象内考虑是否需要保护共享资源。

## 许可证

`SimpleTimer` 使用 MIT 许可证，详情请见 [LICENSE](LICENSE)。

------

