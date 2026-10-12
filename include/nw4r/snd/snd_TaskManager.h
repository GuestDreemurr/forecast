#ifndef NW4R_SND_TASK_MANAGER_H
#define NW4R_SND_TASK_MANAGER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Task.h>

#include <nw4r/ut.h>

#include <revolution/MEM.h>
#include <revolution/OS.h>

namespace nw4r {
namespace snd {
namespace detail {

class TaskManager {
public:
    enum TaskPriority {
        PRIORITY_LOW = 0,
        PRIORITY_MIDDLE = 1,
        PRIORITY_HIGH = 2,

        PRIORITY_MAX
    };

    static const int TASK_SIZE_MAX = 0x40;
    static const int TASK_COUNT_MAX = 128;

public:
    static TaskManager& GetInstance();

    void* Alloc();
    void AppendTask(Task* pTask, TaskPriority priority = PRIORITY_MIDDLE);

    void Execute();
    bool ExecuteTask();

    void CancelByTaskId(u32 taskId);

private:
    static const int TASK_AREA_SIZE = TASK_COUNT_MAX * TASK_SIZE_MAX + 0x44;

private:
    TaskManager() : mCurrentTask(NULL) {
        OSInitMutex(&mMutex);
        mHeap = MEMCreateUnitHeapEx(sTaskArea, TASK_AREA_SIZE, TASK_SIZE_MAX,
                                    4, 0);
    }

    void Free(Task* pTask) {
        ut::AutoInterruptLock lock;
        MEMFreeToUnitHeap(mHeap, pTask);
    }

    Task* PopTask(TaskPriority priority) {
        ut::AutoInterruptLock lock;

        if (mTaskList[priority].IsEmpty()) {
            return NULL;
        }

        Task* pTask = &mTaskList[priority].GetFront();
        mTaskList[priority].PopFront();

        return pTask;
    }

private:
    OSMutex mMutex;                   // at 0x0
    MEMiHeapHead* mHeap;              // at 0x18
    Task* mCurrentTask;               // at 0x1C
    TaskList mTaskList[PRIORITY_MAX]; // at 0x20

    static u8 sTaskArea[TASK_AREA_SIZE];
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
