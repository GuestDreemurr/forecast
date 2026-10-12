#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {
namespace detail {

/**
 * Dummy class to instantiate necessary weak functions
 */
class MidiSeqPlayer : public SeqPlayer {
private:
    MidiSeqPlayer();
};

MidiSeqPlayer::MidiSeqPlayer() {}

} // namespace detail
} // namespace snd
} // namespace nw4r
