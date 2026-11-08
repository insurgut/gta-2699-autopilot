#pragma once
#include "route.hpp"
#include "speed_profile.hpp"
#include "route_sections.hpp"
#include <limits>
#include <cstdint>
struct DriveOutput {float steering,targetSpeed,error;size_t segment;bool arrived;};
// Small recorded steering taps should not cause repeated speed dips. Steering
// still follows the path; large bends and substantial tracking errors retain
// the original lateral-acceleration limit.
inline float bendSpeedLimit(float angle,float goalDistance,float cap,float trackingError){
    float curvature=2*std::abs(std::sin(angle))/std::max(4.0f,goalDistance);
    float limited=curvature>.001f?std::min(cap,std::sqrt(4.0f/curvature)):cap;
    constexpr float small=10*3.14159265359f/180,full=20*3.14159265359f/180;
    float mix=trackingError>1.5f?1.0f:std::clamp((std::abs(angle)-small)/(full-small),0.0f,1.0f);
    mix=mix*mix*(3-2*mix);
    return cap+(limited-cap)*mix;
}
inline DriveOutput driveOutput(const std::vector<Point>&r,size_t from,Point pos,float fx,float fy,float speed,float cap,size_t stopAt=SIZE_MAX,float lateralShift=0,bool anticipate=true,float maxLook=65.0f,float stopDeceleration=4.5f){
    if(r.size()<2)return {0,0,1e6f,0,false};
    size_t end=std::min(r.size()-1,stopAt);if(end==0)return {0,0,distance(pos,r.front()),0,true};
    from=std::min(from,end-1);
    float best=std::numeric_limits<float>::infinity(),bestT=0;size_t bestI=from;
    // Limited forward search prevents jumping between remote portions of a crossing.
    size_t search=std::max(size_t(8),size_t(speed*0.2f));
    for(size_t i=from;i<end&&i<=from+search;i++){
        auto a=r[i],b=r[i+1];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
        float length=dx*dx+dy*dy+dz*dz;
        float t=length>1e-4f?std::clamp(((pos.x-a.x)*dx+(pos.y-a.y)*dy+(pos.z-a.z)*dz)/length,0.0f,1.0f):0;
        Point p{a.x+t*dx,a.y+t*dy,a.z+t*dz,0};float d=distance(pos,p);
        if(d<best){best=d;bestT=t;bestI=i;}
    }
    float defaultCap=cap;cap=routeSpeedAt(r[bestI],r[bestI+1],bestT,cap);
    auto a=r[bestI],b=r[bestI+1];Point goal{a.x+(b.x-a.x)*bestT,a.y+(b.y-a.y)*bestT,a.z+(b.z-a.z)*bestT,a.speed+(b.speed-a.speed)*bestT};
    float look=std::clamp(4.0f+speed*0.65f,4.0f,maxLook);size_t i=bestI+1;
    while(i<=end){
        float d=distance(goal,r[i]);if(d>=look&&d>0){float t=look/d;goal.x+=(r[i].x-goal.x)*t;goal.y+=(r[i].y-goal.y)*t;goal.z+=(r[i].z-goal.z)*t;break;}
        look-=d;goal=r[i];++i;
    }
    goal.x+=fy*lateralShift;goal.y-=fx*lateralShift;
    float dx=goal.x-pos.x,dy=goal.y-pos.y;
    float angle=std::atan2(fy*dx-fx*dy,fx*dx+fy*dy); // positive => right key D
    float steering=std::clamp(angle*1.45f,-1.0f,1.0f);
    // Low speed at large heading errors also permits a forward U-turn for return trips.
    float target=anticipate?anticipatedSpeed(r,bestI,pos,speed,cap,end):cap;
    target=std::min(target,routeSpeedAheadCap(r,bestI,pos,defaultCap,end));
    // No hidden straight-line speed ceiling; bends are constrained by lateral acceleration.
    if(anticipate){target=std::min(target,bendSpeedLimit(angle,std::hypot(dx,dy),cap,best));if(std::abs(angle)>1.4f)target=std::min(target,3.0f);}
    // Measure remaining path length, rather than distance through a corner.
    float remaining=distance(pos,r[bestI+1]);
    for(size_t j=bestI+1;j<end;j++)remaining+=distance(r[j],r[j+1]);
    target=std::min(target,std::sqrt(2*stopDeceleration*std::max(0.0f,remaining-1.0f)));
    float finalDist=distance(pos,r[end]);
    bool arrived=bestI+3>=end&&finalDist<(stopAt<r.size()?1.8f:3.0f);
    return {steering,arrived?0:target,best,bestI,arrived};
}
inline DriveOutput gearDriveOutput(const std::vector<Point>&r,size_t from,Point pos,float fx,float fy,float speed,float cap,int direction,size_t stopAt=SIZE_MAX,bool anticipate=true,float maxLook=65.0f,float stopDeceleration=4.5f){
    if(stopAt==SIZE_MAX&&!r.empty()&&r.back().enterCount)stopAt=r.size()-1;
    auto out=driveOutput(r,from,pos,fx*direction,fy*direction,speed,cap,stopAt,0,anticipate,maxLook,stopDeceleration);
    out.steering*=direction;return out;
}
