#ifndef CHANNEL_COLOR_H
#define CHANNEL_COLOR_H
#include <types.h>

class Color {
public:
    Color(); // white
    Color(u8 r, u8 g, u8 b, u8 a) {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }
    Color(u32 rgba) {
        *(u32*)this = rgba;
    }
    ~Color();

    u8 r; // at 0x0
    u8 g; // at 0x1
    u8 b; // at 0x2
    u8 a; // at 0x3
};

#endif
