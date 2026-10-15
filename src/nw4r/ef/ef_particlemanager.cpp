#include <nw4r/ef.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>

#include <cmath>
#include <cstring>

namespace nw4r {
namespace ef {

math::MTX34 ParticleManager::smDrawMtxPMtoEM;

ParticleManager::ParticleManager()
    : mActivityList(offsetof(Particle, mActivityLink)) {}

void ParticleManager::SendClosing() {
    mManagerEM->Closing(this);
}

void ParticleManager::DestroyFunc() {}

bool ParticleManager::Closing(Particle* pParticle) {
    pParticle->mParticleManager->UnRef();

    mActivityList.ToClosing(pParticle);
    pParticle->mLifeStatus = NW4R_EF_LS_CLOSING;

    return true;
}

// Only used inline in this library version
inline int ParticleManager::RetireParticle(Particle* pParticle) {
    if (pParticle->mLifeStatus != NW4R_EF_LS_ACTIVE) {
        return 0;
    }

    mActivityList.ToWait(pParticle);
    pParticle->Destroy();
    return 1;
}

int ParticleManager::RetireParticleAll() {
    int num = 0;

    Particle* pIt;
    Particle* pNext;

    for (pIt = static_cast<Particle*>(mActivityList.mActiveList.headObject);
         pIt != NULL; pIt = pNext) {

        pNext = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject);

        if (pIt->mLifeStatus == NW4R_EF_LS_ACTIVE) {
            num += RetireParticle(pIt);
        }
    }

    return num;
}

bool ParticleManager::Initialize(Emitter* pParent, EmitterResource* pResource) {
    ReferencedObject::Initialize();

    mActivityList.Initialize();

    mModifier.mScale.x = 1.0f;
    mModifier.mScale.y = 1.0f;

    mModifier.mRotate.x = 0.0f;
    mModifier.mRotate.y = 0.0f;
    mModifier.mRotate.z = 0.0f;

    mManagerEM = pParent;
    pParent->Ref();

    mResource = pResource;
    mFlag = 0;
    SetMtxDirty();

    EmitterDesc* pDesc = mResource->GetEmitterDesc();

    mDrawStrategy =
        DrawStrategyBuilder::Create(
            pDesc->ptcltype);

    mLastCalced = NULL;

    Modifier_SetSimpleLightParameter(pDesc->drawSetting);

    return true;
}

ParticleManager::~ParticleManager() {}

Particle*
ParticleManager::CreateParticle(u16 life, math::VEC3 pos, math::VEC3 vel,
                                const math::MTX34* pSpace, f32 momentum,
                                const EmitterInheritSetting* pSetting,
                                Particle* pReferencePtcl, u16 calcRemain) {

    Particle* pParticle =
        mManagerEM->mManagerEF->mManagerES->GetMemoryManager()->AllocParticle();

    if (pParticle == NULL) {
        return NULL;
    }

    if (!pParticle->Initialize(life, pos, vel, this, pSpace, momentum, pSetting,
                               pReferencePtcl)) {
        return NULL;
    }

    pParticle->mCalcRemain += calcRemain;
    mActivityList.ToActive(pParticle);
    pParticle->mLifeStatus = NW4R_EF_LS_ACTIVE;

    return pParticle;
}

// Field parameters follow the curve's tables (this library version).
struct FieldGravityInfo {
    f32 power;          // at 0x0
    math::VEC3 dir;     // at 0x4
    u8 useEmitterSpace; // at 0x10
    u8 target;          // at 0x11
};

struct FieldRandomInfo {
    f32 power;          // at 0x0
    f32 diffusion;      // at 0x4
    u16 interval;       // at 0x8
    u8 flags;           // at 0xA
    u8 target;          // at 0xB
};

struct FieldSpinParam {
    f32 speed;          // at 0x0
    math::VEC3 rotate;  // at 0x4
    u8 useEmitterSpace; // at 0x10
    u8 target;          // at 0x11
    u8 PADDING_0x12;    // at 0x12
    u8 PADDING_0x13;    // at 0x13
};

struct FieldMagnetParam {
    f32 power;          // at 0x0
    math::VEC3 pos;     // at 0x4
    u8 useEmitterSpace; // at 0x10
    u8 target;          // at 0x11
    u8 PADDING_0x12;    // at 0x12
    u8 PADDING_0x13;    // at 0x13
};

struct FieldNewtonParam {
    f32 power;          // at 0x0
    f32 refDistance;    // at 0x4
    math::VEC3 pos;     // at 0x8
    u8 useEmitterSpace; // at 0x14
    u8 target;          // at 0x15
    u8 PADDING_0x16;    // at 0x16
    u8 PADDING_0x17;    // at 0x17
};

struct FieldVortexParam {
    f32 innerSpeed;     // at 0x0
    f32 outerSpeed;     // at 0x4
    f32 radius;         // at 0x8
    math::VEC3 rotate;  // at 0xC
    u8 useEmitterSpace; // at 0x18
    u8 target;          // at 0x19
    u8 PADDING_0x1A;    // at 0x1A
    u8 PADDING_0x1B;    // at 0x1B
};

enum FieldTarget { FIELD_TARGET_VELOCITY, FIELD_TARGET_POSITION };

enum FieldRandomFlag { FIELD_RANDOM_FLAG_CUBE = (1 << 1) };

static inline u8* GetFieldInfo(AnimCurveHeader* pHeader) {
    return reinterpret_cast<u8*>(pHeader) + sizeof(AnimCurveHeader) +
           pHeader->keyTable + pHeader->rangeTable + pHeader->randomTable +
           pHeader->nameTable;
}

static inline u32 MixRandom(u32 rnd) {
    return rnd * 0x343FD + 0x269EC3;
}

void ParticleManager::Calc() {
    Particle* pIt = static_cast<Particle*>(
        ut::List_GetNext(&mActivityList.mActiveList, mLastCalced));

    if (pIt == NULL) {
        return;
    }

    math::MTX34 mtxLocToGlb;
    CalcGlobalMtx(&mtxLocToGlb);

    math::MTX34 mtxGlbToLoc;
    math::MTX34Inv(&mtxGlbToLoc, &mtxLocToGlb);

    math::MTX34 mtxEmToGlb;
    mManagerEM->CalcGlobalMtx(&mtxEmToGlb);

    math::MTX34 mtxEmToLoc;
    math::MTX34Mult(&mtxEmToLoc, &mtxGlbToLoc, &mtxEmToGlb);

    math::MTX34 mtxLocToEm;
    math::MTX34Inv(&mtxLocToEm, &mtxEmToLoc);

    math::MTX34 mtxGlbToLocNoTrans = mtxGlbToLoc;
    mtxGlbToLocNoTrans._03 = 0.0f;
    mtxGlbToLocNoTrans._13 = 0.0f;
    mtxGlbToLocNoTrans._23 = 0.0f;

    math::MTX34 mtxLocToEmNoTrans = mtxLocToEm;
    mtxLocToEmNoTrans._03 = 0.0f;
    mtxLocToEmNoTrans._13 = 0.0f;
    mtxLocToEmNoTrans._23 = 0.0f;

    math::MTX34 mtxEmToLocNoTrans = mtxEmToLoc;
    mtxEmToLocNoTrans._03 = 0.0f;
    mtxEmToLocNoTrans._13 = 0.0f;
    mtxEmToLocNoTrans._23 = 0.0f;

    Particle* pNext;

    for (; pIt != NULL; pIt = pNext) {
        pNext = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject);

        if (pIt->mLifeStatus != NW4R_EF_LS_ACTIVE) {
            continue;
        }

        if (pIt->mEvalStatus != NW4R_EF_ES_WAIT) {
            continue;
        }

        pIt->mEvalStatus = NW4R_EF_ES_DONE;

        if (pIt->mCalcRemain != 0) {
            mManagerEM->mManagerEF->SetFlagExistCalcRemain(true);
        }

        math::VEC3 prevPos = pIt->mParameter.mPosition;
        math::VEC3 prevVel = pIt->mParameter.mVelocity;

        math::VEC3 prevDir;
        pIt->GetMoveDir(&prevDir);

        pIt->mParameter.mPrevPosition = pIt->mParameter.mPosition;

        if (pIt->mLife <= pIt->mTick) {
            RetireParticle(pIt);
            continue;
        }

        math::VEC3 addVel(0.0f, 0.0f, 0.0f);
        math::VEC3 addPos(0.0f, 0.0f, 0.0f);

        for (u16 i = pIt->mTick == 0 ? 0 : mResource->NumPtclInitTrack();
             i < mResource->NumPtclTrack(); i++) {

            u8* pPtclTrack = mResource->GetPtclTrack(i);

#define pTrackAsHeader reinterpret_cast<AnimCurveHeader*>(pPtclTrack)

            if (pTrackAsHeader->processFlag & AnimCurveHeader::PROC_FLAG_STOP) {
                continue;
            }

            if (pIt->mTick != 0 && pTrackAsHeader->frameLength <= 1 &&
                pTrackAsHeader->kindType < AC_TARGET_TEXTUREINDTRANSLATE + 1) {
                continue;
            }

            u32 tick;
            u32 life;
            u16 seed;
            f32 speed; // Vortex field

            u32 timing =
                pTrackAsHeader->processFlag & AnimCurveHeader::PROC_FLAG_TIMING;

            if (timing) {
                tick = pIt->mParticleManager->mManagerEM->mTick;

                if (pIt->mParticleManager->mManagerEM->mParameter.mComFlags &
                    EmitterDesc::CMN_FLAG_MAX_LIFE) {
                    life = 0xFFFFFFFF;
                } else {
                    life =
                        pIt->mParticleManager->mManagerEM->mParameter.mEmitSpan;
                }

                seed = pIt->mParticleManager->mManagerEM->mRandSeed;
            } else {
                tick = pIt->mTick;
                life = pIt->mLife;
                seed = pIt->mRandSeed;
            }

            if (pTrackAsHeader->magic != NW4R_EF_MAGIC_ANIMCURVE) {
                continue;
            }

            // Load curveFlag and kindEnable
            u16 ctrl = *reinterpret_cast<u16*>(&pTrackAsHeader->curveFlag);
            u8 kind = pTrackAsHeader->kindType;

            switch (ctrl) {
            case (AC_TYPE_PARTICLE_U8 << 8 | 0b001): {
                u8* pTarget;

                switch (kind) {
                case AC_TARGET_ALPHA0PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_PRI].a;
                    break;
                }
                case AC_TARGET_ALPHA0SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_SEC].a;
                    break;
                }
                case AC_TARGET_ALPHA1PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_PRI].a;
                    break;
                }
                case AC_TARGET_ALPHA1SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_SEC].a;
                    break;
                }
                case AC_TARGET_ACMPREF0: {
                    pTarget = &pIt->mParameter.mACmpRef0;
                    break;
                }
                case AC_TARGET_ACMPREF1: {
                    pTarget = &pIt->mParameter.mACmpRef1;
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteU8(pPtclTrack, pTarget, tick, seed, life);
                break;
            }

            case (AC_TYPE_PARTICLE_U8 << 8 | 0b111): {
                u8* pTarget;

                switch (kind) {
                case AC_TARGET_COLOR0PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_PRI].r;
                    break;
                }
                case AC_TARGET_COLOR0SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_0][COLOR_IDX_SEC].r;
                    break;
                }
                case AC_TARGET_COLOR1PRI: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_PRI].r;
                    break;
                }
                case AC_TARGET_COLOR1SEC: {
                    pTarget =
                        &pIt->mParameter.mColor[COLOR_LAYER_1][COLOR_IDX_SEC].r;
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteU8Vec3(pPtclTrack, pTarget, tick, seed, life);
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b001): {
                switch (kind) {
                case AC_TARGET_TEXTURE1ROTATE: {
                    AnimCurveExecuteF32(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_1], tick,
                        seed, life);
                    break;
                }

                case AC_TARGET_TEXTURE2ROTATE: {
                    AnimCurveExecuteF32(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_2], tick,
                        seed, life);
                    break;
                }

                case AC_TARGET_TEXTUREINDROTATE: {
                    AnimCurveExecuteF32(
                        pPtclTrack, NULL,
                        &pIt->mParameter.mTextureRotate[TEX_LAYER_IND], tick,
                        seed, life);
                    break;
                }

                case AC_TARGET_FIELD_SPEED: {
                    f32 speed;
                    AnimCurveExecuteF32(pPtclTrack, NULL, &speed, tick, seed,
                                        life);

                    math::VEC3 speedVel;
                    math::VEC3Scale(&speedVel, &prevVel, speed - 1.0f);
                    addVel += speedVel;
                    break;
                }

                case AC_TARGET_FIELD_GRAVITY: {
                    FieldGravityInfo* pInfo =
                        reinterpret_cast<FieldGravityInfo*>(
                            GetFieldInfo(pTrackAsHeader));

                    f32 power = pInfo->power;
                    AnimCurveExecuteF32(pPtclTrack, NULL, &power, tick, seed,
                                        life);

                    math::VEC3 gravity;
                    gravity.x = pInfo->dir.x * power;
                    gravity.y = pInfo->dir.y * power;
                    gravity.z = pInfo->dir.z * power;

                    if (pInfo->useEmitterSpace) {
                        math::VEC3Transform(&gravity, &mtxEmToLocNoTrans,
                                            &gravity);
                    } else {
                        math::VEC3Transform(&gravity, &mtxGlbToLocNoTrans,
                                            &gravity);
                    }

                    switch (pInfo->target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += gravity;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += gravity;
                        break;
                    }
                    }
                    break;
                }

                case AC_TARGET_FIELD_RANDOM: {
                    FieldRandomInfo* pInfo = reinterpret_cast<FieldRandomInfo*>(
                        GetFieldInfo(pTrackAsHeader));

                    f32 power;
                    union {
                        u32 value;
                        u8 bytes[4];
                    } rndSeed;

                    rndSeed.value = seed * 0x3F81F635 +
                                    pTrackAsHeader->randomSeed * 0x30A74193 +
                                    static_cast<u16>(tick) * 0x371097E7 +
                                    0x4BF53;

                    rndSeed.bytes[2] ^= rndSeed.bytes[3];
                    rndSeed.bytes[1] ^= rndSeed.bytes[2];
                    rndSeed.bytes[0] ^= rndSeed.bytes[1];

                    if (!timing) {
                        if (tick == 0) {
                            break;
                        }

                        if (tick % (pInfo->interval + 1) != 0) {
                            break;
                        }
                    } else {
                        if (pIt->mTick != 0 &&
                            tick % (pInfo->interval + 1) != 0) {
                            break;
                        }
                    }

                    power = pInfo->power;
                    AnimCurveExecuteF32(pPtclTrack, NULL, &power, tick, seed,
                                        life);

                    math::VEC3 rnd;

                    if (pInfo->flags & FIELD_RANDOM_FLAG_CUBE) {
                        u32 random = rndSeed.value;
                        rnd.x =
                            static_cast<s16>(random >> 16) / 32768.0f * power;
                        random = MixRandom(random);
                        rnd.y =
                            static_cast<s16>(random >> 16) / 32768.0f * power;
                        random = MixRandom(random);
                        rnd.z =
                            static_cast<s16>(random >> 16) / 32768.0f * power;
                    } else {
                        math::VEC3 dir;

                        if (pIt->mTick == 0) {
                            dir = prevVel;
                        } else {
                            dir = prevDir;
                        }

                        math::VEC3Transform(&dir, &mtxLocToEmNoTrans, &dir);

                        if (math::VEC3LenSq(&dir) < NW4R_MATH_FLT_MIN) {
                            dir.y = 1.0f;
                        } else {
                            math::VEC3Normalize(&dir, &dir);
                        }

                        math::MTX34 dirMtx;
                        GetDirMtxY(&dirMtx, dir);

                        if (pInfo->diffusion != 0.0f) {
                            u32 random = rndSeed.value;

                            f32 theta =
                                (random >> 16) / 65535.0f * pInfo->diffusion;
                            random = MixRandom(random);
                            f32 phi = 2.0f * (NW4R_MATH_PI *
                                              ((random >> 16) / 65535.0f));

                            rnd.x = std::sinf(theta) * std::sinf(phi);
                            rnd.y = std::cosf(theta);
                            rnd.z = std::sinf(theta) * std::cosf(phi);

                            random = MixRandom(random);
                            rnd *= (random >> 16) / 65535.0f * power;
                            math::VEC3Transform(&rnd, &dirMtx, &rnd);
                        } else {
                            u32 random = rndSeed.value;

                            rnd.x = 0.0f;
                            rnd.y =
                            static_cast<s16>(random >> 16) / 32768.0f * power;
                            rnd.z = 0.0f;
                            math::VEC3Transform(&rnd, &dirMtx, &rnd);
                        }
                    }

                    math::VEC3Transform(&rnd, &mtxEmToLocNoTrans, &rnd);

                    switch (pInfo->target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += rnd;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += rnd;
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b011): {
                f32* pTarget;

                switch (kind) {
                case AC_TARGET_SIZE: {
                    pTarget = reinterpret_cast<f32*>(&pIt->mParameter.mSize);
                    break;
                }
                case AC_TARGET_SCALE: {
                    pTarget = reinterpret_cast<f32*>(&pIt->mParameter.mScale);
                    break;
                }
                case AC_TARGET_TEXTURE1SCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_1]);
                    break;
                }
                case AC_TARGET_TEXTURE2SCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_2]);
                    break;
                }
                case AC_TARGET_TEXTUREINDSCALE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureScale[TEX_LAYER_IND]);
                    break;
                }
                case AC_TARGET_TEXTURE1TRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_1]);
                    break;
                }
                case AC_TARGET_TEXTURE2TRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_2]);
                    break;
                }
                case AC_TARGET_TEXTUREINDTRANSLATE: {
                    pTarget = reinterpret_cast<f32*>(
                        &pIt->mParameter.mTextureTranslate[TEX_LAYER_IND]);
                    break;
                }
                default: {
                    continue;
                }
                }

                AnimCurveExecuteF32Vec2(pPtclTrack, NULL, pTarget, tick, seed,
                                        life);
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b1111): {
                switch (kind) {
                case AC_TARGET_FIELD_SPIN: {
                    FieldSpinParam param = *reinterpret_cast<FieldSpinParam*>(
                        GetFieldInfo(pTrackAsHeader));

                    AnimCurveExecuteF32N(pPtclTrack, &param.speed, tick, seed,
                                         life, static_cast<u8>(ctrl));

                    math::VEC3 axis;
                    Rotation2VecY(param.rotate, &axis);

                    math::MTX34 rotMtx;
                    math::MTX34RotAxisRad(&rotMtx, &axis, param.speed);

                    math::VEC3 pos;
                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);
                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 spin;
                    math::VEC3Transform(&spin, &rotMtx, &pos);
                    math::VEC3Sub(&spin, &spin, &pos);

                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&spin, &mtxEmToLocNoTrans, &spin);
                    } else {
                        math::VEC3Transform(&spin, &mtxGlbToLocNoTrans, &spin);
                    }

                    switch (param.target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += spin;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += spin;
                        break;
                    }
                    }
                    break;
                }

                case AC_TARGET_FIELD_MAGNET: {
                    FieldMagnetParam param =
                        *reinterpret_cast<FieldMagnetParam*>(
                            GetFieldInfo(pTrackAsHeader));

                    AnimCurveExecuteF32N(pPtclTrack, &param.power, tick, seed,
                                         life, static_cast<u8>(ctrl));

                    math::VEC3 pos;
                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);
                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 magnet;
                    math::VEC3Sub(&magnet, &param.pos, &pos);

                    if (magnet.x != 0.0f || magnet.y != 0.0f ||
                        magnet.z != 0.0f) {
                        math::VEC3Normalize(&magnet, &magnet);
                    }

                    math::VEC3Scale(&magnet, &magnet, param.power);

                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&magnet, &mtxEmToLocNoTrans,
                                            &magnet);
                    } else {
                        math::VEC3Transform(&magnet, &mtxGlbToLocNoTrans,
                                            &magnet);
                    }

                    switch (param.target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += magnet;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += magnet;
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b11111): {
                switch (kind) {
                case AC_TARGET_FIELD_NEWTON: {
                    FieldNewtonParam param =
                        *reinterpret_cast<FieldNewtonParam*>(
                            GetFieldInfo(pTrackAsHeader));

                    AnimCurveExecuteF32N(pPtclTrack, &param.power, tick, seed,
                                         life, static_cast<u8>(ctrl));

                    math::VEC3 pos;
                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);
                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    math::VEC3 newton;
                    math::VEC3Sub(&newton, &param.pos, &pos);

                    f32 distSq = math::VEC3LenSq(&newton);

                    if (newton.x != 0.0f || newton.y != 0.0f ||
                        newton.z != 0.0f) {
                        math::VEC3Normalize(&newton, &newton);
                    }

                    math::VEC3Scale(&newton, &newton, param.power);

                    f32 refSq = param.refDistance * param.refDistance;
                    if (distSq > refSq) {
                        math::VEC3Scale(&newton, &newton, refSq / distSq);
                    }

                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&newton, &mtxEmToLocNoTrans,
                                            &newton);
                    } else {
                        math::VEC3Transform(&newton, &mtxGlbToLocNoTrans,
                                            &newton);
                    }

                    switch (param.target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += newton;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += newton;
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_F32 << 8 | 0b111111): {
                switch (kind) {
                case AC_TARGET_FIELD_VORTEX: {
                    FieldVortexParam param =
                        *reinterpret_cast<FieldVortexParam*>(
                            GetFieldInfo(pTrackAsHeader));

                    AnimCurveExecuteF32N(pPtclTrack, &param.innerSpeed, tick,
                                         seed, life, static_cast<u8>(ctrl));

                    math::VEC3 axis;
                    Rotation2VecY(param.rotate, &axis);

                    math::VEC3 pos;
                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&pos, &mtxLocToEm, &prevPos);
                    } else {
                        math::VEC3Transform(&pos, &mtxLocToGlb, &prevPos);
                        math::VEC3 origin(0.0f, 0.0f, 0.0f);
                        math::VEC3Transform(&origin, &mtxEmToGlb, &origin);
                        math::VEC3Sub(&pos, &pos, &origin);
                    }

                    f32 dot = math::VEC3Dot(&axis, &pos);

                    param.radius *= param.radius;
                    f32 invRadiusSq = 1.0f / param.radius;

                    math::VEC3 radial;
                    math::VEC3Scale(&radial, &axis, dot);
                    math::VEC3Sub(&radial, &pos, &radial);

                    f32 distSq = math::VEC3LenSq(&radial);
                    if (distSq == 0.0f) {
                        break;
                    }

                    if (distSq >= param.radius) {
                        speed = param.outerSpeed;
                    } else {
                        f32 rate = distSq * invRadiusSq;
                        speed = (1.0f - rate) * param.innerSpeed +
                                rate * param.outerSpeed;
                    }

                    math::VEC3Normalize(&radial, &radial);

                    math::VEC3 vortex;
                    math::VEC3Cross(&vortex, &radial, &axis);
                    math::VEC3Scale(&vortex, &vortex, speed);

                    if (param.useEmitterSpace) {
                        math::VEC3Transform(&vortex, &mtxEmToLocNoTrans,
                                            &vortex);
                    } else {
                        math::VEC3Transform(&vortex, &mtxGlbToLocNoTrans,
                                            &vortex);
                    }

                    switch (param.target) {
                    case FIELD_TARGET_VELOCITY: {
                        addVel += vortex;
                        break;
                    }
                    case FIELD_TARGET_POSITION: {
                        addPos += vortex;
                        break;
                    }
                    }
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_ROTATE << 8 | 0b111): {
                switch (kind) {
                case AC_TARGET_ROTATE: {
                    AnimCurveExecuteRotate(pPtclTrack,
                                           &pIt->mParameter.mRotate.x, tick,
                                           seed, life);
                    break;
                }
                }
                break;
            }

            case (AC_TYPE_PARTICLE_TEXTURE << 8 | 0b001): {
                AnimCurveExecuteTexture(pPtclTrack, pIt, tick, seed, life);
                break;
            }

            case (AC_TYPE_CHILD << 8 | 0b001): {
                AnimCurveExecuteChild(pPtclTrack, pIt, tick, seed, life);
                break;
            }
            }

#undef pTrackAsHeader
        }

        pIt->mTick++;

        pIt->mParameter.mVelocity += addVel;

        ParticleParameter& rParam = pIt->mParameter;
        rParam.mPosition.x += addPos.x * rParam.mMomentum;
        rParam.mPosition.y += addPos.y * rParam.mMomentum;
        rParam.mPosition.z += addPos.z * rParam.mMomentum;
        rParam.mPosition.x += rParam.mVelocity.x * rParam.mMomentum;
        rParam.mPosition.y += rParam.mVelocity.y * rParam.mMomentum;
        rParam.mPosition.z += rParam.mVelocity.z * rParam.mMomentum;
    }

    mLastCalced =
        static_cast<Particle*>(ut::List_GetLast(&mActivityList.mActiveList));
}

void ParticleManager::Draw(const DrawInfo& rInfo) {
    const EmitterDesc* pDesc = mResource->GetEmitterDesc();

    if ((pDesc->drawSetting.mFlags & EmitterDrawSetting::FLAG_HIDDEN) ||
        (mManagerEM->mParameter.mComFlags &
         EmitterDesc::CMN_FLAG_DISABLE_DRAW)) {
        return;
    }

    mDrawStrategy->Draw(rInfo, this);
}

math::MTX34* ParticleManager::CalcGlobalMtx(math::MTX34* pResult) {
    if (mMtxDirty) {
        math::MTX34 orig;
        mManagerEM->CalcGlobalMtx(&orig);

        mManagerEM->RestructMatrix(&mMtx, &orig, mFlag & FLAG_MTX_INHERIT_SCALE,
                                   mFlag & FLAG_MTX_INHERIT_ROT,
                                   mInheritTranslate);

        mMtxDirty = false;
    }

    *pResult = mMtx;
    return pResult;
}

void ParticleManager::BeginCalc(bool onlyIfRemain) {
    mLastCalced = NULL;

    Particle* pIt =
        static_cast<Particle*>(mActivityList.mActiveList.headObject);

    // clang-format off
    for (; pIt != NULL; pIt = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject))
    // clang-format on
    {
        if (!onlyIfRemain || pIt->mCalcRemain != 0) {
            if (pIt->mCalcRemain != 0) {
                pIt->mCalcRemain--;
            }

            if (pIt->GetLifeStatus() == NW4R_EF_LS_ACTIVE &&
                pIt->mEvalStatus == NW4R_EF_ES_DONE) {

                pIt->mEvalStatus = NW4R_EF_ES_WAIT;
            }
        }
    }
}

void ParticleManager::EndCalc() {
    Particle* pIt =
        static_cast<Particle*>(mActivityList.mActiveList.headObject);

    // clang-format off
    for (; pIt != NULL; pIt = static_cast<Particle*>(
            NW4R_UT_LIST_GET_LINK(mActivityList.mActiveList, pIt)->nextObject))
    // clang-format on
    {
        if (pIt->GetLifeStatus() == NW4R_EF_LS_ACTIVE &&
            pIt->mEvalStatus == NW4R_EF_ES_SKIP) {

            pIt->mEvalStatus = NW4R_EF_ES_DONE;
        }
    }
}

void ParticleManager::BeginDraw() {
    math::MTX34 emMtx;
    math::MTX34 pmMtx;

    mManagerEM->CalcGlobalMtx(&emMtx);
    CalcGlobalMtx(&pmMtx);

    math::MTX34Inv(&emMtx, &emMtx);
    math::MTX34Mult(&smDrawMtxPMtoEM, &emMtx, &pmMtx);
}

void ParticleManager::EndDraw() {}

void ParticleManager::Draw_ModifyColor(Particle* pParticle, GXColor* pColorPri,
                                       GXColor* pColorSec) {
    switch (mModifier.mLight.mType) {
    case ParticleModifier::SIMPLELIGHT_AMBIENT: {
        pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
        pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
        pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
        pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

        pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
        pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
        pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
        pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
        break;
    }

    case ParticleModifier::SIMPLELIGHT_DIFFUSE: {
        if (mModifier.mLight.mRadius < NW4R_MATH_FLT_EPSILON) {
            // clang-format off
            pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
            pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
            pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
            pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

            pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
            pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
            pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
            pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
            // clang-format on
        } else {
            const math::MTX34* pMtxPMtoEM = Draw_GetMtxPMtoEM();

            math::VEC3 pos;
            math::VEC3Transform(&pos, pMtxPMtoEM,
                                &pParticle->mParameter.mPosition);

            math::VEC3Sub(&pos, &pos, &mModifier.mLight.mPosition);
            f32 dist = math::VEC3Len(&pos);

            if (dist > mModifier.mLight.mRadius) {
                // clang-format off
                pColorPri->r = (pColorPri->r * mModifier.mLight.mAmbient.r + 128) >> 8;
                pColorPri->g = (pColorPri->g * mModifier.mLight.mAmbient.g + 128) >> 8;
                pColorPri->b = (pColorPri->b * mModifier.mLight.mAmbient.b + 128) >> 8;
                pColorPri->a = (pColorPri->a * mModifier.mLight.mAmbient.a + 128) >> 8;

                pColorSec->r = (pColorSec->r * mModifier.mLight.mAmbient.r + 128) >> 8;
                pColorSec->g = (pColorSec->g * mModifier.mLight.mAmbient.g + 128) >> 8;
                pColorSec->b = (pColorSec->b * mModifier.mLight.mAmbient.b + 128) >> 8;
                pColorSec->a = (pColorSec->a * mModifier.mLight.mAmbient.a + 128) >> 8;
                // clang-format on
            } else {
                s32 attn = (256 * dist) / mModifier.mLight.mRadius;

                u16 lr = mModifier.mLight.mDiffuse.r * 256 +
                         attn * (mModifier.mLight.mAmbient.r -
                                 mModifier.mLight.mDiffuse.r);

                u16 lg = mModifier.mLight.mDiffuse.g * 256 +
                         attn * (mModifier.mLight.mAmbient.g -
                                 mModifier.mLight.mDiffuse.g);

                u16 lb = mModifier.mLight.mDiffuse.b * 256 +
                         attn * (mModifier.mLight.mAmbient.b -
                                 mModifier.mLight.mDiffuse.b);

                u16 la = mModifier.mLight.mDiffuse.a * 256 +
                         attn * (mModifier.mLight.mAmbient.a -
                                 mModifier.mLight.mDiffuse.a);

                pColorPri->r = (pColorPri->r * lr + 128) >> 16;
                pColorPri->g = (pColorPri->g * lg + 128) >> 16;
                pColorPri->b = (pColorPri->b * lb + 128) >> 16;
                pColorPri->a = (pColorPri->a * la + 128) >> 16;

                pColorSec->r = (pColorSec->r * lr + 128) >> 16;
                pColorSec->g = (pColorSec->g * lg + 128) >> 16;
                pColorSec->b = (pColorSec->b * lb + 128) >> 16;
                pColorSec->a = (pColorSec->a * la + 128) >> 16;
            }
        }
        break;
    }

    default: {
        break;
    }
    }
}

} // namespace ef
} // namespace nw4r
