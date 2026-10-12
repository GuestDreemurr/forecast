#ifndef NW4R_SND_TASK_THREAD_H
#define NW4R_SND_TASK_THREAD_H
#include <nw4r/types_nw4r.h>

#include <revolution/OS.h>

namespace nw4r {
namespace snd {
namespace detail {

class TaskThread {
public:
    static TaskThread& GetInstance();

    bool Create(s32 priority, void* pStack, u32 stackSize);
    void SendWakeupMessage();

private:
    enum ThreadMessage {
        MSG_NONE,
        MSG_WAKEUP,
        MSG_SHUTDOWN
    };

    static const int MSG_QUEUE_CAPACITY = 2;

private:
    TaskThread() : mStack(NULL), mCreateFlag(false) {}

    static void* ThreadFunc(void* pArg);

private:
    OSThread mThread;                         // at 0x0
    OSThreadQueue mThreadQueue;               // at 0x318
    OSMessageQueue mMsgQueue;                 // at 0x320
    OSMessage mMsgBuffer[MSG_QUEUE_CAPACITY]; // at 0x340
    void* mStack;                             // at 0x348
    bool mCreateFlag;                         // at 0x34C
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
