#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <limits>
struct ScenePoint {float x,y,z;};
struct SceneFootprint {ScenePoint center,forward;float halfWidth,halfLength,halfHeight;};
inline bool finiteScenePoint(ScenePoint v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline bool usableBasis(ScenePoint v){float n=v.x*v.x+v.y*v.y+v.z*v.z;return finiteScenePoint(v)&&n>=0.0025f&&n<=100;}
inline bool modelFootprint(ScenePoint origin,ScenePoint right,ScenePoint forward,ScenePoint up,ScenePoint lo,ScenePoint hi,SceneFootprint&out){
    if(!finiteScenePoint(origin)||!finiteScenePoint(lo)||!finiteScenePoint(hi)||!usableBasis(right)||!usableBasis(forward)||!usableBasis(up))return false;
    ScenePoint extent{hi.x-lo.x,hi.y-lo.y,hi.z-lo.z};
    if(extent.x<0||extent.y<0||extent.z<0||std::max({extent.x,extent.y,extent.z})<0.005f
        ||std::max({extent.x,extent.y,extent.z})>2000)return false;
    ScenePoint center{(lo.x+hi.x)/2,(lo.y+hi.y)/2,(lo.z+hi.z)/2};
    if(std::max({std::abs(center.x),std::abs(center.y),std::abs(center.z)})>5000)return false;
    float fx=forward.x,fy=forward.y,n=std::hypot(fx,fy);
    if(n<0.05f){fx=-right.y;fy=right.x;n=std::hypot(fx,fy);}
    if(n<0.05f){fx=up.x;fy=up.y;n=std::hypot(fx,fy);}
    if(n<0.05f)return false;
    fx/=n;fy/=n;
    float hx=std::max(0.005f,extent.x/2),hy=std::max(0.005f,extent.y/2),hz=std::max(0.005f,extent.z/2);
    auto side=[&](ScenePoint a){return a.x*fy-a.y*fx;};
    auto along=[&](ScenePoint a){return a.x*fx+a.y*fy;};
    out.center={origin.x+right.x*center.x+forward.x*center.y+up.x*center.z,
                origin.y+right.y*center.x+forward.y*center.y+up.y*center.z,
                origin.z+right.z*center.x+forward.z*center.y+up.z*center.z};
    out.forward={fx,fy,0};
    out.halfWidth=std::abs(side(right))*hx+std::abs(side(forward))*hy+std::abs(side(up))*hz;
    out.halfLength=std::abs(along(right))*hx+std::abs(along(forward))*hy+std::abs(along(up))*hz;
    out.halfHeight=std::abs(right.z)*hx+std::abs(forward.z)*hy+std::abs(up.z)*hz;
    return finiteScenePoint(out.center)&&std::isfinite(out.halfWidth)&&std::isfinite(out.halfLength)&&std::isfinite(out.halfHeight)
        &&out.halfWidth>0&&out.halfLength>0&&out.halfHeight>0&&out.halfWidth<=2000&&out.halfLength<=2000&&out.halfHeight<=2000;
}
inline float footprintDistanceSquared(const SceneFootprint&b,ScenePoint p){
    float dx=p.x-b.center.x,dy=p.y-b.center.y;
    float x=std::max(0.0f,std::abs(dx*b.forward.y-dy*b.forward.x)-b.halfWidth);
    float y=std::max(0.0f,std::abs(dx*b.forward.x+dy*b.forward.y)-b.halfLength);
    return x*x+y*y;
}
inline bool modelFootprintSlice(ScenePoint origin,ScenePoint right,ScenePoint forward,ScenePoint up,ScenePoint lo,ScenePoint hi,float lowZ,float highZ,SceneFootprint&out){
    SceneFootprint full{};if(!modelFootprint(origin,right,forward,up,lo,hi,full)||!std::isfinite(lowZ)||!std::isfinite(highZ)||lowZ>highZ)return false;
    ScenePoint corners[8]{};
    for(unsigned i=0;i<8;i++){
        float x=i&1?hi.x:lo.x,y=i&2?hi.y:lo.y,z=i&4?hi.z:lo.z;
        corners[i]={origin.x+right.x*x+forward.x*y+up.x*z,origin.y+right.y*x+forward.y*y+up.y*z,origin.z+right.z*x+forward.z*y+up.z*z};
    }
    float minSide=std::numeric_limits<float>::infinity(),maxSide=-minSide,minAlong=minSide,maxAlong=-minSide,minZ=minSide,maxZ=-minSide;
    auto add=[&](ScenePoint p){float side=p.x*full.forward.y-p.y*full.forward.x,along=p.x*full.forward.x+p.y*full.forward.y;
        minSide=std::min(minSide,side);maxSide=std::max(maxSide,side);minAlong=std::min(minAlong,along);maxAlong=std::max(maxAlong,along);minZ=std::min(minZ,p.z);maxZ=std::max(maxZ,p.z);};
    for(auto p:corners)if(p.z>=lowZ&&p.z<=highZ)add(p);
    for(unsigned i=0;i<8;i++)for(unsigned axis:{1u,2u,4u})if(!(i&axis)){
        auto a=corners[i],b=corners[i|axis];float dz=b.z-a.z;if(std::abs(dz)<1e-7f)continue;
        for(float z:{lowZ,highZ}){float t=(z-a.z)/dz;if(t>=0&&t<=1)add({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,z});}
    }
    if(!std::isfinite(minSide))return false;
    float side=(minSide+maxSide)/2,along=(minAlong+maxAlong)/2;
    out={{full.forward.y*side+full.forward.x*along,-full.forward.x*side+full.forward.y*along,(minZ+maxZ)/2},full.forward,std::max(0.005f,(maxSide-minSide)/2),std::max(0.005f,(maxAlong-minAlong)/2),std::max(0.005f,(maxZ-minZ)/2)};
    return true;
}
template<class T> inline float keepNearest(std::vector<T>&items,size_t limit){
    auto compare=[](const T&a,const T&b){return a.distance2<b.distance2||(a.distance2==b.distance2&&a.entity<b.entity);};
    float firstDropped=std::numeric_limits<float>::infinity();
    if(items.size()>limit){std::partial_sort(items.begin(),items.begin()+limit,items.end(),compare);firstDropped=std::min_element(items.begin()+limit,items.end(),compare)->distance2;items.resize(limit);}
    else std::sort(items.begin(),items.end(),compare);
    return firstDropped;
}
