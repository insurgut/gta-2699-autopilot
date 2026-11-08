#include "panel_layout.hpp"
#include <cassert>
#include <iostream>
int main(){
 for(int sw:{320,800,1280,1920})for(int sh:{240,600,720,1080})for(int h:{160,300,301,480,800})for(int w:{240,360,600,900}){auto r=navigatorLayout(sw,sh,{w,h,50});assert(r.width>0&&r.height>0);assert(r.x+r.width<=sw&&r.y+r.height<=sh);assert(r.visualWidth>0&&r.visualHeight>0);assert(r.visualX+r.visualWidth<=r.width&&r.visualY+r.visualHeight<=r.height);if(r.height<=300)assert(r.horizontal&&r.visualX>0);}
 auto compact=navigatorLayout(1280,720,{600,300,0});assert(compact.horizontal&&compact.visualX==140);auto tall=navigatorLayout(1280,720,{360,480,100});assert(!tall.horizontal&&tall.visualY==110);
 assert(premultiplyPanelPixel(0xabcdef,0)==0);assert(premultiplyPanelPixel(0xabcdef,255)==0xffabcdef);assert(premultiplyPanelPixel(0xffffff,128)==0x80808080);assert(navigatorLayout(100,100,{}).width==0);
 std::cout<<"Responsive 300px layout, screen clamps and transparent/half/opaque alpha passed\n";
}
