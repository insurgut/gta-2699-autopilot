#pragma once
#include "route_sections.hpp"
#include "telemetry.hpp"
#include "route.hpp"
#include "route_catalog.hpp"
#include "radar.hpp"
#include "voxels.hpp"
#include "grid_frame.hpp"
#include "frame_limit.hpp"
#include "acrylic_hud.hpp"
#include "scene_panel.hpp"
#include "route_selection.hpp"
#include "selection_arrow.hpp"
#include "gps_ribbon_overlay.hpp"
#include "gps_navigation.hpp"
#include <chrono>
class Overlay {
    HWND hwnd=nullptr;HDC buffer=nullptr;HBITMAP bufferBitmap=nullptr;HGDIOBJ originalBitmap=nullptr;int bufferW=0,bufferH=0;
    bool picking=false;HWND gameWindow=nullptr;Telemetry frozen{};POINT gameOrigin{};
    HDC snapshot=nullptr;HBITMAP snapshotBitmap=nullptr;HGDIOBJ snapshotOld=nullptr;
    std::vector<ThreadSegment> threadLines;std::vector<Point> pickRoute;ThreadGesture gesture;
    void clearSnapshot(){if(snapshot){SelectObject(snapshot,snapshotOld);DeleteObject(snapshotBitmap);DeleteDC(snapshot);}snapshot=nullptr;snapshotBitmap=nullptr;}
    GpsRibbonOverlay gpsBand;std::vector<GpsPoint> gpsLine;float gpsWidth=1;
    ScenePanel scenePanel;AcrylicHud hud;FrameLimit40 frameClock;VoxelGrid visualGrid;
    const Point*markerRoute=nullptr;size_t markerSize=SIZE_MAX;uint64_t markerRevision=0;unsigned markerTail=0;std::vector<Point> enterMarkers;
    uint64_t fpsTick=0;unsigned drawn=0,drawFps=0;
    void clearBuffer(){if(buffer){SelectObject(buffer,originalBitmap);DeleteObject(bufferBitmap);DeleteDC(buffer);}buffer=nullptr;bufferBitmap=nullptr;bufferW=bufferH=0;}
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
        auto self=reinterpret_cast<Overlay*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams));return TRUE;}
        if(m==WM_NCHITTEST)return self&&self->picking?HTCLIENT:HTTRANSPARENT;
        if(self&&self->picking){
            float x=float(short(LOWORD(b))),y=float(short(HIWORD(b)));
            if(m==WM_SETCURSOR){SetCursor(LoadCursorW(nullptr,IDC_CROSS));return TRUE;}
            if(m==WM_ERASEBKGND)return 1;
            if(m==WM_LBUTTONDOWN){if(self->gesture.begin(self->threadLines,x,y))SetCapture(w);return 0;}
            if(m==WM_MOUSEMOVE){self->gesture.move(self->threadLines,x,y);return 0;}
            if(m==WM_LBUTTONUP){self->gesture.move(self->threadLines,x,y);self->gesture.end();ReleaseCapture();return 0;}
            if(m==WM_RBUTTONDOWN){self->cancelSelection();return 0;}
            if(m==WM_ACTIVATE&&LOWORD(a)==WA_INACTIVE){self->cancelSelection(false);return 0;}
            if(m==WM_CAPTURECHANGED&&self->gesture.dragging)self->gesture.reset();
        }
        return DefWindowProcW(w,m,a,b);
    }
    static BOOL CALLBACK find(HWND w,LPARAM arg){auto data=reinterpret_cast<std::pair<DWORD,HWND>*>(arg);DWORD p=0;GetWindowThreadProcessId(w,&p);if(p==data->first&&IsWindowVisible(w)&&!GetWindow(w,GW_OWNER)){data->second=w;return FALSE;}return TRUE;}
    static bool projectScreen(const Telemetry&s,Vec p,int width,int height,GpsScreenPoint&out){
        const float*m=s.matrix;float z=m[3]*p.x+m[7]*p.y+m[11]*p.z+m[15];
        if(!std::isfinite(z)||z<0.1f)return false;
        float x=(m[1]*p.x+m[5]*p.y+m[9]*p.z+m[13])/z;
        float y=(m[2]*p.x+m[6]*p.y+m[10]*p.z+m[14])/z;
        if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>10||std::abs(y)>10)return false;
        out={(x+1)*width/2,(1-y)*height/2};return true;
    }
    static bool project(const Telemetry&s,Vec p,int width,int height,POINT&out){
        GpsScreenPoint screen{};
        if(!projectScreen(s,p,width,height,screen))return false;
        out={LONG(screen.x),LONG(screen.y)};return true;
    }
public:
    void gpsPrediction(const std::vector<GpsPoint>&line,float width){gpsLine=line;gpsWidth=width;}
    void notifications(std::vector<NavigatorNotice> rows){scenePanel.notices=std::move(rows);}
    void navigatorActivity(bool recording,bool playing,const std::wstring& name,uint32_t color){scenePanel.activity(recording,playing,name,color);}
    const sceneview::Frame* navigatorPreview(const Telemetry& s,const std::vector<RadarBox>& boxes,
                                  const std::vector<Point>& route,uint32_t color,float speed){return scenePanel.preview(s,boxes,route,color,speed);}
    void configureScene(NavigatorSettings settings){scenePanel.configure(settings);}
    auto mapImage(ArchiveTile tile){return scenePanel.mapImage(tile);}
    void startMapCache(const std::filesystem::path& dir){scenePanel.startMapCache(dir);}
    MapTileProgress mapDownloadProgress(){return scenePanel.mapDownloadProgress();}
    void restartMapTile(ArchiveTile tile){scenePanel.restartMapTile(tile);}
    void prioritizeMapTile(ArchiveTile tile){scenePanel.prioritizeMapTile(tile);}
    void loadNavigatorMap(const std::filesystem::path& file,const std::filesystem::path& decoder){scenePanel.loadMap(file,decoder);}
    bool loadSceneModels(const std::filesystem::path&directory){return scenePanel.load(directory);}
    bool sceneModelsReady()const{return scenePanel.ready();}
    bool selecting()const{return picking;}
    bool selectionFocused()const{return picking&&GetForegroundWindow()==hwnd;}
    bool beginSelection(const std::vector<Point>&r){
        if(!hwnd||!gameWindow||!buffer||!frozen.cameraValid||r.size()<2)return false;
        clearSnapshot();auto screen=GetDC(nullptr);snapshot=CreateCompatibleDC(screen);snapshotBitmap=CreateCompatibleBitmap(screen,bufferW,bufferH);
        if(!snapshot||!snapshotBitmap){if(snapshotBitmap)DeleteObject(snapshotBitmap);if(snapshot)DeleteDC(snapshot);snapshot=nullptr;snapshotBitmap=nullptr;ReleaseDC(nullptr,screen);return false;}
        snapshotOld=SelectObject(snapshot,snapshotBitmap);bool ok=BitBlt(snapshot,0,0,bufferW,bufferH,screen,gameOrigin.x,gameOrigin.y,SRCCOPY|CAPTUREBLT)!=0;ReleaseDC(nullptr,screen);if(!ok){clearSnapshot();return false;}
        pickRoute=r;gesture.reset();picking=true;hud.hide();
        SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)&~(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE));
        ClipCursor(nullptr);SetForegroundWindow(hwnd);SetFocus(hwnd);if(GetForegroundWindow()!=hwnd){cancelSelection(false);return false;}return true;
    }
    void cancelSelection(bool restore=true){if(!picking)return;picking=false;ReleaseCapture();gesture.reset();pickRoute.clear();clearSnapshot();SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE);if(restore&&gameWindow)SetForegroundWindow(gameWindow);}
    bool takeSelection(RoutePick&pick,bool&reverse){if(!picking||!gesture.ready)return false;pick=gesture.start;reverse=gesture.reverse;cancelSelection();return true;}
    int backdropMode()const{return hud.mode();}
    void close(){cancelSelection(false);clearSnapshot();hud.close();gpsBand.close();scenePanel.close();clearBuffer();frameClock.reset();if(hwnd)DestroyWindow(hwnd);hwnd=nullptr;markerRoute=nullptr;markerSize=SIZE_MAX;enterMarkers.clear();}
    void hide(){if(picking)cancelSelection(false);hud.hide();scenePanel.hide();gpsBand.hide();frameClock.reset();if(hwnd)ShowWindow(hwnd,SW_HIDE);}
    void draw(DWORD pid,const Telemetry&live,const VoxelGrid&worldGrid,const std::vector<Point>&liveRoute,const std::vector<GridPoint>&planned,bool enabled,bool sceneEnabled,float speed,float targetSpeed,bool foreground,bool radarEnabled,const std::wstring&status,const std::unordered_map<uint32_t,unsigned>&rules,uint64_t revision,const std::vector<RadarBox>&worldBoxes,uint32_t routeColor,const std::wstring&navigatorStatus,const std::vector<SavedRoute>&savedRoutes,bool gpsMode=false,size_t displayedRoute=SIZE_MAX){
        const auto* displayed=displayedRoute<savedRoutes.size()?&savedRoutes[displayedRoute]:nullptr;
        if(!foreground&&!picking){hide();return;}
        const Telemetry&s=picking?frozen:live;const auto&route=picking?pickRoute:liveRoute;
        uint64_t ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(!frameClock.due(ns))return;
        uint64_t now=GetTickCount64();if(!fpsTick)fpsTick=now;
        if(now-fpsTick>=1000){drawFps=unsigned(drawn*1000/(now-fpsTick));drawn=0;fpsTick=now;}
        if(!hwnd){WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"GTA5RadarOverlay03";RegisterClassW(&c);
            hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TRANSPARENT|WS_EX_LAYERED|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,c.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,c.hInstance,this);
            if(!hwnd)return;
            SetLayeredWindowAttributes(hwnd,RGB(255,0,255),255,LWA_COLORKEY);
        }
        std::pair<DWORD,HWND> pair{pid,nullptr};EnumWindows(find,reinterpret_cast<LPARAM>(&pair));if(!pair.second){hide();return;}
        RECT bounds{};GetClientRect(pair.second,&bounds);POINT origin{0,0};ClientToScreen(pair.second,&origin);int w=bounds.right,h=bounds.bottom;gameWindow=pair.second;if(!picking){gameOrigin=origin;frozen=live;}else{origin=gameOrigin;w=bufferW;h=bufferH;}
        if(w<100||h<100){hide();return;}
        SetWindowPos(hwnd,HWND_TOPMOST,origin.x,origin.y,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        const auto nav=sceneEnabled&&!picking?scenePanel.layout(w,h):NavigatorGroup{};
        auto masked=[&](int x,int y){return navigatorPixel(nav,x,y);};
        if(!picking&&s.valid&&s.cameraValid&&!gpsLine.empty())gpsBand.draw(origin,w,h,gpsLine,gpsWidth,[&](GpsPoint p,GpsScreenPoint&out){return projectScreen(s,{p.x,p.y,p.z+GPS_GRID_Z},w,h,out);},masked);else gpsBand.hide();
        HDC target=GetDC(hwnd);if(!target)return;
        if(!buffer||bufferW!=w||bufferH!=h){clearBuffer();buffer=CreateCompatibleDC(target);bufferBitmap=CreateCompatibleBitmap(target,w,h);
            if(!buffer||!bufferBitmap){if(bufferBitmap)DeleteObject(bufferBitmap);if(buffer)DeleteDC(buffer);buffer=nullptr;bufferBitmap=nullptr;ReleaseDC(hwnd,target);return;}
            originalBitmap=SelectObject(buffer,bufferBitmap);bufferW=w;bufferH=h;
        }
        HDC dc=buffer;
        struct RouteClip {
            RGNDATAHEADER header{};
            std::array<RECT,1050> rows{};
        } shape;
        HRGN excluded=nullptr;
        if(nav.nav.width){
            auto& head=shape.header;
            head.dwSize=sizeof(head);head.iType=RDH_RECTANGLES;
            head.rcBound={nav.nav.x,nav.nav.y,nav.nav.x+nav.nav.width,nav.nav.y+nav.totalHeight};
            for(int y=head.rcBound.top;y<head.rcBound.bottom;++y){
                const int middle=nav.nav.x+nav.nav.width/2;
                if(!masked(middle,y))continue;
                int x=nav.nav.x;while(!masked(x,y))++x;
                const LONG right=nav.nav.x+nav.nav.width-(x-nav.nav.x);
                if(head.nCount&&shape.rows[head.nCount-1].left==x&&shape.rows[head.nCount-1].bottom==y)
                    ++shape.rows[head.nCount-1].bottom;
                else shape.rows[head.nCount++]={x,y,right,y+1};
            }
            head.nRgnSize=head.nCount*sizeof(RECT);
            if(head.nCount)excluded=ExtCreateRegion(nullptr,head.dwSize+head.nRgnSize,reinterpret_cast<RGNDATA*>(&shape));
        }
        auto clipRoute=[&]{
            if(!excluded)return 0;
            const int saved=SaveDC(dc);
            if(saved){IntersectClipRect(dc,0,0,w,h);ExtSelectClipRgn(dc,excluded,RGN_DIFF);}
            return saved;
        };
        RECT canvas{0,0,w,h};HBRUSH transparent=CreateSolidBrush(RGB(255,0,255));FillRect(dc,&canvas,transparent);DeleteObject(transparent);if(picking&&snapshot)BitBlt(dc,0,0,w,h,snapshot,0,0,SRCCOPY);SetBkMode(dc,TRANSPARENT);HFONT font=CreateFontW(-19,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(dc,font);
        if(s.valid&&enabled){
        bool world=s.cameraValid!=0;
        auto frame=displayFrame(route,GridFrame::make(s.position.x,s.position.y,s.position.z,s.forward.x,s.forward.y));
        auto localCar=frame.local(s.position.x,s.position.y);
        if(radarEnabled){
            std::vector<RadarBox> rotated;rotated.reserve(worldBoxes.size());for(const auto&b:worldBoxes)if(!passableRule(b.passability))rotated.push_back(frame.localBox(b));
            visualGrid.clear();visualGrid.rebuild(rotated.data(),rotated.size(),localCar.x,localCar.y,s.position.z,now,false);
            for(const auto&c:worldGrid.contact){auto p=frame.local((c.x+.5f)*CELL,(c.y+.5f)*CELL);visualGrid.contact.insert(cellAt(p.x,p.y,0));}
            const auto&grid=visualGrid;
            struct Edge {Cell from;int axis;bool operator==(const Edge&e)const{return from==e.from&&axis==e.axis;}};
            struct Hash {size_t operator()(const Edge&e)const{return CellHash{}(e.from)^(size_t(e.axis+1)*2654435761u);}};
            std::unordered_map<Edge,int,Hash> edges;
            auto center=cellAt(localCar.x,localCar.y,s.position.z);
            float plane=frame.z-0.7f;
            int radius=int(std::ceil(DRAW_RADIUS/CELL));
            int drawStep=std::max(1,int(std::ceil(float(radius)/40)));
            auto addCell=[&](Cell c,int state,int step){
                for(int axis=0;axis<2;axis++)for(int a=0;a<2;a++){
                    Cell v=c;if(axis==0)v.y+=a*step;else v.x+=a*step;
                    // Coarse empty lattice is drawn separately; coloured cells keep their real size.
                    auto it=edges.find({v,axis});if(it==edges.end())edges[{v,axis}]=state;
                    else {auto rank=[](int s){return s==6?6:s==3?5:s==2?4:s==5?3:s==4?2:s;};if(rank(state)>rank(it->second))it->second=state;}
                }
            };
            HPEN basePen=CreatePen(PS_SOLID,1,RGB(73,73,73));auto baseOld=SelectObject(dc,basePen);
            auto drawBase=[&](Vec p,Vec q){auto wp=frame.world(p.x,p.y),wq=frame.world(q.x,q.y);p.x=wp.x;p.y=wp.y;q.x=wq.x;q.y=wq.y;POINT a{},b{};bool good=false;if(world)good=project(s,p,w,h,a)&&project(s,q,w,h,b);else{a={LONG(w/2+(p.x-s.position.x)*7),LONG(h-180-(p.y-s.position.y)*7)};b={LONG(w/2+(q.x-s.position.x)*7),LONG(h-180-(q.y-s.position.y)*7)};good=true;}if(good){MoveToEx(dc,a.x,a.y,nullptr);LineTo(dc,b.x,b.y);}};
            // Cell coordinates are in the car/start frame; all geometry is transformed back to world space.
            for(int x=int(std::floor(float(center.x-radius)/drawStep))*drawStep;x<=center.x+radius;x+=drawStep){float wx=x*CELL;float dx=wx-localCar.x;if(std::abs(dx)>DRAW_RADIUS)continue;float span=std::sqrt(std::max(0.0f,DRAW_RADIUS*DRAW_RADIUS-dx*dx));drawBase({wx,localCar.y-span,plane},{wx,localCar.y+span,plane});}
            for(int y=int(std::floor(float(center.y-radius)/drawStep))*drawStep;y<=center.y+radius;y+=drawStep){float wy=y*CELL;float dy=wy-localCar.y;if(std::abs(dy)>DRAW_RADIUS)continue;float span=std::sqrt(std::max(0.0f,DRAW_RADIUS*DRAW_RADIUS-dy*dy));drawBase({localCar.x-span,wy,plane},{localCar.x+span,wy,plane});}
            SelectObject(dc,baseOld);DeleteObject(basePen);
            std::vector<Cell> coloured;auto gather=[&](const auto&set){for(const auto&c:set){float dx=(c.x+0.5f)*CELL-localCar.x,dy=(c.y+0.5f)*CELL-localCar.y;if(dx*dx+dy*dy<=DRAW_RADIUS*DRAW_RADIUS)coloured.push_back(c);}};
            gather(grid.occupied);gather(grid.contact);gather(grid.passable);
            std::sort(coloured.begin(),coloured.end(),[&](Cell a,Cell b){auto d=[&](Cell c){return std::hypot(float(c.x-center.x),float(c.y-center.y));};return d(a)<d(b);});
            if(coloured.size()>20000)coloured.resize(20000);
            for(const auto&c:coloured)addCell(c,s.radarValid?grid.columnState(c.x,c.y,s.position.z):0,1);
            HPEN pens[]={CreatePen(PS_SOLID,1,RGB(100,100,100)),CreatePen(PS_SOLID,1,RGB(82,82,82)),CreatePen(PS_SOLID,2,RGB(170,170,170)),CreatePen(PS_SOLID,2,RGB(138,138,138)),CreatePen(PS_SOLID,2,RGB(25,25,25)),CreatePen(PS_SOLID,3,RGB(29,29,29)),CreatePen(PS_SOLID,2,RGB(157,157,157))};
            auto oldPen=SelectObject(dc,pens[0]);
            for(const auto&e:edges){Vec p{e.first.from.x*CELL,e.first.from.y*CELL,plane},q=p;if(e.first.axis==0)q.x+=CELL;else q.y+=CELL;
                auto wp=frame.world(p.x,p.y),wq=frame.world(q.x,q.y);p.x=wp.x;p.y=wp.y;q.x=wq.x;q.y=wq.y;
                POINT a{},b{};bool good=false;
                if(world)good=project(s,p,w,h,a)&&project(s,q,w,h,b);
                else { // Explicitly labelled top-down fallback; not a fake world-space projection.
                    auto top=[&](Vec v){return POINT{LONG(w/2+(v.x-s.position.x)*7),LONG(h-180-(v.y-s.position.y)*7)};};a=top(p);b=top(q);good=true;
                }
                if(good){SelectObject(dc,pens[e.second]);MoveToEx(dc,a.x,a.y,nullptr);LineTo(dc,b.x,b.y);
                    if(e.second==5){SelectObject(dc,GetStockObject(DC_PEN));SetDCPenColor(dc,RGB(123,123,123));POINT mid{(a.x+b.x)/2,(a.y+b.y)/2};MoveToEx(dc,a.x,a.y,nullptr);LineTo(dc,mid.x,mid.y);}}

            }
            SelectObject(dc,oldPen);for(auto pen:pens)DeleteObject(pen);
        }
        // An independent, oriented footprint lattice moves and turns with the car.
        auto projectPoint=[&](Vec v,POINT&p){if(world)return project(s,v,w,h,p);p={LONG(w/2+(v.x-s.position.x)*7),LONG(h-180-(v.y-s.position.y)*7)};return true;};
        auto line=[&](Vec a,Vec b){POINT p{},q{};if(projectPoint(a,p)&&projectPoint(b,q)){MoveToEx(dc,p.x,p.y,nullptr);LineTo(dc,q.x,q.y);}};
        const int threadClip=clipRoute();
        if(!picking&&!gpsMode)for(const auto&saved:savedRoutes){if(saved.points.empty()||&saved==displayed)continue;
            auto c=saved.color;auto pen=CreatePen(PS_SOLID,2,RGB((c>>16)&255,(c>>8)&255,c&255));auto old=SelectObject(dc,pen);
            for(size_t i=0;i+1<saved.points.size();i++){float a=0,b=1;const auto&p=saved.points[i];const auto&q=saved.points[i+1];if(!clipThreadRadius(p,q,{s.position.x,s.position.y,s.position.z,0},70,a,b))continue;line({p.x+(q.x-p.x)*a,p.y+(q.y-p.y)*a,p.z+(q.z-p.z)*a-.65f},{p.x+(q.x-p.x)*b,p.y+(q.y-p.y)*b,p.z+(q.z-p.z)*b-.65f});}
            SelectObject(dc,old);DeleteObject(pen);
        }
        threadLines.clear();float threadArc=0;
        auto threadPen=CreatePen(PS_SOLID,3,RGB((routeColor>>16)&255,(routeColor>>8)&255,routeColor&255));auto threadOld=SelectObject(dc,threadPen);
        for(size_t i=0;!gpsMode&&i+1<route.size();i++){
            const auto&originalA=route[i];const auto&originalB=route[i+1];float length=distance(originalA,originalB),from=0,to=1;
            if(!clipThreadRadius(originalA,originalB,{s.position.x,s.position.y,s.position.z,0},70,from,to)){threadArc+=length;continue;}
            auto color=routeSectionColor(routeColor,originalB);auto edgePen=CreatePen(PS_SOLID,3,RGB((color>>16)&255,(color>>8)&255,color&255));SelectObject(dc,edgePen);
            auto at=[&](float t){return Point{originalA.x+(originalB.x-originalA.x)*t,originalA.y+(originalB.y-originalA.y)*t,originalA.z+(originalB.z-originalA.z)*t,0};};
            auto a=at(from),b=at(to);POINT p{},q{};
            if(projectPoint({a.x,a.y,a.z-.65f},p)&&projectPoint({b.x,b.y,b.z-.65f},q)){
                if((p.x>=0||q.x>=0)&&(p.y>=0||q.y>=0)&&(p.x<w||q.x<w)&&(p.y<h||q.y<h)){
                    MoveToEx(dc,p.x,p.y,nullptr);LineTo(dc,q.x,q.y);threadLines.push_back({i,float(p.x),float(p.y),float(q.x),float(q.y),threadArc+length*from,length*(to-from),world?s.matrix[3]*a.x+s.matrix[7]*a.y+s.matrix[11]*(a.z-.65f)+s.matrix[15]:1,world?s.matrix[3]*b.x+s.matrix[7]*b.y+s.matrix[11]*(b.z-.65f)+s.matrix[15]:1,from,to});
                }
            }SelectObject(dc,threadPen);DeleteObject(edgePen);threadArc+=length;
        }
        if(picking){
            auto selected=gesture.dragging?gesture.start:gesture.hover;
            if(selected.valid){auto brush=CreateSolidBrush(RGB((routeColor>>16)&255,(routeColor>>8)&255,routeColor&255));auto oldBrush=SelectObject(dc,brush);Ellipse(dc,int(selected.x)-6,int(selected.y)-6,int(selected.x)+7,int(selected.y)+7);SelectObject(dc,oldBrush);DeleteObject(brush);}
            if(gesture.dragging&&std::abs(gesture.last.arc-gesture.start.arc)>.001f){
                auto color=selectionArrowColor(routeColor);LOGBRUSH brush{BS_SOLID,RGB((color>>16)&255,(color>>8)&255,color&255),0};
                auto pen=ExtCreatePen(PS_GEOMETRIC|PS_SOLID|PS_ENDCAP_ROUND|PS_JOIN_ROUND,8,&brush,0,nullptr);auto oldArrow=SelectObject(dc,pen);
                for(const auto&chain:selectionArrowChains(threadLines,gesture.start.arc,gesture.last.arc)){
                    auto path=roundedArrowPath(chain);if(path.empty())continue;
                    BeginPath(dc);MoveToEx(dc,LONG(path.front().end.x),LONG(path.front().end.y),nullptr);
                    for(size_t i=1;i<path.size();i++){const auto&v=path[i];if(v.curve){POINT c[3]={{LONG(v.c1.x),LONG(v.c1.y)},{LONG(v.c2.x),LONG(v.c2.y)},{LONG(v.end.x),LONG(v.end.y)}};PolyBezierTo(dc,c,3);}else LineTo(dc,LONG(v.end.x),LONG(v.end.y));}EndPath(dc);StrokePath(dc);
                }
                const auto&tip=gesture.last;for(const auto&l:threadLines)if(l.index==tip.segment){float dx=(l.x2-l.x1)*(gesture.reverse?-1:1),dy=(l.y2-l.y1)*(gesture.reverse?-1:1),n=std::hypot(dx,dy);if(n>.1f){dx/=n;dy/=n;POINT head[3]={{LONG(tip.x-dx*20-dy*10),LONG(tip.y-dy*20+dx*10)},{LONG(tip.x),LONG(tip.y)},{LONG(tip.x-dx*20+dy*10),LONG(tip.y-dy*20-dx*10)}};Polyline(dc,head,3);}break;}
                SelectObject(dc,oldArrow);DeleteObject(pen);
            }
        }
        SelectObject(dc,threadOld);DeleteObject(threadPen);
        if(threadClip)RestoreDC(dc,threadClip);
        HPEN ownPen=CreatePen(PS_SOLID,2,RGB(38,38,38));auto previousPen=SelectObject(dc,ownPen);
        float hw=s.ownHalfWidth>0?s.ownHalfWidth:1.05f,hl=s.ownHalfLength>0?s.ownHalfLength:2.5f;
        const float headingLength=std::max(.01f,std::hypot(s.forward.x,s.forward.y));
        const float ownFx=s.forward.x/headingLength,ownFy=s.forward.y/headingLength;
        auto own=[&](float x,float y){return Vec{s.position.x+ownFy*x+ownFx*y,s.position.y-ownFx*x+ownFy*y,s.position.z+GPS_GRID_Z};};
        for(int i=0;i<=int(std::ceil(2*hw/CELL));i++){float x=std::min(hw,-hw+i*CELL);line(own(x,-hl),own(x,hl));}
        for(int i=0;i<=int(std::ceil(2*hl/CELL));i++){float y=std::min(hl,-hl+i*CELL);line(own(-hw,y),own(hw,y));}
        SelectObject(dc,previousPen);DeleteObject(ownPen);
        if(!gpsMode&&!planned.empty()){const int clipped=clipRoute();HPEN pen=CreatePen(PS_SOLID,3,RGB(178,178,178));auto old=SelectObject(dc,pen);Vec from{s.position.x,s.position.y,s.position.z-0.65f};
            for(const auto&p:planned){Vec to{p.x,p.y,s.position.z-0.65f};line(from,to);from=to;}SelectObject(dc,old);DeleteObject(pen);if(clipped)RestoreDC(dc,clipped);}
        SetTextColor(dc,RGB(173,173,173));
        for(unsigned i=0;i<std::min(MAX_OBSTACLES,s.obstacleCount);i++){
            const auto&o=s.obstacles[i];auto rule=rules.find(o.modelHash);
            if(rule==rules.end()||rule->second!=WAIT_OBJECT||std::hypot(o.position.x-s.position.x,o.position.y-s.position.y)>DRAW_RADIUS)continue;
            POINT label{};if(projectPoint({o.position.x,o.position.y,o.position.z+o.halfHeight+0.2f},label))TextOutW(dc,label.x+4,label.y-18,L"Ожидание",8);
        }
        if(markerRevision!=revision||markerRoute!=route.data()||markerSize!=route.size()||markerTail!=(route.empty()?0:route.back().enterCount)){
            markerRevision=revision;markerRoute=route.data();markerSize=route.size();markerTail=route.empty()?0:route.back().enterCount;enterMarkers.clear();for(const auto&p:route)if(p.enterCount)enterMarkers.push_back(p);
        }
        if(world&&!enterMarkers.empty()){
            unsigned shown=0;HPEN outline=CreatePen(PS_SOLID,5,RGB(18,18,18)),pole=CreatePen(PS_SOLID,3,RGB(240,240,240));auto old=SelectObject(dc,outline);
            for(const auto&p:enterMarkers){
                if(std::hypot(p.x-s.position.x,p.y-s.position.y)>70||shown>=512)continue;
                POINT bottom{},top{};if(!project(s,{p.x,p.y,p.z-.65f},w,h,bottom)||!project(s,{p.x,p.y,p.z+1.8f},w,h,top))continue;++shown;
                SelectObject(dc,outline);MoveToEx(dc,bottom.x,bottom.y,nullptr);LineTo(dc,top.x,top.y);SelectObject(dc,pole);MoveToEx(dc,bottom.x,bottom.y,nullptr);LineTo(dc,top.x,top.y);
            }SelectObject(dc,old);DeleteObject(outline);DeleteObject(pole);
        }
        }
        const int startClip=clipRoute();
        // Start lattices are an independent layer, including when radar/route/HUD are off.
        if(s.valid&&s.cameraValid){
            auto startGrid=[&](const std::vector<Point>&points,uint32_t color){if(points.empty())return;auto start=startFrame(points);const auto&p=points.front();float hw=p.startHalfWidth>0?p.startHalfWidth:1.05f,hl=p.startHalfLength>0?p.startHalfLength:2.5f;
                auto line=[&](float x1,float y1,float x2,float y2){auto a=start.world(x1,y1),b=start.world(x2,y2);POINT pa{},pb{};if(project(s,{a.x,a.y,p.z-.68f},w,h,pa)&&project(s,{b.x,b.y,p.z-.68f},w,h,pb)){MoveToEx(dc,pa.x,pa.y,nullptr);LineTo(dc,pb.x,pb.y);}};
                auto pen=CreatePen(PS_SOLID,2,RGB((color>>16)&255,(color>>8)&255,color&255));auto old=SelectObject(dc,pen);
                for(int i=0;i<=int(std::ceil(2*hw/CELL));i++){float x=std::min(hw,-hw+i*CELL);line(x,-hl,x,hl);}
                for(int i=0;i<=int(std::ceil(2*hl/CELL));i++){float y=std::min(hl,-hl+i*CELL);line(-hw,y,hw,y);}
                SelectObject(dc,old);DeleteObject(pen);
            };
            if(!gpsMode)startGrid(route,routeColor);
            for(const auto&saved:savedRoutes)if(&saved!=displayed||route.empty())startGrid(saved.points,saved.color);
        }
        if(startClip)RestoreDC(dc,startClip);
        if(excluded)DeleteObject(excluded);
        if(sceneEnabled&&!picking)scenePanel.draw(origin,w,h,s,worldBoxes,route,speed,targetSpeed,navigatorStatus,routeColor);else scenePanel.hide();
        if(BitBlt(target,0,0,w,h,dc,0,0,SRCCOPY))++drawn;
        SelectObject(dc,oldFont);DeleteObject(font);ReleaseDC(hwnd,target);
        if(!picking){if(sceneEnabled)hud.hide();else hud.show(origin,w,status);}
    }
};
