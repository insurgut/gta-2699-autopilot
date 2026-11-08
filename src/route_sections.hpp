#pragma once
#include "route.hpp"
#include <string>
// Parameters belong to the incoming edge of a point, like the gear direction.
inline size_t routePolicySegment(const std::vector<Point>&r,size_t from,Point car,size_t stopAt=SIZE_MAX){
 if(r.size()<2)return 0;
 size_t end=std::min(r.size()-1,stopAt);if(!end)return 0;from=std::min(from,end-1);size_t best=from;float gap=1e30f;
 for(size_t i=from;i<end&&i<=from+8;i++){auto a=r[i],b=r[i+1];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=dx*dx+dy*dy+dz*dz,t=n>.0001f?std::clamp(((car.x-a.x)*dx+(car.y-a.y)*dy+(car.z-a.z)*dz)/n,0.0f,1.0f):0,d=distance(car,{a.x+t*dx,a.y+t*dy,a.z+t*dz,0});if(d<gap){gap=d;best=i;}}
 return best;
}
inline bool recordedSpeedEdge(const Point& a,const Point& b){
    return a.followRecordedSpeed&&b.followRecordedSpeed&&b.speedLimit<=0;
}
inline float routeSpeedAt(const Point& a,const Point& b,float progress,float global){
    if(b.speedLimit>0)return b.speedLimit;
    if(!recordedSpeedEdge(a,b))return global;
    // Stopping points use the route stop logic. Keep enough throttle to reach them.
    return std::max(.8f,a.speed+(b.speed-a.speed)*std::clamp(progress,0.0f,1.0f));
}
inline float routeSpeedCap(const std::vector<Point>&r,size_t segment,float global){
    if(r.empty())return global;
    const size_t end=std::min(segment+1,r.size()-1);
    return routeSpeedAt(r[end?end-1:end],r[end],1,global);
}
inline float routeSpeedAheadCap(const std::vector<Point>&r,size_t segment,Point car,float global,size_t stopAt=SIZE_MAX){
    if(r.size()<2||segment>=r.size()-1)return global;
    const auto& a=r[segment];
    const auto& b=r[segment+1];
    const float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
    const float length2=dx*dx+dy*dy+dz*dz;
    const float progress=length2>.0001f?
        ((car.x-a.x)*dx+(car.y-a.y)*dy+(car.z-a.z)*dz)/length2:0;
    float cap=routeSpeedAt(a,b,progress,global),ahead=0;
    Point cursor=car;
    const size_t end=std::min(stopAt,r.size()-1);
    for(size_t j=segment+1;j<=end;j++){
        const auto& prev=r[j-1];
        const auto& next=r[j];
        const float length=distance(cursor,next);
        const bool recorded=recordedSpeedEdge(prev,next);
        const float limit=routeSpeedAt(prev,next,1,global);
        if((j>segment+1||recorded)&&limit<cap){
            const float brakingDistance=std::max(0.0f,ahead+(recorded?length:0)-1);
            cap=std::min(cap,std::sqrt(limit*limit+6.4f*brakingDistance));
        }
        ahead+=length;
        cursor=next;
        if(ahead>150)break;
    }
    return cap;
}
inline bool editRouteSection(std::vector<Point>&r,size_t from,size_t to,float kmh,bool ignore){
 if(from>to||to+1>=r.size()||!std::isfinite(kmh)||kmh<0||kmh>3600)return false;
 for(size_t i=from+1;i<=to+1;i++){r[i].speedLimit=kmh/3.6f;r[i].ignoreObstacles=ignore;}
 return true;
}
inline bool parseSectionEdit(const std::wstring&a,const std::wstring&b,const std::wstring&v,const std::wstring&i,const std::wstring&count,size_t points,size_t&from,size_t&to,float&speed,bool&ignore){
 auto number=[](const std::wstring&s,size_t&out){if(s.empty()||s.find_first_not_of(L"0123456789")!=std::wstring::npos)return false;try{out=std::stoull(s);return true;}catch(...){return false;}};
 size_t n=0;if(!number(a,from)||!number(b,to)||!number(count,n)||n!=points||points<2||from>to||to>=points-1||(i!=L"0"&&i!=L"1"))return false;
 try{size_t used=0;speed=std::stof(v,&used);if(used!=v.size()||!std::isfinite(speed)||speed<0||speed>3600)return false;}catch(...){return false;}ignore=i==L"1";return true;
}
// HSV hue is invariant: only value/saturation encode the section settings.
inline uint32_t routeSectionColor(uint32_t rgb,const Point&p){
 if(p.speedLimit<=0&&!p.ignoreObstacles)return rgb;
 float r=float((rgb>>16)&255)/255,g=float((rgb>>8)&255)/255,b=float(rgb&255)/255;
 float hi=std::max({r,g,b}),lo=std::min({r,g,b}),delta=hi-lo;
 float saturation=hi>0?delta/hi:0,value=std::clamp(.55f+(p.speedLimit>0?std::min(p.speedLimit*3.6f,100.0f)/100*.35f:.2f)+(p.ignoreObstacles?.1f:0),.4f,1.0f);
 float newSat=std::clamp(saturation*(p.ignoreObstacles?.45f:1.1f),0.0f,1.0f);
 auto channel=[&](float c){return unsigned(std::lround(255*value*(1-newSat+(delta>.00001f?(c-lo)/delta:0)*newSat)));};
 return (channel(r)<<16)|(channel(g)<<8)|channel(b);
}
