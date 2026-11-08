#pragma once
#include <cstdint>
struct SlowdownConfirmation {
    uint64_t since=0;
    bool update(uint64_t now,bool low){if(!low){since=0;return false;}if(!since)since=now;return now-since>=500;}
    void reset(){since=0;}
};
inline bool canResumeBlocked(bool complete,bool fresh,bool clear,float speed){return complete&&fresh&&clear&&speed<0.45f;}
