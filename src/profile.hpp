#pragma once
#include <cstdint>
constexpr uint32_t DISTRICT_TIMESTAMP=0x6a8715a7;
constexpr uint32_t DISTRICT_IMAGE_SIZE=102903808;
constexpr uint32_t GAME_TIMESTAMP=0x62d9a9e4;
constexpr uint32_t GAME_IMAGE_SIZE=0x4a6de00;
constexpr uint32_t WORLD_RVA=0x26684d8;
constexpr uint32_t WORLD_LOAD_RVA=0x796d;
constexpr uint32_t REPLAY_RVA=0x20304c8;
constexpr uint32_t VIEWPORT_RVA=0x20d8c90;
constexpr uint32_t PED_VEHICLE_OFFSET=0xd30;
constexpr uint32_t ACTIVE_DRIVER_OFFSET=0xc68;
constexpr uint32_t VIEWPORT_MATRIX_OFFSET=0x24c;
inline bool worldInstructionMatches(const uint8_t*code,int32_t relative){
    return (code[0]==0x48||code[0]==0x4c)&&code[1]==0x8b&&(code[2]&0xc7)==0x05
        &&int64_t(WORLD_LOAD_RVA)+7+relative==WORLD_RVA;
}
inline bool driverMatches(uint64_t ped,uint64_t occupant){return ped>0x10000&&ped==occupant;}
inline bool profileHeaderMatches(uint32_t timestamp,uint32_t imageSize){
    // The subprocess module extent and the overlaid GTA PE header size differ.
    return (timestamp==DISTRICT_TIMESTAMP||timestamp==GAME_TIMESTAMP)
        &&(imageSize==DISTRICT_IMAGE_SIZE||imageSize==GAME_IMAGE_SIZE);
}
