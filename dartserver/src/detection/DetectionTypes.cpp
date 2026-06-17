#include "DetectionTypes.hpp"

namespace dart::detect {

const char* toString(CamReady s) {
    switch (s) {
        case CamReady::Warmup: return "warmup";
        case CamReady::Normal: return "normal";
        case CamReady::Human:  return "human";
        case CamReady::Clean:  return "clean";
    }
    return "warmup";
}

bool BoardStatus::operator==(const BoardStatus& o) const {
    if (allReady != o.allReady) return false;
    if (round.phase != o.round.phase || round.nextDart != o.round.nextDart ||
        round.message != o.round.message)
        return false;
    for (std::size_t i = 0; i < cams.size(); ++i)
        if (cams[i].state != o.cams[i].state || cams[i].ready != o.cams[i].ready)
            return false;
    return true;
}

} // namespace dart::detect
