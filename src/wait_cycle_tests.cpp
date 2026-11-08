#include "wait_cycle.hpp"
#include <cassert>
int main(){
    WaitPose closed{123,{0,10,0},{1,0,0},{0,1,0},{0,0,1},{{0,10,0},{0,1,0},3,.2f,1}};
    WaitCycle c;c.begin(closed,0,1);auto moved=closed;moved.origin.x=.2f;
    c.observe(1000,&moved);assert(!c.motionStarted);c.observe(1080,&moved);assert(c.motionStarted==1000);
    assert(!c.releaseReady(5999,true,true));assert(c.releaseReady(6000,true,true));assert(!c.releaseReady(6000,false,true));assert(!c.releaseReady(6000,true,false));
    c.startPass();assert(!c.past({0,12,0},2.5f));assert(c.past({0,15,0},2.5f));c.observe(9000,&closed);assert(c.passing);
    c.begin(closed,0,1);c.observe(1000,nullptr);c.observe(1080,nullptr);assert(c.motionStarted==1000);c.observe(1100,&closed);assert(!c.motionStarted);
    auto rotated=closed;rotated.right={.99f,0,.1f};c.observe(1200,&rotated);c.observe(1280,&rotated);assert(c.motionStarted==1200);
    c.observe(1400,&closed);c.observe(1900,&closed);assert(!c.motionStarted);
    c.begin(closed,0,1,2500);c.observe(1000,&moved);c.observe(1080,&moved);assert(!c.releaseReady(3499,true,true));assert(c.releaseReady(3500,true,true));assert(!c.releaseReady(3500,true,false));c.begin(closed,0,1,0);c.observe(1000,&moved);c.observe(1080,&moved);assert(c.releaseReady(1080,true,true));
    SceneFootprint b{};float n=std::sqrt(.5f);
    assert(modelFootprintSlice({0,0,0},{n,0,n},{0,1,0},{-n,0,n},{0,-.1f,-.1f},{6,.1f,.1f},0,1.5f,b));
    assert(b.center.x+b.halfWidth<1.7f);
    assert(!modelFootprintSlice({0,0,3},{1,0,0},{0,1,0},{0,0,1},{0,-.1f,-.1f},{6,.1f,.1f},0,1.5f,b));
}
