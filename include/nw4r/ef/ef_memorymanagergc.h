#ifndef NW4R_EF_MEMORY_MANAGER_GC_H
#define NW4R_EF_MEMORY_MANAGER_GC_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/ef_memorymanagertmp.h>

// MemoryManagerTmp::GarbageCollection specializations.
//
// These must come after the MemoryManager class definition: they are what
// first instantiates the MemoryManagerTmp classes, and the order of
// instantiation decides the order of the vtables in .data (reversed).

NW4R_EF_MEMORY_MANAGER_NAMESPACE_OPEN;

template <> inline void TEffectOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TEffect* pObj = static_cast<TEffect*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerES->mActivityList[pObj->mGroupID].ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

template <> inline void TEmitterOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TEmitter* pObj = static_cast<TEmitter*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerEF->mActivityList.ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

template <> inline void TParticleManagerOM::GarbageCollection() {
    void* pPtr = ::nw4r::ut::List_GetFirst(&mLeasedList);

    if (pPtr != NULL) {
        void* pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);

        while (pPtr != NULL) {
            TParticleManager* pObj = static_cast<TParticleManager*>(pPtr);

            if (pObj->GetLifeStatus() ==
                    ::nw4r::ef::ReferencedObject::NW4R_EF_LS_CLOSING &&
                pObj->GetRefCount() == 0) {

                pObj->mManagerEM->mActivityList.ToFree(pObj);
                ::nw4r::ut::List_Remove(&mLeasedList, pObj);
                ::nw4r::ut::List_Append(&mFreeList, pObj);
            }

            pPtr = pNext;
            pNext = ::nw4r::ut::List_GetNext(&mLeasedList, pPtr);
        }
    }
}

#if !defined(NONMATCHING)
template <> inline void TParticleOM::GarbageCollection() {}
#endif

NW4R_EF_MEMORY_MANAGER_NAMESPACE_CLOSE;

#endif
