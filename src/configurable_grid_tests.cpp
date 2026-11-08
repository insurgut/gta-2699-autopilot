#include "voxels.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    assert(setCellSize(0.10f));assert(!setCellSize(0.09f));assert(!setCellSize(5.01f));assert(!setCellSize(std::numeric_limits<float>::infinity()));
    VoxelGrid grid;grid.halfWidth=0.2f;
    RadarBox thin{0.05f,4.05f,0.8f,0,1,0.5f,0.04f,0.5f};grid.rebuild(&thin,1,0,0,0,1000);
    GridPoint a{0.05f,0.05f,0},b{0.05f,10.05f,0};assert(!grid.pathClear(a,b));
    auto path=grid.plan(a,b);assert(!path.empty());auto previous=a;
    for(const auto&p:path){assert(grid.pathClear(previous,p));previous=p;} // Fine obstacles must not be skipped by coarse navigation nodes.
    thin.passability=1;grid.rebuild(&thin,1,0,0,0,1100);assert(grid.occupied.empty()&&!grid.passable.empty()&&grid.pathClear(a,b));
    auto c=cellAt(thin.x,thin.y,0);assert(grid.columnState(c.x,c.y,0)==4);
    RadarBox overlap[]={thin,thin};overlap[1].passability=0;grid.rebuild(overlap,2,0,0,0,1200);assert(grid.columnState(c.x,c.y,0)==3&&!grid.pathClear(a,b));
    assert(setCellSize(5));grid.clear();grid.rebuild(nullptr,0,0,0,0,1300);assert(grid.pathClear({0,0,0},{20,0,0}));
    assert(setCellSize(1.25f));std::cout<<"Configurable grid tests passed: 0.10..5 range, fine obstacle traversal, passable classifications and solid precedence.\n";
}
