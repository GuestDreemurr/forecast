#ifndef CHANNEL_VECTOR2_H
#define CHANNEL_VECTOR2_H
#include <types.h>
#include <revolution/MTX.h>
#include <nw4r/math/math_types.h>

// A Vec2 with a destructor
class Vector2 : public Vec2 {
public:
    Vector2() {}
    Vector2(f32 x, f32 y) {
        this->x = x;
        this->y = y;
    }
    explicit Vector2(const nw4r::math::VEC2& v) {
        x = v.x;
        y = v.y;
    }
    ~Vector2() {}
};

#endif
