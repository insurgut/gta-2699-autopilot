#include "progress_policy.hpp"
#include <cassert>
#include <iostream>
int main(){
    setCellSize(.5f);VoxelGrid grid;grid.halfWidth=.25f;
    std::vector<Point> curve{{0,0,0,4},{10,0,0,4},{10,10,0,4}};
    RadarBox inside{5,5,.8f,1,0,.6f,.6f,.7f,0};grid.rebuild(&inside,1,0,0,0,1000);
    assert(!grid.pathClear({0,0,0},{10,10,0}));assert(routeCorridorClear(grid,{0,0,0},curve,0,SIZE_MAX,25));
    RadarBox ahead{10,5,.8f,1,0,.6f,.6f,.7f,0};grid.rebuild(&ahead,1,0,0,0,1100);
    assert(!routeCorridorClear(grid,{0,0,0},curve,0,SIZE_MAX,25));assert(routeCorridorClear(grid,{0,0,0},curve,0,SIZE_MAX,8));
    StallDetector stall;assert(!stall.update(1000,0,0,0,1,true));assert(!stall.update(4000,0,0,0,.5f,true));assert(!stall.update(5000,0,0,0,1,true));assert(stall.update(9000,0,0,0,1,true));
    stall.reset();assert(!stall.update(10000,0,0,0,1,true));assert(!stall.update(13000,.4f,0,0,1,true));assert(!stall.update(15000,.4f,0,0,1,true));
    HoldClock hold;assert(!hold.update(1000,.4f,2000));assert(!hold.update(1500,.55f,2000));assert(!hold.update(2000,.7f,2000));assert(hold.update(3000,.5f,2000));
    hold.reset();assert(!hold.update(4000,.4f,1000));assert(!hold.update(4500,2,1000));assert(!hold.update(5100,2,1000));assert(!hold.update(5200,.4f,1000));assert(hold.update(6200,.4f,1000));
    std::cout<<"Polyline obstruction, stop jitter and stall demand regressions passed\n";
}
