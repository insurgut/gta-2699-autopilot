#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
constexpr uint32_t MARKER_MAGIC=0x4d393244,MARKER_VERSION=1;
struct NativeMarkerState {
    uint32_t magic=MARKER_MAGIC,version=MARKER_VERSION,bytes=sizeof(NativeMarkerState),pid=0;
    uint64_t controllerBeat=0,loadedBeat=0,scriptBeat=0,drawFrames=0;
    uint32_t enabled=0,status=0; // 0 absent, 1 registered, 2 script running, 3 missing API, 4 submitted draw
    float x=0,y=0,z=0,heading=0;
    uint64_t revision=0;
};
static_assert(sizeof(NativeMarkerState)==80,"Shared marker protocol layout changed");
inline bool markerFresh(const NativeMarkerState&s,uint64_t now){return s.magic==MARKER_MAGIC&&s.version==MARKER_VERSION&&s.bytes==sizeof(s)&&s.enabled==1&&s.controllerBeat&&now>=s.controllerBeat&&now-s.controllerBeat<=750&&std::isfinite(s.x)&&std::isfinite(s.y)&&std::isfinite(s.z)&&std::isfinite(s.heading)&&std::abs(s.x)<100000&&std::abs(s.y)<100000&&std::abs(s.z)<10000;}
inline uint64_t nativeFloat(float f){uint32_t bits=0;std::memcpy(&bits,&f,sizeof(bits));return bits;}
// DRAW_MARKER ABI: integer/pointer slots and bit-preserving zero-extended float slots.
inline std::array<uint64_t,24> markerArguments(const NativeMarkerState&s,int type,float z,float sx,float sy,float sz,int alpha,int red=70,int green=220,int blue=165){
    return {uint64_t(type),nativeFloat(s.x),nativeFloat(s.y),nativeFloat(z),nativeFloat(0),nativeFloat(0),nativeFloat(0),nativeFloat(0),nativeFloat(0),nativeFloat(s.heading),nativeFloat(sx),nativeFloat(sy),nativeFloat(sz),uint64_t(red),uint64_t(green),uint64_t(blue),uint64_t(alpha),0,0,2,0,0,0,0};
}
