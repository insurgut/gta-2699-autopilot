#pragma once
#include "forward_navigation.hpp"
struct EnterApproach {
    float distance=0,remaining=0,speedCap=0;bool blocked=false,ready=false;
};
inline EnterApproach approachEnter(const NavigationScene&scene,NavPose car,Point target,float speed){
    float d=std::hypot(target.x-car.x,target.y-car.y),free=scene.freeDistance(car,target.x,target.y);bool blocked=free+.05f<d;
    float left=blocked?std::max(0.0f,free-.3f):std::max(0.0f,d-.25f);
    return {d,left,std::min(4.0f,std::sqrt(4*left)),blocked,(d<=.4f||(blocked&&left<=.3f))&&speed<=.35f};
}
struct EnterStall {
    uint64_t since=0;float x=0,y=0;
    void reset(){since=0;}
    bool update(uint64_t now,NavPose p,float speed,bool demanded){if(!demanded||speed>.25f){reset();return false;}if(!since||std::hypot(p.x-x,p.y-y)>.12f){since=now;x=p.x;y=p.y;}return now-since>=1800;}
};
