#ifndef NW4R_EF_DRAW_STRATEGY_BUILDER_H
#define NW4R_EF_DRAW_STRATEGY_BUILDER_H
#include <nw4r/types_nw4r.h>

namespace nw4r {
namespace ef {

// Forward declarations
class DrawStrategy;

enum DrawStrategyType {
    DRAWSTRATEGY_POINT,
    DRAWSTRATEGY_LINE,
    DRAWSTRATEGY_FREE,
    DRAWSTRATEGY_BILLBOARD,
    DRAWSTRATEGY_DIRECTIONAL,
    DRAWSTRATEGY_STRIPE
};

class DrawStrategyBuilder {
public:
    static DrawStrategy* Create(u32 type);
};

} // namespace ef
} // namespace nw4r

#endif
