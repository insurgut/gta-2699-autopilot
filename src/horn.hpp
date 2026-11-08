#pragma once
#include <cstdint>
struct HornPulse {
 unsigned stage=0;uint64_t due=0,last=0;bool held=false,armed=true;
 // Event driven: never block the controller loop for either of the two taps.
 int update(uint64_t now,bool enabled,bool nearby){
  if(!enabled){int event=held?-1:0;stage=0;held=false;due=0;armed=true;return event;}
  if(!nearby)armed=true;
  if(!stage&&nearby&&armed&&(!last||now-last>=10000)){stage=1;held=true;due=now+100;last=now;armed=false;return 1;}
  if(stage&&now>=due){if(stage==1){stage=2;held=false;due=now+150;return -1;}if(stage==2){stage=3;held=true;due=now+100;return 1;}stage=0;held=false;return -1;}return 0;
 }
};
