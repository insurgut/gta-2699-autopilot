#include "recovery.hpp"
#include <cassert>
#include <iostream>
int main(){
    VoxelGrid grid;GridPoint start{CELL/2,CELL/2,0},goal{CELL/2,15,0};
    // A pocket with a closed front but an open rear: a direction change is necessary.
    for(int x=-3;x<=3;x++)grid.occupied.insert({x,4,0});
    for(int y=0;y<=4;y++){grid.occupied.insert({-3,y,0});grid.occupied.insert({3,y,0});}
    assert(!grid.pathClear(start,goal));auto forward=grid.plan(start,goal);assert(!forward.empty());
    auto escape=escapePlan(grid,start,0,1,1,goal);assert(escape.valid&&escape.gear==-1&&escape.distance<=4.5f&&escape.end.y<start.y);
    assert(grid.pathClear(start,escape.end)); // Selected straight rear candidate in this symmetric pocket.
    assert(!grid.plan(escape.end,goal).empty());
    for(int x=-8;x<=8;x++)grid.occupied.insert({x,-2,0});
    assert(!escapePlan(grid,start,0,1,1,goal).valid); // Never reverse blindly into a known rear wall.
    VoxelGrid mirror;for(int x=-3;x<=3;x++)mirror.occupied.insert({x,-4,0});
    for(int y=-4;y<=0;y++){mirror.occupied.insert({-3,y,0});mirror.occupied.insert({3,y,0});}
    auto opposite=escapePlan(mirror,start,0,1,-1,{CELL/2,-15,0});assert(opposite.valid&&opposite.gear==1&&opposite.end.y>start.y);
    std::cout<<"Recovery tests passed: reverse escape, continuation, rear blockage rejection and reciprocal forward recovery.\n";
}
