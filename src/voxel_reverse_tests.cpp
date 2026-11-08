#include "voxels.hpp"
#include "driving.hpp"
#include "stops.hpp"
#include <cassert>
#include <set>
#include <iostream>
int main(){
    assert(cellAt(-0.01f,0,0).x==-1);assert(cellAt(0,0,0).x==0);assert(cellAt(CELL,0,0).x==1);
    VoxelGrid grid;RadarBox box{CELL*0.5f,CELL*8.5f,CELL*0.5f,0,1,0.2f,0.2f,0.2f};
    grid.rebuild(&box,1,CELL*0.5f,CELL*0.5f,0,1000);assert(grid.occupied.size()==1&&grid.occupied.count({0,8,0}));
    GridPoint start{CELL*0.5f,CELL*0.5f,0},end{CELL*0.5f,CELL*19.5f,0};assert(!grid.pathClear(start,end));
    auto path=grid.plan(start,end);assert(!path.empty());GridPoint last=start;bool side=false;
    for(auto p:path){assert(grid.pathClear(last,p));if(std::abs(p.x-start.x)>2)side=true;last=p;}assert(side);
    // A wall spanning the entire bounded search area must not be traversed.
    grid.occupied.clear();int extent=int(std::ceil(32/CELL))+2;for(int x=-extent;x<=extent;x++)grid.occupied.insert({x,8,0});assert(grid.plan(start,end).empty());
    grid.occupied.clear();grid.markContact(0,0,0,5000);grid.rebuild(nullptr,0,0,0,0,6000);assert(!grid.contact.empty());grid.rebuild(nullptr,0,0,0,0,66000);assert(grid.contact.empty());
    std::vector<Point> r;for(int i=0;i<=40;i++)r.push_back({0,float(-i*2),0,4,0,-1});
    Point p{0,0,0,0};float yaw=0,v=0,maxError=0;size_t segment=0;bool arrived=false;
    for(int n=0;n<15000;n++){
        auto c=gearDriveOutput(r,segment,p,std::sin(yaw),std::cos(yaw),v,5,-1);segment=c.segment;maxError=std::max(maxError,c.error);if(c.arrived){arrived=true;break;}
        v=std::max(0.0f,v+std::clamp((c.targetSpeed-v)*2,-4.0f,2.0f)*0.02f);
        yaw+=-v/2.8f*std::tan(c.steering*0.45f)*0.02f;p.x-=std::sin(yaw)*v*0.02f;p.y-=std::cos(yaw)*v*0.02f;
    }assert(arrived&&maxError<0.1f);
    // In reverse, steering correction has opposite wheel polarity.
    auto correction=gearDriveOutput(r,0,{2,0,0,0},0,1,2,5,-1);assert(correction.steering<0);
    r[10].waitMs=2100;auto back=returnRoute(r);assert(back.size()==r.size()&&back[1].direction==1&&back[r.size()-11].waitMs==2100);
    auto file=std::filesystem::temp_directory_path()/"gta-reverse-format3.txt";assert(writeRoute(file,r));std::vector<Point> read;assert(loadRoute(file,read)&&read[1].direction==-1&&read[10].waitMs==2100);std::filesystem::remove(file);
    std::cout<<"Global cell indexing, single-cell occupancy, A* detour, no wall crossing, contact TTL, reverse driving, return gear and format 3 passed.\n";
}
