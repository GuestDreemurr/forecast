#ifndef RVL_SDK_MEM_UNIT_HEAP_H
#define RVL_SDK_MEM_UNIT_HEAP_H
#include <revolution/MEM/mem_heapCommon.h>
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

MEMiHeapHead* MEMCreateUnitHeapEx(void* start, u32 size, u32 unitSize,
                                  s32 align, u16 opt);
void* MEMAllocFromUnitHeap(MEMiHeapHead* heap);
void MEMFreeToUnitHeap(MEMiHeapHead* heap, void* memBlock);

#ifdef __cplusplus
}
#endif
#endif
