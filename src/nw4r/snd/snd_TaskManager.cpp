#include <nw4r/snd.h>

namespace nw4r {
namespace snd {
namespace detail {

u8 TaskManager::sTaskArea[TASK_AREA_SIZE];

TaskManager& TaskManager::GetInstance() {
    static TaskManager instance;
    return instance;
}

void* TaskManager::Alloc() {
    ut::AutoInterruptLock lock;

    void* pTask = MEMAllocFromUnitHeap(mHeap);

    while (pTask == NULL) {
        ExecuteTask();
        pTask = MEMAllocFromUnitHeap(mHeap);
    }

    return pTask;
}

void TaskManager::AppendTask(Task* pTask, TaskPriority priority) {
    ut::AutoInterruptLock lock;
    mTaskList[priority].PushBack(pTask);
}

void TaskManager::Execute() {
    while (ExecuteTask()) {
        ;
    }
}

bool TaskManager::ExecuteTask() {
    ut::detail::AutoLock<OSMutex> lock(mMutex);

    Task* pTask;

    pTask = PopTask(PRIORITY_HIGH);
    mCurrentTask = pTask;

    if (pTask == NULL) {
        pTask = PopTask(PRIORITY_MIDDLE);
        mCurrentTask = pTask;

        if (pTask == NULL) {
            pTask = PopTask(PRIORITY_LOW);
            mCurrentTask = pTask;

            if (pTask == NULL) {
                return false;
            }
        }
    }

    mCurrentTask->Execute();
    Free(mCurrentTask);
    mCurrentTask = NULL;

    return true;
}

void TaskManager::CancelByTaskId(u32 taskId) {
    ut::AutoInterruptLock lock;

    for (int i = 0; i < PRIORITY_MAX; i++) {
        for (TaskList::Iterator it = mTaskList[i].GetBeginIter();
             it != mTaskList[i].GetEndIter();) {

            TaskList::Iterator curr = it++;

            if (taskId == curr->mTaskId) {
                mTaskList[i].Erase(curr);
                Free(&*curr);
            }
        }
    }

    if (mCurrentTask != NULL && taskId == mCurrentTask->mTaskId) {
        mCurrentTask->Cancel();
    }
}

} // namespace detail
} // namespace snd
} // namespace nw4r
