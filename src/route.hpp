#pragma once
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>
#include <cstdint>
#include "input_keys.hpp"
struct Point { float x,y,z,speed; unsigned waitMs=0; int direction=1; unsigned enterCount=0; float facingX=0,facingY=0,startHalfWidth=0,startHalfLength=0; float speedLimit=0; bool ignoreObstacles=false; bool followRecordedSpeed=false; };
inline float distance(const Point&a,const Point&b) {
    return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));
}
inline bool valid(const Point&p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.speed)
      &&std::isfinite(p.speedLimit)&&p.speedLimit>=0&&p.speedLimit<=1000&&std::abs(p.x)<100000&&std::abs(p.y)<100000&&std::abs(p.z)<10000&&p.speed>=0&&p.waitMs<=86400000&&p.enterCount<=255&&std::isfinite(p.facingX)&&std::isfinite(p.facingY)&&std::hypot(p.facingX,p.facingY)<=1.1f&&std::isfinite(p.startHalfWidth)&&p.startHalfWidth>=0&&p.startHalfWidth<=10&&std::isfinite(p.startHalfLength)&&p.startHalfLength>=0&&p.startHalfLength<=30&&(p.direction==1||p.direction==-1);
}
struct InputEvent {uint64_t ms=0;unsigned key=0;bool down=false;};
struct InputAnchor {uint64_t ms=0;Point pose{};};
enum class RecordedPace { Unavailable, Disabled, Enabled };
struct RouteInputs {
 RecordedPace pace=RecordedPace::Unavailable;
 uint64_t duration=0;std::vector<InputEvent> events;std::vector<InputAnchor> anchors;
 bool available()const{return duration>0&&anchors.size()>=2;}
};
inline bool validInputs(const RouteInputs&input){
 if(input.duration>86400000||input.events.size()>1000000||input.anchors.size()>200000)return false;
 InputState held{};uint64_t last=0;for(const auto&e:input.events){if(!validInputKey(e.key)||e.ms<last||e.ms>input.duration||held[e.key]==e.down)return false;held[e.key]=e.down;last=e.ms;}if(std::any_of(held.begin(),held.end(),[](bool v){return v;}))return false;
 bool first=true;last=0;for(const auto&a:input.anchors){if(a.ms>input.duration||(!first&&a.ms<=last)||!valid(a.pose)||std::hypot(a.pose.facingX,a.pose.facingY)<.5f)return false;first=false;last=a.ms;}
 return input.anchors.empty()?input.events.empty()&&input.duration==0:input.available();
}
inline bool loadRoute(const std::filesystem::path&path,std::vector<Point>&out,RouteInputs*input=nullptr) {
    std::ifstream f(path); std::string tag; unsigned version; size_t count;
    if(!(f>>tag>>version>>count)||tag!="GTA_AUTOPILOT"||(version<1||version>8)||count<2||count>100000) return false;
    std::vector<Point> r(count);RouteInputs timing;
    for(auto&p:r) {if(!(f>>p.x>>p.y>>p.z>>p.speed))return false;if(version>=2&&!(f>>p.waitMs))return false;if(version>=3&&!(f>>p.direction))return false;if(version>=4&&!(f>>p.enterCount))return false;if(version>=5&&!(f>>p.facingX>>p.facingY>>p.startHalfWidth>>p.startHalfLength))return false;if(version>=7){unsigned ignore=0;if(!(f>>p.speedLimit>>ignore)||ignore>1)return false;p.ignoreObstacles=ignore!=0;}if(!valid(p))return false;}
    if(version>=6){size_t events=0,anchors=0;unsigned format=0;if(!(f>>tag>>format>>timing.duration>>events>>anchors)||tag!="INPUTS"||format!=1||events>1000000||anchors>200000)return false;timing.events.resize(events);timing.anchors.resize(anchors);
        for(auto&e:timing.events){unsigned down=0;if(!(f>>e.ms>>e.key>>down)||down>1)return false;e.down=down!=0;}
        for(auto&a:timing.anchors){auto&p=a.pose;if(!(f>>a.ms>>p.x>>p.y>>p.z>>p.speed>>p.direction>>p.facingX>>p.facingY))return false;}
        if(!validInputs(timing))return false;
    }
    if(version>=8){
        unsigned enabled=0;
        if(!(f>>tag>>enabled)||tag!="PACE"||enabled>1)return false;
        timing.pace=enabled?RecordedPace::Enabled:RecordedPace::Disabled;
    }
    f>>std::ws; if(!f.eof()) return false;
    out=std::move(r);if(input)*input=std::move(timing);return true;
}
inline bool writeRoute(const std::filesystem::path&path,const std::vector<Point>&r,const RouteInputs*input=nullptr) {
    if(r.size()<2||r.size()>100000||(input&&!validInputs(*input))) return false;
    for(auto&p:r) if(!valid(p)) return false;
    const bool pace=input&&input->pace!=RecordedPace::Unavailable;
    std::ofstream f(path,std::ios::trunc); f<<"GTA_AUTOPILOT "<<(pace?8:7)<<' '<<r.size()<<'\n'<<std::setprecision(9);
    for(auto&p:r) f<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.speed<<' '<<p.waitMs<<' '<<p.direction<<' '<<p.enterCount<<' '<<p.facingX<<' '<<p.facingY<<' '<<p.startHalfWidth<<' '<<p.startHalfLength<<' '<<p.speedLimit<<' '<<p.ignoreObstacles<<'\n';
    RouteInputs empty; if(!input)input=&empty;
    if(input){f<<"INPUTS 1 "<<input->duration<<' '<<input->events.size()<<' '<<input->anchors.size()<<'\n';for(const auto&e:input->events)f<<e.ms<<' '<<e.key<<' '<<e.down<<'\n';for(const auto&a:input->anchors){const auto&p=a.pose;f<<a.ms<<' '<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.speed<<' '<<p.direction<<' '<<p.facingX<<' '<<p.facingY<<'\n';}}
    if(pace)f<<"PACE "<<(input->pace==RecordedPace::Enabled)<<'\n';
    f.close(); return bool(f);
}
// Select a nearby recorded goal without jumping across corners or crossings.
inline size_t nextGoal(const std::vector<Point>&r,size_t from,int dir) {
    float length=0; size_t i=from;
    while((dir>0&&i+1<r.size())||(dir<0&&i>0)) {
        size_t j=dir>0?i+1:i-1;
        length+=distance(r[i],r[j]); i=j;
        if(length>=22) break;
    }
    return i;
}
