#include "SimpleGlobe.h"
#include "SceneBase.h"
#include "SimpleModel.h"
#include "System.h"
#include "nw4r/g3d/g3d_camera.h"
#include "nw4r/g3d/g3d_scnobj.h"
#include "nw4r/g3d/g3d_scnroot.h"
#include "revolution/MTX/mtxtypes.h"
#include <cstddef>

const f32 lbl_8018F730[10] = {
    1.0f, 2.0f, 5.0f, 8.0f, 12.0f,
    17.0f, 25.0f, 40.0f, 65.0f, 100.0f,
};

const f32 gGlobeZooms[6] = {
    0.0f, 45.0f, 55.0f, 65.0f, 73.0f, 80.0f,
};

f32 speedX = 0.0f;
f32 speedY = 0.0f;

SimpleGlobe::SimpleGlobe() :
mScnRoot(NULL), mView(NULL),
mRotation(0.0f, 0.0f, 0.0f),
unk14(0.0f), unk18(gModelRange), unk1C(0.0f),
unk20(0.0f), unk24(-gModelRange), unk28(0.0f)
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
    this->mView->setZoom(this->unkB4);

    this->mGrabbed[0] = 0;
    this->mGrabbed[1] = 0;
    this->mGrabbed[2] = 0;
    this->mGrabbed[3] = 0;
}

SimpleGlobe::~SimpleGlobe() {
    delete this->mView;
    this->mScnRoot->Destroy();
}

void SimpleGlobe::SetRotation(const Vec* rotation, s32 frames) {
    nw4r::g3d::ScnRoot *scnRoot = this->mScnRoot;
    this->mZoomIn = 0;
    this->mZoomOut = 0;
    this->mTiltUp = 0;
    this->mTiltDown = 0;

    while (scnRoot->Size() != 0) {
        u32 i = scnRoot->Size();
        if (i > 0) {
            scnRoot->Remove(--i);
        }
    }

    if (gEarthModel != 0) {
        gEarthModel->Calc();
        this->mScnRoot->Insert(this->mScnRoot->Size(), (nw4r::g3d::ScnObj *)gEarthModel->mScnMdl);
    }

    this->mGrabbed[0] = 0;
    this->mGrabbed[1] = 0;
    this->mGrabbed[2] = 0;
    this->mGrabbed[3] = 0;

    this->mRotation = *static_cast<const Vector3 *>(rotation);

    this->mSpinning = 0;

    this->mSpeed.x = speedX;
    this->mSpeed.y = speedY;

    this->mZoomLevel = frames;

    this->unkB4 = lbl_8018F730[frames];
    this->unkB8 = lbl_8018F730[frames];

    this->mView->setZoom(lbl_8018F730[frames]);

    this->mView->unk10(&this->mRotation);

    nw4r::g3d::Camera camera = this->mScnRoot->GetCamera(1);
    camera.Init(gRenderMode.fbWidth, gRenderMode.efbHeight, gRenderMode.fbWidth, gRenderMode.xfbHeight, (gWidescreen) ? 0x340 : 0x260, 0x1C8);
    camera.SetPerspective(this->mView->getFOVy(), this->mView->getAspect(), this->mView->getNear(), this->mView->getFar());
    camera.SetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    camera.SetViewport(0.0f, 0.0f, (int)gRenderMode.fbWidth, (int)gRenderMode.efbHeight);
}

Vec GetPosition(GlobeView *view) {
    return *view->getPosition();
}

Vec GetOrientation(GlobeView *view) {
    return *view->getOrientation();
}
