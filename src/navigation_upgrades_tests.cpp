#include "direct_rejoin.hpp"
#include "driving.hpp"
#include "progress_policy.hpp"
#include <cassert>
#include <iostream>
int main(){
 std::vector<Point> straight{{0,0,0,20},{0,200,0,20}},turn{{0,0,0,20},{0,20,0,20},{100,20,0,20}};
 assert(anticipatedSpeed(straight,0,straight.front(),20,25)==25);auto early=anticipatedSpeed(turn,0,turn.front(),20,25);assert(early<16&&early>5);
 std::vector<Point> taps;for(int i=0;i<=100;i++)taps.push_back({i%2?.1f:-.1f,float(i)*2,0,20});assert(anticipatedSpeed(taps,0,{0,0,0,0},20,25)>24);
 std::vector<Point> dense;for(int i=0;i<=20;i++)dense.push_back({0,float(i),0,20});for(int i=1;i<=100;i++)dense.push_back({float(i),20,0,20});assert(std::abs(anticipatedSpeed(dense,0,dense.front(),20,25)-early)<.01f);
 SpeedDemand demand;assert(demand.update(1000,20,10)==10);assert(demand.update(1100,20,10)<=10.21f);assert(demand.update(1200,4,10)==4);
 NavigationScene free(nullptr,0,0,1.05f,2.5f);std::vector<Point> route;std::vector<NavPose> path;for(int y=0;y<=60;y++){route.push_back({0,float(y),0,8});path.push_back({0,float(y),0,0});}
 ForwardDetour nav;nav.set(path,60,path.back());auto follow=nav.follow({0,0,0,0},7,free);assert(follow.valid&&follow.speedCap>=8.0f);
 assert(!nav.replaceInvalid(1000,false,1));assert(!nav.replaceInvalid(2000,false,1));assert(nav.active());assert(nav.replaceInvalid(2400,false,1));assert(!nav.replaceInvalid(2500,true,0));assert(!nav.replaceInvalid(3000,false,1));assert(nav.replaceInvalid(3001,false,5));
 auto direct=directRejoin(free,{4,0,0,0},route,0,60,7);assert(!direct.path.empty()&&direct.routeGoal>8);for(size_t i=1;i<direct.path.size();i++)assert(free.edge(direct.path[i-1],direct.path[i]));
 RadarBox wall{0,6,1,0,1,40,1,1,0};NavigationScene blocked(&wall,1,0,1.05f,2.5f);assert(directRejoin(blocked,{4,0,0,0},route,0,60,7).path.empty());
 auto bounded=directRejoin(free,{4,0,0,0},route,0,10,7);assert(bounded.path.empty()||bounded.routeGoal<=10);
 StallDetector stall;assert(!stall.update(1000,0,0,.4f,.8f,true));assert(stall.update(4000,.1f,0,.4f,.8f,true));assert(!stall.update(4100,.1f,0,.7f,.8f,true));
 std::cout<<"Anticipatory bends, tap/density invariance, smooth acceleration, 30kmh generated path, path commitment, direct rejoin and low-speed stall passed\n";
}
