#include "route_actions.hpp"
#include "driving.hpp"
#include <cassert>
int main(){
    std::vector<Point> r{{0,0,0,5},{3,0,0,5},{6,0,0,5},{9,0,0,5}};
    assert(recordEnter(r,{12,0,0,4}));assert(recordEnter(r,{12,0,0,4}));assert(r.back().enterCount==2);
    r[1].enterCount=1;r[2].enterCount=1;
    RouteActions a;assert(a.poll(r,0,{0,0,0,0})==0);assert(a.poll(r,0,{3,0,0,0})==1);assert(a.poll(r,0,{3,0,0,0})==0);
    assert(a.poll(r,1,{6,0,0,0})==1);assert(a.poll(r,3,{12,0,0,0})==2);assert(a.poll(r,3,{12,0,0,0})==0);
    a.reset();r[0].enterCount=1;assert(a.poll(r,0,{0,0,0,0})==1);
    a.reset();r[0].enterCount=0;assert(a.poll(r,0,{0,0,0,0})==0);assert(a.poll(r,2,{8,0,0,0})==2); // swept crossing
    a.reset();assert(a.poll(r,0,{30,30,0,0})==0);assert(a.poll(r,3,{30,35,0,0})==0); // no remote firing
    assert(!gearDriveOutput(r,3,{9.5f,0,0,0},1,0,1,5,1).arrived);assert(gearDriveOutput(r,3,{10.5f,0,0,0},1,0,1,5,1).arrived);
    auto f=std::filesystem::temp_directory_path()/"district-action-route.txt";assert(writeRoute(f,r));std::vector<Point> loaded;assert(loadRoute(f,loaded));assert(loaded[1].enterCount==1&&loaded[4].enterCount==2);
    std::ofstream(f)<<"GTA_AUTOPILOT 3 2\n0 0 0 0 0 1\n1 0 0 0 0 1\n";assert(loadRoute(f,loaded)&&loaded[0].enterCount==0);
    std::ofstream(f)<<"GTA_AUTOPILOT 4 2\n0 0 0 0 0 1 256\n1 0 0 0 0 1 0\n";assert(!loadRoute(f,loaded));std::filesystem::remove(f);
}
