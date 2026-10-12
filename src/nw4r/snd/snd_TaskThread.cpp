#include <nw4r/snd.h>

namespace nw4r {
namespace snd {
namespace detail {

TaskThread& TaskThread::GetInstance() {
    static TaskThread instance;
    return instance;
}

bool TaskThread::Create(s32 priority, void* pStack, u32 stackSize) {
    if (mCreateFlag) {
        return true;
    }

    mCreateFlag = true;

    OSInitMessageQueue(&mMsgQueue, mMsgBuffer, MSG_QUEUE_CAPACITY);
    OSInitThreadQueue(&mThreadQueue);

    mStack = pStack;

    BOOL result = OSCreateThread(&mThread, ThreadFunc, &GetInstance(),
                                 static_cast<u8*>(pStack) + stackSize,
                                 stackSize, priority, 0);

    if (result) {
        OSResumeThread(&mThread);
    }

    return result;
}

void TaskThread::SendWakeupMessage() {
    OSSendMessage(&mMsgQueue, reinterpret_cast<OSMessage>(MSG_WAKEUP), 0);
}

void* TaskThread::ThreadFunc(void* pArg) {
    TaskThread* p = static_cast<TaskThread*>(pArg);
    OSMessage msg;

    while (true) {
        OSReceiveMessage(&p->mMsgQueue, &msg, OS_MSG_BLOCKING);

        if (reinterpret_cast<u32>(msg) == MSG_WAKEUP) {
            TaskManager::GetInstance().Execute();
        } else if (reinterpret_cast<u32>(msg) == MSG_SHUTDOWN) {
            break;
        }
    }

    return NULL;
}

} // namespace detail
} // namespace snd
} // namespace nw4r
