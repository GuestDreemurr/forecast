#include <nw4r/ef.h>

#include <cmath>

namespace nw4r {
namespace ef {

namespace {

// Key of a float curve with N components
template <int N> struct AnimCurveKeyF32 {
    u16 frame;       // at 0x0
    u16 interp;      // at 0x2, two bits per component
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        f32 value[N]; // at 0x8
        u16 rangeIdx; // at 0x8
    };
};

// Random range of a float curve with N components
template <int N> struct AnimCurveRangeF32 {
    struct {
        f32 base;  // at 0x0
        f32 width; // at 0x4
    } comp[N];
};

// Random range of a rotation curve
struct AnimCurveRangeRotate {
    struct {
        f32 base;  // at 0x0
        f32 width; // at 0x4
    } comp[3];
    u8 randomSign;     // at 0x18
    u8 PADDING_0x19[3]; // at 0x19
};

// Key of a byte curve with N components
template <int N> struct AnimCurveKeyU8 {
    u16 frame;       // at 0x0
    u16 interp;      // at 0x2, two bits per component
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        u8 value[N];  // at 0x8
        u16 rangeIdx; // at 0x8
    };
};

struct AnimCurveRangeCompU8 {
    u8 base;  // at 0x0
    u8 width; // at 0x1, scaled by a signed random in [-1, 1)
};

// Random range of a byte curve with N components
template <int N> struct AnimCurveRangeU8 {
    AnimCurveRangeCompU8 comp[N];
};

// Key of a texture curve
struct AnimCurveKeyTexture {
    u16 frame;       // at 0x0
    u16 PADDING_0x2; // at 0x2
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        struct {
            u8 wrap;    // at 0x8
            u8 reverse; // at 0x9
        };
        u16 rangeIdx; // at 0x8
    };
    u16 name; // at 0xA
};

struct AnimCurveRangeTexture {
    u8 wrap;          // at 0x0
    u8 reverse;       // at 0x1
    u16 name;         // at 0x2
    u8 randomReverse; // at 0x4
    u8 PADDING_0x5[3]; // at 0x5
};

struct AnimCurveRandomTexture {
    u8 wrap;    // at 0x0
    u8 reverse; // at 0x1
    u16 name;   // at 0x2
};

template <typename T> struct AnimCurveTable {
    u16 count;       // at 0x0
    u16 PADDING_0x2; // at 0x2
    T datas[1];      // at 0x4
};

enum InterpType {
    INTERP_LINEAR,
    INTERP_SMOOTH,
    INTERP_STEP,
};

enum KeyRandomFlag {
    KEY_RANDOM_FLAG_TABLE = (1 << 1),
};

} // namespace

// Finds the key at or before the frame. Returns whether the key value can be
// used as is (no interpolation).
template <typename TKey>
static inline bool SearchKey(AnimCurveTable<TKey>* pKeyTbl, int frame,
                             f32 time, int& rIdx, u16& rPrevFrame,
                             u16& rNextFrame) {
    TKey* pKeys = pKeyTbl->datas;

    int lo = 0;
    int hi = pKeyTbl->count - 1;
    int mid = hi / 2;

    bool exact =
        std::fabs(static_cast<f32>(frame) - time) < NW4R_MATH_FLT_EPSILON;

    rPrevFrame = pKeys[0].frame;

    if (frame < pKeys[0].frame) {
        rIdx = 0;
        return true;
    }

    if (frame == pKeys[0].frame) {
        rIdx = 0;

        if (hi == 0) {
            return true;
        }

        if (!exact) {
            rNextFrame = pKeys[1].frame;
        }
        return exact;
    }

    rNextFrame = pKeys[hi].frame;
    if (rNextFrame <= frame) {
        rIdx = hi;
        return true;
    }

    int midFrame = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == midFrame) {
            rIdx = mid;

            if (!exact) {
                rPrevFrame = midFrame;
                rNextFrame = pKeys[mid + 1].frame;
            }
            return exact;
        }

        if (midFrame < frame) {
            lo = mid;
            rPrevFrame = midFrame;
        } else {
            hi = mid;
            rNextFrame = midFrame;
        }

        mid = (lo + hi) / 2;
        midFrame = pKeys[mid].frame;
    }

    rIdx = lo;
    return false;
}

// Frame of a key in a table with variable sized keys
static inline u16 KeyFrame(u8* pKeys, int idx, int keySize) {
    return *reinterpret_cast<u16*>(pKeys + idx * keySize);
}

static inline bool SearchKeyStride(AnimCurveTable<u8>* pKeyTbl, int keySize,
                                   int frame,
                             f32 time, int& rIdx, u16& rPrevFrame,
                             u16& rNextFrame) {
    u8* pKeys = pKeyTbl->datas;

    int lo = 0;
    int hi = pKeyTbl->count - 1;
    int mid = hi / 2;

    bool exact =
        std::fabs(static_cast<f32>(frame) - time) < NW4R_MATH_FLT_EPSILON;

    rPrevFrame = KeyFrame(pKeys, 0, keySize);

    if (frame < KeyFrame(pKeys, 0, keySize)) {
        rIdx = 0;
        return true;
    }

    if (frame == KeyFrame(pKeys, 0, keySize)) {
        rIdx = 0;

        if (hi == 0) {
            return true;
        }

        if (!exact) {
            rNextFrame = KeyFrame(pKeys, 1, keySize);
        }
        return exact;
    }

    rNextFrame = KeyFrame(pKeys, hi, keySize);
    if (rNextFrame <= frame) {
        rIdx = hi;
        return true;
    }

    int midFrame = KeyFrame(pKeys, mid, keySize);

    while (lo < mid) {
        if (frame == midFrame) {
            rIdx = mid;

            if (!exact) {
                rPrevFrame = midFrame;
                rNextFrame = KeyFrame(pKeys, mid + 1, keySize);
            }
            return exact;
        }

        if (midFrame < frame) {
            lo = mid;
            rPrevFrame = midFrame;
        } else {
            hi = mid;
            rNextFrame = midFrame;
        }

        mid = (lo + hi) / 2;
        midFrame = KeyFrame(pKeys, mid, keySize);
    }

    rIdx = lo;
    return false;
}

// Time of a looping curve stretched over the particle life
static inline f32 CalcFittingLoopTime(AnimCurveHeader* pHeader, u8 flag,
                                      u32 tick, u32 life, u32& rLoop) {
    u32 period = pHeader->frameLength - 1;
    f32 scale = pHeader->loopCount *
                (static_cast<f32>(static_cast<int>(period)) / (life - 1));

    u32 loop = tick * scale / static_cast<int>(period);
    rLoop = loop;

    f32 time = tick * scale - loop * period;

    if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
        return time;
    }

    if (loop % 2 == 0) {
        return time;
    }

    return static_cast<f32>(static_cast<int>(period)) - time;
}

// Converts the particle tick to a curve time (loop, turn and fitting modes)
static inline f32 CalcTime(AnimCurveHeader* pHeader, u32 tick, u32 life,
                           u16& rFrame, u32& rLoop) {
    u16 frameLength = pHeader->frameLength;
    rLoop = 0;
    f32 time;

    if (frameLength <= 1) {
        time = 0.0f;
        rLoop = tick;
        rFrame = 0;
    } else {
        u8 flag = pHeader->processFlag;

        if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
            pHeader->loopCount <= 1) {

            if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                rFrame = tick;
                if (tick >= frameLength - 1) {
                    rFrame = frameLength - 1;
                }
                time = rFrame;
            } else {
                time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                if (time > static_cast<f32>(frameLength - 1)) {
                    time = static_cast<f32>(frameLength - 1);
                }

                rFrame = time;
            }
        } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            u32 period = frameLength - 1;
            rLoop = tick / period;

            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    rLoop >= pHeader->loopCount) {
                    rFrame = period;
                    rLoop = static_cast<u8>(pHeader->loopCount - 1);
                } else {
                    rFrame = tick - rLoop * period;
                }
            } else {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    rLoop >= pHeader->loopCount) {
                    if (pHeader->loopCount % 2 == 0) {
                        rFrame = 0;
                    } else {
                        rFrame = period;
                    }
                    rLoop = static_cast<u8>(pHeader->loopCount - 1);
                } else if (rLoop % 2 == 0) {
                    rFrame = tick - rLoop * period;
                } else {
                    rFrame = period * (rLoop + 1) - tick;
                }
            }

            time = rFrame;
        } else if (tick >= life - 1) {
            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
                pHeader->loopCount % 2 != 0) {
                rFrame = static_cast<u8>(frameLength - 1);
            } else {
                rFrame = 0;
            }

            time = rFrame;
            rLoop = static_cast<u8>(pHeader->loopCount - 1);
        } else {
            time = CalcFittingLoopTime(pHeader, flag, tick, life, rLoop);
            rFrame = time;
        }
    }


    return time;
}

// Turning curves play the keys backwards on odd loops, so the random seed of
// the key on the far side belongs to the neighbouring loop.
static inline void AdjustTurnLoop(AnimCurveHeader* pHeader, u16 numKey, int idx,
                                  u32 loop, u32& rLoop0, u32& rLoop1) {
    u8 flag = pHeader->processFlag;

    if (flag & AnimCurveHeader::PROC_FLAG_TURN) {
        if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
            pHeader->loopCount > 1) {

            if (!(loop & 1) && idx + 1 >= numKey - 1) {
                if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
                    loop < pHeader->loopCount - 1) {
                    rLoop1 = loop + 1;
                }
            }

            if ((loop & 1) && idx == 0 && loop != 0) {
                rLoop0 = loop + 1;
            }
        }
    }
}

static inline u32 MixRandom(u32 rnd) {
    return rnd * 0x343FD + 0x269EC3;
}

// Picks the random range of a key. rRnd receives the random value.
template <typename TRange, typename TKey>
static inline TRange* GetRandomRange(AnimCurveHeader* pHeader,
                                     AnimCurveTable<TRange>* pRangeTbl,
                                     AnimCurveTable<TRange>* pRandomTbl,
                                     TKey* pKey, u32 loop, u16 seed,
                                     u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    rndSeed.value = seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
                    (loop * 0x7B929 + pKey->rangeIdx * 0x371097E7 + 0x4BF53);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    TRange* pRange;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rnd >> 16) % pRandomTbl->count];
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return pRange;
}

// GetRandomRange with the range index read through the key values
template <typename TRange>
static inline TRange* GetRandomRangeV(AnimCurveHeader* pHeader,
                                      AnimCurveTable<TRange>* pRangeTbl,
                                      AnimCurveTable<TRange>* pRandomTbl,
                                      u8 random, u16* pRangeIdx, u32 loop,
                                      u16 seed, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    rndSeed.value = seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
                    (loop * 0x7B929 + *pRangeIdx * 0x371097E7 + 0x4BF53);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    TRange* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[*pRangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rnd >> 16) % pRandomTbl->count];
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return pRange;
}

// GetRandomRange for ranges of variable size
static inline AnimCurveRangeF32<1>*
GetRandomRangeStride(AnimCurveHeader* pHeader, AnimCurveTable<u8>* pRangeTbl,
                     AnimCurveTable<u8>* pRandomTbl, int rangeSize, u8 random,
                     u16* pRangeIdx, u32 loop, u16 seed, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    rndSeed.value = seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
                    (loop * 0x7B929 + *pRangeIdx * 0x371097E7 + 0x4BF53);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    u8* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = pRangeTbl->datas + *pRangeIdx * rangeSize;
    } else {
        pRange = pRandomTbl->datas + (rnd >> 16) % pRandomTbl->count * rangeSize;
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return reinterpret_cast<AnimCurveRangeF32<1>*>(pRange);
}

// Rotation ranges can pick a random sign. rRnd advances when they do.
static inline bool GetRandomSign(AnimCurveRangeRotate* pRange, u32& rRnd) {
    bool negate = false;

    if (pRange->randomSign) {
        if (!(rRnd & 0x10000)) {
            negate = true;
        }
        rRnd = MixRandom(rRnd);
    }

    return negate;
}

template <typename TRange>
static inline f32 GetRangeValueF32(TRange* pRange, int i, u32 rnd) {
    return pRange->comp[i].base + pRange->comp[i].width * (rnd >> 16);
}

template <typename TRange, typename TKey>
static inline f32 CalcRandomF32(AnimCurveHeader* pHeader,
                                AnimCurveTable<TRange>* pRangeTbl,
                                AnimCurveTable<TRange>* pRandomTbl,
                                TKey* pKey, u32 loop, u16 seed) {
    u32 rnd;
    TRange* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl, pKey,
                                    loop, seed, rnd);
    return GetRangeValueF32(pRange, 0, rnd);
}

// Byte curves use a 16.16 fixed point t
static inline u8 InterpolateU8(u16 interp, u32 t, u8 v0, u8 v1) {
    switch (interp & 3) {
    case INTERP_LINEAR: {
        return v0 + ((t * (v1 - v0)) >> 16);
    }
    case INTERP_SMOOTH: {
        f32 ft = t / 65536.0f;
        return v0 + ft * (ft * ((3.0f - 2.0f * ft) * (v1 - v0)));
    }
    case INTERP_STEP: {
        return v0;
    }
    default: {
        return 0;
    }
    }
}

static inline int GetRangeValueU8(AnimCurveRangeCompU8* pComp, u32 rnd) {
    int value =
        pComp->base + static_cast<s16>(rnd >> 16) * pComp->width / 32768;

    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }

    return value;
}

static inline f32 InterpolateF32(u16 interp, f32 t, f32 v0, f32 v1) {
    switch (interp & 3) {
    case INTERP_LINEAR: {
        return v0 + t * (v1 - v0);
    }
    case INTERP_SMOOTH: {
        return v0 + t * (t * ((3.0f - 2.0f * t) * (v1 - v0)));
    }
    case INTERP_STEP: {
        return v0;
    }
    default: {
        return 0.0f;
    }
    }
}

static inline f32 InterpolateConstF32(u16 interp, f32 t, f32 v0, f32 v1) {
    if (v0 == v1) {
        return v0;
    }

    return InterpolateF32(interp, t, v0, v1);
}

void AnimCurveExecuteU8Vec3(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                        u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyU8<3> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyU8<3> >*>(
            pCmdList + sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeU8<3> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<3> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeU8<3> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<3> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyU8<3>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyU8<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        u32 t = static_cast<u32>(65536.0f * (time - prevFrame)) /
                (nextFrame - prevFrame);

        AnimCurveKeyU8<3>* pKey0 = &pKeys[idx];
        u8* pValue0 = pKey0->value;
        u16 interp = pKey0->interp;
        AnimCurveKeyU8<3>* pKey1 = &pKeys[idx + 1];
        u8* pValue1 = pKey1->value;

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;

        if (!random0 && !random1) {
            for (int i = 0; i < 3; i++, interp >>= 2, pValue0++, pValue1++) {
                u8 value1 = *pValue1;
                u8 value0 = *pValue0;

                if (value0 == value1) {
                    *pTarget++ = value0;
                } else {
                    *pTarget++ = InterpolateU8(interp, t, value0, value1);
                }
            }
        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeU8<3>* pRange = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd);
            AnimCurveRangeCompU8* pComp = pRange->comp;

            for (int i = 0; i < 3; i++, interp >>= 2, pValue1++) {
                int value = GetRangeValueU8(pComp++, rnd);
                rnd = MixRandom(rnd);
                *pTarget++ = InterpolateU8(interp, t, value, *pValue1);
            }
        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeU8<3>* pRange = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd);
            AnimCurveRangeCompU8* pComp = pRange->comp;

            for (int i = 0; i < 3; i++, interp >>= 2, pValue0++) {
                int value = GetRangeValueU8(pComp++, rnd);
                rnd = MixRandom(rnd);
                *pTarget++ = InterpolateU8(interp, t, *pValue0, value);
            }
        } else {
            u32 rnd0;
            AnimCurveRangeU8<3>* pRange0 = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd0);
            AnimCurveRangeCompU8* pComp0 = pRange0->comp;
            u32 rnd1;
            AnimCurveRangeU8<3>* pRange1 = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd1);
            AnimCurveRangeCompU8* pComp1 = pRange1->comp;

            for (int i = 0; i < 3; i++, interp >>= 2) {
                int value0 = GetRangeValueU8(pComp0++, rnd0);
                int value1 = GetRangeValueU8(pComp1++, rnd1);
                rnd0 = MixRandom(rnd0);
                rnd1 = MixRandom(rnd1);
                *pTarget++ = InterpolateU8(interp, t, value0, value1);
            }
        }
        return;
    }

    if (pKey->random == 0) {
        for (int i = 0; i < 3; i++) {
            pTarget[i] = pKey->value[i];
        }
    } else {
        u32 rnd;
        AnimCurveRangeU8<3>* pRange = GetRandomRange(
            pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, rnd);
        AnimCurveRangeCompU8* pComp = pRange->comp;

        for (int i = 0; i < 3; i++) {
            pTarget[i] = GetRangeValueU8(pComp++, rnd);
            rnd = MixRandom(rnd);
        }
    }
}

void AnimCurveExecuteU8(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                        u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyU8<1> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyU8<1> >*>(
            pCmdList + sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeU8<1> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<1> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeU8<1> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<1> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyU8<1>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyU8<1>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        u32 t = static_cast<u32>(65536.0f * (time - prevFrame)) /
                (nextFrame - prevFrame);

        AnimCurveKeyU8<1>* pKey0 = &pKeys[idx];
        AnimCurveKeyU8<1>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        if (!random0 && !random1) {
            u8 value1 = pKey1->value[0];
            u8 value0 = pKey0->value[0];

            if (value0 == value1) {
                *pTarget = value0;
            } else {
                *pTarget = InterpolateU8(interp, t, value0, value1);
            }
        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeU8<1>* pRange = GetRandomRange(
                pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd);

            int value = GetRangeValueU8(pRange->comp, rnd);
            *pTarget = InterpolateU8(interp, t, value, pKey1->value[0]);
        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeU8<1>* pRange = GetRandomRange(
                pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, rnd);

            int value = GetRangeValueU8(pRange->comp, rnd);
            *pTarget = InterpolateU8(interp, t, pKey0->value[0], value);
        } else {
            u32 rnd0;
            AnimCurveRangeU8<1>* pRange0 = GetRandomRange(
                pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeU8<1>* pRange1 = GetRandomRange(
                pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, rnd1);

            int value0 = GetRangeValueU8(pRange0->comp, rnd0);
            int value1 = GetRangeValueU8(pRange1->comp, rnd1);
            *pTarget = InterpolateU8(interp, t, value0, value1);
        }
        return;
    }

    if (pKey->random == 0) {
        *pTarget = pKey->value[0];
    } else {
        u32 rnd;
        AnimCurveRangeU8<1>* pRange = GetRandomRange(
            pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, rnd);

        *pTarget = GetRangeValueU8(pRange->comp, rnd);
    }
}

void AnimCurveExecuteF32Vec2(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                             u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<2> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<2> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeF32<2> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<2> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeF32<2> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<2> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyF32<2>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<2>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<2>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<2>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            if (pBase != NULL) {
                reinterpret_cast<u8*>(pBase)[0x11] = 0;
            }

            value = InterpolateConstF32(interp, t, pKey0->value[0],
                                        pKey1->value[0]);
            pTarget[0] = value;
            if (pBase != NULL) {
                pBase[0] = value;
            }

            value = InterpolateConstF32(interp >> 2, t, pKey0->value[1],
                                        pKey1->value[1]);
            pTarget[1] = value;
            if (pBase != NULL) {
                pBase[1] = value;
            }

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<2>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            pTarget[1] = InterpolateF32(interp >> 2, t, value, pKey1->value[1]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<2>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            pTarget[1] = InterpolateF32(interp >> 2, t, pKey0->value[1], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<2>* pRange0 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<2>* pRange1 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);

            f32 value0;
            f32 value1;

            value0 = pRange0->comp[0].base +
        pRange0->comp[0].width * (rnd0 >> 16);
            value1 = pRange1->comp[0].base +
        pRange1->comp[0].width * (rnd1 >> 16);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

            value0 = pRange0->comp[1].base +
        pRange0->comp[1].width * (rnd0 >> 16);
            value1 = pRange1->comp[1].base +
        pRange1->comp[1].width * (rnd1 >> 16);
            pTarget[1] = InterpolateF32(interp >> 2, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];

    } else {
        u32 rnd;
        AnimCurveRangeF32<2>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);


        pTarget[0] = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
        rnd = MixRandom(rnd);
        pTarget[1] = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);

    }
}

void AnimCurveExecuteF32Vec3(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                             u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<3> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<3> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeF32<3> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<3> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeF32<3> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<3> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyF32<3>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<3>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<3>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            value = InterpolateConstF32(interp, t, pKey0->value[0],
                                        pKey1->value[0]);
            pTarget[0] = value;

            interp >>= 2;

            value = InterpolateConstF32(interp, t, pKey0->value[1],
                                        pKey1->value[1]);
            pTarget[1] = value;

            interp >>= 2;

            value = InterpolateConstF32(interp, t, pKey0->value[2],
                                        pKey1->value[2]);
            pTarget[2] = value;

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<3>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            interp >>= 2;

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, value, pKey1->value[1]);

            interp >>= 2;

            value = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);
            pTarget[2] = InterpolateF32(interp, t, value, pKey1->value[2]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<3>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

            interp >>= 2;

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, pKey0->value[1], value);

            interp >>= 2;

            value = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);
            pTarget[2] = InterpolateF32(interp, t, pKey0->value[2], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<3>* pRange0 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<3>* pRange1 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);

            f32 value0;
            f32 value1;

            value0 = pRange0->comp[0].base +
        pRange0->comp[0].width * (rnd0 >> 16);
            value1 = pRange1->comp[0].base +
        pRange1->comp[0].width * (rnd1 >> 16);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = pRange0->comp[1].base +
        pRange0->comp[1].width * (rnd0 >> 16);
            value1 = pRange1->comp[1].base +
        pRange1->comp[1].width * (rnd1 >> 16);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[1] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = pRange0->comp[2].base +
        pRange0->comp[2].width * (rnd0 >> 16);
            value1 = pRange1->comp[2].base +
        pRange1->comp[2].width * (rnd1 >> 16);
            pTarget[2] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];
        pTarget[2] = pKey->value[2];

    } else {
        u32 rnd;
        AnimCurveRangeF32<3>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);


        pTarget[0] = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
        rnd = MixRandom(rnd);
        pTarget[1] = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
        rnd = MixRandom(rnd);
        pTarget[2] = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);

    }
}

void AnimCurveExecuteF32N(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                          u32 life, u8 enable) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<u8>* pKeyTbl =
        reinterpret_cast<AnimCurveTable<u8>*>(pCmdList +
                                              sizeof(AnimCurveHeader));
    AnimCurveTable<u8>* pRangeTbl = reinterpret_cast<AnimCurveTable<u8>*>(
        reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<u8>* pRandomTbl = reinterpret_cast<AnimCurveTable<u8>*>(
        reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    // The highest enabled bit gives the number of components
    int num = 8;
    u8 bit;
    for (bit = 0x80; bit != 0; bit >>= 1, num--) {
        if (enable & bit) {
            break;
        }
    }

    if (num == 0) {
        return;
    }

    int keySize = num * sizeof(f32) + 8;
    int rangeSize = num * sizeof(AnimCurveRangeF32<1>);
    u8* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKeyStride(pKeyTbl, keySize, frame, time, idx,
                                 prevFrame, nextFrame);

    f32* pValue;

    if (found) {
        pValue = reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + idx * keySize)
                     ->value;
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<1>* pKey0 =
            reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + idx * keySize);
        f32* pValue0 = pKey0->value;
        AnimCurveKeyF32<1>* pKey1 =
            reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + (idx + 1) * keySize);
        f32* pValue1 = pKey1->value;

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        if (!random0 && !random1) {
            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    *pTarget =
                        InterpolateConstF32(interp, t, *pValue0, *pValue1);
                    pValue0++;
                    pValue1++;
                }
            }
        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRangeStride(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd);

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    f32 value = GetRangeValueF32(pRange, 0, rnd);
                    rnd = MixRandom(rnd);
                    pRange++;
                    *pTarget = InterpolateF32(interp, t, value, *pValue1);
                    pValue1++;
                }
            }
        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRangeStride(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd);

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    f32 value = GetRangeValueF32(pRange, 0, rnd);
                    rnd = MixRandom(rnd);
                    pRange++;
                    *pTarget = InterpolateF32(interp, t, *pValue0, value);
                    pValue0++;
                }
            }
        } else {
            u32 rnd0;
            AnimCurveRangeF32<1>* pRange0 = GetRandomRangeStride(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<1>* pRange1 = GetRandomRangeStride(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd1);

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    f32 value0 = GetRangeValueF32(pRange0, 0, rnd0);
                    f32 value1 = GetRangeValueF32(pRange1, 0, rnd1);
                    rnd0 = MixRandom(rnd0);
                    rnd1 = MixRandom(rnd1);
                    pRange0++;
                    pRange1++;
                    *pTarget = InterpolateF32(interp, t, value0, value1);
                }
            }
        }
        return;
    }

    AnimCurveKeyF32<1>* pKey = reinterpret_cast<AnimCurveKeyF32<1>*>(
        reinterpret_cast<u8*>(pValue) - 8);

    if (pKey->random == 0) {
        for (; bit != 0; bit >>= 1, pValue++, pTarget++) {
            if (enable & bit) {
                *pTarget = *pValue;
            }
        }
    } else {
        u32 rnd;
        AnimCurveRangeF32<1>* pRange =
            GetRandomRangeStride(pHeader, pRangeTbl, pRandomTbl, rangeSize,
                                 pKey->random, reinterpret_cast<u16*>(pValue),
                                 loop, seed, rnd);

        for (; bit != 0; bit >>= 1, pTarget++) {
            if (enable & bit) {
                *pTarget = GetRangeValueF32(pRange, 0, rnd);
                rnd = MixRandom(rnd);
                pRange++;
            }
        }
    }
}

void AnimCurveExecuteRotate(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<3> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<3> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeRotate >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeRotate >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeRotate >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeRotate >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyF32<3>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<3>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<3>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            value = InterpolateConstF32(interp, t, pKey0->value[0],
                                        pKey1->value[0]);
            pTarget[0] = value;

            interp >>= 2;

            value = InterpolateConstF32(interp, t, pKey0->value[1],
                                        pKey1->value[1]);
            pTarget[1] = value;

            interp >>= 2;

            value = InterpolateConstF32(interp, t, pKey0->value[2],
                                        pKey1->value[2]);
            pTarget[2] = value;

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeRotate* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);
            bool negate = GetRandomSign(pRange, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            interp >>= 2;

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, value, pKey1->value[1]);

            interp >>= 2;

            value = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            pTarget[2] = InterpolateF32(interp, t, value, pKey1->value[2]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeRotate* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);
            bool negate = GetRandomSign(pRange, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);
            rnd = MixRandom(rnd);

            interp >>= 2;

            value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            pTarget[1] = InterpolateF32(interp, t, pKey0->value[1], value);
            rnd = MixRandom(rnd);

            interp >>= 2;

            value = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);
            if (negate) {
                value = -value;
            }
            pTarget[2] = InterpolateF32(interp, t, pKey0->value[2], value);

        } else {
            u32 rnd0;
            AnimCurveRangeRotate* pRange0 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeRotate* pRange1 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);
            bool negate0 = GetRandomSign(pRange0, rnd0);
            bool negate1 = GetRandomSign(pRange1, rnd1);

            f32 value0;
            f32 value1;

            value0 = pRange0->comp[0].base +
        pRange0->comp[0].width * (rnd0 >> 16);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[0].base +
        pRange1->comp[0].width * (rnd1 >> 16);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[0] = InterpolateF32(interp, t, value0, value1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);

            interp >>= 2;

            value0 = pRange0->comp[1].base +
        pRange0->comp[1].width * (rnd0 >> 16);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[1].base +
        pRange1->comp[1].width * (rnd1 >> 16);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[1] = InterpolateF32(interp, t, value0, value1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);

            interp >>= 2;

            value0 = pRange0->comp[2].base +
        pRange0->comp[2].width * (rnd0 >> 16);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[2].base +
        pRange1->comp[2].width * (rnd1 >> 16);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[2] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];
        pTarget[2] = pKey->value[2];

    } else {
        u32 rnd;
        AnimCurveRangeRotate* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);
        bool negate = GetRandomSign(pRange, rnd);

        f32 value;
        value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
        if (negate) {
            value = -value;
        }
        pTarget[0] = value;
        rnd = MixRandom(rnd);
        value = pRange->comp[1].base +
        pRange->comp[1].width * (rnd >> 16);
        if (negate) {
            value = -value;
        }
        pTarget[1] = value;
        rnd = MixRandom(rnd);
        value = pRange->comp[2].base +
        pRange->comp[2].width * (rnd >> 16);
        if (negate) {
            value = -value;
        }
        pTarget[2] = value;

    }
}

void AnimCurveExecuteF32(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                         u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<1> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<1> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeF32<1> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<1> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeF32<1> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<1> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u32 loop;
    u16 frame;
    f32 time = CalcTime(pHeader, tick, life, frame, loop);

    AnimCurveKeyF32<1>* pKeys = pKeyTbl->datas;
    if (pKeyTbl->count == 0) {
        return;
    }

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<1>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(pHeader, pKeyTbl->count, idx, loop, loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<1>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<1>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            if (pBase != NULL) {
                reinterpret_cast<u8*>(pBase)[0x11] = 0;
            }

            value = InterpolateConstF32(interp, t, pKey0->value[0],
                                        pKey1->value[0]);
            pTarget[0] = value;
            if (pBase != NULL) {
                pBase[0] = value;
            }

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<1>* pRange0 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<1>* pRange1 = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);

            f32 value0;
            f32 value1;

            value0 = pRange0->comp[0].base +
        pRange0->comp[0].width * (rnd0 >> 16);
            value1 = pRange1->comp[0].base +
        pRange1->comp[0].width * (rnd1 >> 16);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];

    } else {
        u32 rnd;
        AnimCurveRangeF32<1>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);


        pTarget[0] = pRange->comp[0].base +
        pRange->comp[0].width * (rnd >> 16);

    }
}

void AnimCurveExecuteTexture(u8* pCmdList, Particle* pParticle, u32 tick,
                             u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyTexture>* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyTexture>*>(
            pCmdList + sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeTexture>* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeTexture>*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRandomTexture>* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRandomTexture>*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);
    AnimCurveTable<TextureData*>* pNameTbl =
        reinterpret_cast<AnimCurveTable<TextureData*>*>(
            reinterpret_cast<u8*>(pRandomTbl) + pHeader->randomTable);

    u8 flag = pHeader->processFlag;
    u32 loop = 0;
    u16 frame;

    if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
        pHeader->loopCount <= 1) {

        if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            if (tick < pHeader->frameLength) {
                frame = tick;
            } else {
                frame = pHeader->frameLength;
            }
        } else {
            frame = pHeader->frameLength * tick / life;
        }
    } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
        frame = pHeader->frameLength;

        if (frame <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            loop = tick / frame;

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                loop >= pHeader->loopCount) {
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                frame = tick - loop * frame;
            }
        } else {
            u32 period = frame - 1;
            loop = tick / period;

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                loop >= pHeader->loopCount) {
                if (pHeader->loopCount % 2 == 0) {
                    frame = 0;
                } else {
                    frame = static_cast<u8>(period);
                }
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else if (loop % 2 == 0) {
                frame = tick - loop * period;
            } else {
                frame = period * (loop + 1) - tick;
            }
        }
    } else if (tick >= life) {
        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
            pHeader->loopCount % 2 != 0) {
            frame = pHeader->frameLength;
        } else {
            frame = 0;
        }
        loop = static_cast<u8>(pHeader->loopCount - 1);
    } else {
        u16 frameLength = pHeader->frameLength;

        if (frameLength <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            f32 scale = pHeader->loopCount *
                        (static_cast<f32>(frameLength) / static_cast<f32>(life));

            loop = tick * scale / frameLength;
            frame = tick * scale - loop * frameLength;
        } else {
            u32 period = frameLength - 1;
            u8 loopCount = pHeader->loopCount;

            u32 pos = tick * ((1.0f + static_cast<int>(period) * loopCount) /
                              life);
            loop = pos / period;

            if (loop >= loopCount) {
                if (loopCount % 2 == 0) {
                    frame = 0;
                } else {
                    frame = static_cast<u8>(period);
                }
                loop = static_cast<u8>(loopCount - 1);
            } else if (loop % 2 == 0) {
                frame = pos - loop * period;
            } else {
                frame = period * (loop + 1) - pos;
            }
        }
    }

    TextureData** ppTexture;
    int layer;

    switch (pHeader->kindType) {
    case AC_TARGET_TEXTURE1: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_1];
        layer = TEX_LAYER_1;
        break;
    }
    case AC_TARGET_TEXTURE2: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_2];
        layer = TEX_LAYER_2;
        break;
    }
    case AC_TARGET_TEXTUREIND: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_IND];
        layer = TEX_LAYER_IND;
        break;
    }
    default: {
        return;
    }
    }

    // Step curve: the last key at or before the frame
    AnimCurveKeyTexture* pKeys = pKeyTbl->datas;
    int iframe = frame;
    int lo = 0;
    int hi = pKeyTbl->count - 1;
    int mid = hi / 2;

    if (iframe < pKeys[0].frame) {
        hi = 0;
    } else if (pKeys[hi].frame < iframe || iframe == pKeys[hi].frame) {
    } else {
        int midFrame = pKeys[mid].frame;

        while (lo < mid) {
            if (iframe == midFrame) {
                lo = mid;
            } else {
                if (midFrame < iframe) {
                    lo = mid;
                }
                if (midFrame >= iframe) {
                    hi = mid;
                }
            }

            mid = (lo + hi) / 2;
            midFrame = pKeys[mid].frame;
        }

        hi = lo;
    }

    AnimCurveKeyTexture* pKey = &pKeys[hi];

    u16 name;
    u8 wrap;
    u8 reverse;

    if (pKey->random == 0) {
        name = pKey->name;
        wrap = pKey->wrap;
        reverse = pKey->reverse;
    } else {
        union {
            u32 value;
            u8 bytes[4];
        } rndSeed;

        rndSeed.value =
            seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
            (loop * 0x7B929 + pKey->rangeIdx * 0x371097E7 + 0x4BF53);

        rndSeed.bytes[2] ^= rndSeed.bytes[3];
        rndSeed.bytes[1] ^= rndSeed.bytes[2];
        rndSeed.bytes[0] ^= rndSeed.bytes[1];

        if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
            AnimCurveRangeTexture* pRange = &pRangeTbl->datas[pKey->rangeIdx];

            name = pRange->name;
            wrap = pRange->wrap;
            reverse = pRange->reverse;

            switch (pRange->randomReverse) {
            case 1: {
                reverse = (reverse & 2) | ((rndSeed.value >> 16) & 1);
                break;
            }
            case 2: {
                reverse = ((rndSeed.value >> 16) & 2) | (reverse & 1);
                break;
            }
            case 3: {
                reverse = (rndSeed.value >> 16) & 3;
                break;
            }
            }
        } else {
            AnimCurveRandomTexture* pRandom =
                &pRandomTbl->datas[(rndSeed.value >> 16) % pRandomTbl->count];

            name = pRandom->name;
            wrap = pRandom->wrap;
            reverse = pRandom->reverse;
        }
    }

    *ppTexture = pNameTbl->datas[name];

    pParticle->mParameter.mTextureWrap =
        (pParticle->mParameter.mTextureWrap & ~(0xF << (layer * 4))) |
        ((wrap & 0xF) << (layer * 4));
    pParticle->mParameter.mTextureReverse =
        (pParticle->mParameter.mTextureReverse & ~(3 << (layer * 2))) |
        ((reverse & 3) << (layer * 2));
}

} // namespace ef
} // namespace nw4r
