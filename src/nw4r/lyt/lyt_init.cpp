#include <nw4r/lyt.h>

#include <revolution/OS.h>

namespace nw4r {
namespace lyt {

// This version registers no library version string.
void LytInit() {
    OSInitFastCast();
}

} // namespace lyt
} // namespace nw4r
