#ifndef NW4R_EF_EMITTER_FORM_H
#define NW4R_EF_EMITTER_FORM_H
#include <nw4r/types_nw4r.h>

#include <nw4r/math.h>

#include <revolution/GX.h>

namespace nw4r {
namespace ef {

// Forward declarations
class Emitter;
class ParticleManager;

class EmitterForm {
public:
    EmitterForm() {}

    virtual void Emission(Emitter* pEmitter, ParticleManager* pManager,
                          int count, u32 flags, f32* pParams, u16 life,
                          f32 lifeRnd, const math::MTX34* pSpace) = 0; // at 0x8

    virtual void Draw(Emitter* pEmitter,
                      const math::MTX34& rViewMtx) = 0; // at 0xC

    void CalcVelocity(math::VEC3* pVel, Emitter* pEmitter,
                      const math::VEC3& rPos, const math::VEC3& rNormal,
                      const math::VEC3& rFromOrigin,
                      const math::VEC3& rFromYAxis) const;

    u16 CalcLife(u16 life, f32 lifeRnd, Emitter* pEmitter);

protected:
    // Graphics state shared by every form's wireframe preview
    static void SetupDrawGX() {
        GXSetNumChans(1);

        GXColor color;
        color.r = color.g = color.b = color.a = 128;

        GXSetChanMatColor(GX_COLOR0A0, color);
        GXSetChanAmbColor(GX_COLOR0A0, color);
        GXSetChanCtrl(GX_COLOR0A0, FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL,
                      GX_DF_NONE, GX_AF_NONE);

        GXSetNumTexGens(0);
        GXSetNumIndStages(0);
        GXSetNumTevStages(1);
        GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP_NULL, GX_COLOR0A0);
        GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

        GXSetFog(GX_FOG_NONE, color, 0.0f, 1.0f, 0.0f, 1.0f);
        GXSetZMode(TRUE, GX_LESS, FALSE);
        GXSetZCompLoc(TRUE);
        GXSetCullMode(GX_CULL_BACK);
        GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
                       GX_LO_CLEAR);
        GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    }
};

} // namespace ef
} // namespace nw4r

#endif
