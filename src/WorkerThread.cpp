// Background thread with its own stack (used to decompress the earth model)
#include <channel/WorkerThread.h>

WorkerThread::WorkerThread(OSThreadFunc func) {
    OSCreateThread(&mThread, func, NULL, mStack + WORKER_THREAD_STACK_SIZE, WORKER_THREAD_STACK_SIZE, 31, 0);
    OSResumeThread(&mThread);
}

WorkerThread::~WorkerThread() {
    OSJoinThread(&mThread, NULL);
}

void WorkerThread::Restart(OSThreadFunc func) {
    OSJoinThread(&mThread, NULL);
    OSCreateThread(&mThread, func, NULL, mStack + WORKER_THREAD_STACK_SIZE, WORKER_THREAD_STACK_SIZE, 31, 0);
    OSResumeThread(&mThread);
}
