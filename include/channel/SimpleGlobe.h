#ifndef CHANNEL_SIMPLE_GLOBE_H
#define CHANNEL_SIMPLE_GLOBE_H
#include <types.h>
#include <nw4r/g3d/g3d_camera.h>
#include <revolution/MTX.h>

namespace nw4r {
namespace g3d {
class ScnRoot;
}
}

// Globe view state (d_weather_around)
struct GlobeView {
    u8 unk0[0x4];              // at 0x0
    nw4r::g3d::Camera mCamera; // at 0x4
    u8 unk8[0x90 - 0x8];       // at 0x8
    f32 mLatitude;             // at 0x90
    f32 mLongitude;            // at 0x94
    u8 unk98[0x9C - 0x98];     // at 0x98
    f32 mTilt;                 // at 0x9C
    u8 unkA0[0xA4 - 0xA0];     // at 0xA0
    f32 mRotation;             // at 0xA4
    u8 unkA8[0xC0 - 0xA8];     // at 0xA8
    u8 mResetting;             // at 0xC0
    u8 unkC1[0xD4 - 0xC1];     // at 0xC1
    f32 mZoom;                 // at 0xD4
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

    GlobeView* GetView() {
        return mView;
    }

    nw4r::g3d::ScnRoot* mScnRoot; // at 0x0
    GlobeView* mView;             // at 0x4
    u8 unk8[0x6C - 0x8];          // at 0x8
    Vec2 mSpeed;                  // at 0x6C, rotation speed
    u8 unk74[0x8C - 0x74];        // at 0x74
    u8 mGrabbed[4];               // at 0x8C
    u8 mSpinning;                 // at 0x90
    u8 mZoomIn;                   // at 0x91
    u8 mZoomOut;                  // at 0x92
    u8 mTiltUp;                   // at 0x93
    u8 mTiltDown;                 // at 0x94
    u8 unk95[0x9C - 0x95];        // at 0x95
    s32 mZoomLevel;               // at 0x9C
    s32 mTiltLevel;               // at 0xA0
    u8 unkA4[0xC8 - 0xA4];        // at 0xA4
    f32 mZoom;                    // at 0xC8
    u8 unkCC[0xD0 - 0xCC];        // at 0xCC
};


extern SimpleGlobe* gSimpleGlobe;

#endif
