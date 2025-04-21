# SimpleTimer — A Lightweight Timer

[![Timer](https://img.shields.io/badge/SimpleTimer-8A2BE2)](https://github.com/abin-z/SimpleTimer) [![headeronly](https://img.shields.io/badge/Header_Only-green)](include/simple_timer/simple_timer.h) [![moderncpp](https://img.shields.io/badge/Modern_C%2B%2B-218c73)](https://learn.microsoft.com/en-us/cpp/cpp/welcome-back-to-cpp-modern-cpp?view=msvc-170) [![licenseMIT](https://img.shields.io/badge/License-MIT-green)](https://opensource.org/license/MIT) [![version](https://img.shields.io/badge/version-0.9.1-green)](https://github.com/abin-z/SimpleTimer/releases)

🌍 Languages/语言:  [English](README.md)  |  [简体中文](README.zh-CN.md)

**SimpleTimer** is a cross-platform, lightweight C++11 timer class designed to run tasks periodically in a background thread. It is suitable for scenarios where scheduled task execution is needed. The class supports pause, resume, interval adjustment, and more, all without relying on any third-party libraries (only the C++11 standard library).

## Features

- **Cross-platform**: Works on multiple platforms (Windows, Linux, macOS) using the C++11 standard library.
- **Thread-safe**: Built with `std::thread` and `std::condition_variable`.
- **Flexible Intervals**: Uses `std::chrono::duration` for time intervals, supporting any time unit (minutes, seconds, milliseconds, etc.).
- **Execution Modes**: Supports both one-shot (single execution) and periodic execution modes.
- **Control**: Provides capabilities to pause, resume, restart, and dynamically modify the interval of the timer.
- **Timer Precision**: The timer’s precision is dependent on the system clock, typically millisecond precision.
- **Automatic Cleanup**: `SimpleTimer` objects automatically stop the timer on destruction, ensuring proper resource cleanup even if `stop` is not explicitly called.
- **Minimal Dependencies**: No third-party libraries required, except for POSIX systems, where the `pthread` library must be linked.

## Getting Started

Copy the [`simple_timer.h`](include/simple_timer/simple_timer.h) file into your project directory. Then, simply include it in your source code:

```cpp
#include "simple_timer.h"
```

> On POSIX systems, since `std::thread` is implemented using `pthread`, you'll need to link against the `pthread` library (e.g., using `-lpthread`).

## Basic API Usage

### Creating a Timer

You can create a timer with different constructors to configure the interval and mode of execution:

```cpp
#include "simple_timer.h"
int main()
{
  // Default constructor, with a default interval of 10 seconds
  SimpleTimer timer;

  // Set timer interval using std::chrono::duration
  SimpleTimer timer(std::chrono::seconds(1));          // Executes every 1 second
  SimpleTimer timer(std::chrono::seconds(5), true);    // One-shot, fires after 5 seconds

  // Set interval in milliseconds directly
  SimpleTimer timer(1000LL);  // Interval: 1000 milliseconds
}
```

### Starting the Timer

Call `start` with a callable (e.g., lambda) to begin periodic execution in a new thread:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start([]() {
      std::cout << "Timer task executed!" << std::endl;
  });
}
```

### Pausing and Resuming the Timer

You can pause and resume the timer while it’s running:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start(task);
  timer.pause();   // Pause the timer
  timer.resume();  // Resume the timer
}
```

### Set One-Shot Execution

The timer can be configured for one-shot execution, meaning it will only run once.

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(5), true);  // One-shot execution: 5-second interval
  timer.start(task);  // Executes 'task' after 5 seconds, does not repeat
}
```

### Changing the Timer Interval

You can change the timer interval dynamically:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));   // 1-second interval
  timer.start(task);
  timer.set_interval(std::chrono::seconds(2));  // Set new interval to 2 seconds
}
```

### Stopping the Timer

Use `stop` to stop the timer. It will wait for the current task to finish before stopping (blocking call):

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start(task);
  timer.stop();  // Stop the timer
}
```

### Restarting the Timer

Use `restart` to restart the timer with a new task:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start(task);
  timer.restart([]() {
      std::cout << "Timer restarted and task executed!" << std::endl;
  });
}
```

### Checking Timer Status

You can query the timer's current status:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start(task);
  if (timer.is_running()) {
      std::cout << "The timer is currently running!" << std::endl;
  }
}
```

## Timer States

The `SimpleTimer` class defines three possible states:

- `Stopped`: The timer is not running
- `Running`: The timer is currently active
- `Paused`: The timer is paused

Use the `state()` method to check the current state:

```cpp
#include "simple_timer.h"
int main()
{
  SimpleTimer timer(std::chrono::seconds(1));
  timer.start(task);
  if (timer.state() == SimpleTimer::State::Running) {
      std::cout << "Timer is running!" << std::endl;
  }
}
```

## Example

Here’s a full example demonstrating how to use the `SimpleTimer` class:

```cpp
#include "simple_timer.h"
#include <iostream>

int main() {
    SimpleTimer timer(std::chrono::seconds(1));  // 1-second interval, periodic task

    timer.start([]() {
        std::cout << "Timer task executed!" << std::endl;
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait for 5 seconds

    timer.pause();
    std::cout << "Timer paused..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));  // Wait 3 seconds

    timer.resume();
    std::cout << "Timer resumed..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));  // Wait 5 more seconds

    timer.stop();
    std::cout << "Timer stopped" << std::endl;

    return 0;
}
```

## More Usage Examples

Want to schedule a function with parameters? No problem! Check out more usage examples in the [examples](examples) folder.

## Notes

- Timer accuracy depends on the system clock, typically accurate to the millisecond.
- If your task accesses shared resources, consider using proper synchronization within the task callable.

## License

`SimpleTimer` is released under the MIT License. See [LICENSE](LICENSE) for details.