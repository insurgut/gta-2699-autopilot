#pragma once
#include <algorithm>
#include <array>
#include <cmath>

struct WaitApproach {
    float stop=.5f,innerBoundary=6,outer=12;
    std::array<float,3> kmh{20,20,25};

    bool valid()const{
        return std::isfinite(stop)&&std::isfinite(innerBoundary)&&std::isfinite(outer)&&
            stop>=.2f&&stop<=30&&innerBoundary>stop&&outer>innerBoundary&&outer<=120&&
            std::all_of(kmh.begin(),kmh.end(),[](float n){return std::isfinite(n)&&n>=1&&n<=130;});
    }
    float speed(float gap)const{
        if(!std::isfinite(gap)||gap<=stop)return 0;
        float cap=kmh[gap<innerBoundary?0:gap<outer?1:2]/3.6f;
        const std::array<float,3> distance{stop,innerBoundary,outer};
        const std::array<float,3> boundary{0,kmh[0]/3.6f,kmh[1]/3.6f};
        for(size_t i=0;i<distance.size();++i)if(gap>=distance[i])
            cap=std::min(cap,std::sqrt(boundary[i]*boundary[i]+4*(gap-distance[i])));
        return cap;
    }
};
