#ifndef RVL_SDK_NET_CRC_H
#define RVL_SDK_NET_CRC_H
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

u32 NETCalcCRC32(const void* data, u32 size);

#ifdef __cplusplus
}
#endif
#endif
