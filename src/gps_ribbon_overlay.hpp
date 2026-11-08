#pragma once
#include "telemetry.hpp"
#include "gps_ribbon_geometry.hpp"
class GpsRibbonOverlay{
 HWND window=nullptr;HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ old=nullptr;uint32_t*pixels=nullptr;int width=0,height=0;
 static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;return DefWindowProcW(w,m,a,b);}
 void clear(){if(dc){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);}dc=nullptr;bitmap=nullptr;pixels=nullptr;width=height=0;}
 bool buffer(int w,int h){if(dc&&width==w&&height==h)return true;clear();dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);if(!dc||!bitmap||!pixels){clear();return false;}old=SelectObject(dc,bitmap);width=w;height=h;return true;}
 // The four bytes temporarily hold 2x2 coverage samples. Shared edges never
 // blend over each other, and the final white pixels are premultiplied.
 void triangle(GpsScreenPoint a,GpsScreenPoint b,GpsScreenPoint c,float aa,float ab,float ac,RECT& dirty){
    const float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
    if(std::abs(area)<.0001f)return;
    const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x}))));
    const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y}))));
    const int x1=std::min(width,int(std::ceil(std::max({a.x,b.x,c.x}))));
    const int y1=std::min(height,int(std::ceil(std::max({a.y,b.y,c.y}))));
    if(x0>=x1||y0>=y1)return;
    dirty.left=std::min(dirty.left,LONG(x0));dirty.top=std::min(dirty.top,LONG(y0));
    dirty.right=std::max(dirty.right,LONG(x1));dirty.bottom=std::max(dirty.bottom,LONG(y1));
    const float ux=(b.y-c.y)/area,uy=(c.x-b.x)/area;
    const float vx=(c.y-a.y)/area,vy=(a.x-c.x)/area;
    for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){
        auto& pixel=pixels[size_t(y)*width+x];
        const float u=1+(x+.25f-a.x)*ux+(y+.25f-a.y)*uy;
        const float v=(x+.25f-a.x)*vx+(y+.25f-a.y)*vy;
        for(unsigned sub=0;sub<4;++sub){
            const float dx=(sub&1)*.5f,dy=(sub>>1)*.5f;
            const float wa=u+dx*ux+dy*uy,wb=v+dx*vx+dy*vy,wc=1-wa-wb;
            if(std::min({wa,wb,wc})<-.00001f)continue;
            const unsigned value=unsigned(std::clamp(wa*aa+wb*ab+wc*ac,0.0f,255.0f)+.5f);
            const unsigned shift=sub*8;
            if(value>((pixel>>shift)&255))pixel=(pixel&~(255u<<shift))|(value<<shift);
        }
    }
 }
public:
 ~GpsRibbonOverlay(){close();}
 void hide(){if(window)ShowWindow(window,SW_HIDE);}
 void close(){hide();clear();if(window)DestroyWindow(window);window=nullptr;}
 template<class Project,class Mask>void draw(POINT origin,int w,int h,const std::vector<GpsPoint>&line,float halfWidth,Project project,Mask masked){
  if(line.size()<2||w<100||h<100){hide();return;}if(!window){WNDCLASSW cls{};cls.lpfnWndProc=proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"DistrictGPSRibbon33";RegisterClassW(&cls);window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,cls.hInstance,nullptr);}if(!window||!buffer(w,h)){hide();return;}std::fill_n(pixels,size_t(w)*h,0);
  RECT dirty{w,h,0,0};
  const auto slices=gpsBandSlices(line,halfWidth);
  GpsScreenPoint previousLeft{},previousRight{};
  bool previousValid=false;
  float previousAlpha=0;
  for(const auto& slice:slices){
    GpsScreenPoint left{},right{};
    const bool valid=project(slice.left,left)&&project(slice.right,right);
    if(valid&&previousValid){
        triangle(previousLeft,previousRight,right,previousAlpha,previousAlpha,slice.alpha,dirty);
        triangle(previousLeft,right,left,previousAlpha,slice.alpha,slice.alpha,dirty);
    }
    previousLeft=left;previousRight=right;previousAlpha=slice.alpha;previousValid=valid;
  }
  if(dirty.left>=dirty.right){hide();return;}
  for(int y=dirty.top;y<dirty.bottom;++y)for(int x=dirty.left;x<dirty.right;++x){
    auto& pixel=pixels[size_t(y)*width+x];
    if(masked(x,y)){pixel=0;continue;}
    const unsigned alpha=((pixel&255)+((pixel>>8)&255)+((pixel>>16)&255)+(pixel>>24)+2)/4;
    pixel=alpha*0x01010101u;
  }
  POINT source{0,0};SIZE size{w,h};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};if(UpdateLayeredWindow(window,nullptr,&origin,&size,dc,&source,0,&blend,ULW_ALPHA))SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);else hide();
 }
};
