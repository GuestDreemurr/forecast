#ifndef CHANNEL_TWEEN_H
#define CHANNEL_TWEEN_H
#include <types.h>

// A value that moves toward a target by a fixed step every Update()
class Tween {
public:
    Tween();
    ~Tween() {}

    void Update();

    f32 mValue;  // at 0x0
    f32 mTarget; // at 0x4
    f32 mStep;   // at 0x8
};

#endif
