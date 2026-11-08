#include "route_selection.hpp"
#include "route_actions.hpp"
#include "forward_navigation.hpp"
#include <cassert>
int main(){
 std::vector<ThreadSegment> lines{{0,0,0,100,0,0,10},{1,100,0,100,100,10,10}};
 auto hit=pickThread(lines,25,15);assert(hit.valid&&hit.segment==0&&std::abs(hit.t-.25f)<.001f);
 assert(!pickThread(lines,25,80).valid);
 ThreadGesture g;assert(g.begin(lines,20,5));g.move(lines,95,3);g.move(lines,101,60);assert(!g.reverse);g.end();assert(g.ready&&g.start.segment==0);
 g.reset();assert(g.begin(lines,100,80));g.move(lines,100,20);g.move(lines,80,0);assert(g.reverse);g.end();assert(g.ready);
 g.reset();assert(g.begin(lines,20,0));g.end();assert(!g.reverse&&g.ready); // plain click
 // Perspective midpoint on screen is not the world midpoint.
 std::vector<ThreadSegment> perspective{{0,0,0,100,0,0,10,10,30}};
 auto ph=pickThread(perspective,50,0);assert(std::abs(ph.t-.25f)<.001f&&std::abs(ph.arc-2.5f)<.001f);
 // A crossing on a distant part of the route must not steal a drag.
 auto crossing=lines;crossing.push_back({70,0,50,100,50,500,10});
 RoutePick previousHit{1,.5f,15,100,50,true};auto branch=pickThread(crossing,98,50,48,&previousHit);assert(branch.valid&&branch.segment==1);
 std::vector<Point> r{{0,0,0,3},{0,10,0,3},{10,10,0,3},{10,20,0,0}};r[1].enterCount=1;r[2].waitMs=900;r[2].enterCount=2;
 RoutePick chosen{1,.5f,15,0,0,true};auto forward=continueRoute(r,chosen,false);assert(forward.size()==3&&forward.front().x==5&&forward.front().y==10);assert(forward.front().enterCount==0&&forward[1].enterCount==2&&forward[1].waitMs==900&&forward.back().y==20);
 auto reverse=continueRoute(r,chosen,true);assert(reverse.size()==3&&reverse.front().x==5&&reverse[1].enterCount==1&&reverse.back().x==0&&reverse.back().y==0);assert(reverse[1].direction==-1);
 auto exact=continueRoute(r,{1,0,10,0,0,true},false);assert(exact.front().enterCount==1);assert(exact.size()==3);
 auto revExact=continueRoute(r,{1,0,10,0,0,true},true);assert(revExact.size()==2&&revExact.front().enterCount==1&&revExact.back().y==0);
 assert(continueRoute(r,{2,1,30,0,0,true},false).size()==1);
 auto mixed=r;mixed[2].direction=-1;auto mixedBack=continueRoute(mixed,chosen,true);assert(mixedBack[1].direction==1&&mixedBack[2].direction==-1);
 RouteActions actions;assert(actions.consume(forward,1)==2&&actions.consume(forward,1)==0);actions.reset();assert(actions.consume(reverse,1)==1);
 float from=0,to=1;assert(clipThreadRadius({-100,0,0,0},{100,0,0,0},{0,0,0,0},70,from,to));assert(std::abs(from-.15f)<.001f&&std::abs(to-.85f)<.001f);
 assert(!clipThreadRadius({-100,71,0,0},{100,71,0,0},{0,0,0,0},70,from,to));
 assert(!clipThreadRadius({0,0,71,0},{10,0,71,0},{0,0,0,0},70,from,to));
 auto clipped=pickThread({{2,0,0,100,0,3,14,1,1,.15f,.85f}},50,0);assert(std::abs(clipped.t-.5f)<.001f);
 auto nearest=nearestRoutePoint(r,{5,12,0,0});assert(nearest.valid&&nearest.segment==1&&std::abs(nearest.t-.5f)<.001f);
 auto shifted=advanceRoutePick(r,nearest,4);assert(shifted.segment==1&&std::abs(shifted.t-.9f)<.001f);
 auto backShift=advanceRoutePick(r,nearest,-10);assert(backShift.segment==0&&std::abs(backShift.t-.5f)<.001f);
 NavigationScene scene(nullptr,0,0,1,2.5f);auto connector=planForward(scene,{0,0,0,0},{0,60,0,0},12000,70);assert(!connector.empty()&&connector.back().y==60);
}
