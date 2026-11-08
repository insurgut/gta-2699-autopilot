#pragma once
#include "route_catalog.hpp"
#include "wait_approach.hpp"
#include <unordered_set>
#include <unordered_map>
struct WaitingMarker {Point pose;uint32_t model=0;unsigned delayMs=5000;WaitApproach approach{};};
struct WaitingMarkers {
 std::vector<WaitingMarker> entries;std::unordered_set<uint64_t> seen;std::unordered_map<uint64_t,size_t> bindings;uint64_t serial=0;
 void clear(){entries.clear();seen.clear();bindings.clear();serial=0;}
 bool observe(uint64_t entity,uint32_t model,Point p,const std::vector<Point>&path){
  if(!model||!valid(p)||entries.size()>=4096||seen.count(entity))return false;
  bool beside=false;
  for(size_t i=0;i<path.size();i++){Point a=path[i],b=path[std::min(i+1,path.size()-1)];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=dx*dx+dy*dy+dz*dz,t=n>.0001f?std::clamp(((p.x-a.x)*dx+(p.y-a.y)*dy+(p.z-a.z)*dz)/n,0.0f,1.0f):0;if(distance(p,{a.x+t*dx,a.y+t*dy,a.z+t*dz,0})<15){beside=true;break;}}
  if(!beside)return false;
  if(entity)seen.insert(entity);
  for(size_t i=0;i<entries.size();i++)if(entries[i].model==model&&distance(entries[i].pose,p)<3){if(entity)bindings[entity]=i;return false;}
  if(entity){bindings[entity]=entries.size();}entries.push_back({p,model});return true;
 }
 bool setDelay(size_t index,double seconds,uint32_t model){if(index>=entries.size()||entries[index].model!=model||!std::isfinite(seconds)||seconds<0||seconds>3600)return false;entries[index].delayMs=unsigned(std::llround(seconds*1000));return true;}
 bool setApproach(size_t index,const WaitApproach& profile){
  if(index>=entries.size()||!profile.valid())return false;
  entries[index].approach=profile;return true;
 }
 const WaitingMarker* find(uint64_t entity,uint32_t model,Point pose)const{
  const auto binding=bindings.find(entity);
  if(binding!=bindings.end()&&binding->second<entries.size()&&entries[binding->second].model==model)return &entries[binding->second];
  const WaitingMarker* best=nullptr;
  float gap=15;
  for(const auto& entry:entries){const float d=distance(entry.pose,pose);if(entry.model==model&&d<gap){gap=d;best=&entry;}}
  return best;
 }
 unsigned delayFor(uint64_t entity,uint32_t model,Point pose)const{
  const auto* marker=find(entity,model,pose);return marker?marker->delayMs:5000;
 }
 bool save(const std::filesystem::path& path)const{
  auto temp=path;temp+=".tmp";
  std::ofstream f(temp);f<<"WAIT_POINTS 3 "<<entries.size()<<'\n'<<std::setprecision(9);
  for(const auto& e:entries){
   f<<e.pose.x<<' '<<e.pose.y<<' '<<e.pose.z<<' '<<e.model<<' '<<e.delayMs
    <<' '<<e.approach.stop<<' '<<e.approach.innerBoundary<<' '<<e.approach.outer;
   for(float cap:e.approach.kmh)f<<' '<<cap;
   f<<'\n';
  }
  f.close();return bool(f)&&replaceRouteFile(temp,path);
 }
 template<class Rule>bool load(const std::filesystem::path& path,Rule rule){
  clear();std::ifstream f(path);std::string tag;unsigned version=0;size_t count=0;
  if(!(f>>tag>>version>>count)||tag!="WAIT_POINTS"||version<1||version>3||count>4096)return false;
  std::vector<WaitingMarker> loaded;
  for(size_t i=0;i<count;i++){
   WaitingMarker m{};
   if(!(f>>m.pose.x>>m.pose.y>>m.pose.z>>m.model)||!valid(m.pose)||!m.model)return false;
   if(version>=2&&(!(f>>m.delayMs)||m.delayMs>3600000))return false;
   if(version>=3){
    auto& p=m.approach;
    if(!(f>>p.stop>>p.innerBoundary>>p.outer>>p.kmh[0]>>p.kmh[1]>>p.kmh[2])||!p.valid())return false;
   }
   if(rule(m.model))loaded.push_back(m);
  }
  f>>std::ws;if(!f.eof())return false;entries=std::move(loaded);return true;
 }
};
