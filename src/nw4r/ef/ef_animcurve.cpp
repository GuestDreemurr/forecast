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

            value = GetRangeValueF32(pRange, 0, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            value = GetRangeValueF32(pRange, 1, rnd);
            pTarget[1] = InterpolateF32(interp >> 2, t, value, pKey1->value[1]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<2>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = GetRangeValueF32(pRange, 0, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

            value = GetRangeValueF32(pRange, 1, rnd);
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

            value0 = GetRangeValueF32(pRange0, 0, rnd0);
            value1 = GetRangeValueF32(pRange1, 0, rnd1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

            value0 = GetRangeValueF32(pRange0, 1, rnd0);
            value1 = GetRangeValueF32(pRange1, 1, rnd1);
            pTarget[1] = InterpolateF32(interp >> 2, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];

    } else {
        u32 rnd;
        AnimCurveRangeF32<2>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl, pKey,
                                     loop, seed, rnd);

        pTarget[0] = GetRangeValueF32(pRange, 0, rnd);
        rnd = MixRandom(rnd);
        pTarget[1] = GetRangeValueF32(pRange, 1, rnd);

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

            value = GetRangeValueF32(pRange, 0, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            interp >>= 2;

            value = GetRangeValueF32(pRange, 1, rnd);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, value, pKey1->value[1]);

            interp >>= 2;

            value = GetRangeValueF32(pRange, 2, rnd);
            pTarget[2] = InterpolateF32(interp, t, value, pKey1->value[2]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<3>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = GetRangeValueF32(pRange, 0, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

            interp >>= 2;

            value = GetRangeValueF32(pRange, 1, rnd);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, pKey0->value[1], value);

            interp >>= 2;

            value = GetRangeValueF32(pRange, 2, rnd);
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

            value0 = GetRangeValueF32(pRange0, 0, rnd0);
            value1 = GetRangeValueF32(pRange1, 0, rnd1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = GetRangeValueF32(pRange0, 1, rnd0);
            value1 = GetRangeValueF32(pRange1, 1, rnd1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);
            pTarget[1] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = GetRangeValueF32(pRange0, 2, rnd0);
            value1 = GetRangeValueF32(pRange1, 2, rnd1);
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
        AnimCurveRangeF32<3>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl, pKey,
                                     loop, seed, rnd);

        pTarget[0] = GetRangeValueF32(pRange, 0, rnd);
        rnd = MixRandom(rnd);
        pTarget[1] = GetRangeValueF32(pRange, 1, rnd);
        rnd = MixRandom(rnd);
        pTarget[2] = GetRangeValueF32(pRange, 2, rnd);

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

            value = GetRangeValueF32(pRange, 0, rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = GetRangeValueF32(pRange, 0, rnd);
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

            value0 = GetRangeValueF32(pRange0, 0, rnd0);
            value1 = GetRangeValueF32(pRange1, 0, rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];

    } else {
        u32 rnd;
        AnimCurveRangeF32<1>* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl, pKey,
                                     loop, seed, rnd);

        pTarget[0] = GetRangeValueF32(pRange, 0, rnd);

    }
}

} // namespace ef
} // namespace nw4r
