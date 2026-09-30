#ifndef CHANNEL_VECTOR2_H
#define CHANNEL_VECTOR2_H
#include <types.h>
#include <revolution/MTX.h>

// A Vec2 with a (non-inline) destructor
class Vector2 : public Vec2 {
public:
    Vector2() {}
    Vector2(f32 x, f32 y) {
        this->x = x;
        this->y = y;
    }
    ~Vector2();
};

#endif
