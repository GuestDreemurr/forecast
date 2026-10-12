#ifndef NW4R_EF_ANIM_CURVE_H
#define NW4R_EF_ANIM_CURVE_H
#include <nw4r/types_nw4r.h>

namespace nw4r {
namespace ef {

// Forward declarations
class Particle;

// This library version has one routine per curve dimension.
// The names are descriptive; the originals are unknown.
void AnimCurveExecuteU8Vec3(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                            u32 life);

void AnimCurveExecuteU8(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                        u32 life);

void AnimCurveExecuteF32Vec2(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                             u16 seed, u32 life);

void AnimCurveExecuteF32Vec3(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                             u32 life);

void AnimCurveExecuteF32N(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                          u32 life, u8 enable);

void AnimCurveExecuteF32(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                         u16 seed, u32 life);

void AnimCurveExecuteRotate(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life);

void AnimCurveExecuteTexture(u8* pCmdList, Particle* pParticle, u32 tick,
                             u16 seed, u32 life);

void AnimCurveExecuteChild(u8* pCmdList, Particle* pParticle, u32 tick,
                           u16 seed, u32 life);

} // namespace ef
} // namespace nw4r

#endif
