#pragma once
#include "route.hpp"
#include "radar.hpp"
struct PlanePoint {float x,y;};
struct GridFrame {
    float x=0,y=0,z=0,fx=0,fy=1;
    static GridFrame make(float x,float y,float z,float fx,float fy){float n=std::hypot(fx,fy);if(!std::isfinite(n)||n<.01f){fx=0;fy=1;n=1;}return {x,y,z,fx/n,fy/n};}
    PlanePoint local(float wx,float wy)const{float dx=wx-x,dy=wy-y;return {dx*fy-dy*fx,dx*fx+dy*fy};}
    PlanePoint world(float u,float v)const{return {x+fy*u+fx*v,y-fx*u+fy*v};}
    RadarBox localBox(RadarBox b)const{auto p=local(b.x,b.y);float u=b.fx*fy-b.fy*fx,v=b.fx*fx+b.fy*fy;b.x=p.x;b.y=p.y;b.fx=u;b.fy=v;return b;}
};
inline GridFrame startFrame(const std::vector<Point>&route){
    if(route.empty())return {};
    const auto&p=route.front();float fx=p.facingX,fy=p.facingY;
    if(std::hypot(fx,fy)<.01f){for(size_t i=1;i<route.size();i++){fx=(route[i].x-p.x)*route[i].direction;fy=(route[i].y-p.y)*route[i].direction;if(std::hypot(fx,fy)>.1f)break;}}
    return GridFrame::make(p.x,p.y,p.z,fx,fy);
}
inline GridFrame displayFrame(const std::vector<Point>&route,GridFrame car){if(!route.empty()&&distance({car.x,car.y,car.z,0},route.front())<=5)return startFrame(route);return car;}
