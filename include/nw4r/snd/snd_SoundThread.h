#ifndef NW4R_SND_SOUND_THREAD_H
#define NW4R_SND_SOUND_THREAD_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_AxManager.h>

#include <nw4r/ut.h>

#include <revolution/OS.h>

namespace nw4r {
namespace snd {
namespace detail {

class SoundThread {
    friend class AutoLock; // Prevent locking without AutoLock

public:
    /******************************************************************************
     * PlayerCallback
     ******************************************************************************/
    class PlayerCallback {
    public:
        NW4R_UT_LINKLIST_NODE_DECL(); // at 0x0

        virtual ~PlayerCallback() {}               // at 0x8
        virtual void OnUpdateFrameSoundThread() {} // at 0xC
        virtual void OnUpdateVoiceSoundThread() {} // at 0x10
    };

    NW4R_UT_LINKLIST_TYPEDEF_DECL(PlayerCallback);

    /******************************************************************************
     * AutoLock
     ******************************************************************************/
    class AutoLock : public ut::detail::AutoLock<OSMutex> {
    public:
        AutoLock()
            : ut::detail::AutoLock<OSMutex>(SoundThread::GetInstance().mMutex) {
        }
    };

public:
    static SoundThread& GetInstance();

    bool Create(s32 priority, void* pStack, u32 stackSize);

private:
    enum ThreadMessage {
        MSG_NONE,
        MSG_AX_CALLBACK,
        MSG_SHUTDOWN,
    };

    static const int MSG_QUEUE_CAPACITY = 4;

private:
    SoundThread() : mStack(NULL), mCreateFlag(false) {}

    static void AxCallbackFunc();

    static void* SoundThreadFunc(void* pArg);
    void SoundThreadProc();

private:
    OSThread mThread;                         // at 0x0
    OSThreadQueue mThreadQueue;               // at 0x318
    OSMessageQueue mMsgQueue;                 // at 0x320
    OSMessage mMsgBuffer[MSG_QUEUE_CAPACITY]; // at 0x340
    void* mStack;                             // at 0x350
    OSMutex mMutex;                           // at 0x354

    AxManager::CallbackListNode mAxCallbackNode; // at 0x36C
    PlayerCallbackList mPlayerCallbackList;      // at 0x378

    u32 mProcessTick; // at 0x384
    bool mCreateFlag; // at 0x388

    // Never referenced by the linked code; the instance is 0x3A8 bytes
    u8 UNK_0x38C[0x3A8 - 0x38C];
};

} // namespace detail
} // namespace snd
} // namespace nw4r

#endif
