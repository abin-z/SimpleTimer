# SimpleTimer — A Lightweight Timer

**SimpleTimer** is a lightweight C++11 timer class that runs tasks periodically in a background thread. It is suitable for scenarios requiring scheduled task execution. It supports features like pause, resume, interval adjustment, and more — all without relying on any third-party libraries, using only the C++11 standard library.

## Features

- Thread-safe implementation using `std::thread` and `std::condition_variable`
- Uses `std::chrono::duration` for flexible time intervals (supports minutes, seconds, milliseconds, microseconds, nanoseconds, etc.)
- Supports one-shot (single execution) and periodic execution modes
- Allows pausing, resuming, restarting the timer, and updating the interval dynamically
- Timer accuracy depends on the system clock, typically at the millisecond level
- Automatically stops and releases resources upon destruction, even if `stop` is not explicitly called
- No third-party dependencies (on POSIX systems, linking with `pthread` is required)

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
// Default constructor, with a default interval of 10 seconds
SimpleTimer timer;

// Set timer interval using std::chrono::duration
SimpleTimer timer(std::chrono::seconds(1));          // Executes every 1 second
SimpleTimer timer(std::chrono::seconds(5), true);    // One-shot, fires after 5 seconds

// Set interval in milliseconds directly
SimpleTimer timer(1000LL);  // Interval: 1000 milliseconds
```

### Starting the Timer

Call `start` with a callable (e.g., lambda) to begin periodic execution in a new thread:

```cpp
timer.start([]() {
    std::cout << "Timer task executed!" << std::endl;
});
```

### Pausing and Resuming the Timer

You can pause and resume the timer while it’s running:

```cpp
timer.pause();   // Pause the timer
timer.resume();  // Resume the timer
```

### Changing the Timer Interval

You can change the timer interval dynamically:

```cpp
cpp


CopyEdit
timer.set_interval(std::chrono::seconds(2));  // Set new interval to 2 seconds
```

### Stopping the Timer

Use `stop` to stop the timer. It will wait for the current task to finish before stopping (blocking call):

```cpp
cpp


CopyEdit
timer.stop();  // Stop the timer
```

### Restarting the Timer

Use `restart` to restart the timer with a new task:

```cpp
timer.restart([]() {
    std::cout << "Timer restarted and task executed!" << std::endl;
});
```

### Checking Timer Status

You can query the timer's current status:

```cpp
if (timer.is_running()) {
    std::cout << "The timer is currently running!" << std::endl;
}
```

## Timer States

The `SimpleTimer` class defines three possible states:

- `Stopped`: The timer is not running
- `Running`: The timer is currently active
- `Paused`: The timer is paused

Use the `state()` method to check the current state:

```cpp
if (timer.state() == SimpleTimer::State::Running) {
    std::cout << "Timer is running!" << std::endl;
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

## Notes

- Timer accuracy depends on the system clock, typically accurate to the millisecond.
- If your task accesses shared resources, consider using proper synchronization within the task callable.

## License

`SimpleTimer` is released under the MIT License. See [LICENSE](LICENSE) for details.