#include "physical_recovery.hpp"
#include <cassert>
int main(){
 RadarBox wall{0,3,1,0,1,6,.5f,1,0};NavigationScene scene(&wall,1,0,1,2.5f);NavPose car{0,0,0,0};
 assert(!scene.clear(car));auto escape=physicalEscape(scene,car,1,{0,15,0},0);assert(escape.valid&&escape.gear==-1&&escape.end.y< -1);
 assert(scene.clear({escape.end.x,escape.end.y,0,0}));
 auto repeated=physicalEscape(scene,car,1,{0,15,0},20);assert(repeated.valid); // no arbitrary two-attempt cutoff
 assert(!scene.escapeStep(car,car,{0,.25f,0,0}));assert(scene.escapeStep(car,car,{0,-.25f,0,0}));
 RadarBox rear{0,-3,1,0,1,6,.5f,1,0};RadarBox both[]{wall,rear};NavigationScene tight(both,2,0,1,2.5f);assert(!physicalEscape(tight,car,1,{0,15,0},0).valid);
 NavigationScene open(nullptr,0,0,1,2.5f);assert(physicalEscape(open,car,-1,{0,-15,0},0).gear==1);
 ContactBackoffGate gate;assert(!gate.update(100,true,0));assert(gate.update(1900,true,0));assert(!gate.update(2000,false,0));assert(!gate.update(4000,true,1));
}
