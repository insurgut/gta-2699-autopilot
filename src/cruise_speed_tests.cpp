#include "driving.hpp"
#include <cassert>
#include <cmath>
int main(){
 std::vector<Point> route;for(int i=0;i<250;i++)route.push_back({float(i*10),0,0,12});
 auto straight=gearDriveOutput(route,10,{100,0,0,0},1,0,25,50,1,SIZE_MAX,false);assert(std::abs(straight.targetSpeed-50)<.001f); // 180 km/h, no 70 limit
 // Heading/cross-track and curvature governors do not silently replace the explicit cruise demand.
 auto correction=gearDriveOutput(route,10,{100,8,0,0},1,0,25,50,1,SIZE_MAX,false);assert(std::abs(correction.targetSpeed-50)<.001f);
 auto reduced=gearDriveOutput(route,10,{100,8,0,0},1,0,25,50,1,SIZE_MAX,true);assert(reduced.targetSpeed<50);
 assert(editRouteSection(route,5,80,144,false));auto custom=gearDriveOutput(route,10,{100,0,0,0},1,0,25,20,1,SIZE_MAX,false);assert(std::abs(custom.targetSpeed-40)<.001f);
 auto normal=gearDriveOutput(route,85,{850,0,0,0},1,0,25,30,1,SIZE_MAX,false);assert(std::abs(normal.targetSpeed-30)<.001f);
 auto exit=gearDriveOutput(route,79,{790,0,0,0},1,0,25,20,1,SIZE_MAX,false);assert(exit.targetSpeed<40); // decelerate towards default after edited zone
 auto finish=gearDriveOutput(route,247,{2478,0,0,0},1,0,25,50,1,SIZE_MAX,false);assert(finish.targetSpeed<50);
}
