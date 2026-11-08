#include "radar_pause.hpp"
#include "stops.hpp"
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    FreshSceneGate g;assert(!g.update(1000,1000,true));assert(!g.update(1100,1000,true));assert(!g.update(1200,1000,true));assert(!g.update(1250,1000,true));
    assert(!g.update(1300,1300,true));assert(g.update(1400,1400,true));
    assert(!g.update(1500,1500,false));assert(!g.update(1600,1600,true));assert(!g.update(1700,1700,true));assert(g.update(1800,1800,true));assert(!g.update(2300,1800,true));
    assert(!g.update(2400,2500,true));assert(!g.update(2500,2500,true));assert(!g.update(2600,2400,true));
    assert(poolLayoutValid(0,0));assert(poolLayoutValid(0x20000,0));assert(!poolLayoutValid(0,1));assert(!poolLayoutValid(0x20000,10001));assert(poolLayoutValid(0x20000,10000));
    if(argc>1){std::vector<Point> route;assert(loadRoute(argv[1],route));assert(route.size()==214);assert(nextStop(route,1)==58);assert(nextStop(route,59)==62);assert(route[62].direction==-1);}
    std::cout<<"Radar pause gates, empty pool validation and uploaded route stop sequence passed\n";
}
