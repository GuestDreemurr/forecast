#include "SimpleGlobe.h"
#include "SimpleModel.h"
#include "System.h"
#include "nw4r/g3d/g3d_scnroot.h"
#include <cstddef>

const f32 lbl_8018F730[10] = {
    1.0f, 2.0f, 5.0f, 8.0f, 12.0f,
    17.0f, 25.0f, 40.0f, 65.0f, 100.0f,
};

const f32 gGlobeZooms[6] = {
    0.0f, 45.0f, 55.0f, 65.0f, 73.0f, 80.0f,
};

SimpleGlobe::SimpleGlobe() :
mScnRoot(NULL), mView(NULL),
unk8(0.0f), unkC(0.0f), unk10(0.0f), unk14(0.0f),
unk18(gModelRange), unk1C(0.0f), unk20(0.0f),
unk24(-gModelRange), unk28(0.0f)
{
    u32 val;

    this->mSpeed.x = 0.0f;
    this->mSpeed.y = 0.0f;

    this->unk74 = 0.0f;
    this->unk78 = 0.0f;
    this->unk7C = 0.0f;
    this->unk80 = 0.0f;
    this->unk84 = 0.0f;
    this->unk88 = 0.0f;

    this->mSpinning = 0;
    this->mZoomIn = 0;
    this->mZoomOut = 0;
    this->mTiltUp = 0;
    this->mTiltDown = 0;
    this->unk95 = 0;
    this->unk96 = 0;
    this->unk97 = 0;
    this->unk98 = 0;
    this->unk99 = 0;
    this->unk9A = 0;
    this->unk9B = 0;

    this->mZoomLevel = 0;
    this->mTiltLevel = 0;

    this->unkB4 = lbl_8018F730[8];
    this->unkB8 = lbl_8018F730[8];
    this->unkBC = 0.0f;
    this->unkC0 = 0.0f;
    this->mZoom = 1.0f;

    this->mScnRoot = nw4r::g3d::ScnRoot::Construct(&gMEM2Allocator, &val, 0x1F, 0x100, 0x80, 0x80);
    this->mScnRoot->SetCurrentCamera(0);

    this->mView = new GlobeView(this->mScnRoot->GetCurrentCamera());
    this->mView->mZoom = this->unkB4;

    this->mGrabbed[0] = 0;
    this->mGrabbed[1] = 0;
    this->mGrabbed[2] = 0;
    this->mGrabbed[3] = 0;
}

SimpleGlobe::~SimpleGlobe() {
}
