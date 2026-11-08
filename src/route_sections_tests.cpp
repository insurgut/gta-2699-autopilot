#include "route_sections.hpp"
#include "driving.hpp"
#include "stops.hpp"
#include "route_selection.hpp"
#include "route_catalog.hpp"
#include <cassert>
#include <limits>
int main(){
 std::vector<Point> r;for(int i=0;i<40;i++)r.push_back({float(i*5),0,0,8});
 assert(editRouteSection(r,4,8,20,true));assert(r[4].speedLimit==0);for(size_t i=5;i<=9;i++)assert(r[i].ignoreObstacles&&std::abs(r[i].speedLimit-20/3.6f)<.001f);assert(!r[10].ignoreObstacles);
 assert(routePolicySegment(r,3,{21,0,0,0})==4);assert(routePolicySegment(r,8,{51,0,0,0})==10);assert(routePolicySegment(r,3,{100,0,0,0},5)==4); // stops fence policy projection
 assert(!editRouteSection(r,9,8,20,false));assert(!editRouteSection(r,0,39,20,false));assert(!editRouteSection(r,0,1,std::numeric_limits<float>::quiet_NaN(),false));
 assert(std::abs(routeSpeedCap(r,4,4)-20/3.6f)<.001f); // section overrides default
 auto drive=gearDriveOutput(r,5,{26,0,0,0},1,0,5,20,1);assert(drive.targetSpeed<=20/3.6f+.001f);
 auto before=gearDriveOutput(r,2,{10,0,0,0},1,0,15,20,1,SIZE_MAX,false);assert(before.targetSpeed<20); // braking before zone even without optional turn smoothing
 auto reverse=returnRoute(r);assert(reverse[r.size()-5].ignoreObstacles==r[5].ignoreObstacles);assert(reverse[r.size()-5].speedLimit==r[5].speedLimit);
 RoutePick pick{4,.5f,0,0,0,true};auto tail=continueRoute(r,pick,false);assert(tail.front().ignoreObstacles&&tail.front().speedLimit==r[5].speedLimit);
 auto file=std::filesystem::temp_directory_path()/"district-section-28.txt";assert(writeRoute(file,r));std::vector<Point> loaded;assert(loadRoute(file,loaded));assert(loaded[5].ignoreObstacles&&loaded[5].speedLimit==r[5].speedLimit);assert(!loaded[10].ignoreObstacles);
 size_t a=0,b=0;float v=0;bool ignore=false;assert(parseSectionEdit(L"4",L"8",L"20",L"1",L"40",40,a,b,v,ignore));assert(!parseSectionEdit(L"-1",L"8",L"20",L"1",L"40",40,a,b,v,ignore));assert(!parseSectionEdit(L"4",L"8",L"nan",L"1",L"40",40,a,b,v,ignore));assert(!parseSectionEdit(L"4",L"8",L"20x",L"1",L"40",40,a,b,v,ignore));assert(!parseSectionEdit(L"4",L"8",L"20",L"1",L"39",40,a,b,v,ignore));
 assert(routeSectionColor(0xabcdef,r[5])!=0xabcdef);assert(routeSectionColor(0xffffff,r[5])>>16==(routeSectionColor(0xffffff,r[5])&255));
 auto dir=std::filesystem::temp_directory_path()/"district-section-catalog-28";std::filesystem::remove_all(dir);RouteCatalog c;c.open(dir);assert(c.add(r,"Route"));assert(editRouteSection(r,4,8,0,false));assert(c.update(0,r));RouteCatalog reopened;reopened.open(dir);assert(!reopened.entries[0].points[5].ignoreObstacles&&reopened.entries[0].points[5].speedLimit==0);std::filesystem::remove_all(dir);std::filesystem::remove(file);
 std::vector<Point> speedTrace{{0,0,0,5},{20,0,0,15},{60,0,0,15}};
 for(auto& p:speedTrace)p.followRecordedSpeed=true;
 assert(std::abs(routeSpeedAheadCap(speedTrace,0,{10,0,0,0},30)-10)<.01f);
 speedTrace[1].speedLimit=6;
 assert(std::abs(routeSpeedAheadCap(speedTrace,0,{10,0,0,0},30)-6)<.01f);
 speedTrace[1].speedLimit=0;
 for(auto& p:speedTrace)p.followRecordedSpeed=false;
 assert(routeSpeedAheadCap(speedTrace,0,{10,0,0,0},30)==30);
}
