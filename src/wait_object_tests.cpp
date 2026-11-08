#include "radar.hpp"
#include "radar_pause.hpp"
#include "scene_recovery.hpp"
#include "voxels.hpp"
#include <cassert>
int main(){
    assert(waitingApproachSpeed(0.5f)==0);
    assert(waitingApproachSpeed(3)>=10.0f/3.6f-0.001f);
    assert(std::abs(waitingApproachSpeed(20)-25.0f/3.6f)<0.001f);
    assert(std::abs(waitingApproachSpeed(11)-20.0f/3.6f)<0.001f);
    for(float initial:{0.0f,8.0f,15.0f}){
        float gap=std::max(8.0f,initial*1.4f+initial*initial/8+2),v=initial;
        std::array<float,12> delayed{};unsigned cursor=0;bool stopped=false;
        for(unsigned i=0;i<60000;i++){
            float target=waitingApproachSpeed(gap);
            float acceleration=target<=0?-8.0f:std::clamp((target-v)*3.0f,-5.0f,2.0f);
            float applied=delayed[cursor];delayed[cursor]=acceleration;cursor=(cursor+1)%delayed.size();
            v=std::max(0.0f,v+applied*0.01f);gap-=v*0.01f;
            if(gap<=WAIT_STOP_GAP&&v==0){stopped=true;break;}
        }
        assert(stopped&&gap>0.15f&&gap<=0.6f);
    }
    RadarBox barrier{0,10,0,0,1,4,0.2f,2,WAIT_OBJECT};
    assert(waitingObjectDistance(0,0,0,0,1,&barrier,1,1,2.5f,20)<8);
    VoxelGrid grid;grid.rebuild(&barrier,1,0,0,0,1000);
    auto cell=cellAt(0,10,0);assert(grid.blocked(cell.x,cell.y,0));
    assert(grid.columnState(cell.x,cell.y,0)==6);
    auto permitted=barrier;permitted.passability=WAIT_PASS;grid.rebuild(&permitted,1,0,0,0,1100);assert(!grid.blocked(cell.x,cell.y,0));assert(grid.columnState(cell.x,cell.y,0)==6);
    auto b=barrier;b.x=20;assert(waitingObjectDistance(0,0,0,0,1,&b,1,1,2.5f,20)>20);
    b=barrier;b.y=-20;assert(waitingObjectDistance(0,0,0,0,1,&b,1,1,2.5f,20)>20);
    b=barrier;b.z=20;assert(waitingObjectDistance(0,0,0,0,1,&b,1,1,2.5f,20)>20);
    b=barrier;b.passability=0;assert(waitingObjectDistance(0,0,0,0,1,&b,1,1,2.5f,20)>20);
    b=barrier;b.passability=1;auto scan=radarScan(0,0,0,0,1,&b,1,30);assert(scan.clear[2]==30);
    RadarBox floor{0,0,-0.8f,0,1,100,100,0.1f,0};
    assert(corridorObjectDistance(0,0,0,0,1,&floor,1,1,2.5f,20,false)>20);
    assert(waitingObjectDistance(0,0,0,0,-1,&barrier,1,1,2.5f,20)>20);
    assert(waitingObjectDistance(0,20,0,0,-1,&barrier,1,1,2.5f,20)<8);
    FreshSceneGate gate;
    for(uint64_t now=1000;now<2000;now+=100)assert(!gate.update(now,now,processingReady(true,true,true,2,0,true)));
    assert(!gate.update(2000,2000,processingReady(true,true,true,1,0,true)));
    assert(!gate.update(2100,2100,processingReady(true,true,true,1,0,true)));
    assert(gate.update(2200,2200,processingReady(true,true,true,1,0,true)));
    assert(!processingReady(true,true,true,1,16,true));
    assert(!processingReady(true,true,true,1,0,false));
    assert(!processingReady(false,true,true,1,0,true));
    assert(readyPools(true,true,true,true,true,300,256)); // no local-member presence requirement
    assert(!readyPools(true,true,true,true,true,0,256));
    assert(!readyPools(true,true,true,true,false,300,256));
    assert(!readyPools(true,true,true,false,true,300,256));
}
