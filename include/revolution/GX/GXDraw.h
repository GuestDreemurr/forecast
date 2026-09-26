#ifndef RVL_SDK_GX_DRAW_H
#define RVL_SDK_GX_DRAW_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

void GXDrawCylinder(u8 sides);
void GXDrawTorus(f32 rc, u8 numc, u8 numt); // https://github.com/doldecomp/melee/blob/43c7de326a8192cac8eccd0af8272933e16a4e7d/libs/dolphin/src/dolphin/gx/GXDraw.c#L160
void GXDrawSphere(u32 stacks, u32 sectors);
static void GXDrawCubeFace(f32 nx, f32 ny, f32 nz, f32 tx, f32 ty, f32 tz,
                           f32 bx, f32 by, f32 bz, GXAttrType binormal,
                           GXAttrType texture); // https://github.com/doldecomp/melee/blob/43c7de326a8192cac8eccd0af8272933e16a4e7d/libs/dolphin/src/dolphin/gx/GXDraw.c#L253
void GXDrawCube(void); // https://github.com/doldecomp/melee/blob/43c7de326a8192cac8eccd0af8272933e16a4e7d/libs/dolphin/src/dolphin/gx/GXDraw.c#L299

#ifdef __cplusplus
}
#endif
#endif
