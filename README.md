# green-threads

A green-threads library in C. It runs many lightweight ("user-level") threads
on a small pool of OS threads, the M:N model, and schedules them itself. The
kernel only ever sees the OS worker threads, not the green threads on top.

I built it to understand how runtimes like Go's goroutines and Java's virtual
threads work underneath: context switching, a scheduler, preemption, and what
changes once the scheduler runs on several cores at the same time.

It started as an M:1 library (everything on one OS thread) and was later
extended to M:N. Tested on Linux and macOS (Apple Silicon).

## What it does

- Each green thread gets its own heap-allocated stack. Switching saves one
  thread's registers and restores another's (`ucontext`).
- N worker threads (pthreads) pull green threads from a shared ready queue, so
  green threads really run in parallel and move between workers.
- Cooperative `yield` and `join`.
- Preemption: a ticker thread sends `SIGALRM` to every worker every 50 ms, so
  a busy loop that never yields can't hog its worker.
- A blocking mutex. A thread that waits for it goes to sleep instead of
  spinning, and the lock is handed directly to the next waiter.
- Shared scheduler state is protected with spinlocks built on C11 atomics.

## Build & run

    make

That builds the library and the examples into `build/`. On macOS you need the
Xcode Command Line Tools (`xcode-select --install`). A few to try:

    ./build/05_join          # one thread waiting for others to finish
    ./build/06_preempt       # two busy threads, neither yields, preemption splits them
    ./build/10_mn 4          # 8 CPU-heavy threads on 4 workers
    ./build/11_mutex_mn 4    # shared counter on 4 workers, with and without the mutex

## Benchmark

    ./build/12_bench

It runs two tests:

1. Two threads on one worker take turns with `gt_yield` a million times, which
   gives the cost of one yield (thread to scheduler to the next thread).
2. It spawns 100,000 green threads at once and runs them on 4 workers. Each
   one yields once and then finishes, and a counter checks that all of them
   really ran.

Results on a MacBook Air (Apple Silicon), three runs:

    yield: 1783 ns po yield-u (2 niti x 1000000)
    niti: 100000 zavrseno od 100000, na 4 workera, za 1577 ms
    yield: 1759 ns po yield-u (2 niti x 1000000)
    niti: 100000 zavrseno od 100000, na 4 workera, za 1210 ms
    yield: 1751 ns po yield-u (2 niti x 1000000)
    niti: 100000 zavrseno od 100000, na 4 workera, za 1057 ms

(The output is in Serbian: "po yield-u" means "per yield", "zavrseno od" means
"finished out of".)

So 100,000 concurrent threads complete in about 1 to 1.5 s, and a yield takes
about 1.75 µs. Most of that time is `swapcontext` itself, which also saves and
restores the signal mask with a system call on every switch. A small
hand-written switch in assembly would avoid that, see "Maybe later".

## Using it

    #include "gthread.h"

    gt_thread_t *t = gt_spawn(fn, arg);   // create a green thread
    gt_run_workers(4);                    // run everything on 4 workers, returns when all are done
    gt_run();                             // same, with a single worker

Inside green threads: `gt_yield()`, `gt_join(t)`, `gt_mutex_lock(&m)` /
`gt_mutex_unlock(&m)`.

`printf` and `malloc` are not safe to interrupt in the middle, so use
`gt_printf` for output, and wrap other such calls in `gt_preempt_disable()` /
`gt_preempt_enable()`.

## Things I ran into

- On macOS, a thread that was preempted inside the signal handler has to resume
  on the same OS thread that received the signal, otherwise `sigreturn` fails.
  Preempted threads therefore go to a per-worker local queue.
- On macOS the main thread doesn't run green threads, only the pthread workers
  do. With main as a worker the program crashed under heavy switching.
- The compiler can compute the address of a `_Thread_local` variable once and
  reuse it after `swapcontext`, when the green thread may already be on a
  different worker. Thread-local state is accessed through small `noinline`
  functions to avoid that.

## Layout

- `include/gthread.h`: the public API
- `src/gthread.c`: the runtime (scheduler, workers, context switching, preemption, mutex)
- `examples/`: small standalone demos and the benchmark

## Maybe later

- A hand-written context switch in assembly instead of `ucontext`.
- A queue per worker with work stealing, instead of one shared queue.
- Letting idle workers sleep instead of spinning on `sched_yield`.
- Python bindings.
- A port to the xv6 teaching OS, which first needs signals added to its kernel.
