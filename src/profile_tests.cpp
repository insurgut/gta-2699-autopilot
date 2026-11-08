#include "profile.hpp"
#include <cassert>
int main(){
    uint8_t code[]={0x48,0x8b,0x05,0,0,0,0};
    int32_t delta=int32_t(WORLD_RVA)-int32_t(WORLD_LOAD_RVA)-7;
    assert(worldInstructionMatches(code,delta));
    assert(!worldInstructionMatches(code,delta+8));
    code[1]=0x8d;assert(!worldInstructionMatches(code,delta));
    code[1]=0x8b;code[2]=0x45;assert(!worldInstructionMatches(code,delta));
    assert(driverMatches(0x12345000,0x12345000));
    assert(!driverMatches(0x12345000,0));
    assert(!driverMatches(0x12345000,0x12346000));
    assert(!driverMatches(0,0));
    assert(profileHeaderMatches(DISTRICT_TIMESTAMP,DISTRICT_IMAGE_SIZE));
    assert(profileHeaderMatches(DISTRICT_TIMESTAMP,GAME_IMAGE_SIZE));
    assert(profileHeaderMatches(GAME_TIMESTAMP,GAME_IMAGE_SIZE));
    assert(profileHeaderMatches(GAME_TIMESTAMP,DISTRICT_IMAGE_SIZE));
    assert(!profileHeaderMatches(DISTRICT_TIMESTAMP,GAME_IMAGE_SIZE+4096));
    assert(!profileHeaderMatches(0xdeadbeef,GAME_IMAGE_SIZE));
}
