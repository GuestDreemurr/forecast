#ifndef CHANNEL_VEC2F_H
#define CHANNEL_VEC2F_H
#include <types.h>
#include <revolution/MTX.h>

// Vec2 with a destructor, which makes functions return it through a hidden pointer
class Vec2F : public Vec2 {
public:
    Vec2F() {}
    Vec2F(f32 x, f32 y) {
        this->x = x;
        this->y = y;
    }
    ~Vec2F() {}
};

#endif
