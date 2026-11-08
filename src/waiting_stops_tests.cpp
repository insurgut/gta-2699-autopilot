#include "waiting_stops.hpp"
#include <cassert>
int main(){
    std::vector<Point> saved;for(unsigned i=0;i<60;i++)saved.push_back({0,float(i*2),0,8,0,1});
    saved[5].waitMs=10000;saved[30].waitMs=7000;
    auto active=saved;RadarBox gate{0,12,0,0,1,4,0.5f,2,WAIT_OBJECT};
    assert(suppressWaitingStops(active,0,&gate,1)==1);
    assert(active[5].waitMs==0&&active[30].waitMs==7000&&saved[5].waitMs==10000);
    assert(nextStop(active,1)==30);
    assert(suppressWaitingStops(active,0,nullptr,0)==0&&active[5].waitMs==0); // stays suppressed after gate disappears
    auto ordinary=gate;ordinary.passability=0;active=saved;
    assert(suppressWaitingStops(active,0,&ordinary,1)==0);
    auto elevated=gate;elevated.z=20;assert(suppressWaitingStops(active,0,&elevated,1)==0);
    active=saved;active[6].direction=-1;
    assert(suppressWaitingStops(active,0,&gate,1)==1&&active[5].waitMs==0);
    assert(nextStop(active,1)==5); // mandatory gear change still needs its short stop
    active=saved;assert(suppressWaitingStops(active,10,&gate,1)==0); // don't alter passed stops
}
