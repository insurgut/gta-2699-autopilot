#pragma once
#include "forward_navigation.hpp"
struct DirectRejoin {std::vector<NavPose> path;size_t routeGoal=0;};
inline DirectRejoin directRejoin(const NavigationScene&scene,NavPose car,const std::vector<Point>&route,size_t from,size_t limit,float speed){
 DirectRejoin result;if(route.size()<2||!scene.clear(car))return result;limit=std::min(limit,route.size()-1);float arc=0;Point previous{car.x,car.y,car.z,0};
 for(size_t i=from+1;i<=limit&&i<from+120;i++){auto q=route[i];arc+=distance(previous,q);previous=q;if(arc>55)break;float dx=q.x-car.x,dy=q.y-car.y,d=std::hypot(dx,dy);if(d<std::max(8.0f,speed*1.2f)||d>45||std::abs(q.z-car.z)>2)continue;
  float bearing=std::atan2(dx,dy);if(std::abs(navAngle(bearing-car.yaw))>.6f)continue;
  size_t after=std::min(i+1,limit),before=i>0?i-1:i;float heading=std::atan2(route[after].x-route[before].x,route[after].y-route[before].y);if(std::abs(navAngle(heading-bearing))>.55f)continue;
  std::vector<NavPose> candidate{car};bool clear=true;int n=std::max(1,int(std::ceil(d/.75f)));
  for(int j=1;j<=n;j++){float t=float(j)/n,at=d*t,yaw=navAngle(car.yaw+navAngle(bearing-car.yaw)*std::min(1.0f,at/4));if(d-at<4)yaw=navAngle(yaw+navAngle(heading-yaw)*(1-(d-at)/4));NavPose p{car.x+dx*t,car.y+dy*t,car.z+(q.z-car.z)*t,yaw};if(!scene.edge(candidate.back(),p)){clear=false;break;}candidate.push_back(p);}
  if(clear){result={std::move(candidate),i};if(d>=std::max(18.0f,speed*2))break;}
 }
 return result;
}
