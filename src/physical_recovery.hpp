#pragma once
#include "forward_navigation.hpp"
#include "recovery.hpp"
struct ContactBackoffGate {
    uint64_t since=0;
    void reset(){since=0;}
    bool update(uint64_t now,bool closeToObstacle,float speed){if(!closeToObstacle||speed>.25f){reset();return false;}if(!since)since=now;return now-since>=1800;}
};
inline EscapePlan physicalEscape(const NavigationScene&scene,NavPose origin,int routeGear,GridPoint goal,unsigned attempt){
    EscapePlan best;float score=-1e9f;int gear=-routeGear;
    for(float steer:{0.0f,-.35f,.35f,-.65f,.65f})for(float length:{1.5f,2.5f,3.5f}){
        NavPose p=origin;bool valid=true;float travelled=0;
        while(travelled<length){float step=std::min(.15f,length-travelled);float yaw=p.yaw+gear/2.8f*std::tan(steer*.45f)*step;NavPose q{p.x+std::sin((p.yaw+yaw)/2)*gear*step,p.y+std::cos((p.yaw+yaw)/2)*gear*step,p.z,yaw};if(!scene.escapeStep(origin,p,q)){valid=false;break;}p=q;travelled+=step;}
        if(!valid||!scene.clear(p))continue;
        float rank=20-length-std::abs(steer)*2-std::hypot(p.x-goal.x,p.y-goal.y)*.02f+(attempt%2?steer:-steer)*.15f;
        if(rank>score){score=rank;best={true,gear,steer,length,{p.x,p.y,p.z}};}
    }return best;
}
