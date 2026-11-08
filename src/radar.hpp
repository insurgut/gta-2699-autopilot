#pragma once
#include <algorithm>
#include <array>
#include <cmath>
struct RadarBox {float x,y,z,fx,fy,halfWidth,halfLength,halfHeight;unsigned passability=0;};
constexpr unsigned WAIT_OBJECT=4;
constexpr unsigned WAIT_PASS=5;
inline bool passableRule(unsigned value){return (value>=1&&value<=3)||value==WAIT_PASS;}
inline float corridorObjectDistance(float x,float y,float z,float fx,float fy,const RadarBox*boxes,size_t count,float halfWidth,float halfLength,float range,bool waitingOnly){
    float nearest=range+1,norm=std::hypot(fx,fy);if(norm<0.5f)return nearest;fx/=norm;fy/=norm;
    for(size_t i=0;i<count;i++){
        const auto&b=boxes[i];if((waitingOnly?b.passability!=WAIT_OBJECT:(b.passability==WAIT_OBJECT||passableRule(b.passability)))||std::abs(b.z-z)>b.halfHeight+1.5f)continue;
        if(!waitingOnly&&(b.z+b.halfHeight<z+0.1f||b.z-b.halfHeight>z+1.6f))continue;
        float n=std::hypot(b.fx,b.fy);if(n<0.5f)continue;
        float dot=(b.fx*fx+b.fy*fy)/n,cross=(b.fx*fy-b.fy*fx)/n;
        float side=std::abs(cross)*b.halfLength+std::abs(dot)*b.halfWidth;
        float front=std::abs(dot)*b.halfLength+std::abs(cross)*b.halfWidth;
        float dx=b.x-x,dy=b.y-y,along=dx*fx+dy*fy;
        if(along+front< -halfLength||std::abs(dx*fy-dy*fx)>side+halfWidth+0.5f)continue;
        nearest=std::min(nearest,std::max(0.0f,along-front-halfLength));
    }
    return nearest;
}
inline float waitingObjectDistance(float x,float y,float z,float fx,float fy,const RadarBox*boxes,size_t count,float halfWidth,float halfLength,float range){
    return corridorObjectDistance(x,y,z,fx,fy,boxes,count,halfWidth,halfLength,range,true);
}
constexpr float WAIT_STOP_GAP=0.5f;
inline float waitingApproachSpeed(float gap){
    if(!std::isfinite(gap)||gap<=WAIT_STOP_GAP)return 0;
    float cap=gap<12?20.0f/3.6f:25.0f/3.6f;
    return std::min(cap,std::sqrt(4*std::max(0.0f,gap-WAIT_STOP_GAP)));
}
struct RadarResult {std::array<float,5> clear;float range;};
// Five parallel swept corridors, all aligned with the vehicle, indexed left to right.
inline RadarResult radarScan(float x,float y,float z,float fx,float fy,const RadarBox*boxes,size_t count,float range,float vehicleHalfWidth=1.0f){
    RadarResult out{{range,range,range,range,range},range};float norm=std::hypot(fx,fy);if(norm<0.5f)return out;fx/=norm;fy/=norm;
    for(size_t n=0;n<count;n++){
        const auto&b=boxes[n];if(passableRule(b.passability)||std::abs(b.z-z)>b.halfHeight+1.5f)continue;
        float dx=b.x-x,dy=b.y-y,lateral=dx*fy-dy*fx,along=dx*fx+dy*fy-3.0f;
        float bf=std::hypot(b.fx,b.fy);if(bf<0.5f)continue;
        float dot=(b.fx*fx+b.fy*fy)/bf,cross=(b.fx*fy-b.fy*fx)/bf;
        float sideExtent=std::abs(cross)*b.halfLength+std::abs(dot)*b.halfWidth;
        float frontExtent=std::abs(dot)*b.halfLength+std::abs(cross)*b.halfWidth;
        if(along+frontExtent<0)continue;
        for(int lane=0;lane<5;lane++)if(std::abs(lateral-float(lane-2)*2.2f)<sideExtent+vehicleHalfWidth+0.35f)
            out.clear[lane]=std::min(out.clear[lane],std::max(0.0f,along-frontExtent-1.0f));
    }return out;
}
struct Avoidance {float lateral,speedCap;int lane;bool blocked;};
inline Avoidance avoid(const RadarResult&r,float speed,int previousLane=2){
    float need=std::max(10.0f,speed*1.4f+speed*speed/9.0f);
    if(r.clear[2]>=std::min(need,r.range*0.9f))return {0,1e9f,2,false};
    int best=2;float score=-1e9f;
    for(int i=0;i<5;i++){
        // A lane change also crosses the intervening corridors: reject blocked transitions.
        float room=r.clear[i];for(int j=std::min(i,2);j<=std::max(i,2);j++)room=std::min(room,r.clear[j]+std::abs(j-2)*3.0f);
        float candidate=std::min(r.clear[i],need)*1.2f-std::abs(i-2)*3.5f+(i==previousLane?1.5f:0);
        if(room<3||r.clear[i]<8)continue;
        if(candidate>score){score=candidate;best=i;}
    }
    float cap=std::sqrt(2*4.5f*std::max(0.0f,r.clear[2]-2.5f));
    bool blocked=best==2&&r.clear[2]<8;
    return {float(best-2)*2.2f,blocked?0:std::min(cap,8.0f),best,blocked};
}
