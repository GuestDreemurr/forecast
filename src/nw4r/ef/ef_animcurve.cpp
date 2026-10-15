#include <nw4r/ef.h>

#include <cmath>

namespace nw4r {
namespace ef {

namespace {

// Key of a float curve
struct AnimCurveKeyF32 {
    u16 frame;       // at 0x0
    u16 interp;      // at 0x2
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        f32 value;    // at 0x8
        u16 rangeIdx; // at 0x8
    };
};

// Random range of a float curve
struct AnimCurveRangeF32 {
    f32 base;  // at 0x0
    f32 width; // at 0x4
};

struct AnimCurveRangeTableF32 {
    u16 count;                   // at 0x0
    u16 PADDING_0x2;             // at 0x2
    AnimCurveRangeF32 datas[1];  // at 0x4
};

struct AnimCurveKeyTableF32 {
    u16 count;                  // at 0x0
    u16 PADDING_0x2;            // at 0x2
    AnimCurveKeyF32 datas[1];   // at 0x4
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
static inline bool SearchKeyF32(AnimCurveKeyTableF32* pKeyTbl, int frame,
                                f32 time, int& rIdx, u16& rPrevFrame,
                                u16& rNextFrame) {
    AnimCurveKeyF32* pKeys = pKeyTbl->datas;

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

static inline u32 MixRandom(u32 rnd) {
    return rnd * 0x343FD + 0x269EC3;
}

// Picks the random range of a key. rRnd receives the random value.
static inline AnimCurveRangeF32*
GetRandomRangeF32(AnimCurveHeader* pHeader, AnimCurveRangeTableF32* pRangeTbl,
                  AnimCurveRangeTableF32* pRandomTbl, AnimCurveKeyF32* pKey,
                  u32 loop, u16 seed, u32& rRnd) {
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
    AnimCurveRangeF32* pRange;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rnd >> 16) % pRandomTbl->count];
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return pRange;
}

static inline f32 GetRangeValueF32(AnimCurveRangeF32* pRange, u32 rnd) {
    return pRange->base + pRange->width * (rnd >> 16);
}

static inline f32 CalcRandomF32(AnimCurveHeader* pHeader,
                                AnimCurveRangeTableF32* pRangeTbl,
                                AnimCurveRangeTableF32* pRandomTbl,
                                AnimCurveKeyF32* pKey, u32 loop, u16 seed) {
    u32 rnd;
    AnimCurveRangeF32* pRange = GetRandomRangeF32(
        pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, rnd);
    return GetRangeValueF32(pRange, rnd);
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

void AnimCurveExecuteF32(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                         u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveKeyTableF32* pKeyTbl =
        reinterpret_cast<AnimCurveKeyTableF32*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveRangeTableF32* pRangeTbl =
        reinterpret_cast<AnimCurveRangeTableF32*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveRangeTableF32* pRandomTbl =
        reinterpret_cast<AnimCurveRangeTableF32*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u16 frameLength = pHeader->frameLength;
    u32 loop = 0;
    f32 time;
    u16 frame;

    if (frameLength <= 1) {
        time = 0.0f;
        loop = tick;
        frame = 0;
    } else {
        u8 flag = pHeader->processFlag;

        if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
            pHeader->loopCount <= 1) {

            if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                frame = tick;
                if (tick >= frameLength - 1) {
                    frame = frameLength - 1;
                }
                time = frame;
            } else {
                time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                if (time > static_cast<f32>(frameLength - 1)) {
                    time = static_cast<f32>(frameLength - 1);
                }

                frame = time;
            }
        } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            u32 period = frameLength - 1;
            loop = tick / period;

            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    loop >= pHeader->loopCount) {
                    frame = period;
                    loop = static_cast<u8>(pHeader->loopCount - 1);
                } else {
                    frame = tick - loop * period;
                }
            } else {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    loop >= pHeader->loopCount) {
                    if (pHeader->loopCount % 2 == 0) {
                        frame = 0;
                    } else {
                        frame = period;
                    }
                    loop = static_cast<u8>(pHeader->loopCount - 1);
                } else if (loop % 2 == 0) {
                    frame = tick - loop * period;
                } else {
                    frame = period * (loop + 1) - tick;
                }
            }

            time = frame;
        } else if (tick >= life - 1) {
            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
                pHeader->loopCount % 2 != 0) {
                frame = static_cast<u8>(frameLength - 1);
            } else {
                frame = 0;
            }

            time = frame;
            loop = static_cast<u8>(pHeader->loopCount - 1);
        } else {
            time = CalcFittingLoopTime(pHeader, flag, tick, life, loop);
            frame = time;
        }
    }

    AnimCurveKeyF32* pKeys = pKeyTbl->datas;
    if (pKeyTbl->count == 0) {
        return;
    }

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    bool found =
        SearchKeyF32(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;

        u8 flag = pHeader->processFlag;
        if (flag & AnimCurveHeader::PROC_FLAG_TURN) {
            if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
                pHeader->loopCount > 1) {

                if (!(loop & 1) && idx + 1 >= pKeyTbl->count - 1) {
                    if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
                        loop < pHeader->loopCount - 1) {
                        loop1 = loop + 1;
                    }
                }

                if ((loop & 1) && idx == 0 && loop != 0) {
                    loop0 = loop + 1;
                }
            }
        }

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32* pKey0 = &pKeys[idx];
        AnimCurveKeyF32* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            if (pBase != NULL) {
                reinterpret_cast<u8*>(pBase)[0x11] = 0;
            }

            value = InterpolateConstF32(interp, t, pKey0->value, pKey1->value);

            *pTarget = value;
            if (pBase != NULL) {
                *pBase = value;
            }
        } else if (random0 && !random1) {
            value = CalcRandomF32(pHeader, pRangeTbl, pRandomTbl, pKey0, loop0,
                                  seed);
            *pTarget = InterpolateF32(interp, t, value, pKey1->value);
        } else if (!random0 && random1) {
            value = CalcRandomF32(pHeader, pRangeTbl, pRandomTbl, pKey1, loop1,
                                  seed);
            *pTarget = InterpolateF32(interp, t, pKey0->value, value);
        } else {
            u32 rnd0;
            AnimCurveRangeF32* pRange0 = GetRandomRangeF32(
                pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32* pRange1 = GetRandomRangeF32(
                pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, rnd1);

            f32 value0 = GetRangeValueF32(pRange0, rnd0);
            f32 value1 = GetRangeValueF32(pRange1, rnd1);
            *pTarget = InterpolateF32(interp, t, value0, value1);
        }
        return;
    }

    if (pKey->random == 0) {
        *pTarget = pKey->value;
    } else {
        *pTarget =
            CalcRandomF32(pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed);
    }
}

} // namespace ef
} // namespace nw4r
