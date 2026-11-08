#pragma once
#include "scene_view.hpp"
#include "satellite_tiles.hpp"
#include "telemetry.hpp"
#include "navigator_group.hpp"
#include "navigator_notices.hpp"
#include "notification_icons.hpp"
#include <map>
class ScenePanel {
    sceneview::Renderer renderer;sceneview::Frame frame,previewFrame;HWND window=nullptr;
    HDC memory=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ original=nullptr;uint32_t*pixels=nullptr;int bw=0,bh=0;
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;return DefWindowProcW(w,m,a,b);}
    void clear(){if(memory){SelectObject(memory,original);DeleteObject(bitmap);DeleteDC(memory);}memory=nullptr;bitmap=nullptr;pixels=nullptr;bw=bh=0;}
    bool buffer(int w,int h){if(memory&&bw==w&&bh==h)return true;clear();memory=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;bitmap=CreateDIBSection(memory,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);if(!bitmap||!pixels){clear();return false;}original=SelectObject(memory,bitmap);bw=w;bh=h;return true;}
    bool wasRecording=false,wasPlaying=false;
    uint64_t bannerAt=0;
    uint32_t bannerColor=0;
    bool bannerRecord=false;
    std::wstring bannerName;
    std::map<std::pair<int,int>,HFONT> fonts;
    sceneview::GroundAtlas ground;
    SatelliteTiles satellite;

    void glyph(unsigned icon,int x,int y,int size,uint32_t color){
        const auto& mask=notificationIcons[icon];
        for(int row=0;row<size;++row)for(int col=0;col<size;++col){
            const unsigned a=mask[(row*32/size)*32+col*32/size];
            if(!a)continue;
            auto& pixel=pixels[size_t(y+row)*bw+x+col];
            uint32_t mixed=0;
            for(int shift:{0,8,16})
                mixed|=((((color>>shift)&255)*a+((pixel>>shift)&255)*(255-a)+127)/255)<<shift;
            pixel=mixed;
        }
    }
    void drawScene(sceneview::Frame& out,int width,int height,const Telemetry& s,
                   const std::vector<RadarBox>& boxes,const std::vector<Point>& route,
                   bool valid,bool fresh,uint32_t routeColor,float speed){
        std::vector<sceneview::Item> items;
        if(fresh){
            const size_t count=std::min(size_t(std::min(MAX_OBSTACLES,s.obstacleCount)),boxes.size());
            items.reserve(count);
            for(size_t i=0;i<count;++i)items.push_back({boxes[i],s.obstacles[i].kind,s.obstacles[i].entity});
        }
        sceneview::Pose car{{s.position.x,s.position.y,s.position.z},s.forward.x,s.forward.y,
                            s.ownHalfWidth>0?s.ownHalfWidth:1.05f,s.ownHalfLength>0?s.ownHalfLength:2.5f};
        renderer.render(out,width,height,car,items,route,valid,routeColor,s.ownPaint);
        if(valid&&settings.mapEnabled&&settings.mapOpacity>0){
            const auto tiles=satellite.snapshot(car.center.x,car.center.y,car.fx,car.fy,speed);
            ground.draw(out,car,[&](float x,float y,uint32_t& color){return tiles.sample(x,y,color);});
        }
    }
public:
    NavigatorSettings settings;
    std::vector<NavigatorNotice> notices;
    bool load(const std::filesystem::path&directory){return renderer.load(directory);}
    void loadMap(const std::filesystem::path& file,const std::filesystem::path& decoder){
        satellite.load(decoder);
        auto atlas=static_cast<HBITMAP>(LoadImageW(nullptr,file.c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION));
        if(!atlas)return;
        DIBSECTION dib{};
        if(GetObjectW(atlas,sizeof(dib),&dib)==sizeof(dib)&&dib.dsBm.bmBits&&
           dib.dsBm.bmBitsPixel==8&&dib.dsBm.bmWidth>1&&dib.dsBm.bmWidth<=4096&&
           dib.dsBm.bmHeight>1&&dib.dsBm.bmHeight<=4096){
            ground.width=dib.dsBm.bmWidth;
            ground.height=dib.dsBm.bmHeight;
            ground.pixels.resize(size_t(ground.width)*ground.height);
            const auto* data=static_cast<const uint8_t*>(dib.dsBm.bmBits);
            // The bundled BMP has an identity grayscale palette, so indices are luminance.
            for(int y=0;y<ground.height;++y){
                const int row=dib.dsBmih.biHeight>0?ground.height-1-y:y;
                std::copy_n(data+size_t(row)*dib.dsBm.bmWidthBytes,ground.width,
                            ground.pixels.data()+size_t(y)*ground.width);
            }
        }
        DeleteObject(atlas);
    }
    bool ready()const{return renderer.ready();}
    void configure(NavigatorSettings value){settings=value;}
    NavigatorGroup layout(int width,int height)const{return navigatorGroup(width,height,settings,int(notices.size()));}
    void startMapCache(const std::filesystem::path& dir){satellite.start(dir);}
    MapTileProgress mapDownloadProgress(){return satellite.downloadProgress();}
    void restartMapTile(ArchiveTile tile){satellite.restart(tile);}
    void prioritizeMapTile(ArchiveTile tile){satellite.prioritize(tile);}
    void hide(){if(window)ShowWindow(window,SW_HIDE);}
    void close(){hide();clear();for(auto& font:fonts)DeleteObject(font.second);fonts.clear();if(window)DestroyWindow(window);window=nullptr;}
    void activity(bool recording,bool playing,const std::wstring& name,uint32_t color){
        if((recording&&!wasRecording)||(playing&&!wasPlaying)){
            bannerAt=GetTickCount64();
            bannerRecord=recording;
            bannerName=name;
            bannerColor=color;
        }
        wasRecording=recording;wasPlaying=playing;
    }
    const sceneview::Frame* preview(const Telemetry& s,const std::vector<RadarBox>& boxes,
                         const std::vector<Point>& route,uint32_t color,float speed){
        const auto now=GetTickCount64();
        const bool valid=s.valid&&s.sampleBeat&&now>=s.sampleBeat&&now-s.sampleBeat<=600;
        const bool fresh=s.radarValid&&s.radarBeat&&now>=s.radarBeat&&now-s.radarBeat<=600;
        if(!valid||!renderer.ready())return {};
        drawScene(previewFrame,240,174,s,boxes,route,valid,fresh,color,speed);
        const unsigned alpha=unsigned(settings.mapOpacity)*255/100;
        auto pixel=[&](size_t i){
            if(previewFrame.depth[i]>0)return previewFrame.pixels[i];
            if(previewFrame.depth[i]==0)return 0x1d1d1du;
            return (premultiplyPanelPixel(previewFrame.pixels[i],alpha)+
                    premultiplyPanelPixel(0x1d1d1d,255-alpha))&0xffffff;
        };
        for(size_t i=0;i<previewFrame.pixels.size();++i)previewFrame.pixels[i]=pixel(i);
        return &previewFrame;
    }
    auto mapImage(ArchiveTile tile){return satellite.image(tile);}
    void draw(POINT origin,int width,int height,const Telemetry&s,const std::vector<RadarBox>&boxes,const std::vector<Point>&route,float speed,float target,const std::wstring&status,uint32_t routeColor){
        auto group=layout(width,height);auto r=group.nav;if(!r.width||!r.height){hide();return;}
        if(!window){WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"DistrictNavigator21";RegisterClassW(&c);window=CreateWindowExW(WS_EX_TOPMOST|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,c.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,c.hInstance,nullptr);}
        if(!window||!buffer(r.width,group.totalHeight)){hide();return;}
        uint64_t now=GetTickCount64();bool valid=s.valid&&s.sampleBeat&&now>=s.sampleBeat&&now-s.sampleBeat<=600,fresh=s.radarValid&&s.radarBeat&&now>=s.radarBeat&&now-s.radarBeat<=600;
        r.visualX=std::clamp(r.width*40/100,104,160);
        const bool showMap=valid&&settings.mapEnabled&&settings.mapOpacity>0;
        constexpr int inset=8,radius=12;
        const int metricsWidth=r.visualX;
        r.visualWidth=r.width-r.visualX-(showMap?inset:0);
        r.visualY=showMap?inset:0;
        r.visualHeight=r.height-2*r.visualY;
        auto inScene=[&](int x,int y){return !showMap||groupPixelInside(x,y,r.visualWidth,r.visualHeight,radius,radius);};
        drawScene(frame,r.visualWidth,r.visualHeight,s,boxes,route,valid,fresh,routeColor,speed);
        std::fill_n(pixels,size_t(bw)*bh,0x1d1d1d);std::vector<unsigned char> opaque(size_t(bw)*bh,0);
        // One layered surface: identical background opacity and separate rounded masks at every seam.
        auto inside=[&](int x,int y){return navigatorPixel(group,x+group.nav.x,y+group.nav.y);};
        for(int y=0;y<bh;y++)for(int x=0;x<bw;x++)if(inside(x,y)&&(!inside(x-1,y)||!inside(x+1,y)||!inside(x,y-1)||!inside(x,y+1)))pixels[size_t(y)*bw+x]=0x444444;
        for(int y=0;y<r.visualHeight;y++)for(int x=0;x<r.visualWidth;x++){size_t src=size_t(y)*r.visualWidth+x,dst=size_t(y+r.visualY)*bw+x+r.visualX;if(frame.depth[src]>0&&inScene(x,y)){pixels[dst]=frame.pixels[src];opaque[dst]=1;}}
        const int left=metricsWidth, top=std::max(8,(r.height-128)/2);
        auto capsule=[&](int y,int height,uint32_t color){
            for(int py=0;py<height;++py)for(int x=0;x<left-2*inset;++x)
                if(groupPixelInside(x,py,left-2*inset,height,radius,radius))pixels[size_t(y+py)*bw+x+inset]=color;
        };
        capsule(top,58,0x303034);capsule(top+63,26,0x2a2a2e);capsule(top+94,26,0x2a2a2e);
        auto before=std::vector<uint32_t>(pixels,pixels+size_t(bw)*bh);
        SetBkMode(memory,TRANSPARENT);
        auto text=[&](const wchar_t*value,RECT rect,int size,int weight,COLORREF color,UINT align){
            auto& font=fonts[{size,weight}];
            if(!font)font=CreateFontW(-size,0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
            auto old=SelectObject(memory,font);SetTextColor(memory,color);
            DrawTextW(memory,value,-1,&rect,align|DT_NOPREFIX|((align&DT_WORDBREAK)?0:DT_SINGLELINE|DT_END_ELLIPSIS));
            SelectObject(memory,old);
        };
        wchar_t speedText[20],targetText[35];
        if(valid)swprintf(speedText,20,L"%.0f",std::max(0.0f,std::isfinite(speed)?speed*3.6f:0));else wcscpy(speedText,L"—");
        swprintf(targetText,35,L"Цель %.0f",std::max(0.0f,std::isfinite(target)?target*3.6f:0));
        const unsigned statusIcon=status.find(L"Ошибка")!=std::wstring::npos?1u:
            status==L"Запись"?9u:status==L"Воспроизводится"?4u:5u;
        glyph(0,17,top+15,18,0xf6f6f7);
        glyph(10,16,top+69,14,0xf6f6f7);
        glyph(statusIcon,16,top+100,14,0xf6f6f7);
        text(speedText,{39,top+1,left-12,top+38},left<118?24:29,FW_SEMIBOLD,RGB(250,250,250),DT_LEFT|DT_VCENTER);
        text(L"км/ч",{40,top+35,left-12,top+54},11,FW_NORMAL,RGB(193,193,198),DT_LEFT|DT_VCENTER);
        text(targetText,{35,top+63,left-12,top+89},11,FW_SEMIBOLD,RGB(233,233,237),DT_LEFT|DT_VCENTER);
        const wchar_t* shortStatus=status==L"Не воспроизводится"?L"Ожидание":status==L"Воспроизводится"?L"Проезд":
            status==L"Маршрут завершён"?L"Финиш":status.c_str();
        text(shortStatus,{35,top+94,left-12,top+120},11,FW_SEMIBOLD,RGB(233,233,237),DT_LEFT|DT_VCENTER);
        if(!valid)text(L"Сядь за руль",{left+4,r.height/2-12,bw-7,r.height/2+12},12,FW_NORMAL,RGB(185,185,185),DT_CENTER|DT_VCENTER);
        for(int i=0;i<group.count;i++){
            const auto&n=notices[size_t(i)];int top=group.rowTop(i),iconY=top+15;
            glyph(unsigned(n.icon),14,iconY,32,0xebebeb);
            bool detail=!n.detail.empty();text(n.title.c_str(),{58,top+(detail?7:16),bw-12,top+(detail?29:46)},13,FW_SEMIBOLD,RGB(238,238,238),DT_LEFT|DT_VCENTER);
            if(detail)text(n.detail.c_str(),{58,top+28,bw-12,top+59},11,FW_NORMAL,RGB(190,190,190),DT_LEFT|DT_WORDBREAK);
        }
        GdiFlush();unsigned base=unsigned(std::clamp(settings.opacity,0,100)*255/100);
        for(int y=0;y<bh;y++)for(int x=0;x<bw;x++){size_t at=size_t(y)*bw+x;bool content=opaque[at]||(pixels[at]&0xffffff)!=(before[at]&0xffffff);unsigned alpha=content?255:base;if(!inside(x,y))alpha=0;pixels[at]=premultiplyPanelPixel(pixels[at],alpha);}
        const unsigned mapAlpha=unsigned(settings.mapOpacity)*255/100;
        if(showMap)
        for(int y=0;y<r.visualHeight;++y)for(int x=0;x<r.visualWidth;++x){
            const size_t src=size_t(y)*r.visualWidth+x;
            const int panelX=x+r.visualX,panelY=y+r.visualY;
            if(frame.depth[src]>=0||!inScene(x,y)||!inside(panelX,panelY))continue;
            auto& pixel=pixels[size_t(panelY)*bw+panelX];
            const auto background=premultiplyPanelPixel(pixel,255-mapAlpha)&0xffffff;
            const auto backgroundAlpha=((pixel>>24)*(255-mapAlpha)/255)<<24;
            pixel=premultiplyPanelPixel(frame.pixels[src],mapAlpha)+background+backgroundAlpha;
        }
        if(bannerAt&&now>=bannerAt&&now-bannerAt<2300){
            const float elapsed=float(now-bannerAt);
            float mix=std::min({1.0f,elapsed/280,(2300-elapsed)/480});
            mix=mix*mix*(3-2*mix);
            const size_t area=size_t(bw)*r.height;
            std::vector<uint32_t> underlying(pixels,pixels+area);
            std::fill_n(pixels,area,bannerColor);
            const unsigned luminance=(((bannerColor>>16)&255)*3+((bannerColor>>8)&255)*6+(bannerColor&255))/10;
            const uint32_t ink=luminance>145?0x131313:0xffffff;
            glyph(bannerRecord?9u:4u,(bw-32)/2,r.height/2-41,32,ink);
            const auto label=bannerRecord?L"Запись начата":L"Воспроизведение";
            const auto color=RGB((ink>>16)&255,(ink>>8)&255,ink&255);
            text(label,{12,r.height/2-2,bw-12,r.height/2+25},18,FW_SEMIBOLD,color,DT_CENTER|DT_VCENTER);
            if(!bannerRecord)text(bannerName.c_str(),{16,r.height/2+27,bw-16,r.height/2+51},13,FW_NORMAL,color,DT_CENTER|DT_VCENTER);
            GdiFlush();
            for(int y=0;y<r.height;++y)for(int x=0;x<bw;++x){
                const size_t at=size_t(y)*bw+x;
                uint32_t result=0;
                if(inside(x,y)){
                    const uint32_t over=pixels[at]|0xff000000u;
                    for(int shift:{0,8,16,24})
                        result|=uint32_t(((underlying[at]>>shift)&255)*(1-mix)+((over>>shift)&255)*mix+.5f)<<shift;
                }
                pixels[at]=result;
            }
        }
        POINT dest{origin.x+r.x,origin.y+r.y},source{0,0};SIZE size{bw,bh};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(UpdateLayeredWindow(window,nullptr,&dest,&size,memory,&source,0,&blend,ULW_ALPHA))SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);else hide();
    }
};
