#ifndef CHANNEL_SIMPLEMODEL_H
#define CHANNEL_SIMPLEMODEL_H
#include <types.h>
#include <nw4r/g3d/res/g3d_resmdl.h>

namespace nw4r {
namespace g3d {
class ScnMdlSimple;
}
}
#include <nw4r/math.h>

// A single-model nw4r::g3d scene object (used for the globe, earth.brres)
class SimpleModel {
public:
    SimpleModel(void* resData);
    virtual ~SimpleModel();

    void Calc();
    void UpdateMtx();
    void Draw();
    nw4r::math::MTX34 CalcMtx(nw4r::math::VEC3& rot);

    u32 unk4;                          // at 0x4
    nw4r::g3d::ResMdl mResMdl;         // at 0x8
    nw4r::g3d::ScnMdlSimple* mScnMdl;  // at 0xC
    nw4r::math::MTX34 mMtx;            // at 0x10
    nw4r::math::VEC3 mTrans;           // at 0x40
    nw4r::math::VEC3 mRot;             // at 0x4C
    nw4r::math::VEC3 mScale;           // at 0x58
};

extern const f32 gModelRange;

#endif
