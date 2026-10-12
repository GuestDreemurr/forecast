#include <nw4r/ef.h>
#include <nw4r/math.h>

#include <revolution/GX.h>

namespace nw4r {
namespace ef {

static u8 directional_tex0_u8[] ALIGN(32) = {0x00, 0x01, 0x00, 0x00,
                                             0x01, 0x00, 0x01, 0x01};

DrawDirectionalStrategy::DrawDirectionalStrategy() {}

static math::MTX34 CalcLocalTransform(f32 px, f32 py, f32 sx, f32 sy,
                                      f32 stretch,
                                      const nw4r::math::MTX34& rotMtx,
                                      u8 option) {
    switch (option) {
    case 0: {
        f32 sx_px = sx * px;
        f32 sy_st = sy * stretch;
        f32 sy_py = sy * (py + stretch - 1.0f);

        return math::MTX34(
               rotMtx._00 * sx,
               rotMtx._01 * sy_st,
               rotMtx._02 * sx,
               -rotMtx._00 * sx_px - rotMtx._01 * sy_py + px,
               rotMtx._10 * sx,
               rotMtx._11 * sy_st,
               rotMtx._12 * sx,
               -rotMtx._10 * sx_px - rotMtx._11 * sy_py + py,
               rotMtx._20 * sx,
               rotMtx._21 * sy_st,
               rotMtx._22 * sx,
               -rotMtx._20 * sx_px - rotMtx._21 * sy_py);
    }

    case 1:
    default: {
        f32 sx_px = sx * px;
        f32 sy_py = sy * py;

        return math::MTX34(
               rotMtx._00 * sx,
               rotMtx._02 * sy,
               rotMtx._01 * sx,
               -rotMtx._00 * sx_px - rotMtx._02 * sy_py + px,
               rotMtx._10 * sx,
               rotMtx._12 * sy,
               rotMtx._11 * sx,
               -rotMtx._10 * sx_px - rotMtx._12 * sy_py,
               rotMtx._20 * sx,
               rotMtx._22 * sy,
               rotMtx._21 * sx,
               -rotMtx._20 * sx_px - rotMtx._22 * sy_py - py);
    }
    }
}

static inline void MtxSet(math::MTX34* pMtx, f32 m00, f32 m01, f32 m02, f32 m03,
                          f32 m10, f32 m11, f32 m12, f32 m13, f32 m20, f32 m21,
                          f32 m22, f32 m23) {
    pMtx->_00 = m00;
    pMtx->_01 = m01;
    pMtx->_02 = m02;
    pMtx->_03 = m03;
    pMtx->_10 = m10;
    pMtx->_11 = m11;
    pMtx->_12 = m12;
    pMtx->_13 = m13;
    pMtx->_20 = m20;
    pMtx->_21 = m21;
    pMtx->_22 = m22;
    pMtx->_23 = m23;
}

static inline void CalcRotate(math::MTX34* pMtx, Particle* pParticle, u8 axis) {
    math::VEC3 rot;
    pParticle->Draw_GetRotate(&rot);

    f32 sinX = 0.0f, cosX = 1.0f;
    f32 sinY = 0.0f, cosY = 1.0f;
    f32 sinZ = 0.0f, cosZ = 1.0f;

    switch (axis) {
    case 0: {
        if (rot.x != 0.0f) {
            math::SinCosRad(&sinX, &cosX, -rot.x);
        }

        MtxSet(pMtx, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, cosX, -sinX, 0.0f, 0.0f, sinX, cosX, 0.0f);
        break;
    }

    case 1: {
        if (rot.y != 0.0f) {
            math::SinCosRad(&sinY, &cosY, -rot.y);
        }

        MtxSet(pMtx, cosY, 0.0f, sinY, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -sinY, 0.0f, cosY, 0.0f);
        break;
    }

    case 2: {
        if (rot.z != 0.0f) {
            math::SinCosRad(&sinZ, &cosZ, -rot.z);
        }

        MtxSet(pMtx, cosZ, -sinZ, 0.0f, 0.0f, sinZ, cosZ, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        break;
    }

    case 3:
    default: {
        if (rot.x) {
            math::SinCosRad(&sinX, &cosX, -rot.x);
        }
        if (rot.y) {
            math::SinCosRad(&sinY, &cosY, -rot.y);
        }
        if (rot.z) {
            math::SinCosRad(&sinZ, &cosZ, -rot.z);
        }

        f32 sx_sy = sinX * sinY;
        f32 cx_sy = cosX * sinY;

        MtxSet(pMtx,
               cosY * cosZ,
               sx_sy * cosZ - cosX * sinZ,
               cx_sy * cosZ + sinX * sinZ,
               0.0f,
               cosY * sinZ,
               sx_sy * sinZ + cosX * cosZ,
               cx_sy * sinZ - sinX * cosZ,
               0.0f,
               -sinY,
               cosY * sinX,
               cosX * cosY,
               0.0f);
        break;
    }
    }
}

static void DrawQuad(const math::MTX34& rMtx, const math::_VEC3* pPosArray,
                     bool texCoord) {

    math::VEC3 p0, p1, p2, p3;

    math::VEC3Transform(&p0, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[0]));
    math::VEC3Transform(&p1, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[1]));
    math::VEC3Transform(&p2, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[2]));
    math::VEC3Transform(&p3, &rMtx,
                        static_cast<const math::VEC3*>(&pPosArray[3]));

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    {
        GXPosition(p0);
        if (texCoord) {
            GXTexCoord1x8(0);
        }

        GXPosition(p1);
        if (texCoord) {
            GXTexCoord1x8(1);
        }

        GXPosition(p2);
        if (texCoord) {
            GXTexCoord1x8(2);
        }

        GXPosition(p3);
        if (texCoord) {
            GXTexCoord1x8(3);
        }
    }
    GXEnd();
}

void DrawDirectionalStrategy::Draw(const DrawInfo& rInfo,
                                   ParticleManager* pManager) {
    InitGraphics(rInfo, pManager);

    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();
    const EmitterDrawSetting& rSetting = pDesc->drawSetting;

    AheadContext context(*rInfo.GetViewMtx(), pManager);
    CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);

    math::MTX34 glbMtx;
    math::MTX34 posMtx;

    pManager->CalcGlobalMtx(&glbMtx);
    math::MTX34Mult(&posMtx, rInfo.GetViewMtx(), &glbMtx);
    GXLoadPosMtxImm(posMtx, GX_PNMTX0);

    // Emitter matrix in the particle manager's space
    math::MTX34 emMtx;
    math::MTX34 invMtx;

    pManager->mManagerEM->CalcGlobalMtx(&emMtx);
    math::MTX34Inv(&invMtx, &glbMtx);
    math::MTX34Mult(&emMtx, &invMtx, &emMtx);

    math::VEC3 emAxisY;
    math::VEC3 emAxisZ;
    emAxisY = math::VEC3(emMtx._01, emMtx._11, emMtx._21);
    emAxisZ = math::VEC3(emMtx._02, emMtx._12, emMtx._22);
    Normalize(&emAxisZ);

    // Particles that have not been drawn yet start from the emitter's Y axis
    for (Particle* pIt = GetYoungestParticle(pManager);
         pIt != NULL && pIt->mPrevAxis.x > 1.0f;
         pIt = GetElderParticle(pManager, pIt)) {

        pIt->mPrevAxis = emAxisY;
    }

    f32 px = pDesc->pivotX / 100.0f;
    f32 py = pDesc->pivotY / 100.0f;

    GetFirstDrawParticleFunc pGetFirstFunc = GetGetFirstDrawParticleFunc(
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER);

    GetNextDrawParticleFunc pGetNextFunc = GetGetNextDrawParticleFunc(
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER);

    bool first = true;

    for (Particle* pParticle = pGetFirstFunc(pManager); pParticle != NULL;
         pParticle = pGetNextFunc(pManager, pParticle)) {

        f32 sx = pParticle->Draw_GetSizeX();
        if (sx < NW4R_MATH_FLT_EPSILON) {
            continue;
        }

        f32 sy = pParticle->Draw_GetSizeY();
        if (sy < NW4R_MATH_FLT_EPSILON) {
            continue;
        }

        // Stretch along the direction of movement
        f32 stretch = 1.0f;
        if (pDesc->typeOption0 != 0) {
            math::VEC3 move;
            pParticle->GetMoveDir(&move);

            stretch += 0.5f * math::FSqrt(math::VEC3Dot(&move, &move)) / sy;
        }

        SetupGP(pParticle, rSetting, rInfo, first, false);
        first = false;

        math::MTX34 rotMtx;
        CalcRotate(&rotMtx, pParticle, pDesc->typeAxis);

        math::MTX34 locMtx = CalcLocalTransform(px, py, sx, sy, stretch, rotMtx,
                                                pDesc->typeOption1);

        math::VEC3 axisZ;
        math::VEC3 axisY;
        math::VEC3 axisX;

        pCalcAheadFunc(&axisY, &context, pParticle);
        math::VEC3Cross(&axisX, &pParticle->mPrevAxis, &axisY);

        if (!Normalize(&axisX)) {
            axisX = emAxisZ;
        }

        math::VEC3Cross(&axisZ, &axisY, &axisX);

        pParticle->mPrevAxis = axisZ;

        math::VEC3 pos = pParticle->mParameter.mPosition;

        math::MTX34 dirMtx(
            axisZ.x, axisY.x, axisX.x, pos.x,
            axisZ.y, axisY.y, axisX.y, pos.y,
            axisZ.z, axisY.z, axisX.z, pos.z);

        math::MTX34 drawMtx;
        math::MTX34Mult(&drawMtx, &dirMtx, &locMtx);

        // clang-format off
        static const math::_VEC3 p[4] = {
            -1.0f, -1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
             1.0f, -1.0f, 0.0f
        };
        // clang-format on

        DrawQuad(drawMtx, p, mNumTexmap > 0);

        if (pDesc->typeOption == EmitterDrawSetting::TYPE_CMN_CROSS) {
            // clang-format off
            static const math::_VEC3 px[4] = {
                0.0f, -1.0f,  1.0f,
                0.0f,  1.0f,  1.0f,
                0.0f,  1.0f, -1.0f,
                0.0f, -1.0f, -1.0f
            };
            // clang-format on

            DrawQuad(drawMtx, px, mNumTexmap > 0);
        }
    }
}

void DrawDirectionalStrategy::InitGraphics(const DrawInfo& rInfo,
                                           ParticleManager* pManager) {

    const EmitterDrawSetting& rSetting =
        *pManager->mResource->GetEmitterDrawSetting();

    InitTexture(rSetting);
    InitTev(rSetting, rInfo);
    InitColor(pManager, rSetting, rInfo);

    GXEnableTexOffsets(GX_TEXCOORD0, TRUE, TRUE);

    GXSetArray(GX_VA_TEX0, directional_tex0_u8, 2);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (mNumTexmap > 0) {
        GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_U8, 0);

    GXSetCurrentMtx(GX_PNMTX0);
}

DrawStrategyImpl::CalcAheadFunc
DrawDirectionalStrategy::GetCalcAheadFunc(ParticleManager* pManager) {
    switch (pManager->mResource->GetEmitterDesc()->typeDir) {
    case EmitterDrawSetting::AHEAD_BB_SPEED: {
        return CalcAhead_Speed;
    }

    case EmitterDrawSetting::AHEAD_BB_EMITTER_CENTER: {
        return CalcAhead_EmitterCenter;
    }

    case EmitterDrawSetting::AHEAD_BB_EMITTER_DESIGN: {
        return CalcAhead_EmitterDesign;
    }

    case EmitterDrawSetting::AHEAD_BB_PARTICLE: {
        return CalcAhead_Particle;
    }

    case EmitterDrawSetting::AHEAD_CMN_NODESIGN: {
        return CalcAhead_NoDesign;
    }

    default: {
        return CalcAhead_Speed;
    }
    }
}

} // namespace ef
} // namespace nw4r
