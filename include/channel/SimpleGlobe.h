#ifndef CHANNEL_SIMPLE_GLOBE_H
#define CHANNEL_SIMPLE_GLOBE_H
#include <types.h>
#include <nw4r/g3d/g3d_camera.h>

namespace nw4r {
namespace g3d {
class ScnRoot;
}
}

// Globe view state (d_weather_around)
struct GlobeView {
    u8 unk0[0x4];              // at 0x0
    nw4r::g3d::Camera mCamera; // at 0x4
};

// d_scene's m_pSimpleGlobe (size 0xD0)
class SimpleGlobe {
public:
    SimpleGlobe();
    ~SimpleGlobe();

    void Calc();

    nw4r::g3d::ScnRoot* mScnRoot; // at 0x0
    GlobeView* mView;             // at 0x4
    u8 unk8[0xD0 - 0x8];          // at 0x8
};

extern SimpleGlobe* gSimpleGlobe;

#endif
