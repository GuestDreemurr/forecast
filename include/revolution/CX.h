#ifndef RVL_SDK_CX_H
#define RVL_SDK_CX_H
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CXUncompContextLZ {
    u8* destp;          // at 0x0
    s32 destCount;      // at 0x4
    s32 forceDestCount; // at 0x8
    s32 length;         // at 0xC
    u8 lengthFlg;       // at 0x10
    u8 flags;           // at 0x11
    u8 flagIndex;       // at 0x12
    u8 headerSize;      // at 0x13
    u8 exFormat;        // at 0x14
} CXUncompContextLZ;

typedef struct CXUncompContextHuffman {
    u8* destp;          // at 0x0
    s32 destCount;      // at 0x4
    s32 forceDestCount; // at 0x8
    u8* treep;          // at 0xC
    u32 srcTmp;         // at 0x10
    u32 destTmp;        // at 0x14
    s16 treeSize;       // at 0x18
    u8 srcTmpCnt;       // at 0x1A
    u8 destTmpCnt;      // at 0x1B
    u8 bitSize;         // at 0x1C
    u8 headerSize;      // at 0x1D
    u8 tree[0x200];     // at 0x20
} CXUncompContextHuffman;

#define CXIsFinishedUncompLZ(ctx) (((ctx)->destCount > 0 || (ctx)->headerSize > 0) ? FALSE : TRUE)

u32 CXGetUncompressedSize(const void* src);
void CXUncompressLZ(const void* src, void* dst);
void CXUncompressHuffman(const void* src, void* dst);

void CXInitUncompContextLZ(CXUncompContextLZ* ctx, void* dest);
s32 CXReadUncompLZ(CXUncompContextLZ* ctx, const void* data, u32 len);

void CXInitUncompContextHuffman(CXUncompContextHuffman* ctx, void* dest);
s32 CXReadUncompHuffman(CXUncompContextHuffman* ctx, const void* data, u32 len);

#ifdef __cplusplus
}
#endif
#endif
