#pragma once
#include "scene_geometry.hpp"
#include "wait_approach.hpp"
#include <cstdint>
struct WaitPose {uint64_t id=0;ScenePoint origin{},right{},forward{},up{};SceneFootprint footprint{};};
struct WaitCycle {
    uint64_t id=0,motionCandidate=0,motionStarted=0,baselineSince=0;
    bool candidateAbsent=false,absenceMotion=false,passing=false;
    unsigned delayMs=5000;WaitApproach approach{};WaitPose closed{};float travelX=0,travelY=1;
    void reset(){*this={};}
    void begin(const WaitPose&p,float fx,float fy,unsigned delay=5000,WaitApproach profile={}){reset();approach=profile;delayMs=std::min(delay,3600000u);id=p.id;closed=p;float n=std::hypot(fx,fy);if(n>0.5f){travelX=fx/n;travelY=fy/n;}}
    bool changed(const WaitPose&p)const{
        auto delta=[](ScenePoint a,ScenePoint b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;};
        return delta(p.origin,closed.origin)>0.12f*0.12f||delta(p.right,closed.right)>0.04f*0.04f||delta(p.forward,closed.forward)>0.04f*0.04f||delta(p.up,closed.up)>0.04f*0.04f;
    }
    void observe(uint64_t now,const WaitPose*pose){
        if(!id||passing)return;
        bool moved=pose&&pose->id==id&&changed(*pose);
        if(pose&&!moved){
            motionCandidate=0;
            if(absenceMotion){motionStarted=0;absenceMotion=false;}
            if(!baselineSince)baselineSince=now;
            if(motionStarted&&now-baselineSince>=500)motionStarted=0;
            return;
        }
        baselineSince=0;
        if(motionStarted){if(moved)absenceMotion=false;return;}
        if(!motionCandidate||candidateAbsent!=(pose==nullptr)){motionCandidate=now;candidateAbsent=pose==nullptr;}
        if(now>=motionCandidate&&now-motionCandidate>=80){motionStarted=motionCandidate;absenceMotion=pose==nullptr;}
    }
    bool delayComplete(uint64_t now)const{return motionStarted&&now>=motionStarted&&now-motionStarted>=delayMs;}
    uint64_t remaining(uint64_t now)const{return !motionStarted?delayMs:(delayComplete(now)?0:delayMs-(now-motionStarted));}
    bool releaseReady(uint64_t now,bool processed,bool clear)const{return processed&&clear&&delayComplete(now);}
    void startPass(){passing=true;}
    bool past(ScenePoint car,float halfLength)const{
        const auto&b=closed.footprint;float dx=car.x-b.center.x,dy=car.y-b.center.y;
        float dot=travelX*b.forward.x+travelY*b.forward.y,cross=travelX*b.forward.y-travelY*b.forward.x;
        float extent=std::abs(dot)*b.halfLength+std::abs(cross)*b.halfWidth;
        return dx*travelX+dy*travelY>extent+halfLength+2||std::hypot(dx,dy)>std::max(45.0f,extent+halfLength+15);
    }
};
