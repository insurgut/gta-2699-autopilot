#pragma once
#include <algorithm>
#include <cstdint>
struct NavigatorSettings {int width=360,height=480,opacity=85;bool mapEnabled=false;int mapOpacity=75;};
struct NavigatorLayout {int x=16,y=100,width=0,height=0,visualX=0,visualY=0,visualWidth=0,visualHeight=0;bool horizontal=false;};
inline NavigatorLayout navigatorLayout(int screenW,int screenH,NavigatorSettings s){
 NavigatorLayout r;if(screenW<320||screenH<240)return r;
 r.width=std::min(std::clamp(s.width,240,900),screenW-32);r.height=std::min(std::clamp(s.height,160,800),screenH-100);
 r.horizontal=r.height<=300||r.width>=r.height*1.35f;
 if(r.horizontal)r.width=std::min(std::max(r.width,420),screenW-32);
 r.y=std::max(76,(screenH-r.height)/2);r.visualX=r.horizontal?std::min(140,r.width/3):0;r.visualY=r.horizontal?0:110;
 r.visualWidth=r.width-r.visualX;r.visualHeight=r.height-r.visualY-36;return r;
}
inline uint32_t premultiplyPanelPixel(uint32_t rgb,unsigned alpha){return (alpha<<24)|((((rgb>>16)&255)*alpha/255)<<16)|((((rgb>>8)&255)*alpha/255)<<8)|((rgb&255)*alpha/255);}
