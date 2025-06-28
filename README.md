# xv6 Priority Scheduler

A priority-based process scheduler implementation for the xv6 operating system, replacing the default round-robin scheduler with a more sophisticated priority queue system.

## Overview

This project implements a priority-based process scheduler in xv6, a teaching operating system developed at MIT. The scheduler supports multiple priority levels (0-5) with round-robin scheduling within each priority level, following the rules:

1. **Higher priority processes run first** - If Priority(A) > Priority(B), A runs (B doesn't)
2. **Equal priority processes use round-robin** - If Priority(A) = Priority(B), A & B run in round robin

## Key Features

- **Priority Queue System**: Implemented separate runnable queues for each priority level (0-5)
- **System Calls**: Added `getprio()` and `setprio()` system calls for priority management
- **Atomic Operations**: Thread-safe priority scheduling with proper locking mechanisms
- **Process Management**: Enhanced process control blocks to track priority and queue membership

## Implementation Details

### Core Components

**Priority Queue Management** (`kernel/proc.c`):
- `struct runnable_queue`: Lock-protected queues for each priority level
- `enqueue_runnable()`: Adds processes to appropriate priority queue
- `dequeue_runnable()`: Removes highest priority process for scheduling
- `prioscheduler()`: Main scheduling loop replacing default scheduler

**System Call Interface** (`kernel/sysproc.c`):
- `sys_getprio(int pid)`: Returns current priority of specified process
- `sys_setprio(int priority)`: Sets priority of current process
- Input validation and error handling for invalid PIDs/priorities

**Process Structure Enhancements** (`kernel/proc.h`):
```c
struct proc {
    // ... existing fields
    int priority;              // Process priority (0-5)
    struct proc *rqueue_next;  // Queue linkage pointer
};
```

### Technical Challenges Solved

1. **Concurrency Control**: Implemented fine-grained locking for each priority queue to prevent race conditions
2. **Queue Management**: Designed efficient linked-list based queues with O(1) enqueue/dequeue operations
3. **Priority Inheritance**: Ensured child processes inherit appropriate priority levels
4. **System Integration**: Seamlessly integrated new scheduler with existing xv6 process management

## Building and Running

### Prerequisites
- RISC-V cross-compilation toolchain
- QEMU RISC-V system emulator

### Build Instructions
```bash
make qemu
```

### Testing the Scheduler
Run the included test programs in the xv6 shell:
```bash
# Test priority system calls
priotest

# Test equal priority scheduling
equaltest

# Test different priority scheduling  
difftest

# Custom tests
mytests
```

## Code Structure

```
kernel/
├── proc.c          # Priority scheduler implementation
├── proc.h          # Enhanced process structure
├── sysproc.c       # Priority system calls
├── syscall.c       # System call routing
├── param.h         # Priority constants (MAXPRIO, STARTPRIO)
└── defs.h          # Function declarations

user/
├── priotest.c      # Priority system call tests
├── equaltest.c     # Equal priority scheduling tests
├── difftest.c      # Different priority scheduling tests
└── mytests.c       # Custom test suite
```

## Performance Characteristics

- **Time Complexity**: O(1) for process enqueueing/dequeueing
- **Space Complexity**: O(n) where n is the number of priority levels
- **Fairness**: Round-robin within priority levels ensures fairness
- **Responsiveness**: Higher priority processes get immediate CPU access

## Educational Context

This project was developed as part of an operating systems course to demonstrate understanding of:
- Process scheduling algorithms
- Kernel programming and system calls
- Concurrency and synchronization
- Operating system design principles

## Technical Skills Demonstrated

- **C Programming**: Low-level kernel development in C
- **Operating Systems**: Process management, scheduling, system calls
- **Concurrency**: Multi-threaded programming with locks and atomic operations
- **Systems Programming**: Kernel-level debugging and development

## License

Based on xv6, which is licensed under the MIT License. Original xv6 code copyright (c) 2006-2024 Frans Kaashoek, Robert Morris, Russ Cox, Massachusetts Institute of Technology.

## Acknowledgments

Built upon the xv6 operating system developed at MIT. Implementation of priority scheduling system and system calls is original work.
