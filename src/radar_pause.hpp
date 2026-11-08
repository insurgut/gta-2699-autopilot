#pragma once
#include <cstdint>
inline bool processingReady(bool valid,bool sameCar,bool radar,uint32_t state,uint32_t failureReason,bool gridComplete){
    return valid&&sameCar&&radar&&state==1&&failureReason==0&&gridComplete;
}
struct FreshSceneGate {
    uint64_t first=0,lastBeat=0;unsigned frames=0;
    void reset(){first=lastBeat=0;frames=0;}
    bool update(uint64_t now,uint64_t beat,bool valid){
        if(!valid||!beat||beat>now||now-beat>400){reset();return false;}
        if(beat<lastBeat){reset();return false;}
        if(beat!=lastBeat){if(!frames)first=now;lastBeat=beat;++frames;}
        return frames>=3&&now-first>=200;
    }
};
inline bool poolLayoutValid(uintptr_t list,uint32_t capacity){return capacity<=10000&&(capacity==0||list>0x10000);}
