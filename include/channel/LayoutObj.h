#ifndef CHANNEL_LAYOUTOBJ_H
#define CHANNEL_LAYOUTOBJ_H
#include <types.h>

// Wraps an nw4r::lyt layout loaded from an archive (0x12C bytes)
class LayoutObj {
public:
    LayoutObj(const void* archive, const char* layoutName, s32 arg);
    ~LayoutObj();

    void Reset();
    void Calc();
    void Draw();

    u8 unk0[0x12C]; // at 0x0
};

#endif
