#ifndef NW4R_EF_DRAW_STRIPE_STRATEGY_H
#define NW4R_EF_DRAW_STRIPE_STRATEGY_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/drawstrategy/ef_drawstrategyimpl.h>

namespace nw4r {
namespace ef {

class DrawStripeStrategy : public DrawStrategyImpl {
public:
    DrawStripeStrategy();

    virtual void Draw(const DrawInfo& rInfo,
                      ParticleManager* pManager); // at 0xC

    virtual CalcAheadFunc
    GetCalcAheadFunc(ParticleManager* pManager); // at 0x18

    void InitGraphics(const DrawInfo& rInfo, ParticleManager* pManager);

private:
    // AheadContext plus the emitter's X axis in particle manager space
    struct StripeContext : AheadContext {
        math::VEC3 mEmitterAxisX; // at 0xBC

        StripeContext(const math::MTX34& rViewMtx, ParticleManager* pManager)
            : AheadContext(rViewMtx, pManager) {

            math::VEC3 axisX(mCommon.mEmitterMtx._00, mCommon.mEmitterMtx._10,
                             mCommon.mEmitterMtx._20);

            math::VEC3TransformNormal(&mEmitterAxisX,
                                      &mCommon.mParticleManagerMtxInv, &axisX);
        }
    };

    void DrawStripe(StripeContext* pContext, const math::VEC3& rRight,
                    const math::VEC3& rLeft);

    static inline void EndStripe();

    static inline math::MTX34 CalcDirMtx(Particle* pParticle,
                                         StripeContext* pContext,
                                         const math::VEC3* pDir,
                                         const math::VEC3* pPos,
                                         math::VEC3* pPrevAxis);

    static inline math::MTX34 CalcLocMtx(Particle* pParticle, f32 pivot);

    inline void DispVertex(Particle* pParticle, StripeContext* pContext,
                           CalcAheadFunc pCalcAheadFunc,
                           const math::VEC3* pPos, const math::VEC3* pRight,
                           const math::VEC3* pLeft, math::VEC3* pPrevAxis,
                           f32 pivot, f32 texV, bool texCoord);

    static void CalcAhead_Younger(math::VEC3* pAxisY, AheadContext* pContext,
                                  Particle* pParticle);
    static void CalcAhead_Both(math::VEC3* pAxisY, AheadContext* pContext,
                               Particle* pParticle);
    static void CalcAhead_BothLoop(math::VEC3* pAxisY, AheadContext* pContext,
                                   Particle* pParticle);
    static void CalcAhead_BothCenter(math::VEC3* pAxisY,
                                     AheadContext* pContext,
                                     Particle* pParticle);
};

} // namespace ef
} // namespace nw4r

#endif
