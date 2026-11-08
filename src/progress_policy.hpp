#pragma once
#include "voxels.hpp"
#include "route.hpp"
// Check the recorded polyline, never a shortcut between distant waypoints.
inline bool routeCorridorClear(const VoxelGrid&grid,GridPoint start,const std::vector<Point>&route,size_t segment,size_t limit,float horizon){
    if(route.size()<2||segment+1>=route.size())return false;
    limit=std::min(limit,route.size()-1);float used=0;
    for(size_t i=segment+1;i<=limit;i++){
        GridPoint end{route[i].x,route[i].y,route[i].z};float d=std::hypot(end.x-start.x,end.y-start.y);
        if(used+d>horizon&&d>0){float t=(horizon-used)/d;end={start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t,start.z+(end.z-start.z)*t};return grid.pathClear(start,end);}
        if(!grid.pathClear(start,end))return false;
        used+=d;if(used>=horizon)return true;start=end;
    }return true;
}
struct StallDetector {
    uint64_t since=0;float x=0,y=0;
    bool update(uint64_t now,float px,float py,float speed,float demand,bool driving){
        if(!driving||speed>=0.6f||demand<0.65f){since=0;return false;}
        if(!since||std::hypot(px-x,py-y)>0.3f){since=now;x=px;y=py;}
        return now-since>=3000;
    }
    void reset(){since=0;}
};
struct HoldClock {
    uint64_t started=0,movingSince=0;
    void reset(){started=movingSince=0;}
    bool update(uint64_t now,float speed,uint64_t wait){
        if(speed>1.2f){if(!movingSince)movingSince=now;if(now-movingSince>=500)started=0;}
        else movingSince=0;
        if(!started&&speed<=0.6f)started=now;
        return started&&now-started>=wait&&speed<=0.8f;
    }
};
