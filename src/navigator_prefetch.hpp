#pragma once
#include "archive_map.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

inline std::vector<ArchiveTile> navigatorPrefetch(float x,float y,float fx,float fy,float speed,int localRadius){
    constexpr int zoom=8;
    constexpr float unit=9000.0f/ArchiveTile::columns(zoom);
    const float length=std::hypot(fx,fy);
    if(!std::isfinite(length)||length<.01f||!(x>=-4140&&x<4860&&y>-5100&&y<=8400))return {};
    fx/=length;fy/=length;
    const float ahead=std::isfinite(speed)?std::clamp(speed*40,1000.0f,1800.0f):1000.0f;
    constexpr float halfWidth=2.25f*unit;
    const auto column=[](float px){return int(std::floor((px+4140)/unit));};
    const auto row=[](float py){return int(std::floor((8400-py)/unit));};
    const int cx=column(x),cy=row(y);
    if(!ArchiveTile{zoom,cx,cy}.valid())return {};
    const float endX=x+fx*ahead,endY=y+fy*ahead;
    const int left=std::max(0,column(std::min(x,endX)-halfWidth));
    const int right=std::min(ArchiveTile::columns(zoom)-1,column(std::max(x,endX)+halfWidth));
    const int top=std::max(0,row(std::max(y,endY)+halfWidth));
    const int bottom=std::min(ArchiveTile::rows(zoom)-1,row(std::min(y,endY)-halfWidth));

    struct Candidate {ArchiveTile tile;float priority;};
    std::vector<Candidate> candidates;
    for(int ty=top;ty<=bottom;++ty)for(int tx=left;tx<=right;++tx){
        if(std::max(std::abs(tx-cx),std::abs(ty-cy))<=localRadius)continue;
        const float dx=-4140+(tx+.5f)*unit-x,dy=8400-(ty+.5f)*unit-y;
        const float along=dx*fx+dy*fy,side=std::abs(dx*fy-dy*fx);
        if(along>0&&along<=ahead&&side<=halfWidth)candidates.push_back({{zoom,tx,ty},along+.6f*side});
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& a,const Candidate& b){
        return a.priority<b.priority;
    });
    std::vector<ArchiveTile> tiles;
    tiles.reserve(candidates.size());
    for(const auto& entry:candidates)tiles.push_back(entry.tile);
    return tiles;
}
