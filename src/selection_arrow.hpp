#pragma once
#include "route_selection.hpp"
struct ArrowPoint{float x=0,y=0;};
struct ArrowStep{ArrowPoint end,c1{},c2{};bool curve=false;};
inline uint32_t selectionArrowColor(uint32_t rgb){return ((((rgb>>16)&255)*4/5)<<16)|((((rgb>>8)&255)*4/5)<<8)|((rgb&255)*4/5);}
inline std::vector<std::vector<ArrowPoint>> selectionArrowChains(const std::vector<ThreadSegment>&lines,float start,float finish){
 std::vector<std::vector<ArrowPoint>> out;float lo=std::min(start,finish),hi=std::max(start,finish);if(hi-lo<.001f)return out;size_t last=SIZE_MAX;
 for(const auto&l:lines){if(l.length<.001f||l.arc>=hi||l.arc+l.length<=lo)continue;
  auto at=[&](float t){float d=(1-t)*l.depth1+t*l.depth2;float u=d>.001f?t*l.depth2/d:t;return ArrowPoint{l.x1+(l.x2-l.x1)*u,l.y1+(l.y2-l.y1)*u};};
  auto a=at(std::clamp((lo-l.arc)/l.length,0.0f,1.0f)),b=at(std::clamp((hi-l.arc)/l.length,0.0f,1.0f));
  if(out.empty()||l.index!=last+1||std::hypot(out.back().back().x-a.x,out.back().back().y-a.y)>2)out.push_back({a});
  if(std::hypot(out.back().back().x-b.x,out.back().back().y-b.y)>.01f)out.back().push_back(b);
  last=l.index;
 }return out;
}
inline std::vector<ArrowStep> roundedArrowPath(const std::vector<ArrowPoint>&p){
 std::vector<ArrowStep> out;if(p.empty())return out;out.push_back({p.front()});
 auto blend=[](ArrowPoint a,ArrowPoint b,float t){return ArrowPoint{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};};
 for(size_t i=1;i+1<p.size();i++){float a=std::hypot(p[i].x-p[i-1].x,p[i].y-p[i-1].y),b=std::hypot(p[i+1].x-p[i].x,p[i+1].y-p[i].y);if(a<.01f||b<.01f)continue;float radius=std::min({8.0f,a*.25f,b*.25f});auto entry=blend(p[i],p[i-1],radius/a),exit=blend(p[i],p[i+1],radius/b);out.push_back({entry});out.push_back({exit,blend(entry,p[i],2.0f/3),blend(exit,p[i],2.0f/3),true});}
 if(p.size()>1)out.push_back({p.back()});
 return out;
}
