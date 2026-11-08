#pragma once
#include "voxels.hpp"
struct EscapePlan {bool valid=false;int gear=-1;float steering=0;float distance=0;GridPoint end{};};
inline EscapePlan escapePlan(const VoxelGrid&grid,GridPoint start,float fx,float fy,int routeGear,GridPoint goal){
    EscapePlan best;float score=-1e9f;
    for(float steering:{0.0f,-0.65f,0.65f,-0.35f,0.35f})for(float length:{3.0f,4.5f}){
        int gear=-routeGear;float yaw=std::atan2(fx,fy),travelled=0;GridPoint p=start;bool clear=true;
        while(travelled<length){float step=std::min(0.25f,length-travelled);yaw+=gear/2.8f*std::tan(steering*0.45f)*step;
            GridPoint q{p.x+std::sin(yaw)*gear*step,p.y+std::cos(yaw)*gear*step,p.z};
            if(!grid.pathClear(p,q)){clear=false;break;}p=q;travelled+=step;
        }
        if(!clear)continue;
        // Do not reverse into a dead end without a known continuation toward the route.
        auto continuation=grid.plan(p,goal);if(continuation.empty())continue;
        float candidate=10-length-std::abs(steering)*2-float(continuation.size())*0.05f;
        if(candidate>score){score=candidate;best={true,gear,steering,length,p};}
    }return best;
}
