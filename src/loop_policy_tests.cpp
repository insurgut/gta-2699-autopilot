#include "loop_policy.hpp"
#include "route_actions.hpp"
#include "waiting_stops.hpp"
#include "wait_cycle.hpp"
#include "stops.hpp"
#include <cassert>
int main(){
 std::vector<Point> r{{0,0,0,2},{0,20,0,2},{20,20,0,2},{20,0,0,2},{0,0,0,0}};
 r[1].enterCount=2;r[2].waitMs=1200;
 assert(closedLoopRoute(r));auto open=r;open.back().x=8;assert(!closedLoopRoute(open));
 assert(!closedLoopRoute({}));assert(!closedLoopRoute({r.front()}));
 assert(loopRestartReady(true,r,r.back(),0,false));
 assert(!loopRestartReady(false,r,r.back(),0,false));
 assert(!loopRestartReady(true,open,open.back(),0,false));
 assert(!loopRestartReady(true,r,{10,0,0,0},0,false));
 assert(!loopRestartReady(true,r,r.back(),1,false));
 assert(!loopRestartReady(true,r,r.back(),0,true));
 auto level=r;level.back().z=8;assert(!closedLoopRoute(level));
 RouteActions actions;unsigned presses=0;
 for(unsigned lap=0;lap<5;lap++){
   auto active=r;actions.reset();assert(nextStop(active,1)==2);
   presses+=actions.consume(active,1);assert(actions.consume(active,1)==0);
   active[2].waitMs=0; // per-lap mutations must not change saved route
   assert(r[2].waitMs==1200);assert(loopRestartReady(true,r,r.back(),0,false));
 }
 assert(presses==10);
}
