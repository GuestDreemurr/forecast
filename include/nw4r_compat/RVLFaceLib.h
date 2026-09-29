// nw4r_compat: minimal RVLFaceLib declarations used by g3d_scnrfl (the real header is not in the repo).
// Only on the include path of the NW4R library in configure.py. Struct sizes are placeholders.
#ifndef NW4R_COMPAT_RVLFACELIB_H
#define NW4R_COMPAT_RVLFACELIB_H
#include <revolution/GX.h>
#include <revolution/MTX.h>
typedef int RFLErrcode;
enum { RFLErrcode_Success = 0 };
typedef int RFLResolution;
typedef int RFLDataSource;
typedef int RFLExpression;
typedef struct RFLMiddleDB { u8 dummy[0x18]; } RFLMiddleDB;
typedef struct RFLCharModel { u8 dummy[0x60]; } RFLCharModel;
typedef struct RFLDrawSetting {
    BOOL lightEnable;
    u32 lightMask;
    GXDiffuseFn diffuse;
    GXAttnFn attn;
    GXColor ambColor;
    BOOL compLoc;
} RFLDrawSetting;
#ifdef __cplusplus
extern "C" {
#endif
void RFLSetExpression(RFLCharModel*, RFLExpression);
RFLExpression RFLGetExpression(const RFLCharModel*);
RFLErrcode RFLInitCharModel(RFLCharModel*, RFLDataSource, RFLMiddleDB*, u16, void*, RFLResolution, u32);
u32 RFLGetModelBufferSize(RFLResolution, u32);
void RFLSetMtx(RFLCharModel*, const Mtx);
void RFLDrawOpa(const RFLCharModel*);
void RFLDrawXlu(const RFLCharModel*);
void RFLLoadDrawSetting(const RFLDrawSetting*);
#ifdef __cplusplus
}
#endif
#endif
