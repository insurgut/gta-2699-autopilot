#include "hotkeys.hpp"
#include <cassert>
int main(){
    RisingKeys<2> keys;
    assert(keys.pressed(0,true,true));
    for(int i=0;i<100;i++)assert(!keys.pressed(0,true,true));
    assert(!keys.pressed(0,false,true));
    assert(keys.pressed(0,true,true));
    assert(keys.pressed(1,true,true));
    assert(!keys.pressed(1,true,false));
    assert(!keys.pressed(1,true,true)); // gaining focus while held must not start playback
    assert(!keys.pressed(1,false,true));
    assert(keys.pressed(1,true,true));
}
