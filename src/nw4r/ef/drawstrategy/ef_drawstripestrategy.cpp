#include <nw4r/ef.h>
#include <nw4r/math.h>

#include <revolution/GX.h>

namespace nw4r {
namespace ef {

DrawStripeStrategy::DrawStripeStrategy() {}

// The original leaves every call in this function out of line.
#pragma dont_inline on
void DrawStripeStrategy::DrawStripe(StripeContext* pContext,
                                    const math::VEC3& rRight,
                                    const math::VEC3& rLeft) {

    ParticleManager* pManager = pContext->mCommon.mParticleManager;
    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();

    CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);

    int drawOrder = pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER;
    GetFirstDrawParticleFunc pGetFirstFunc =
        GetGetFirstDrawParticleFunc(drawOrder);
    GetNextDrawParticleFunc pGetNextFunc = GetGetNextDrawParticleFunc(drawOrder);

    int num = pManager->mActivityList.GetNumActiveObject();
    f32 pivot = 0.01f * pDesc->pivotX;

    math::VEC3 prevAxis = pContext->mCommon.mEmitterAxisY;

    bool loop = (pDesc->typeOption2 & 7) == EmitterDrawSetting::ASSIST_ST_CROSS;
    bool center =
        (pDesc->typeOption2 & 7) == EmitterDrawSetting::ASSIST_ST_BILLBOARD;

    if (loop || center) {
        num++;
    }

    bool texCoord = mNumTexmap > 0;

    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, num * 2);

    f32 step = 1.0f / (num - 1);

    int idx;
    int dir;

    switch (pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER) {
    case 0: {
        idx = 0;
        dir = 1;
        break;
    }

    case 1:
    default: {
        idx = num - 1;
        dir = -1;
        break;
    }
    }

    if (center && drawOrder == 0) {
        DispVertex(NULL, pContext, pCalcAheadFunc,
                   &pContext->mCommon.mEmitterCenter, &rRight, &rLeft, &prevAxis,
                   pivot, step * idx, texCoord);
        idx += dir;
    }

    for (Particle* pIt = pGetFirstFunc(pManager); pIt != NULL;
         pIt = pGetNextFunc(pManager, pIt), idx += dir) {

        DispVertex(pIt, pContext, pCalcAheadFunc, &pIt->mParameter.mPosition,
                   &rRight, &rLeft, &prevAxis, pivot, step * idx, texCoord);
    }

    if (loop) {
        Particle* pFirst = pGetFirstFunc(pManager);

        DispVertex(pFirst, pContext, pCalcAheadFunc,
                   &pFirst->mParameter.mPosition, &rRight, &rLeft, &prevAxis,
                   pivot, step * idx, texCoord);
    }

    if (center && drawOrder != 0) {
        DispVertex(NULL, pContext, pCalcAheadFunc,
                   &pContext->mCommon.mEmitterCenter, &rRight, &rLeft, &prevAxis,
                   pivot, step * idx, texCoord);
    }

    EndStripe();
}
#pragma dont_inline reset

// Builds the particle's stripe basis and updates its previous axis.
inline math::MTX34
DrawStripeStrategy::CalcDirMtx(Particle* pParticle, StripeContext* pContext,
                               const math::VEC3* pDir, const math::VEC3* pPos,
                               math::VEC3* pPrevAxis) {
    math::VEC3 axisX;
    math::VEC3 axisZ;

    math::VEC3Cross(&axisX, pDir, &pParticle->mPrevAxis);

    if (!Normalize(&axisX)) {
        axisX = pContext->mEmitterAxisX;
    }

    math::VEC3Cross(&axisZ, &axisX, pDir);
    Normalize(&axisZ);

    pParticle->mPrevAxis = axisZ;
    *pPrevAxis = axisZ;

    // clang-format off
    return math::MTX34(
        axisX.x, pDir->x, axisZ.x, pPos->x,
        axisX.y, pDir->y, axisZ.y, pPos->y,
        axisX.z, pDir->z, axisZ.z, pPos->z);
    // clang-format on
}

// Scale and Y rotation of the stripe's cross-section around its pivot.
inline math::MTX34 DrawStripeStrategy::CalcLocMtx(Particle* pParticle,
                                                  f32 pivot) {
    math::VEC3 rot;
    pParticle->Draw_GetRotate(&rot);

    f32 sx = pParticle->Draw_GetSizeX();

    f32 c = sx * math::CosRad(rot.y);
    f32 s = sx * math::SinRad(rot.y);

    // clang-format off
    return math::MTX34(
           c, 0.0f,   -s, pivot - c * pivot,
        0.0f, 1.0f, 0.0f, 0.0f,
           s, 0.0f,    c, -s * pivot);
    // clang-format on
}

inline void DrawStripeStrategy::DispVertex(
    Particle* pParticle, StripeContext* pContext, CalcAheadFunc pCalcAheadFunc,
    const math::VEC3* pPos, const math::VEC3* pRight, const math::VEC3* pLeft,
    math::VEC3* pPrevAxis, f32 pivot, f32 texV, bool texCoord) {

    if (pParticle == NULL) {
        pParticle = GetYoungestDrawParticle(pContext->mCommon.mParticleManager);
    }

    math::MTX34 drawMtx;
    math::VEC3 dir;
    math::VEC3 p0;
    math::VEC3 p1;

    pCalcAheadFunc(&dir, pContext, pParticle);

    math::MTX34 dirMtx = CalcDirMtx(pParticle, pContext, &dir, pPos, pPrevAxis);
    math::MTX34 locMtx = CalcLocMtx(pParticle, pivot);

    math::MTX34Mult(&drawMtx, &dirMtx, &locMtx);

    math::VEC3Transform(&p0, &drawMtx, pRight);
    math::VEC3Transform(&p1, &drawMtx, pLeft);

    GXPosition3f32(p0.x, p0.y, p0.z);
    if (texCoord) {
        GXTexCoord2f32(1.0f, texV);
    }

    GXPosition3f32(p1.x, p1.y, p1.z);
    if (texCoord) {
        GXTexCoord2f32(0.0f, texV);
    }
}

inline void DrawStripeStrategy::EndStripe() {
    GXEnd();
}

void DrawStripeStrategy::Draw(const DrawInfo& rInfo, ParticleManager* pManager) {
    int num = GetNumDrawParticle(pManager);

    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();
    const EmitterDrawSetting& rSetting = pDesc->drawSetting;

    if (num == 0) {
        return;
    }

    StripeContext context(*rInfo.GetViewMtx(), pManager);

    math::MTX34 posMtx;


    // Particles that have not been drawn yet start from the emitter's Y axis
    for (Particle* pIt = GetYoungestParticle(pManager); pIt != NULL;
         pIt = GetElderParticle(pManager, pIt)) {

        if (pIt->mPrevAxis.x <= 1.0f) {
            break;
        }

        pIt->mPrevAxis = context.mCommon.mEmitterAxisY;
    }

    int type = pDesc->typeOption2 & 7;

    if ((type == EmitterDrawSetting::ASSIST_ST_CROSS && num < 3) ||
        (type == EmitterDrawSetting::ASSIST_ST_NORMAL && num < 2)) {

        // Too few particles to draw: only keep the particle axes up to date
        math::_VEC3 prevAxis = context.mCommon.mEmitterAxisY;

        int drawOrder =
            pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER;

        CalcAheadFunc pCalcAheadFunc = GetCalcAheadFunc(pManager);
        GetFirstDrawParticleFunc pGetFirstFunc =
            GetGetFirstDrawParticleFunc(drawOrder);
        GetNextDrawParticleFunc pGetNextFunc =
            GetGetNextDrawParticleFunc(drawOrder);

        for (Particle* pIt = pGetFirstFunc(pManager); pIt != NULL;
             pIt = pGetNextFunc(pManager, pIt)) {

            math::VEC3 dir;
            pCalcAheadFunc(&dir, &context, pIt);

            math::MTX34 dirMtx =
                CalcDirMtx(pIt, &context, &dir, &pIt->mParameter.mPosition,
                           static_cast<math::VEC3*>(&prevAxis));
        }

        return;
    }

    InitGraphics(rInfo, pManager);

    math::MTX34Mult(&posMtx, rInfo.GetViewMtx(),
                    &context.mCommon.mParticleManagerMtx);
    GXLoadPosMtxImm(posMtx, GX_PNMTX0);

    Particle* pFirst = GetGetFirstDrawParticleFunc(
        pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_DRAW_ORDER)(pManager);
    SetupGP(pFirst, rSetting, rInfo, true, false);

    DrawStripe(&context, math::VEC3(1.0f, 0.0f, 0.0f),
               math::VEC3(-1.0f, 0.0f, 0.0f));

    if (pDesc->typeOption == EmitterDrawSetting::TYPE_CMN_CROSS) {
        DrawStripe(&context, math::VEC3(0.0f, 0.0f, -1.0f),
                   math::VEC3(0.0f, 0.0f, 1.0f));
    }
}

void DrawStripeStrategy::InitGraphics(const DrawInfo& rInfo,
                                      ParticleManager* pManager) {

    const EmitterDrawSetting& rSetting =
        *pManager->mResource->GetEmitterDrawSetting();

    InitTexture(rSetting);
    InitTev(rSetting, rInfo);
    InitColor(pManager, rSetting, rInfo);

    GXEnableTexOffsets(GX_TEXCOORD0, TRUE, TRUE);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);

    if (mNumTexmap > 0) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    }

    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GXSetCurrentMtx(GX_PNMTX0);
}

DrawStrategyImpl::CalcAheadFunc
DrawStripeStrategy::GetCalcAheadFunc(ParticleManager* pManager) {
    EmitterDesc* pDesc = pManager->mResource->GetEmitterDesc();

    switch (pDesc->typeDir) {
    case 0: {
        return CalcAhead_Speed;
    }

    case 1: {
        return CalcAhead_EmitterCenter;
    }

    case 2: {
        return CalcAhead_EmitterDesign;
    }

    case 3: {
        return CalcAhead_Younger;
    }

    case 5: {
        return CalcAhead_NoDesign;
    }

    case 6: {
        switch (pDesc->typeOption2 & 7) {
        case EmitterDrawSetting::ASSIST_ST_CROSS: {
            return CalcAhead_BothLoop;
        }

        case EmitterDrawSetting::ASSIST_ST_BILLBOARD: {
            return CalcAhead_BothCenter;
        }

        default: {
            return CalcAhead_Both;
        }
        }
    }

    default: {
        return CalcAhead_Speed;
    }
    }
}

void DrawStripeStrategy::CalcAhead_Younger(math::VEC3* pAxisY,
                                           AheadContext* pContext,
                                           Particle* pParticle) {

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    if (pYounger != NULL) {
        math::VEC3Sub(pAxisY, &pParticle->mParameter.mPosition,
                      &pYounger->mParameter.mPosition);
    } else {
        math::VEC3Sub(pAxisY, &pParticle->mParameter.mPosition,
                      &pContext->mCommon.mEmitterCenter);
    }

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_Both(math::VEC3* pAxisY,
                                        AheadContext* pContext,
                                        Particle* pParticle) {

    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    math::VEC3 elderPos(0.0f, 0.0f, 0.0f);

    if (pElder != NULL) {
        math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);

        if (!Normalize(&elderPos)) {
            elderPos = math::VEC3(0.0f, 0.0f, 0.0f);
        }
    }

    math::VEC3 youngerPos(0.0f, 0.0f, 0.0f);

    if (pYounger != NULL) {
        math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);

        if (!Normalize(&youngerPos)) {
            youngerPos = math::VEC3(0.0f, 0.0f, 0.0f);
        }
    }

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_BothLoop(math::VEC3* pAxisY,
                                            AheadContext* pContext,
                                            Particle* pParticle) {

    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);
    if (pElder == NULL) {
        pElder = GetYoungestDrawParticle(pContext->mCommon.mParticleManager);
    }

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);
    if (pYounger == NULL) {
        pYounger = GetOldestDrawParticle(pContext->mCommon.mParticleManager);
    }

    math::VEC3 elderPos;
    math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                  &pParticle->mParameter.mPosition);

    if (!Normalize(&elderPos)) {
        elderPos = math::VEC3(0.0f, 0.0f, 0.0f);
    }

    math::VEC3 youngerPos;
    math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                  &pParticle->mParameter.mPosition);

    if (!Normalize(&youngerPos)) {
        youngerPos = math::VEC3(0.0f, 0.0f, 0.0f);
    }

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

void DrawStripeStrategy::CalcAhead_BothCenter(math::VEC3* pAxisY,
                                              AheadContext* pContext,
                                              Particle* pParticle) {

    Particle* pElder =
        GetElderDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    Particle* pYounger =
        GetYoungerDrawParticle(pContext->mCommon.mParticleManager, pParticle);

    math::VEC3 elderPos(0.0f, 0.0f, 0.0f);

    if (pElder != NULL) {
        math::VEC3Sub(&elderPos, &pElder->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);
        Normalize(&elderPos);
    }

    math::VEC3 youngerPos;

    if (pYounger != NULL) {
        math::VEC3Sub(&youngerPos, &pYounger->mParameter.mPosition,
                      &pParticle->mParameter.mPosition);
    } else {
        math::VEC3Sub(&youngerPos, &pContext->mCommon.mEmitterCenter,
                      &pParticle->mParameter.mPosition);
    }

    Normalize(&youngerPos);

    math::VEC3Sub(pAxisY, &elderPos, &youngerPos);

    if (!Normalize(pAxisY)) {
        *pAxisY = pContext->mCommon.mEmitterAxisY;
    }
}

} // namespace ef
} // namespace nw4r
