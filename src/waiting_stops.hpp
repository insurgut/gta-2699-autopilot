#pragma once
#include "stops.hpp"
#include "radar.hpp"
#include "scene_geometry.hpp"
inline size_t suppressWaitingStops(std::vector<Point>&route,size_t from,const RadarBox*boxes,size_t count,float radius=12){
    std::vector<SceneFootprint> waiting;
    for(size_t i=0;i<count;i++)if(boxes[i].passability==WAIT_OBJECT){const auto&b=boxes[i];waiting.push_back({{b.x,b.y,b.z},{b.fx,b.fy,0},b.halfWidth,b.halfLength,b.halfHeight});}
    if(waiting.empty()||from>=route.size())return 0;
    size_t changed=0,end=from+std::min(size_t(257),route.size()-from);
    for(size_t i=from;i<end;i++){
        auto&p=route[i];if(p.waitMs<500)continue;
        for(const auto&box:waiting){
            if(std::abs(p.z-box.center.z)>box.halfHeight+3)continue;
            if(footprintDistanceSquared(box,{p.x,p.y,p.z})<=radius*radius){p.waitMs=0;++changed;break;}
        }
    }
    return changed;
}
