#ifndef CHANNEL_SIMPLE_GLOBE_H
#define CHANNEL_SIMPLE_GLOBE_H
#include "Vec3.h"
#include "Vector2.h"
#include "nw4r/g3d/g3d_scnroot.h"
#include <types.h>
#include <nw4r/g3d/g3d_camera.h>
#include <revolution/MTX.h>

// Camera that looks at the globe (d_weather_around)
class GlobeView {
public:
    GlobeView(nw4r::g3d::Camera camera);
    virtual ~GlobeView();

    void unk10(Vec *);
    void CalcCamera();
    void StartReset();
    BOOL IsSettled();
    BOOL UpdateReset();
    BOOL Approach(f32 target, f32* value);
    void Project(f32* screen, const Vec* world);

    nw4r::g3d::Camera *getCamera() { return &mCamera; }
    f32 getFOVy() { return mFOVy; }
    f32 getAspect() { return mAspect; }
    f32 getNear() { return mNear; }
    f32 getFar() { return mFar; }
    Vec *getPosition() { return &mPosition; }
    Vec *getOrientation() { return &mOrientation; }
    u8 getResetting() { return mResetting; }

    void setZoom(f32 zoom) { mZoom = zoom; }

    nw4r::g3d::Camera mCamera; // at 0x4
    Mtx mCameraMtx;            // at 0x8
    Mtx44 mProjMtx;            // at 0x38
    Vec mUp;                   // at 0x78
    Vec mTarget;               // at 0x84
    Vec mPosition;             // at 0x90
    Vec mOrientation;          // at 0x9C
    Vec mCameraPos;            // at 0xA8
    Vec mDirection;            // at 0xB4
    u8 mResetting;             // at 0xC0
    u8 unkC1[0xC4 - 0xC1];     // at 0xC1
    f32 mFOVy;                 // at 0xC4
    f32 mAspect;               // at 0xC8
    f32 mNear;                 // at 0xCC
    f32 mFar;                  // at 0xD0
    f32 mZoom;                 // at 0xD4
    f32 mViewportX;            // at 0xD8
    f32 mViewportY;            // at 0xDC
    f32 mViewportW;            // at 0xE0
    f32 mViewportH;            // at 0xE4
    u16 unkE8;                 // at 0xE8
    u8 unkEA[0xEC - 0xEA];     // at 0xEA
};

// Hack
class Vector3 : public Vec {
public:
    Vector3(f32 x, f32 y, f32 z) {
        this->x = x;
        this->y = y;
        this->z = z;
    }
};

// d_scene's m_pSimpleGlobe (size 0xD0)
class SimpleGlobe {
public:
    SimpleGlobe();
    ~SimpleGlobe();

    void Calc();
    void SetRotation(const Vec* rotation, s32 frames);
    void SetZoom(s32 level, s32 frames);
    void SetMode(s32 mode);
    void SetSpeed(f32 speed);
    void Setup(const Vec* rotation);
    void DrawModel();
    void Draw();
    void ClearInput();
    void UpdateView();
    void UpdateFacing();
    void UpdateLight();
    void UpdateZoom(const s32* sounds);
    void SyncZoom();
    BOOL UpdateGrab(s32 chan);
    s32 UpdateDrag(s32 chan);
    BOOL IsDefaultView();
    void UpdateTilt(u32 arg, const s32* sounds);
    void UpdateRotation(u32 stop);
    void SetZoomLevel(s32 level);
    void PlayRotateSound(u32 id);
    void UpdateWater();

    GlobeView* GetView() {
        return mView;
    }

    nw4r::g3d::ScnRoot* mScnRoot; // at 0x0
    GlobeView* mView;             // at 0x4
    Vector3 mRotation;            // at 0x8
    f32 unk14;                    // at 0x14
    f32 unk18;                    // at 0x18
    f32 unk1C;                    // at 0x1C
    f32 unk20;                    // at 0x20
    f32 unk24;                    // at 0x24
    f32 unk28;                    // at 0x28
    Vector2 unk2C[4];             // at 0x2C
    Vector2 unk4C[4];             // at 0x4C
    Vec2 mSpeed;                  // at 0x6C, rotation speed
    f32 unk74;
    f32 unk78;
    f32 unk7C;
    f32 unk80;
    f32 unk84;
    f32 unk88;
    u8 mGrabbed[4];               // at 0x8C
    u8 mSpinning;                 // at 0x90
    u8 mZoomIn;                   // at 0x91
    u8 mZoomOut;                  // at 0x92
    u8 mTiltUp;                   // at 0x93
    u8 mTiltDown;                 // at 0x94
    u8 unk95;
    u8 unk96;
    u8 unk97;
    u8 unk98;
    u8 unk99;
    u8 unk9A;
    u8 unk9B;
    s32 mZoomLevel;               // at 0x9C
    s32 mTiltLevel;               // at 0xA0
    f32 mRoll[4];                 // at 0xA4, remote roll angle per controller
    f32 unkB4;
    f32 unkB8;
    f32 unkBC;
    f32 unkC0;
    f32 unkC4;
    f32 mZoom;                    // at 0xC8
    f32 unkCC;                    // at 0xCC
};


extern SimpleGlobe* gSimpleGlobe;

#endif
