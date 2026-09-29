#ifndef CHANNEL_WORKER_THREAD_H
#define CHANNEL_WORKER_THREAD_H
#include <types.h>
#include <revolution/OS.h>

#define WORKER_THREAD_STACK_SIZE 0x4000

// An OSThread with its own stack, started as soon as it is created
class WorkerThread {
public:
    WorkerThread(OSThreadFunc func);
    ~WorkerThread();

    void Restart(OSThreadFunc func);

    OSThread mThread;                     // at 0x0
    u8 mStack[WORKER_THREAD_STACK_SIZE]; // at 0x318
};

#endif
