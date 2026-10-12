#ifndef CHANNEL_COLOR_H
#define CHANNEL_COLOR_H
#include <types.h>

#include <nw4r/ut/ut_Color.h>

// The channel's Color is nw4r::ut::Color: nw4r's own CharWriter registers the
// same destructor (0x800074A0) that the channel code calls.
typedef nw4r::ut::Color Color;

#endif
