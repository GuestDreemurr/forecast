#include <nw4r/ef.h>

#include <cmath>

namespace nw4r {
namespace ef {

void EmitterFormLine::Emission(Emitter* pEmitter, ParticleManager* pManager,
                               int count, u32 flags, f32* pParams, u16 life,
                               f32 lifeRnd, const math::MTX34* pSpace) {

    if (count < 1) {
        return;
    }

    for (int i = 0; i < count; i++) {
        f32 fpos;

        if (!(flags & EmitterDesc::EMIT_FLAG_17)) {
            fpos = pEmitter->mRandom.RandFloat();
        } else if (count > 1) {
            fpos = static_cast<f32>(i) / static_cast<f32>(count - 1);
        } else {
            fpos = 0.0f;
        }

        if (flags & EmitterDesc::EMIT_FLAG_26) {
            fpos -= 0.5f;
        }

        fpos *= pParams[0];

        f32 sx = std::sinf(pParams[1]);
        f32 cx = std::cosf(pParams[1]);

        f32 sy = std::sinf(pParams[2]);
        f32 cy = std::cosf(pParams[2]);

        f32 sz = std::sinf(pParams[3]);
        f32 cz = std::cosf(pParams[3]);

        math::VEC3 pos;
        pos.x = (cx * cz * sy + sx * sz) * fpos;
        pos.y = (-cz * sx + cx * sy * sz) * fpos;
        pos.z = (cx * cy) * fpos;

        math::VEC3 normal;
        normal.x = 0.0f;
        normal.y = 1.0f;
        normal.z = 0.0f;

        math::VEC3 fromYAxis(pos.x, 0.0f, pos.z);

        math::VEC3 vel;
        CalcVelocity(&vel, pEmitter, pos, normal, pos, fromYAxis);

        // clang-format off
        pManager->CreateParticle(
            CalcLife(life, lifeRnd, pEmitter),
            pos,
            vel,
            pSpace,
            1.0f + pEmitter->mParameter.mVelMomentumRandom *
                (1.0f / 100.0f) * pEmitter->mRandom.RandFloat(),
            &pEmitter->mInheritSetting,
            pEmitter->mpReferenceParticle,
            pEmitter->mCalcRemain);
        // clang-format on
    }
}

void EmitterFormLine::Draw(Emitter* pEmitter, const math::MTX34& rViewMtx) {
    SetupDrawGX();

    math::MTX34 mtx = rViewMtx;

    math::MTX34 glbMtx;
    pEmitter->CalcGlobalMtx(&glbMtx);
    math::MTX34Mult(&mtx, &mtx, &glbMtx);

    f32 root3 = static_cast<f32>(sqrt(3.0));
    f32 size = 2.0f / root3;

    math::MTX34RotXYZRad(&glbMtx, pEmitter->mParameter.mParams[1], pEmitter->mParameter.mParams[2], pEmitter->mParameter.mParams[3]);
    math::MTX34Mult(&mtx, &mtx, &glbMtx);

    math::VEC3 scale;
    scale.x = 0.4f;
    scale.y = 0.4f;
    scale.z = pEmitter->mParameter.mParams[0] / size;
    math::MTX34Scale(&mtx, &mtx, &scale);

    if (!(pEmitter->mParameter.mEmitFlags & EmitterDesc::EMIT_FLAG_26)) {
        scale.x = 0.0f;
        scale.y = 0.0f;
        scale.z = size / 2.0f;
        math::MTX34Trans(&mtx, &mtx, &scale);
    }

    GXLoadPosMtxImm(mtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXDrawCube();
}

} // namespace ef
} // namespace nw4r
