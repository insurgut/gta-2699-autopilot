#include "finish_hold.hpp"
#include <cassert>
int main(){
    FinishHold hold;hold.input(false,true);assert(!hold.takeover);
    hold.begin();
    for(int i=0;i<10000;i++){hold.input(true,true);hold.input(true,false);hold.input(false,false);}
    assert(hold.active&&!hold.takeover);
    hold.input(false,true);assert(hold.takeover);
    hold.reset();assert(!hold.active&&!hold.takeover);
    hold.begin();assert(!hold.takeover);hold.input(false,true);assert(hold.takeover);
}
