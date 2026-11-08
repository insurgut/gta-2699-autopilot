#pragma once
#include "panel_layout.hpp"
struct NavigatorGroup{NavigatorLayout nav;int count=0,totalHeight=0;static constexpr int gap=3,rowHeight=62;int rowTop(int i)const{return nav.height+gap+i*(rowHeight+gap);}};
inline NavigatorGroup navigatorGroup(int w,int h,NavigatorSettings settings,int notices){NavigatorGroup g;g.nav=navigatorLayout(w,h,settings);if(!g.nav.width)return g;int available=h-32;g.count=std::clamp(notices,0,std::min(3,std::max(0,(available-160)/(NavigatorGroup::rowHeight+NavigatorGroup::gap))));int extra=g.count*(NavigatorGroup::rowHeight+NavigatorGroup::gap);if(g.nav.height+extra>available){settings.height=available-extra;g.nav=navigatorLayout(w,h,settings);}g.totalHeight=g.nav.height+extra;g.nav.y=std::clamp(g.nav.y,16,std::max(16,h-g.totalHeight-16));return g;}
inline bool groupPixelInside(int x,int y,int w,int h,int top,int bottom){if(x<0||x>=w||y<0||y>=h)return false;int r=y<h/2?top:bottom;if(r<=0)return true;float px=x+.5f,py=y+.5f,cx=px<r?float(r):px>w-r?float(w-r):px,cy=py<r?float(r):py>h-r?float(h-r):py;return (px-cx)*(px-cx)+(py-cy)*(py-cy)<=r*r;}
inline bool navigatorPixel(const NavigatorGroup& g,int x,int y){
    x-=g.nav.x;y-=g.nav.y;
    if(y<0||y>=g.totalHeight||x<0||x>=g.nav.width)return false;
    if(y<g.nav.height)return groupPixelInside(x,y,g.nav.width,g.nav.height,14,g.count?4:14);
    for(int i=0;i<g.count;++i)if(y>=g.rowTop(i)&&y<g.rowTop(i)+NavigatorGroup::rowHeight)
        return groupPixelInside(x,y-g.rowTop(i),g.nav.width,NavigatorGroup::rowHeight,4,i+1==g.count?14:4);
    return false;
}
