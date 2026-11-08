#pragma once
#include <cstdint>
struct FrameLimit40 {
    static constexpr uint64_t period=25000000; // monotonic nanoseconds, 40 frames/second
    uint64_t last=0;bool started=false;
    bool due(uint64_t now){if(started&&now>=last&&now-last<period)return false;last=now;started=true;return true;}
    void reset(){started=false;}
};
