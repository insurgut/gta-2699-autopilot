#pragma once
#include "route.hpp"
#include <cstdint>
// Resample by distance: dense recording points and tiny key taps do not amplify
// curvature. Opposite taps cancel in the longer before/after chords.
inline float anticipatedSpeed(const std::vector<Point>&route,size_t from,Point car,float speed,float cap,size_t stopAt=SIZE_MAX){
 if(route.size()<2||from+1>=route.size())return cap;
 size_t end=std::min(stopAt,route.size()-1);if(from>=end)return cap;
 float horizon=std::clamp(30.0f+speed*2.0f,30.0f,90.0f),arc=0,next=2;std::vector<Point> samples{car};Point previous=car;
 for(size_t i=from+1;i<=end&&arc<horizon;i++){const auto&q=route[i];float length=distance(previous,q);if(length>.001f){while(next<=arc+length&&next<=horizon){float t=(next-arc)/length;samples.push_back({previous.x+(q.x-previous.x)*t,previous.y+(q.y-previous.y)*t,previous.z+(q.z-previous.z)*t,0});next+=2;}}arc+=length;previous=q;}
 float result=cap;
 for(size_t i=3;i+3<samples.size();i++){auto a=samples[i-3],b=samples[i],c=samples[i+3];float ax=b.x-a.x,ay=b.y-a.y,bx=c.x-b.x,by=c.y-b.y;
  if(std::hypot(ax,ay)<1||std::hypot(bx,by)<1)continue;
  float angle=std::abs(std::atan2(ax*by-ay*bx,ax*bx+ay*by));
  float mix=std::clamp((angle-10*3.14159265359f/180)/(10*3.14159265359f/180),0.0f,1.0f);mix=mix*mix*(3-2*mix);if(mix<=0)continue;
  float curvature=angle/6,turn=std::min(cap,std::sqrt(3.2f/std::max(.001f,curvature)));turn=cap+(turn-cap)*mix;
  float before=std::max(0.0f,float(i)*2-6);result=std::min(result,std::sqrt(turn*turn+2*3.2f*before));
 }
 return result;
}
struct SpeedDemand {
 float value=0;uint64_t beat=0;
 void reset(){value=0;beat=0;}
 float update(uint64_t now,float requested,float actual,bool enabled=true){if(!enabled){reset();return requested;}if(!beat||now<beat||now-beat>1000)value=actual;float dt=beat&&now>=beat?std::min(.2f,float(now-beat)/1000):0;beat=now;value=std::min(requested,value+2.0f*dt);return value;}
};
