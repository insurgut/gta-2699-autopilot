#include "forward_navigation.hpp"
#include "voxels.hpp"
#include <cassert>
#include <iostream>
int main(){
 setCellSize(1.25f);
 RadarBox side{2.1f,8,1,0,1,.3f,1,1,0};NavigationScene sideScene(&side,1,0,1.05f,2.5f);
 std::vector<Point> route;for(int y=0;y<=26;y++)route.push_back({.6f,float(y),0,4});
 assert(sideScene.routeClear({.6f,0,0,0},route,0,route.size()-1,20));
 VoxelGrid coarse;coarse.halfWidth=1.2f;coarse.rebuild(&side,1,.6f,0,0,1000);assert(!coarse.pathClear({.6f,0,0},{.6f,20,0})); // former false positive
 RadarBox block{0,12,1,0,1,1.3f,.8f,1,0};NavigationScene scene(&block,1,0,1.05f,2.5f);
 auto path=planForward(scene,{0,0,0,0},{0,25,0,0});std::cerr<<"plan nodes "<<path.size()<<'\n';assert(path.size()>2);
 for(size_t i=1;i<path.size();i++)assert(scene.edge(path[i-1],path[i]));
 ForwardDetour nav;nav.set(path,25,{0,25,0,0});NavPose car{0,0,0,0};bool arrived=false;
 for(int i=0;i<12000;i++){
  auto follow=nav.follow(car,2.3f,scene);if(!follow.valid){std::cerr<<"follow failed "<<i<<" at "<<car.x<<','<<car.y<<" error "<<follow.error<<'\n';break;}
  if(follow.arrived){arrived=true;break;}
  float v=std::min(2.3f,follow.speedCap),dt=.02f,wb=2.875f;car.yaw=navAngle(car.yaw+std::tan(follow.steering*.55f)/wb*v*dt);car.x+=std::sin(car.yaw)*v*dt;car.y+=std::cos(car.yaw)*v*dt;assert(scene.clear(car));
 }
 assert(arrived);
 ForwardDetour quicker;quicker.set(path,25,{0,25,0,0});NavPose fastCar{0,0,0,0};float v=0;bool fastArrived=false;
 for(int i=0;i<12000;i++){auto f=quicker.follow(fastCar,v,scene);assert(f.valid);if(f.arrived){fastArrived=true;break;}v=std::max(0.0f,v+std::clamp((f.speedCap-v)*2,-4.0f,2.0f)*.02f);auto prev=fastCar;fastCar.yaw=navAngle(fastCar.yaw+std::tan(f.steering*.55f)/2.875f*v*.02f);fastCar.x+=std::sin(fastCar.yaw)*v*.02f;fastCar.y+=std::cos(fastCar.yaw)*v*.02f;if(!scene.edge(prev,fastCar)){std::cerr<<"fast collision "<<i<<" position "<<fastCar.x<<","<<fastCar.y<<" v "<<v<<" steer "<<f.steering<<" cap "<<f.speedCap<<"\n";assert(false);}}assert(fastArrived);

 RadarBox alternateBoxes[]={block,{-4,13,1,0,1,.5f,9,1,0}};NavigationScene alternate(alternateBoxes,2,0,1.05f,2.5f);auto right=planForward(alternate,{0,0,0,0},{0,25,0,0});assert(!right.empty());bool goesRight=false;for(auto p:right){goesRight=goesRight||p.x>2.5f;assert(alternate.clear(p));}assert(goesRight);
 auto blockedPath=path[path.size()/2];RadarBox changedBoxes[]={block,{blockedPath.x,blockedPath.y,1,0,1,.6f,.6f,1,0}};NavigationScene changed(changedBoxes,2,0,1.05f,2.5f);ForwardDetour stale;stale.set(path,25,{0,25,0,0});assert(!stale.follow({0,0,0,0},2,changed).valid||!stale.follow(blockedPath,2,changed).valid);
 auto behind=path[std::min(size_t(8),path.size()-2)];ForwardDetour next;next.set(path,25,{0,25,0,0});next.cursor=4;assert(next.follow(behind,2,scene).valid);
 RadarBox wall{0,8,1,0,1,40,1,1,0};NavigationScene closed(&wall,1,0,1.05f,2.5f);assert(planForward(closed,{0,0,0,0},{0,20,0,0},1800).empty());
 RadarBox pass=block;pass.passability=WAIT_PASS;NavigationScene opened(&pass,1,0,1.05f,2.5f);assert(opened.clear({0,12,0,0}));
 std::cout<<"Physical corridor, side-object false positive, curved bypass and repeated following passed\n";
}
