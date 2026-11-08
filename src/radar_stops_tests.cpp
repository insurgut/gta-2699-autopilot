#include "radar.hpp"
#include "stops.hpp"
#include "driving.hpp"
#include <cassert>
#include <iostream>
int main(){
    auto empty=radarScan(0,0,0,0,1,nullptr,0,100);
    assert(empty.clear[2]==100&&avoid(empty,10).lateral==0);
    RadarBox box{0,22,0,0,1,1,2,1};auto blocked=radarScan(0,0,0,0,1,&box,1,100);
    assert(blocked.clear[2]<20&&blocked.clear[0]==100&&blocked.clear[4]==100);
    auto choice=avoid(blocked,12);assert(choice.lateral!=0&&choice.speedCap<12);
    RadarBox wall{0,8,0,0,1,20,2,3};auto all=radarScan(0,0,0,0,1,&wall,1,100);assert(avoid(all,12).blocked&&avoid(all,12).speedCap==0);
    box.z=20;assert(radarScan(0,0,0,0,1,&box,1,100).clear[2]==100); // Bridge overhead excluded.
    box={22,0,0,1,0,1,2,1};assert(radarScan(0,0,0,1,0,&box,1,100).clear[2]<20); // Rotates with car.
    box={0,-10,0,0,1,2,3,1};assert(radarScan(0,0,0,0,1,&box,1,100).clear[2]==100);
    std::vector<Point> r{{0,0,0,0},{0,5,0,3}};StopRecorder recorder;
    recorder.update(r,{0,5,0,0},0,1000);recorder.update(r,{0,5,0,0},0,1600);recorder.update(r,{0,5,0,0},0,3600);
    assert(r.size()==3&&r.back().waitMs==2600);recorder.update(r,{0,6,0,2},2,3700);assert(recorder.anchor==SIZE_MAX);
    auto path=std::filesystem::temp_directory_path()/"gta-stops-v3-test.txt";
    assert(writeRoute(path,r));std::vector<Point> loaded;assert(loadRoute(path,loaded)&&loaded.back().waitMs==2600);
    std::filesystem::remove(path);
    std::vector<Point> road;for(int i=0;i<100;i++)road.push_back({0,float(i*2),0,1});road[10].waitMs=3000;
    auto control=driveOutput(road,0,{0,0,0,0},0,1,0,60);assert(control.targetSpeed>12); // Old hidden cap removed.
    auto stop=driveOutput(road,0,{0,18.5f,0,0},0,1,3,60,nextStop(road,1));assert(stop.arrived&&stop.segment<10);
    std::cout<<"Radar geometry, obstacle avoidance, blocked corridors, orientation, stop recording/serialization and stop barriers passed.\n";
}
