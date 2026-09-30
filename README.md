# System Logger

A Linux system monitoring utility written in C that uses POSIX threads to collect CPU, memory, and disk utilization and continuously record the results to a timestamped log file.

## Overview

System Logger is designed to demonstrate practical Linux system programming concepts such as multithreading, mutex-based synchronization, signal handling, `/proc` filesystem access, filesystem statistics, and POSIX file I/O.

The application uses two worker threads:

- **Collector Thread** — collects CPU, memory, and disk utilization.
- **Logger Thread** — safely reads the collected statistics and writes timestamped entries to a log file.

A shared `Snapshot` structure stores the latest resource statistics. Access to this shared data is protected using a POSIX mutex to prevent concurrent read/write conflicts.

## Key Features

- Multithreaded system monitoring using POSIX `pthread`
- CPU utilization calculation using `/proc/stat`
- Memory utilization calculation using `/proc/meminfo`
- Disk utilization monitoring using `statvfs()`
- Mutex-based synchronization for shared data
- Graceful shutdown using `SIGINT` / `Ctrl+C`
- Timestamped system logs
- Configurable disk path
- Configurable logging interval
- POSIX file operations for persistent logging
- Linux command-line interface

## Architecture

```text
                         Main Thread
                              |
                +-------------+-------------+
                |                           |
          pthread_create              pthread_create
                |                           |
                v                           v
       +------------------+        +------------------+
       | Collector Thread |        |  Logger Thread   |
       +------------------+        +------------------+
                |                           |
                | Collects                  | Reads
                v                           v
       +------------------+        +------------------+
       | /proc/stat       |        | Shared Snapshot |
       | /proc/meminfo    |        +------------------+
       | statvfs()         |                 ^
       +------------------+                 |
                |                           |
                +-------- Mutex ------------+
                            |
                            v
                  +-------------------+
                  | Marvellous_log.txt|
                  +-------------------+
```

## System Information Collection

### CPU Usage

CPU statistics are obtained from:

```text
/proc/stat
```

The application calculates CPU utilization using the change in total CPU time and idle CPU time between two measurements.
Marvellous
### Memory Usage

Memory information is obtained from:

```text
/proc/meminfo
```

The application uses `MemTotal` and `MemAvailable` to calculate memory utilization.

### Disk Usage

Filesystem information is collected using the Linux `statvfs()` interface. The monitored path can be supplied as a command-line argument.

## Thread Synchronization

The collector and logger threads access the same `Snapshot` structure.

A POSIX mutex protects this critical section:

```c
pthread_mutex_lock(&mtx);

/* access shared snapshot */

pthread_mutex_unlock(&mtx);
```

The collector thread locks the mutex while updating CPU, memory, and disk values. The logger thread locks the same mutex while reading those values.

This provides synchronized access to shared data and prevents race conditions between concurrent threads.

## Signal Handling

The application handles `SIGINT`, allowing the user to terminate the logger with:

```text
Ctrl+C
```

The signal handler sets a shared `stop_flag`, allowing both worker threads to exit their execution loops and perform cleanup before the application terminates.

## Logging

System statistics are written to:

```text
System_log.txt
```

Each log entry contains:

- Timestamp
- CPU utilization
- Memory utilization
- Disk utilization
- Monitored disk path

Example:

```text
[2026-09-30 10:15:20] CPU:  12.45% | MEM:  48.21% | DISK(/):  63.17%
```

## Project Structure

```text
.
├── SysLogger.c
├── System_log.txt
└── README.md
```

## Requirements

- Linux operating system
- GCC
- POSIX threads
- GNU Make
- Linux `/proc` filesystem
- Standard Linux development environment

## Compilation

Compile the program directly with GCC:

```bash
gcc -Wall -Wextra -pthread SysLogger.c -o myexe
```

Or use the project's Makefile if provided.

## Usage

### Default Configuration

Run with default settings:

```bash
./myexe
```

The default configuration monitors:

```text
Disk path: /
Logging interval: 2 seconds
```

### Specify Disk Path

```bash
./myexe /home/Demo
```

### Specify Disk Path and Logging Interval

```bash
./myexe /home/Demo 5
```

The above command monitors `/home/Demo` and uses a 5-second logging interval.

## Command-Line Arguments

```text
./myexe [disk_path] [interval_seconds]
```

| Argument | Description | Default |
|---|---|---|
| `disk_path` | Filesystem path whose disk usage is monitored | `/` |
| `interval_seconds` | Logging interval in seconds | `2` |

Invalid or non-positive intervals fall back to the default interval.

## Graceful Shutdown

Press:

```text
Ctrl+C
```

The program sets the termination flag, allows the worker threads to finish, writes a final log footer, closes the log file, and terminates cleanly.

## Concepts Demonstrated

- C programming
- POSIX threads (`pthread`)
- Thread creation and joining
- Mutex synchronization
- Critical sections
- Shared data protection
- Race-condition prevention
- Signal handling
- `SIGINT`
- `/proc` filesystem
- CPU and memory monitoring
- `statvfs()`
- File descriptors
- `open()`, `write()`, and `close()`
- Command-line argument handling
- Linux system programming

## Learning Outcomes

This project provided hands-on experience with concurrent Linux programming and system-level resource monitoring. It demonstrates how multiple threads can cooperate through shared data while using mutex synchronization to safely coordinate access.

It also provides practical exposure to Linux's `/proc` interface, filesystem statistics, POSIX APIs, signal handling, and low-level file I/O.

## Author

**Mangesh Bedre**