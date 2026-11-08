#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
constexpr unsigned GPS_MAGIC=0x47505344,GPS_VERSION=2,GPS_MAX_POINTS=2048,GPS_MAX_RIBBON=96;
struct GpsPoint{float x=0,y=0,z=0;};
struct GpsFrame{
 uint32_t magic=GPS_MAGIC,version=GPS_VERSION,bytes=sizeof(GpsFrame),pid=0;
 uint64_t beat=0,serial=0,controllerBeat=0;
 uint32_t status=0,count=0,enabled=0,draw=0,ribbonCount=0;
 uint32_t provider=0,reason=0,rawCount=0,rawActive=0;uint64_t rawPointer=0;
 float halfWidth=1;GpsPoint waypoint{},mirror{},rawFirst{},rawLast{};std::array<GpsPoint,GPS_MAX_POINTS> points{};std::array<GpsPoint,GPS_MAX_RIBBON> ribbon{};
};
inline bool gpsPointValid(GpsPoint p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<100000&&std::abs(p.y)<100000&&std::abs(p.z)<10000;}
inline float gpsDistance(GpsPoint a,GpsPoint b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);}
inline bool gpsFresh(const GpsFrame&s,uint32_t pid,uint64_t now){if(s.magic!=GPS_MAGIC||s.version!=GPS_VERSION||s.bytes!=sizeof(GpsFrame)||s.pid!=pid||!s.beat||now<s.beat||now-s.beat>1800||s.status!=4||s.count<2||s.count>GPS_MAX_POINTS||!gpsPointValid(s.waypoint))return false;for(size_t i=0;i<s.count;i++)if(!gpsPointValid(s.points[i])||(i&&gpsDistance(s.points[i-1],s.points[i])>30))return false;return std::hypot(s.points[s.count-1].x-s.waypoint.x,s.points[s.count-1].y-s.waypoint.y)<60;}
