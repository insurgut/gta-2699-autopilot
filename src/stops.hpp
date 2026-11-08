#pragma once
#include "route.hpp"
#include <cstdint>
struct StopRecorder {
    uint64_t started=0;size_t anchor=SIZE_MAX;bool moved=false;
    void update(std::vector<Point>&r,Point p,float speed,uint64_t now){
        if(r.empty())return;
        if(distance(p,r.front())>2)moved=true;
        if(speed<0.45f&&moved){
            if(!started)started=now;
            if(now-started>=500){
                if(anchor==SIZE_MAX){r.push_back(p);anchor=r.size()-1;}
                r[anchor].waitMs=unsigned(std::min(uint64_t(86400000),now-started));r[anchor].speed=0;
            }
        }else if(speed>0.85f){started=0;anchor=SIZE_MAX;}
    }
};
inline size_t nextStop(const std::vector<Point>&r,size_t after){for(size_t i=after;i<r.size();i++)if(r[i].waitMs>=500||(i+1<r.size()&&r[i+1].direction!=r[i].direction))return i;return SIZE_MAX;}
inline std::vector<Point> returnRoute(const std::vector<Point>&r){
    std::vector<Point> out(r.rbegin(),r.rend());
    // Direction belongs to the incoming edge. Traverse each edge in reverse with inverse gear.
    for(size_t i=1;i<out.size();i++){out[i].direction=-r[r.size()-i].direction;out[i].speedLimit=r[r.size()-i].speedLimit;out[i].ignoreObstacles=r[r.size()-i].ignoreObstacles;}
    if(out.size()>1)out[0].direction=out[1].direction;
    return out;
}
