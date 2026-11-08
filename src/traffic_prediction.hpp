#pragma once
#include "forward_navigation.hpp"
#include <unordered_map>
struct TrafficObservation {uint64_t id=0;uint32_t model=0;RadarBox box{};};
struct TrafficRisk {float cap=1e9f,ttc=0;uint64_t entity=0;};
struct TrafficYield {
 uint64_t entity=0,started=0,lastSeen=0,passed=0;
 bool update(uint64_t now,uint64_t approaching){
  if(!approaching)passed=0;
  if(approaching&&approaching!=passed){
   if(!entity){entity=approaching;started=now;}
   lastSeen=now;
  }
  if(!entity)return false;
  if(now-started>=8000){passed=approaching;entity=0;return false;}
  if(now-started>=1000&&now-lastSeen>=650){entity=0;return false;}
  return true;
 }
 void reset(){*this={};}
};
class TrafficPredictor {
 struct Track {RadarBox box{};uint32_t model=0;float vx=0,vy=0;uint64_t beat=0;unsigned samples=0;};
 std::unordered_map<uint64_t,Track> tracks;
public:
 void reset(){tracks.clear();}
 void observe(uint64_t beat,const std::vector<TrafficObservation>&observations){
  for(const auto&o:observations){if(!o.id||!std::isfinite(o.box.x)||!std::isfinite(o.box.y)||!std::isfinite(o.box.z)||passableRule(o.box.passability))continue;auto&t=tracks[o.id];if(beat==t.beat)continue;float dt=beat>t.beat?float(beat-t.beat)/1000:0;if(t.beat&&t.model==o.model&&dt>0&&dt<.05f)continue;
   float dx=o.box.x-t.box.x,dy=o.box.y-t.box.y;
   if(t.beat&&t.model==o.model&&dt>=.05f&&dt<=.6f&&std::hypot(dx,dy)<=dt*45&&std::abs(o.box.z-t.box.z)<2){float vx=dx/dt,vy=dy/dt;t.vx=t.samples?t.vx*.35f+vx*.65f:vx;t.vy=t.samples?t.vy*.35f+vy*.65f:vy;t.samples++;}
   else{t.vx=t.vy=0;t.samples=0;}t.box=o.box;t.beat=beat;t.model=o.model;
  }
  for(auto it=tracks.begin();it!=tracks.end();)if(beat<it->second.beat||beat-it->second.beat>1500)it=tracks.erase(it);else ++it;
 }
 uint64_t approaching(uint64_t now,NavPose car,float speed,float hw,float hl,uint64_t yielding)const{
  const float fx=std::sin(car.yaw),fy=std::cos(car.yaw);
  uint64_t candidate=0;
  float earliest=4.2f;
  for(const auto&entry:tracks){
   const auto&t=entry.second;
   if(!t.samples||now<t.beat||now-t.beat>450||std::abs(t.box.z-car.z)>2.8f)continue;
   const float age=float(now-t.beat)/1000;
   const float dx=t.box.x-car.x+t.vx*age,dy=t.box.y-car.y+t.vy*age;
   const float along=dx*fx+dy*fy,lateral=dx*fy-dy*fx;
   const float forward=t.vx*fx+t.vy*fy,sideways=t.vx*fy-t.vy*fx;
   const float passingWidth=std::clamp(hw+t.box.halfWidth+2.4f,3.5f,5.5f);
   const float overlap=std::clamp(hl+t.box.halfLength+2,5.0f,12.0f);
   if(entry.first==yielding&&std::abs(along)<=overlap&&std::abs(lateral)<=passingWidth&&std::abs(forward)>1.2f)return yielding;
   if(std::abs(along)>65||std::abs(lateral)>passingWidth+2||std::abs(forward)<1.7f||std::abs(sideways)>std::abs(forward)*.6f)continue;
   const float closing=along>0?speed-forward:forward-speed;
   if(closing<=1.4f||(along>=0&&forward>=0))continue;
   const float time=std::max(0.0f,std::abs(along)-overlap)/closing;
   if(time<earliest&&std::abs(lateral+sideways*time)<=passingWidth){earliest=time;candidate=entry.first;}
  }
  return candidate;
 }
 TrafficRisk risk(uint64_t now,NavPose car,float speed,float hw,float hl,const std::vector<Point>&path,size_t segment)const{
  TrafficRisk result;if(speed<.3f||path.size()<2)return result;
  for(const auto&entry:tracks){const auto&t=entry.second;if(!t.samples||now<t.beat||now-t.beat>500||std::hypot(t.vx,t.vy)<.5f||std::hypot(t.box.x-car.x,t.box.y-car.y)>90||std::abs(t.box.z-car.z)>4)continue;
   float age=float(now-t.beat)/1000;NavPose pose=car;size_t next=std::min(segment+1,path.size()-1);
   for(float future=.2f;future<=3.01f;future+=.2f){float travel=speed*.2f;
    while(travel>0&&next<path.size()){float dx=path[next].x-pose.x,dy=path[next].y-pose.y,d=std::hypot(dx,dy);if(d<.001f){++next;continue;}float take=std::min(d,travel);pose.x+=dx/d*take;pose.y+=dy/d*take;pose.yaw=std::atan2(dx,dy);travel-=take;if(take>=d-.001f)++next;}
    auto b=t.box;b.x+=t.vx*(age+future);b.y+=t.vy*(age+future);NavigationScene collision(&b,1,car.z,hw,hl);
    if(!collision.clear(pose)){float cap=speed*std::clamp((future-.6f)/2.0f,0.0f,1.0f);if(cap<result.cap)result={cap,future,entry.first};break;}
   }
  }return result;
 }
};
