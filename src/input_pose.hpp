#pragma once
#include "route.hpp"
inline InputAnchor inputPose(const RouteInputs&r,uint64_t ms){
 if(r.anchors.empty())return {};
 auto it=std::lower_bound(r.anchors.begin(),r.anchors.end(),ms,[](const auto&a,uint64_t t){return a.ms<t;});if(it==r.anchors.begin())return *it;if(it==r.anchors.end())return r.anchors.back();auto b=*it,a=*(it-1);float t=float(ms-a.ms)/float(b.ms-a.ms);Point p=a.pose;p.x+=(b.pose.x-p.x)*t;p.y+=(b.pose.y-p.y)*t;p.z+=(b.pose.z-p.z)*t;p.speed+=(b.pose.speed-p.speed)*t;float ax=p.facingX+(b.pose.facingX-p.facingX)*t,ay=p.facingY+(b.pose.facingY-p.facingY)*t,n=std::hypot(ax,ay);if(n>.01f){p.facingX=ax/n;p.facingY=ay/n;}return {ms,p};
}
