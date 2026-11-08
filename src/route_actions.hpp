#pragma once
#include "route.hpp"
struct RouteActions {
    size_t cursor=0;Point previous{};bool havePrevious=false;
    void reset(){*this={};}
    size_t next(const std::vector<Point>&r)const{size_t i=cursor;while(i<r.size()&&!r[i].enterCount)++i;return i;}
    unsigned consume(const std::vector<Point>&r,size_t i){if(i<cursor||i>=r.size()||!r[i].enterCount)return 0;cursor=i+1;havePrevious=false;return r[i].enterCount;}
    unsigned poll(const std::vector<Point>&route,size_t segment,Point car){
        unsigned count=0;
        while(cursor<route.size()){
            if(!route[cursor].enterCount){++cursor;continue;}
            if(cursor>segment+1)break;
            bool reached=distance(car,route[cursor])<=2.0f;
            if(!reached&&havePrevious&&distance(car,previous)<20){
                auto m=route[cursor];float x=car.x-previous.x,y=car.y-previous.y,z=car.z-previous.z,n=x*x+y*y+z*z;
                float t=n>1e-5f?std::clamp(((m.x-previous.x)*x+(m.y-previous.y)*y+(m.z-previous.z)*z)/n,0.0f,1.0f):0;
                Point p{previous.x+t*x,previous.y+t*y,previous.z+t*z,0};reached=distance(p,m)<=1.5f;
            }
            if(!reached)break;
            count+=route[cursor++].enterCount;
        }
        previous=car;havePrevious=true;return count;
    }
};
inline bool recordEnter(std::vector<Point>&route,Point p){
    if(!valid(p)||route.empty()||route.size()>=100000)return false;
    if(distance(route.back(),p)<=0.05f){if(route.back().enterCount>=255)return false;++route.back().enterCount;}
    else{p.enterCount=1;route.push_back(p);}return true;
}
