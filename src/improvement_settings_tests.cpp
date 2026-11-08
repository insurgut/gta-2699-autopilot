#include "improvement_settings.hpp"
#include "driving.hpp"
#include "forward_navigation.hpp"
#include "flight_recorder.hpp"
#include <cassert>
#include <map>
#include <iostream>
int main(){
 for(unsigned mask=0;mask<32;mask++){ImprovementSettings settings;std::map<std::wstring,bool> saved;for(size_t i=0;i<improvementBindings.size();i++){const auto&b=improvementBindings[i];assert(settings.*b.member);bool enabled=(mask&(1u<<i))!=0;assert(settings.set(b.key,enabled));saved[b.key]=enabled;}
  assert(!settings.set(L"unknown",false));ImprovementSettings loaded;loaded.load([&](const wchar_t*key,bool fallback){auto it=saved.find(key);return it==saved.end()?fallback:it->second;});for(const auto&b:improvementBindings)assert(loaded.*b.member==settings.*b.member);
 }
 ImprovementSettings defaults;defaults.load([](const wchar_t*,bool fallback){return fallback;});for(const auto&b:improvementBindings)assert(defaults.*b.member);
 std::vector<Point> route{{0,0,0,25},{0,40,0,25},{100,40,0,25}};
 auto on=gearDriveOutput(route,0,route.front(),0,1,20,25,1,SIZE_MAX,true),off=gearDriveOutput(route,0,route.front(),0,1,20,25,1,SIZE_MAX,false);assert(on.targetSpeed<off.targetSpeed&&off.targetSpeed==25);assert(on.steering==off.steering);
 auto end=gearDriveOutput({{0,0,0,25},{0,10,0,25}},0,{0,0,0,0},0,1,20,25,1,SIZE_MAX,false);assert(end.targetSpeed<10);
 SpeedDemand demand;assert(demand.update(1000,20,0,false)==20);assert(demand.update(1100,20,0,true)==0);
 ForwardDetour detour;assert(!detour.replaceInvalid(1000,false,1,true));assert(detour.replaceInvalid(1001,false,1,false));
 FlightRecorder recorder;FlightFrame frame;frame.now=1000;recorder.push(frame);recorder.mark(1000,"test");assert(recorder.size()==1);recorder.reset();assert(recorder.size()==0&&!recorder.due(5000));
 std::cout<<"All 32 option combinations and reloads, legacy defaults, anticipation bypass with retained stop braking, immediate replan and flight cancellation passed\n";
}
