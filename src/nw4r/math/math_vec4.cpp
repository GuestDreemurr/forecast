// The original name of this file is unknown. It holds a single function
// that sits between the math and lyt libraries.
#include <channel/MtxUtil.h>

void MultVec4(f32* out, const Mtx44 m, const Vec* v) {
    f32 tmp[4];
    tmp[0] = m[0][0] * v->x + m[0][1] * v->y + m[0][2] * v->z + m[0][3];
    tmp[1] = m[1][0] * v->x + m[1][1] * v->y + m[1][2] * v->z + m[1][3];
    tmp[2] = m[2][0] * v->x + m[2][1] * v->y + m[2][2] * v->z + m[2][3];
    tmp[3] = m[3][0] * v->x + m[3][1] * v->y + m[3][2] * v->z + m[3][3];
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
    out[3] = tmp[3];
}
