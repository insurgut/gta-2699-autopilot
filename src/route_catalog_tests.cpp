#include "route_catalog.hpp"
#include <cassert>
#include <iostream>
int main(){
 auto dir=std::filesystem::temp_directory_path()/("district-catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 RouteCatalog c;c.open(dir);std::vector<Point>a{{0,0,0,4},{10,10,0,4}},b{{100,0,0,5},{110,20,0,5}};a[1].enterCount=2;
 assert(c.add(a,u8"Первый маршрут",0xabcdef));auto first=c.entries.front();assert(c.add(b,u8"Второй маршрут",0xabcdef));assert(c.entries.size()==2&&c.entries[0].color!=c.entries[1].color);assert(c.entries[0].id!=c.entries[1].id);assert(c.entries[0].points[1].enterCount==2);
 assert(c.nearestStart({2,0,0,0})==0);assert(c.nearestStart({99,0,0,0})==1);assert(c.nearestStart({50.2f,0,0,0},0,1.5f)==0);assert(c.nearestStart({52,0,0,0},0,1.5f)==1);
 assert(c.nearestLaunch({15,0,0,0})==0);
 assert(c.nearestLaunch({15.01f,0,0,0})==SIZE_MAX);
 assert(c.nearestLaunch({0,0,15.01f,0})==SIZE_MAX);
 assert(c.nearestLaunch({10,10,0,0})==0);
 assert(c.nearestLaunch({110,20,0,0})==SIZE_MAX);
 RouteCatalog overlapping;
 overlapping.entries=c.entries;
 overlapping.entries[1].points.front().x=12;
 assert(overlapping.nearestLaunch({5,0,0,0})==0);
 assert(overlapping.nearestLaunch({7,0,0,0})==1);
 assert(c.rename(0,u8"Дом → работа \"утро\""));assert(!c.rename(0,"bad\nname"));assert(!c.rename(99,"name"));assert(!c.add({a.front()}));assert(c.entries.size()==2);
 RouteCatalog reopened;reopened.open(dir);assert(reopened.entries.size()==2);assert(reopened.entries[0].name==u8"Дом → работа \"утро\"");assert(reopened.entries[0].color==first.color);assert(reopened.entries[0].points[1].enterCount==2);assert(reopened.entries[1].color==c.entries[1].color);
 uint32_t parsed=0;
 assert(parseRouteColor(L"#AabBcC",parsed)&&parsed==0xaabbcc);
 assert(!parseRouteColor(L"#00ZZ00",parsed));
 assert(parseRouteColor(L"#000000",parsed)&&c.setColor(0,parsed)&&c.setColor(1,parsed));
 assert(!c.setColor(0,0x1000000)&&!c.setColor(99,0));
 reopened.open(dir);
 assert(reopened.entries[0].color==0&&reopened.entries[1].color==0);
 assert(reopened.entries[0].name==c.entries[0].name&&reopened.entries[0].points[1].enterCount==2);
 assert(reopened.entries[0].input.pace==RecordedPace::Unavailable);
 assert(!reopened.setRecordedPace(0,true));
 RouteInputs profile;
 profile.pace=RecordedPace::Enabled;
 assert(reopened.add(a,"profile",0x224488,&profile));
 const auto profilePath=reopened.file(2);
 std::vector<Point> profilePoints;
 RouteInputs readProfile;
 assert(loadRoute(profilePath,profilePoints,&readProfile)&&readProfile.pace==RecordedPace::Enabled);
 auto edited=profilePoints;
 edited[1].speedLimit=8;
 assert(reopened.update(2,edited));
 assert(loadRoute(profilePath,profilePoints,&readProfile)&&readProfile.pace==RecordedPace::Disabled);
 assert(profilePoints[1].speed==a[1].speed&&profilePoints[1].speedLimit==8);
 assert(reopened.setRecordedPace(2,true));
 assert(loadRoute(profilePath,profilePoints,&readProfile)&&readProfile.pace==RecordedPace::Enabled);
 assert(reopened.erase(2));
 std::ofstream(dir/"invalid.txt")<<"bad route";std::ofstream(dir/"partial.txt.tmp")<<"bad route";reopened.open(dir);assert(reopened.entries.size()==2);
 auto removedFile=reopened.file(1);auto observed=removedFile;observed+=".observed";std::ofstream(observed)<<"observations";
 assert(!reopened.erase(99));assert(reopened.erase(1));assert(reopened.entries.size()==1&&reopened.entries[0].id==first.id);assert(!std::filesystem::exists(removedFile));assert(!std::filesystem::exists(observed));auto retired=removedFile;retired+=".deleted";assert(std::filesystem::exists(retired));
 RouteCatalog afterDelete;afterDelete.open(dir);assert(afterDelete.entries.size()==1);assert(afterDelete.erase(0));afterDelete.open(dir);assert(afterDelete.entries.empty()&&afterDelete.hasHistory());assert(!afterDelete.erase(0));
 std::filesystem::remove_all(dir);std::cout<<"Catalog isolation, durable colors/names, nearest starts, hysteresis, invalid files and record preservation passed\n";
}
