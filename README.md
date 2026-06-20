# green-threads

A lightweight (green) threads library in C. Many user-level threads are
multiplexed onto a single OS thread (M:1), with cooperative scheduling and
timer-based preemption. Built and tested on Linux.

## Build
    make

## Roadmap
- [x] Project scaffold
- [x] Context switch
- [ ] Thread control block + spawn
- [ ] Cooperative scheduler (yield)
- [ ] Thread lifecycle (join, cleanup)
- [ ] Preemption (timer + signal)
- [ ] Mutex
- [ ] Port to xv6
