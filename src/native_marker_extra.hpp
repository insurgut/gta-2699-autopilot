#pragma once
#include "native_marker_protocol.hpp"
struct MarkerGroundCache {
    float x=0,y=0,z=0,ground=0;uint64_t refreshed=0;bool initialized=false;
    bool changed(const NativeMarkerState&s)const{return !initialized||x!=s.x||y!=s.y||z!=s.z;}
    bool due(const NativeMarkerState&s,uint64_t now)const{return changed(s)||now<refreshed||now-refreshed>=1000;}
    void update(const NativeMarkerState&s,uint64_t now,bool found,float candidate){
        if(changed(s))ground=s.z-.7f;
        if(found&&std::isfinite(candidate)&&std::abs(candidate-s.z)<8)ground=candidate;
        x=s.x;y=s.y;z=s.z;refreshed=now;initialized=true;
    }
};
struct NativeMarkerTest {
    bool armed=false;uint64_t until=0,baselineFrames=0;
    void request(){armed=true;until=0;baselineFrames=0;}
    bool begin(uint64_t now,bool ready,bool focused,uint64_t frames){if(!armed||!ready||!focused)return false;armed=false;until=now+10000;baselineFrames=frames;return true;}
    bool active(uint64_t now)const{return until&&now<until;}
    bool finished(uint64_t now)const{return until&&now>=until;}
    void reset(){*this={};}
};
