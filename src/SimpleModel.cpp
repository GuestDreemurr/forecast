// Single-model nw4r::g3d scene object (the globe, earth.brres)
#include <channel/System.h>
#include <channel/ColorWhite.h>
#include <channel/SimpleModel.h>

#include <nw4r/g3d/res/g3d_resfile.h>
#include <nw4r/g3d/g3d_scnmdlsmpl.h>
#include <nw4r/math.h>

// 3.0f, read from other units (SimpleGlobe ctor, fn_8002B9B0) and negated there
extern const f32 gModelRange = 3.0f;

// Scratch matrix for CalcMtx
extern nw4r::math::MTX34 gModelMtx;

// Not yet decompiled (weather)
void RotateMtxDeg(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);
void TranslateMtx(nw4r::math::MTX34* mtx, f32 x, f32 y, f32 z);

SimpleModel::SimpleModel(void* resData) : mTrans(0.0f, 0.0f, 0.0f), mRot(0.0f, 0.0f, 0.0f), mScale(21.0f, 21.0f, 21.0f) {
    nw4r::g3d::ResFile file(resData);
    u32 size;

    file.Init();
    file.Bind();
    mResMdl = file.GetResMdl(0);
    mScnMdl = nw4r::g3d::ScnMdlSimple::Construct(&gMEM2Allocator32, &size, mResMdl, 1);
}

SimpleModel::~SimpleModel() {
    mScnMdl->Destroy();
}

// Empty in this build
void SimpleModel::Calc() {}

void SimpleModel::UpdateMtx() {
    mMtx = CalcMtx(mRot);
    mScnMdl->SetMtx(nw4r::g3d::ScnObj::MTX_LOCAL, &mMtx);
}

nw4r::math::MTX34 SimpleModel::CalcMtx(const nw4r::math::VEC3& rot) {
    nw4r::math::MTX34RotXYZFIdx(&gModelMtx, 0.0f, 0.7111111f * rot.y, 0.0f);
    f32 z = rot.z;
    f32 x = rot.x;
    RotateMtxDeg(&gModelMtx, x, 0.0f, z);
    TranslateMtx(&gModelMtx, mTrans.x, mTrans.y, mTrans.z);
    return gModelMtx;
}

// Empty in this build
void SimpleModel::Draw() {}
