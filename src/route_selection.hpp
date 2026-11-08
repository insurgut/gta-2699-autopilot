#pragma once
#include "route.hpp"
#include "stops.hpp"
#include <limits>
struct RoutePick {size_t segment=0;float t=0,arc=0,x=0,y=0;bool valid=false;};
struct ThreadSegment {size_t index;float x1,y1,x2,y2,arc,length,depth1=1,depth2=1,from=0,to=1;};
inline RoutePick pickThread(const std::vector<ThreadSegment>&lines,float x,float y,float radius=48,const RoutePick*previousHit=nullptr){
    RoutePick best;float score=std::numeric_limits<float>::infinity();
    for(const auto&l:lines){float dx=l.x2-l.x1,dy=l.y2-l.y1,n=dx*dx+dy*dy;if(n<.01f)continue;
        float t=std::clamp(((x-l.x1)*dx+(y-l.y1)*dy)/n,0.0f,1.0f),px=l.x1+dx*t,py=l.y1+dy*t,d=std::hypot(x-px,y-py);float denom=(1-t)*l.depth2+t*l.depth1;float worldT=denom>.001f?t*l.depth1/denom:t;float arc=l.arc+l.length*worldT;
        if(d>radius)continue;
        // During a drag prefer the adjacent strand at crossings, never a remote lap.
        if(previousHit&&previousHit->valid&&std::abs(arc-previousHit->arc)>std::max(12.0f,l.length*2))continue;
        float rank=d+(previousHit&&previousHit->valid?std::abs(arc-previousHit->arc)*.02f:0);
        if(rank<score){score=rank;best={l.index,l.from+(l.to-l.from)*worldT,arc,px,py,true};}
    }return best;
}
inline Point routeAnchor(const std::vector<Point>&r,RoutePick p){
    if(r.size()<2)return {};
    size_t i=std::min(p.segment,r.size()-2);auto a=r[i],b=r[i+1];float t=std::clamp(p.t,0.0f,1.0f);
    if(t<=.001f)return a;
    if(t>=.999f)return b;
    Point v=a;v.x+=(b.x-a.x)*t;v.y+=(b.y-a.y)*t;v.z+=(b.z-a.z)*t;v.speed=a.speed+(b.speed-a.speed)*t;if(!a.followRecordedSpeed||!b.followRecordedSpeed)v.speed=std::max(1.0f,v.speed);v.enterCount=0;v.waitMs=0;v.direction=b.direction;v.speedLimit=b.speedLimit;v.ignoreObstacles=b.ignoreObstacles;return v;
}
inline std::vector<Point> continueRoute(const std::vector<Point>&r,RoutePick p,bool reverse){
    if(!p.valid||r.size()<2||p.segment+1>=r.size())return {};
    auto a=routeAnchor(r,p);std::vector<Point> out;
    if(!reverse){out.push_back(a);for(size_t i=p.segment+1;i<r.size();i++){if(i==p.segment+1&&p.t>=.999f)continue;out.push_back(r[i]);}}
    else{for(size_t i=0;i<=p.segment;i++){if(i==p.segment&&p.t<=.001f)break;out.push_back(r[i]);}out.push_back(a);out=returnRoute(out);}
    return out;
}
struct ThreadGesture {
    RoutePick hover,start,last;bool dragging=false,ready=false,reverse=false;
    void reset(){*this={};}
    void move(const std::vector<ThreadSegment>&lines,float x,float y){
        auto hit=pickThread(lines,x,y,48,dragging?&last:nullptr);
        if(hit.valid){hover=hit;if(dragging){last=hit;if(std::abs(last.arc-start.arc)>.4f)reverse=last.arc<start.arc;}}else if(!dragging)hover={};
    }
    bool begin(const std::vector<ThreadSegment>&lines,float x,float y){move(lines,x,y);if(!hover.valid)return false;start=last=hover;dragging=true;reverse=false;return true;}
    void end(){if(dragging){dragging=false;ready=true;}}
};

inline bool clipThreadRadius(Point a,Point b,Point center,float radius,float&from,float&to){
    float x=a.x-center.x,y=a.y-center.y,z=a.z-center.z,dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z;
    float aa=dx*dx+dy*dy+dz*dz,bb=2*(x*dx+y*dy+z*dz),cc=x*x+y*y+z*z-radius*radius;
    if(aa<1e-8f){from=0;to=1;return cc<=0;}
    float disc=bb*bb-4*aa*cc;if(disc<0)return false;float root=std::sqrt(disc);
    from=std::max(0.0f,(-bb-root)/(2*aa));to=std::min(1.0f,(-bb+root)/(2*aa));return from<=to;
}
inline RoutePick nearestRoutePoint(const std::vector<Point>&r,Point car){
    RoutePick hit;float best=std::numeric_limits<float>::infinity(),arc=0;
    for(size_t i=0;i+1<r.size();i++){auto a=r[i],b=r[i+1];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=dx*dx+dy*dy+dz*dz;
        float t=n>.001f?std::clamp(((car.x-a.x)*dx+(car.y-a.y)*dy+(car.z-a.z)*dz)/n,0.0f,1.0f):0;
        float d=std::hypot(std::hypot(car.x-a.x-dx*t,car.y-a.y-dy*t),car.z-a.z-dz*t);
        if(d<best){best=d;hit={i,t,arc+std::sqrt(n)*t,0,0,true};}arc+=std::sqrt(n);
    }return hit;
}

inline RoutePick advanceRoutePick(const std::vector<Point>&r,RoutePick start,float offset){
    float target=std::max(0.0f,start.arc+offset),arc=0;RoutePick last=start;
    for(size_t i=0;i+1<r.size();i++){float length=distance(r[i],r[i+1]);if(target<=arc+length||i+2==r.size()){float t=length>.001f?std::clamp((target-arc)/length,0.0f,1.0f):0;return {i,t,arc+length*t,0,0,true};}arc+=length;}return last;
}
