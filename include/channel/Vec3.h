#ifndef CHANNEL_VEC3_H
#define CHANNEL_VEC3_H
#include <types.h>
#include <revolution/MTX.h>

// Vec with an (empty) constructor and destructor, so arrays of it get constructed and destroyed
class Vec3 : public Vec {
public:
    Vec3();
    ~Vec3();
};

#endif
