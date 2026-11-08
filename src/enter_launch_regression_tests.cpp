#include "input_enter_markers.hpp"
#include "physical_presses.hpp"
#include "enter_pulse.hpp"
#include "route_catalog.hpp"
#include <cassert>
#include <iostream>
int main(){
 PhysicalPresses keys;assert(keys.observe(0x0d,true,true,false));keys.observe(0x0d,false,true,false);assert(keys.take(0x0d,true)&&!keys.take(0x0d,true));keys.observe(0x2e,true,true,false);keys.observe(0x2e,true,true,false);keys.observe(0x2e,false,true,false);assert(keys.take(0x2e,true)&&!keys.take(0x2e,true));assert(!keys.observe(0x0d,true,true,true));assert(!keys.take(0x0d,true));keys.observe(0x2e,true,false,false);assert(!keys.take(0x2e,true));keys.observe(0x2e,false,false,false);
 unsigned queue=2;uint64_t deadline=1000;assert(enterPulse(1000,false,queue,false,deadline)==0&&queue==2);assert(enterPulse(1000,true,queue,false,deadline)==1&&queue==1&&deadline==1150);assert(enterPulse(1149,false,queue,true,deadline)==0);assert(enterPulse(1150,false,queue,true,deadline)==-1&&queue==1);assert(enterPulse(1250,false,queue,false,deadline)==0&&queue==1);assert(enterPulse(1300,true,queue,false,deadline)==1&&queue==0);assert(enterPulse(1450,true,queue,true,deadline)==-1);
 Point a{0,0,0,0,0,1,0,0,1},b{0,20,0,0,0,1,0,0,1};std::vector<Point> source{a,b};RouteInputs input;input.duration=10000;input.anchors={{0,a},{10000,b}};input.events={{5000,0x1c,true},{5050,0x1c,false},{6000,0x11c,true},{6050,0x11c,false}};assert(validInputs(input));auto restored=inputEnterMarkers(source,input);assert(restored.size()==4&&restored[1].enterCount==1&&restored[2].enterCount==1);assert(std::abs(restored[1].y-10)<.001f&&std::abs(restored[2].y-12)<.001f);assert(source.size()==2&&!source[0].enterCount);auto twice=inputEnterMarkers(restored,input);assert(twice.size()==restored.size()&&twice[1].enterCount==1);
 auto partial=restored;partial[2].enterCount=0;auto filled=inputEnterMarkers(partial,input);unsigned total=0;for(const auto&p:filled)total+=p.enterCount;assert(total==2);
 input.events={{10000,0x1c,true},{10000,0x1c,false}};auto endpoint=inputEnterMarkers(source,input);assert(endpoint.size()==2&&endpoint.back().enterCount==1);
 RouteInputs standing;standing.duration=1000;standing.anchors={{0,a},{1000,b}};auto dir=std::filesystem::temp_directory_path()/("district-launch-"+std::to_string(inputClock()));RouteCatalog c;c.open(dir);assert(c.add(source,"legacy"));std::vector<Point> other{{5,0,0,0,0,1,0,0,1},{5,20,0,0,0,1,0,0,1}};assert(c.add(other,"timed",0xffffff,&standing));assert(c.nearestStart(a)==0);assert(c.nearestStart(a,0,10)==0);std::filesystem::remove_all(dir);
 std::cout<<"Fast Enter/Del taps, injected filtering, paused Enter pulses, timed Enter restoration without duplication and eligible timed-route selection passed\n";
}
