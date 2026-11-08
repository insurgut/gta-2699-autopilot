#pragma once
#include "radar.hpp"
#include <cstdint>
#include <vector>
struct PassageObject {RadarBox box;uint32_t model;};
struct PassageFrame {float x,y,z,fx,fy,speed,halfLength;uint64_t time;int gear;};
struct PassageObservation {RadarBox box;uint32_t model;};
inline bool centerCrosses(const RadarBox&b,float ax,float ay,float bx,float by){
    float n=std::hypot(b.fx,b.fy);if(n<0.5f)return false;float fx=b.fx/n,fy=b.fy/n;
    float x=(ax-b.x)*fy-(ay-b.y)*fx,y=(ax-b.x)*fx+(ay-b.y)*fy;
    float dx=(bx-ax)*fy-(by-ay)*fx,dy=(bx-ax)*fx+(by-ay)*fy;
    float lo=0,hi=1;
    auto slab=[&](float p,float d,float extent){if(std::abs(d)<1e-6f)return std::abs(p)<=extent;float a=(-extent-p)/d,c=(extent-p)/d;if(a>c)std::swap(a,c);lo=std::max(lo,a);hi=std::min(hi,c);return lo<=hi;};
    return slab(x,dx,b.halfWidth)&&slab(y,dy,b.halfLength);
}
class PassageLearner {
    struct Candidate {PassageObject object;PassageFrame entry;bool crossed=false;};
    std::vector<Candidate> pending;PassageFrame previous{};bool valid=false;
public:
    void reset(){pending.clear();valid=false;}
    std::vector<PassageObservation> update(const PassageFrame&f,const std::vector<PassageObject>&objects){
        std::vector<PassageObservation> learned;
        if(!valid){previous=f;valid=true;return learned;}
        float step=std::hypot(f.x-previous.x,f.y-previous.y);
        if(f.time<=previous.time||f.time-previous.time>500||step>12||f.gear!=previous.gear){reset();previous=f;valid=true;return learned;}
        for(auto it=pending.begin();it!=pending.end();){
            auto&c=*it;auto&b=c.object.box;float fx=c.entry.fx*c.entry.gear,fy=c.entry.fy*c.entry.gear;
            float alignment=(f.fx*f.gear)*fx+(f.fy*f.gear)*fy;
            bool failed=f.time-c.entry.time>8000||f.speed<std::max(0.7f,c.entry.speed*0.35f)||alignment<0.85f||std::abs(f.z-b.z)>3;
            if(!failed&&centerCrosses(b,previous.x,previous.y,f.x,f.y))c.crossed=true;
            float extent=std::abs(b.fx*fx+b.fy*fy)*b.halfLength+std::abs(b.fy*fx-b.fx*fy)*b.halfWidth;
            float ahead=(b.x-f.x)*fx+(b.y-f.y)*fy;
            if(!failed&&c.crossed&&ahead<-(f.halfLength+extent+0.4f)){
                auto box=b;box.passability=3;learned.push_back({box,c.object.model});it=pending.erase(it);
            }else if(failed)it=pending.erase(it);else ++it;
        }
        if(f.speed>1.0f){float fx=f.fx*f.gear,fy=f.fy*f.gear;
            for(const auto&o:objects){const auto&b=o.box;if(!o.model||b.halfWidth<=0||b.halfLength<=0||b.halfWidth>4||b.halfLength>4||b.z+b.halfHeight<f.z+0.1f||b.z-b.halfHeight>f.z+1.6f)continue;
                float n=std::hypot(b.fx,b.fy);if(n<0.9f||n>1.1f)continue;
                float extent=std::abs(b.fx*fx+b.fy*fy)*b.halfLength+std::abs(b.fy*fx-b.fx*fy)*b.halfWidth;
                float ahead=(b.x-f.x)*fx+(b.y-f.y)*fy;
                if(ahead<f.halfLength+extent+0.25f||ahead>f.halfLength+extent+10)continue;
                if(!centerCrosses(b,f.x,f.y,f.x+fx*18,f.y+fy*18))continue;
                bool exists=false;for(const auto&c:pending)if(c.object.model==o.model&&std::hypot(c.object.box.x-b.x,c.object.box.y-b.y)<0.75f){exists=true;break;}
                if(!exists&&pending.size()<128)pending.push_back({o,f,false});
            }
        }previous=f;return learned;
    }
};
inline bool observedMatches(const PassageObservation&o,const PassageObject&p){return o.model==p.model&&std::hypot(o.box.x-p.box.x,o.box.y-p.box.y)<0.75f&&std::abs(o.box.z-p.box.z)<0.75f&&std::abs(o.box.halfWidth-p.box.halfWidth)<0.5f&&std::abs(o.box.halfLength-p.box.halfLength)<0.5f;}
