#include "revolution/CNT/cnt.h"
#include "revolution/ARC/arc.h"
#include "revolution/ESP/esp.h"
#include "revolution/IPC/ipcclt.h"
#include "revolution/MEM/mem_allocator.h"
#include "revolution/OS/OS.h"
#include "revolution/OS/OSInterrupt.h"
#include "revolution/private/es_types.h"
#include <revolution/CNT.h>
#include <revolution/ESP.h>
#include <revolution/OS.h>

s32 ESP_InitLib(void);
s32 ESP_CloseLib(void);
int sprintf(char* s, const char* fmt, ...);
void NANDLoggingAddMessageAsync(int, const char* msg, ...);

static s32 __CNTConvertErrorCode(s32 error);

const char* __CNTVersion =
    "<< RVL_SDK - CNT \trelease build: May 10 2007 19:28:13 (0x4199_60831) >>";

DECOMP_FORCEACTIVE(CNTInitHandle, "/content%d");
DECOMP_FORCEACTIVE(CNTInitHandle, "Warning: CNTInitHandle(): directory '%s' is not found under '/'\n");

unsigned int lbl_80330C88;

void CNTInit(void) {
    BOOL enabled;
    if (lbl_80330C88 == 0) {
        ESP_InitLib();

        enabled = OSDisableInterrupts();
        lbl_80330C88 = 1;
        OSRestoreInterrupts(enabled);
        OSRegisterVersion(__CNTVersion);
    }
}

int CNTShutdown(void) {
    return ESP_CloseLib();
}

CNTError contentInitHandleNAND(u32 contentId, CNTHandleNAND* cntHandle, MEMAllocator* allocator) {
    u8 buf[0x20] ALIGN(32);
    ARCHandle handle;
    void *mem;
    ESError err;
    ESFd fd;
    u32 size;

    fd = ESP_OpenContentFile(contentId);
    if (fd < 0) {
        return __CNTConvertErrorCode(fd);
    }

    err = ESP_ReadContentFile(fd, buf, 0x20);
    if (err < 0) {
        return __CNTConvertErrorCode(err);
    }

    size = (((unsigned int *)buf)[3] + 0x1F) & ~0x1F;
    err = ESP_SeekContentFile(fd, 0, 0);
    if (err < 0) {
        return __CNTConvertErrorCode(err);
    }

    mem = MEMAllocFromAllocator(allocator, size);
    if (mem == (void*)0) {
        return -0x1389;
    }

    err = ESP_ReadContentFile(fd, mem, size);
    if (err < 0) {
        MEMFreeToAllocator(allocator, mem);
        return __CNTConvertErrorCode(err);
    }

    ARCInitHandle(mem, &handle);
    memcpy(cntHandle, &handle, 0x1C);

    cntHandle->fd = fd;
    cntHandle->allocator = allocator;

    return 0;
}

CNTError contentOpenNAND(CNTHandleNAND* cntHandle, const char* path, CNTFileInfoNAND* cntFileInfo) {
    ARCFileInfo af;
    s32 entrynum = ARCConvertPathToEntrynum(&cntHandle->arcHandle, path);
    BOOL valid;
    if (entrynum < 0) {
        return -0x1391;
    }

    valid = ARCFastOpen(&cntHandle->arcHandle, entrynum, &af);
    if (!valid) {
        return -0x1391;
    }

    cntFileInfo->handle = cntHandle;
    cntFileInfo->startOffset = af.startOffset;
    cntFileInfo->length = af.length;
    cntFileInfo->readOffset = 0;

    return 0;
}

CNTError contentFastOpenNAND(CNTHandleNAND* handle, s32 entrynum, CNTFileInfoNAND* info) {
    ARCFileInfo arcInfo;

    if (!ARCFastOpen(&handle->arcHandle, entrynum, &arcInfo)) {
        return -0x1391;
    }

    info->handle = handle;
    info->startOffset = arcInfo.startOffset;
    info->length = arcInfo.length;
    info->readOffset = 0;

    return 0;
}

CNTError contentConvertPathToEntrynumNAND(CNTHandleNAND* handle, const char* path) {
    return ARCConvertPathToEntrynum(&handle->arcHandle, path);
}

s32 contentGetLengthNAND(CNTFileInfoNAND* info) {
    return info->length;
}

CNTError contentSeekNAND(CNTFileInfoNAND* cntFileInfo, s32 offset, u32 whence) {
    s32 position;

    switch (whence) {
        case 0:
            position = offset;
            break;
        case 1:
            position = cntFileInfo->readOffset + offset;
            break;
        case 2:
            position = cntFileInfo->length + offset;
            break;
        default:
            return -0x1391;
    }

    if (position < 0 || position > cntFileInfo->length) return -0x1391;

    cntFileInfo->readOffset = position;
    return 0;
}

s32 contentReadNAND(CNTFileInfoNAND* info, void* dst, u32 len, s32 offset) {
    int err;

    if ((s32)info->readOffset + offset < 0 || info->readOffset + offset > info->length) {
        return -0x1391;
    }

    err = ESP_SeekContentFile(info->handle->fd, info->startOffset + info->readOffset + offset, IPC_SEEK_BEG);
    if (err < 0) {
        return __CNTConvertErrorCode(err);
    }

    return __CNTConvertErrorCode(
        ESP_ReadContentFile(info->handle->fd, dst, len));
}

CNTError contentCloseNAND(CNTFileInfoNAND* cntFileInfo) {
    return 0;
}

CNTError contentReleaseHandleNAND(CNTHandleNAND* cntFileInfo) {
    MEMFreeToAllocator(cntFileInfo->allocator, cntFileInfo->arcHandle.archiveStartAddr);
    return __CNTConvertErrorCode(ESP_CloseContentFile(cntFileInfo->fd));
}

BOOL contentOpenDirNAND(CNTHandleNAND* cntFileInfo, const char* path, ARCDir* dir) {
    return ARCOpenDir(&cntFileInfo->arcHandle, path, dir);
}

#pragma dont_inline on
static s32 __CNTConvertErrorCode(s32 error) {
    int i;
    char msgA[128] ATTRIBUTE_ALIGN(64);
    char msgB[128] ATTRIBUTE_ALIGN(64);

    const s32 errorMap[] = {
        0x0000,  0x0000,  -0x03E9, -0x13C7, -0x03EA, -0x13C7, -0x03EB, -0x13C7,
        -0x03EC, -0x13C7, -0x03ED, -0x13C7, -0x03EE, -0x13C7, -0x03EF, -0x13C7,
        -0x03F0, -0x13C7, -0x03F1, -0x13C7, -0x03F2, -0x13C7, -0x03F3, -0x13C7,
        -0x03F4, -0x13C7, -0x03F5, -0x13C7, -0x03F6, -0x13C7, -0x03F7, -0x13C7,
        -0x03F8, -0x1388, -0x03F9, -0x1391, -0x03FA, -0x13C7, -0x03FB, -0x13C7,
        -0x03FC, -0x13C7, -0x03FD, -0x13C7, -0x03FE, -0x13C7, -0x03FF, -0x13C7,
        -0x0400, -0x1390, -0x0401, -0x13C7, -0x0402, -0x1392, -0x0403, -0x13C7,
        -0x0404, -0x13C7, -0x0405, -0x13C7, -0x0406, -0x13C7, -0x0407, -0x13C7,
        -0x0408, -0x13C7, -0x0409, -0x13C7, -0x040A, -0x13C7, -0x040B, -0x13C7,
        -0x040C, -0x13C7, -0x040D, -0x13C7, -0x040E, -0x13C7, 0x0000,  0x0000,
        -0x0066, -0x1392, -0x0067, -0x1393, -0x0072, -0x1394, -0x0069, -0x13C7,
        -0x0074, -0x1395, -0x0065, -0x1391, -0x006C, -0x13C7, -0x006D, -0x1388,
        -0x006B, -0x13C7, -0x006A, -0x1391, -0x0073, -0x13C7, -0x0068, -0x13C7,
        -0x006F, -0x13C7, -0x0075, -0x13C7, -0x0076, -0x1390, -0x0077, -0x1407,
        -0x0001, -0x1392, -0x0002, -0x13C7, -0x0003, -0x13C7, -0x0004, -0x1391,
        -0x0005, -0x13C7, -0x0006, -0x1391, -0x0007, -0x13C7, -0x0008, -0x1390,
        -0x0009, -0x13C7, -0x000A, -0x13C7, -0x000B, -0x13C7, -0x000C, -0x1394,
        -0x000D, -0x13C7, -0x000E, -0x13C7, -0x000F, -0x13C7, -0x0010, -0x13C7,
        -0x0011, -0x13C7, -0x0012, -0x13C7, -0x0013, -0x13C7, -0x0014, -0x13C7,
        -0x0015, -0x13C7, -0x0016, -0x1390, -0x0017, -0x13C7,
    };

    i = 0;

    if (error >= 0) {
        return error;
    }

    for (; i < ARRAY_SIZE(errorMap); i += 2) {
        if (error == errorMap[i]) {
            if (error == -0x72 || error == -0x74 || error == -0x75 ||
                error == -9 || error == -12) {
                sprintf(msgA, "ES error code: %d", error);
                NANDLoggingAddMessageAsync(0, msgA);
            }
            return errorMap[i + 1];
        }
    }

    OSReport("CAUTION!  Unexpected error code [%d] was found.\n", error);
    sprintf(msgB, "ES unexpected error code: %d", error); /* string not recoverable */
    NANDLoggingAddMessageAsync(0, msgB);

    return -0x13C7;
}
#pragma dont_inline reset
