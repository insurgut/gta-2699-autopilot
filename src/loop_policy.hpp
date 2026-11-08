#pragma once
#include "route.hpp"
// Repetition follows a recorded closed circuit, never an unrecorded return leg.
inline bool closedLoopRoute(const std::vector<Point>&r){
    return r.size()>1&&distance(r.front(),r.back())<=4.0f;
}
inline bool loopRestartReady(bool enabled,const std::vector<Point>&r,Point car,float speed,bool enterPending){
    return enabled&&closedLoopRoute(r)&&distance(car,r.front())<=6.0f&&speed<.45f&&!enterPending;
}
