#include "gps_pace.hpp"
#include "gps_memory.hpp"
#include "gps_ribbon_geometry.hpp"
#include <cassert>
int main(){
 GpsFrame s{};s.pid=7;s.beat=1000;s.status=4;s.count=5;s.waypoint={20,0,0};for(unsigned i=0;i<5;i++)s.points[i]={float(i)*5,0,0};assert(gpsFresh(s,7,1200));assert(!gpsFresh(s,8,1200));assert(!gpsFresh(s,7,3000));s.points[2].x=100;assert(!gpsFresh(s,7,1200));s.points[2].x=10;s.waypoint.x=200;assert(!gpsFresh(s,7,1200));s.waypoint.x=20;s.waypoint.z=500;assert(gpsFresh(s,7,1200));s.waypoint.z=0;s.status=3;assert(!gpsFresh(s,7,1200));s.status=4;
 auto path=gpsRoute(s,10);assert(path.size()==5&&path.back().x==20&&path.back().speed==10);auto band=gpsRibbon(path,0,{0,0,0,0},1,0,2.5f);assert(!band.empty());for(auto p:band)assert(p.x>=2.5f&&p.x<=6.501f&&p.y==0);assert(gpsWheelSpeed(30,120,5)==35);assert(gpsWheelSpeed(30,-120,5)==25);assert(gpsWheelSpeed(5,-120,5)==5);assert(gpsWheelSpeed(300,120,5)==300);

 auto slices=gpsBandSlices(band,1.05f);
 assert(slices.size()==band.size()&&slices.front().alpha==96&&slices.back().alpha==0);
 assert(std::abs(gpsDistance(slices.front().left,slices.front().right)-2.1f)<.001f);
 assert(gpsBandSlices(band,-1).empty());
 for(size_t i=1;i<slices.size();++i)assert(slices[i].alpha<=slices[i-1].alpha);
 std::vector<Point> bend{{0,0,0,0},{50,0,0,0},{50,100,0,0}};
 auto curve=gpsRibbon(bend,0,{45,0,0,0},1,0,2.5f);
 assert(curve.size()>10&&curve.front().x==47.5f&&curve.front().y==0&&curve.back().y>0);
 assert(curve[1].y==0);
 float ribbonLength=0;
 for(size_t i=1;i<curve.size();++i)ribbonLength+=gpsDistance(curve[i-1],curve[i]);
 assert(ribbonLength<=4.001f);
 auto rounded=[](const std::vector<GpsPoint>& points) {
    std::vector<Point> path;
    for(auto p:gpsRoundRoad(points))path.push_back({p.x,p.y,p.z,0});
    return path;
 };
 const auto right=rounded({{0,0,0},{50,0,0},{50,100,0}});
 const auto gentle=rounded({{0,0,0},{50,0,0},{144,34,0}});
 const float cruise=60/3.6f;
 GpsTurnPace pace;
 assert(pace.update(1000,right,0,{5,0,0,0},cruise,cruise,2.8f)<cruise);
 const float corner=pace.update(1500,right,0,{45,0,0,0},8,cruise,2.8f);
 assert(corner>19/3.6f&&corner<24/3.6f);
 std::vector<Point> straightSamples{{50,0,0,0},{50,25,0,0},{50,100,0,0}};
 assert(pace.update(1650,straightSamples,0,{50,7,0,0},5,cruise,2.8f)==cruise);
 for(float kmh:{5.0f,30.0f,50.0f,60.0f}) {
    GpsTurnPace gentlePace;
    const float setSpeed=kmh/3.6f;
    assert(gentlePace.update(1000,gentle,0,{35,0,0,0},1,setSpeed,2.8f)==setSpeed);
 }
 GpsTurnPace slowEntry;
 assert(slowEntry.update(1000,right,0,{45,0,0,0},.5f,30/3.6f,2.8f)>19/3.6f);
 assert(slowEntry.smoothSteering(1000,.04f)==0);
 GpsBrakeModel brake;
 brake.track(1000,false);brake.sample(15,true);
 brake.track(1100,true);brake.sample(14.4f,true);
 assert(brake.observations==1&&brake.measured>4);
 brake.track(1200,false);brake.sample(13.8f,true);
 assert(brake.observations==1&&brake.brakeDuty(14,13)>0&&brake.brakeDuty(14,13)<1);
 assert(brake.brakeDuty(14,0)==1);
}
