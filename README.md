# green-threads

A small green-threads library in C for Linux. It runs many lightweight
("user-level") threads on top of a single OS thread — the M:1 model — and
schedules them itself, without the kernel ever knowing they exist.

I built it to understand how runtimes like Go's goroutines and Java's virtual
threads actually work underneath: context switching, a scheduler, and real
preemption, all from scratch.

## What it does

- Creates threads with their own heap-allocated stacks and switches between
  them by saving and restoring context.
- Round-robin scheduling with cooperative `yield`.
- `join`, so one thread can wait for another to finish.
- Preemption: a periodic timer signal forces a switch even when a thread never
  yields on its own, so a runaway loop can't hog the CPU.
- A blocking mutex that puts a waiting thread to sleep instead of freezing the
  whole process.

## Build & run

    make

That builds the library and the example programs into `build/`. A few to try:

    ./build/04_yield      # threads taking turns with yield
    ./build/05_join       # waiting on other threads to finish
    ./build/06_preempt    # two busy threads, neither yields — preemption splits them
    ./build/07_mutex      # the same counter, with and without a lock

## Layout

- `include/gthread.h` — the public API
- `src/gthread.c` — the runtime: scheduler, context switching, timer, mutex
- `examples/` — small standalone demos

## Maybe later

A port to the xv6 teaching OS. That one's more involved: xv6 has no signals to
build preemption on, so that machinery would have to be added to the kernel
first. For now it's Linux-only.
