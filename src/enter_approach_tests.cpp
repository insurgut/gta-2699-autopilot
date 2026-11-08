#include "enter_approach.hpp"
#include "route_actions.hpp"
#include "hud_status.hpp"
#include <cassert>
int main(){
 NavigationScene empty(nullptr,0,0,1,2.5f);Point target{0,10,0,3};
 assert(!approachEnter(empty,{0,7.5f,0,0},target,0).ready); // front bumper is not the car center
 assert(approachEnter(empty,{0,9.7f,0,0},target,0).ready);
 assert(!approachEnter(empty,{0,9.7f,0,0},target,1).ready); // must stop first
 RadarBox wall{0,8,1,0,1,4,.5f,1,0};NavigationScene blocked(&wall,1,0,1,2.5f);
 auto initial=approachEnter(blocked,{0,0,0,0},target,0);assert(initial.blocked&&!initial.ready&&initial.remaining>4);
 float y=0;for(int i=0;i<200&&!approachEnter(blocked,{0,y,0,0},target,0).ready;i++)y+=.05f;
 auto last=approachEnter(blocked,{0,y,0,0},target,0);assert(last.ready&&last.blocked&&y<5&&y>4);assert(blocked.clear({0,y,0,0}));
 std::vector<Point> r{{0,0,0,0},{0,10,0,3},{0,20,0,0}};r[1].enterCount=1;RouteActions a;assert(a.next(r)==1);assert(a.consume(r,1)==1);assert(a.consume(r,1)==0&&a.next(r)==r.size());
 std::vector<Point> corner{{0,0,0,0},{0,8,0,0},{8,8,0,0}};
 auto curved=empty.routeDistance({0,0,0,0},corner,0,2);assert(!curved.second&&std::abs(curved.first-16)<.01f);
 auto stopped=blocked.routeDistance({0,0,0,0},corner,0,2);assert(stopped.second&&stopped.first>4&&stopped.first<5);
 EnterStall stall;assert(!stall.update(1000,{0,0,0,0},0,true));assert(stall.update(2800,{0,0,0,0},0,true));assert(!stall.update(2810,{0,1,0,0},0,true));
 assert(hudStatus(true,false,false,L"FPS 40")==L"Запись");assert(hudStatus(false,true,false,L"манёвр")==L"Воспроизведение");assert(hudStatus(false,false,false,L"Готово").empty());assert(hudStatus(false,false,true,L"Нет данных")==L"Ошибка: Нет данных");
}
