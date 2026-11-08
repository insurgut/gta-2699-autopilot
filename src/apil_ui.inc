enum Page { Sidebar=-1, Home, Routes, Diagnostics, Settings };
struct ViewControl {HWND handle;Page page;};
std::vector<ViewControl> views;
Page apilPage=Home;
HWND updateLabel=nullptr,versionLabel=nullptr,routeList=nullptr,sectorList=nullptr,objectList=nullptr;
HWND routeFold=nullptr,autoConnectBox=nullptr,gpsBox=nullptr,mapBox=nullptr,hornBox=nullptr,gpsPreviewBox=nullptr;
HFONT titleFont=nullptr,smallFont=nullptr;
HBITMAP fallbackMap=nullptr;
bool autoConnectEnabled=true,activeForm=false,foldedMap=false;
std::vector<std::pair<size_t,size_t>> sectors;
std::vector<TripPoint> tripView;
uint64_t tripCursor=0,visibleRevision=0;
size_t visibleWaits=SIZE_MAX,visibleRoutes=SIZE_MAX,visibleSelection=SIZE_MAX;
RECT mapRect{};
struct MapView {float x=360,y=1650,scale=0;};
MapView routeMap,tripMap;
POINT mapDown{};
MapView beforePan;
bool panning=false,movedMap=false;

bool apilConnected(){
    const auto now=GetTickCount64();
    return shared&&sample.pid==pid&&sample.sampleBeat&&now>=sample.sampleBeat&&now-sample.sampleBeat<1500;
}
std::wstring widen(const std::string& value){
    if(value.empty())return {};
    const int n=MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),nullptr,0);
    std::wstring out(size_t(std::max(n,0)),0);
    if(n>0)MultiByteToWideChar(CP_UTF8,0,value.data(),int(value.size()),out.data(),n);
    return out;
}
std::wstring number(double value){
    wchar_t text[32]{};swprintf(text,32,L"%.3g",value);return text;
}
double numeric(std::wstring raw){
    std::replace(raw.begin(),raw.end(),L',',L'.');
    wchar_t* end=nullptr;const auto n=wcstod(raw.c_str(),&end);
    return end!=raw.c_str()&&!*end&&std::isfinite(n)?n:NAN;
}
HWND makeControl(HWND owner,const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0){
    const bool button=wcscmp(type,L"BUTTON")==0,check=button&&(style&BS_TYPEMASK)==BS_AUTOCHECKBOX;
    if(button)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW|WS_TABSTOP;
    HWND child=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,owner,
        reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    if(button)ui::attach(child,check);
    return child;
}
HWND field(Page page,const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0){
    HWND child=makeControl(window,type,label,style,x,y,w,h,id);views.push_back({child,page});return child;
}
HWND button(Page page,const wchar_t* label,int x,int y,int w,int id,bool check=false){
    return field(page,L"BUTTON",label,check?BS_AUTOCHECKBOX:BS_PUSHBUTTON,x,y,w,33,id);
}
void textIfChanged(HWND child,const std::wstring& text){if(controlText(child)!=text)SetWindowTextW(child,text.c_str());}
void setCheck(HWND child,bool value){SendMessageW(child,BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);}
bool getCheck(HWND child){return SendMessageW(child,BM_GETCHECK,0,0)==BST_CHECKED;}
void place(int id,int x,int y,int w,int h){
    if(HWND child=GetDlgItem(window,id))MoveWindow(child,x,y,std::max(1,w),std::max(1,h),TRUE);
}
void layoutApil(){
    RECT rc{};GetClientRect(window,&rc);
    const int width=int(rc.right),height=int(rc.bottom),x=218,w=std::max(200,width-x-24);
    if(apilPage==Routes){
        const int mapHeight=foldedMap?110:std::clamp((height-280)*2/3,180,370);
        mapRect={x,146,x+w,146+mapHeight};
        const int lower=mapRect.bottom+14,half=(w-12)/2;
        place(403,x,lower,half,28);place(404,x+half+12,lower,half,28);
        const int listHeight=std::max(70,height-lower-87);
        place(402,x,lower+29,half,listHeight);place(405,x+half+12,lower+29,half,listHeight);
        place(406,x,lower+32+listHeight,half,33);place(407,x+half+12,lower+32+listHeight,half,33);
        place(401,x,65,w-126,300);place(408,x+w-116,65,116,33);
    }else if(apilPage==Diagnostics)mapRect={x,118,x+w,std::max(170,height-22)};
    else mapRect={};
    place(501,x,175,w,50);place(502,x,232,w,58);
    InvalidateRect(window,nullptr,FALSE);
}
void showPage(Page page){
    if((page==Routes||page==Diagnostics)&&!apilConnected())return;
    apilPage=page;
    for(const auto& view:views)ShowWindow(view.handle,view.page==Sidebar||view.page==page?SW_SHOW:SW_HIDE);
    layoutApil();
}
void fitRoute(){
    if(route.empty()){routeMap={};return;}
    float left=route.front().x,right=left,top=route.front().y,bottom=top;
    for(const auto& point:route){
        left=std::min(left,point.x);right=std::max(right,point.x);
        bottom=std::min(bottom,point.y);top=std::max(top,point.y);
    }
    routeMap={(left+right)/2,(top+bottom)/2,std::clamp(std::min(
        float(mapRect.right-mapRect.left-56)/std::max(40.0f,right-left),
        float(mapRect.bottom-mapRect.top-32)/std::max(40.0f,top-bottom)),.022f,10.0f)};
}
void refreshRoutes(){
    if(visibleRevision==routeVisualRevision&&visibleWaits==waitingMarkers.entries.size()&&
       visibleRoutes==routeCatalog.entries.size()&&visibleSelection==selectedRoute)return;
    const bool changedSelection=visibleSelection!=selectedRoute;
    const auto prev=SendMessageW(objectList,LB_GETCURSEL,0,0);
    visibleRevision=routeVisualRevision;visibleWaits=waitingMarkers.entries.size();
    visibleRoutes=routeCatalog.entries.size();visibleSelection=selectedRoute;
    SendMessageW(routeList,CB_RESETCONTENT,0,0);
    for(const auto& saved:routeCatalog.entries){
        const auto name=widen(saved.name);
        SendMessageW(routeList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));
    }
    if(selectedRoute<routeCatalog.entries.size())SendMessageW(routeList,CB_SETCURSEL,selectedRoute,0);
    SendMessageW(sectorList,LB_RESETCONTENT,0,0);sectors.clear();
    for(size_t from=0;from+1<route.size();){
        size_t to=from;
        while(to+2<route.size()&&route[to+2].speedLimit==route[from+1].speedLimit&&
              route[to+2].ignoreObstacles==route[from+1].ignoreObstacles)++to;
        sectors.push_back({from,to});
        const auto& p=route[from+1];
        const auto row=std::to_wstring(from+1)+L"–"+std::to_wstring(to+1)+L"   "+
            (p.speedLimit>0?number(p.speedLimit*3.6f)+L" км/ч":L"Скорость маршрута")+
            (p.ignoreObstacles?L" · без препятствий":L"");
        SendMessageW(sectorList,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row.c_str()));from=to+1;
    }
    SendMessageW(objectList,LB_RESETCONTENT,0,0);
    for(size_t i=0;i<waitingMarkers.entries.size();++i){
        const auto& m=waitingMarkers.entries[i];
        wchar_t row[150]{};
        swprintf(row,150,L"%zu · %08X · %.0f, %.0f · %.1f с",i+1,unsigned(m.model),m.pose.x,m.pose.y,m.delayMs/1000.0);
        SendMessageW(objectList,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(row));
    }
    if(!changedSelection&&prev>=0&&size_t(prev)<waitingMarkers.entries.size())SendMessageW(objectList,LB_SETCURSEL,prev,0);
    if(changedSelection)fitRoute();
}
void updateApilUi(){
    if(!window||!updateLabel||IsIconic(window))return;
    const bool connected=apilConnected();
    if(!connected&&(apilPage==Routes||apilPage==Diagnostics))showPage(Home);
    for(int id:{301,302,101,102,103,104}){
        HWND child=GetDlgItem(window,id);
        if(IsWindowEnabled(child)!=connected)EnableWindow(child,connected);
    }
    const bool allowConnect=!connected&&!connecting;
    if(IsWindowEnabled(connectButton)!=allowConnect)EnableWindow(connectButton,allowConnect);
    textIfChanged(connectButton,connected?L"Игра подключена":connecting?L"Подключение…":L"Подключить игру");
    textIfChanged(updateLabel,updater.text());
    textIfChanged(sceneLabel,apilPage==Home?gpsStatus:L"");
    if(apilPage==Routes)refreshRoutes();
    if(apilPage==Diagnostics&&tripView.size()<1000000){
        auto chunk=tripHistory.read(tripCursor,4096);tripCursor+=chunk.size();
        tripView.insert(tripView.end(),chunk.begin(),chunk.end());
    }
    if(apilPage==Routes||apilPage==Diagnostics)InvalidateRect(window,&mapRect,FALSE);
    if(apilPage==Home&&connected){RECT rc{};GetClientRect(window,&rc);rc.left=218;rc.top=328;InvalidateRect(window,&rc,FALSE);}
}
struct Form {
    std::wstring title;
    std::vector<std::wstring> labels,values;
    std::vector<HWND> edits;
    std::function<bool(const std::vector<std::wstring>&)> save;
};
INT_PTR CALLBACK formProc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_INITDIALOG){
        auto* form=reinterpret_cast<Form*>(lp);
        SetWindowLongPtrW(w,DWLP_USER,reinterpret_cast<LONG_PTR>(form));
        SetWindowTextW(w,form->title.c_str());
        for(size_t i=0;i<form->labels.size();++i){
            const int y=16+int(i)*37;
            makeControl(w,L"STATIC",form->labels[i].c_str(),SS_CENTERIMAGE,18,y,235,29);
            form->edits.push_back(makeControl(w,L"EDIT",form->values[i].c_str(),WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,260,y,148,28));
        }
        const int y=23+int(form->labels.size())*37;
        makeControl(w,L"BUTTON",L"Сохранить",0,173,y,112,33,IDOK);
        makeControl(w,L"BUTTON",L"Отмена",0,296,y,112,33,IDCANCEL);
        RECT rc{0,0,426,y+49};AdjustWindowRectEx(&rc,DWORD(GetWindowLongPtrW(w,GWL_STYLE)),FALSE,DWORD(GetWindowLongPtrW(w,GWL_EXSTYLE)));
        SetWindowPos(w,nullptr,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_NOMOVE|SWP_NOZORDER);
        return TRUE;
    }
    if(msg==WM_DRAWITEM)return ui::drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp),font);
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLOREDIT||msg==WM_CTLCOLORDLG){
        auto dc=reinterpret_cast<HDC>(wp);SetBkColor(dc,RGB(29,29,29));SetTextColor(dc,RGB(235,235,235));
        return reinterpret_cast<INT_PTR>(ui::panel());
    }
    if(msg==WM_CLOSE|| (msg==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(w,IDCANCEL);return TRUE;}
    if(msg==WM_COMMAND&&LOWORD(wp)==IDOK){
        auto* form=reinterpret_cast<Form*>(GetWindowLongPtrW(w,DWLP_USER));
        std::vector<std::wstring> values;
        for(auto edit:form->edits)values.push_back(controlText(edit));
        if(form->save(values))EndDialog(w,IDOK);
        else MessageBoxW(w,L"Проверь значения или доступ к файлу настроек.",L"APIL",MB_OK|MB_ICONWARNING);
        return TRUE;
    }
    return FALSE;
}
void editForm(std::wstring title,std::vector<std::wstring> labels,std::vector<std::wstring> values,
              std::function<bool(const std::vector<std::wstring>&)> save){
    Form form{std::move(title),std::move(labels),std::move(values),{},std::move(save)};
    struct alignas(DWORD) Template {DLGTEMPLATE dialog;WORD menu,klass,title;} t{};
    t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER;
    t.dialog.cx=285;t.dialog.cy=short(45+20*form.labels.size());
    activeForm=true;
    DialogBoxIndirectParamW(GetModuleHandleW(nullptr),&t.dialog,window,formProc,reinterpret_cast<LPARAM>(&form));
    activeForm=false;
    updateApilUi();
}
std::wstring routeId(){
    if(selectedRoute>=routeCatalog.entries.size())return {};
    const auto& id=routeCatalog.entries[selectedRoute].id;
    return std::wstring(id.begin(),id.end());
}
void editWaitObject(){
    const auto index=SendMessageW(objectList,LB_GETCURSEL,0,0);
    if(mode!=Mode::Idle||index<0||size_t(index)>=waitingMarkers.entries.size())return;
    const auto m=waitingMarkers.entries[size_t(index)];
    const auto& p=m.approach;
    editForm(L"Объект ожидания "+std::to_wstring(index+1),
        {L"Задержка после движения, с",L"Зазор при остановке, м",L"Граница ближнего сектора, м",L"Граница среднего сектора, м",
         L"Ближний сектор, км/ч",L"Средний сектор, км/ч",L"Дальний сектор, км/ч"},
        {number(m.delayMs/1000.0),number(p.stop),number(p.innerBoundary),number(p.outer),number(p.kmh[0]),number(p.kmh[1]),number(p.kmh[2])},
        [=](const auto& v){
            auto edited=waitingMarkers;
            WaitApproach approach{float(numeric(v[1])),float(numeric(v[2])),float(numeric(v[3])),
                {float(numeric(v[4])),float(numeric(v[5])),float(numeric(v[6]))}};
            auto file=routeFile;file+=L".waiting";
            if(!edited.setDelay(size_t(index),numeric(v[0]),m.model)||!edited.setApproach(size_t(index),approach)||!edited.save(file))return false;
            waitingMarkers=std::move(edited);++routeVisualRevision;message=L"Настройки объекта сохранены.";return true;
        });
}
void editSector(){
    if(mode!=Mode::Idle||route.size()<2||routeId().empty())return;
    const auto row=SendMessageW(sectorList,LB_GETCURSEL,0,0);
    const auto range=row>=0&&size_t(row)<sectors.size()?sectors[size_t(row)]:std::make_pair(size_t(0),size_t(0));
    const auto& p=route[range.first+1];
    editForm(L"Настройка секторов маршрута",{L"Первый сектор",L"Последний сектор",L"Скорость, км/ч (0 = по маршруту)",L"Игнорировать препятствия (0/1)"},
        {std::to_wstring(range.first+1),std::to_wstring(range.second+1),number(p.speedLimit*3.6f),p.ignoreObstacles?L"1":L"0"},
        [](const auto& v){
            const auto from=numeric(v[0]),to=numeric(v[1]),kmh=numeric(v[2]),ignore=numeric(v[3]);
            if(!(from>=1&&to>=from&&to<double(route.size())&&std::floor(from)==from&&std::floor(to)==to&&
                kmh>=0&&kmh<=3600&&(ignore==0||ignore==1)))return false;
            const auto cmd=L"section|"+routeId()+L"|"+std::to_wstring(size_t(from)-1)+L"|"+std::to_wstring(size_t(to)-1)+
                L"|"+std::to_wstring(kmh)+L"|"+std::to_wstring(int(ignore))+L"|"+std::to_wstring(route.size());
            uiCommand(cmd);return message==L"Параметры участка сохранены.";
        });
}
void editRouteInfo(){
    const auto id=routeId();
    if(id.empty()||mode!=Mode::Idle)return;
    const auto& saved=routeCatalog.entries[selectedRoute];
    const auto color=routeHex(saved.color);
    editForm(L"Настройки маршрута",{L"Имя",L"Цвет #RRGGBB",L"Учитывать скорость записи (0/1)"},
        {widen(saved.name),std::wstring(color.begin(),color.end()),saved.input.pace==RecordedPace::Enabled?L"1":L"0"},
        [=](const auto& v){
            uint32_t rgb=0;
            if(v[0].empty()||v[0].find(L'|')!=std::wstring::npos||!parseRouteColor(v[1],rgb)||(v[2]!=L"0"&&v[2]!=L"1"))return false;
            if(!routeCatalog.rename(selectedRoute,utf8(v[0]))||!routeCatalog.setColor(selectedRoute,rgb))return false;
            if(routeCatalog.entries[selectedRoute].input.pace!=RecordedPace::Unavailable)
                uiCommand(L"route|pace|"+id+L"|"+v[2]);
            ++routeVisualRevision;return true;
        });
}
void editNumbers(){
    editForm(L"Параметры управления и панели",
        {L"Скорость маршрута, км/ч",L"Скорость GPS, км/ч",L"Шаг колеса GPS, км/ч",L"Размер ячейки, м",L"Радиус сетки, м",
         L"Ширина навигатора",L"Высота навигатора",L"Непрозрачность навигатора, %",L"Непрозрачность карты, %"},
        {controlText(speedBox),number(gpsSpeed),number(gpsWheelStep),number(CELL),number(DRAW_RADIUS),std::to_wstring(navigatorSettings.width),
         std::to_wstring(navigatorSettings.height),std::to_wstring(navigatorSettings.opacity),std::to_wstring(navigatorSettings.mapOpacity)},
        [](const auto& v){
            const std::array<double,9> lo{1,5,1,.1,1,240,160,0,0},hi{3600,300,30,5,100,900,800,100,100};
            const wchar_t* keys[]={L"target",L"gpsSpeed",L"gpsWheelStep",L"cell",L"radius",L"panelWidth",L"panelHeight",L"panelOpacity",L"panelMapOpacity"};
            for(size_t i=0;i<v.size();++i){const auto n=numeric(v[i]);if(!(n>=lo[i]&&n<=hi[i])||(i>=5&&n!=std::floor(n)))return false;}
            for(size_t i=0;i<v.size();++i){
                auto raw=v[i];std::replace(raw.begin(),raw.end(),L',',L'.');
                uiCommand(std::wstring(i==1||i==2?L"gpsvalue|":L"value|")+keys[i]+L"|"+raw);
            }
            return true;
        });
}
void editModel(){
    editForm(L"Правило для модели объекта",{L"Хеш модели (в десятичном виде)",L"0 обычное / 1 проход / 2 ломаемое / 4 ожидание"},{L"",L"4"},
        [](const auto& v){
            if(v[0].empty()||v[0].find_first_not_of(L"0123456789")!=std::wstring::npos)return false;
            const double model=numeric(v[0]);if(!(model>=1&&model<=UINT32_MAX)||(v[1]!=L"0"&&v[1]!=L"1"&&v[1]!=L"2"&&v[1]!=L"4"))return false;
            uiCommand(L"rule|"+v[0]+L"|"+v[1]);return true;
        });
}
void drawBitmap(HDC dc,const uint32_t* pixels,int w,int h,int x,int y,int width,int height){
    BITMAPINFO info{};auto& b=info.bmiHeader;
    b.biSize=sizeof(b);b.biWidth=w;b.biHeight=-h;b.biPlanes=1;b.biBitCount=32;b.biCompression=BI_RGB;
    StretchDIBits(dc,x,y,width,height,0,0,w,h,pixels,&info,DIB_RGB_COLORS,SRCCOPY);
}
POINT projectMap(const MapView& v,float x,float y){
    return {LONG(std::lround((mapRect.left+mapRect.right)*.5f+(x-v.x)*v.scale)),
            LONG(std::lround((mapRect.top+mapRect.bottom)*.5f-(y-v.y)*v.scale))};
}
int mapZoom(const MapView& v){
    if(v.scale>1.8f)return 8;
    if(v.scale>.44f)return 6;
    return 4;
}
ArchiveTile clickedTile(const MapView& v,POINT p){
    const int z=mapZoom(v);
    const float unit=9000.0f/ArchiveTile::columns(z);
    const auto origin=projectMap(v,-4140,8400);
    return {z,int(std::floor((p.x-origin.x)/(v.scale*unit))),int(std::floor((p.y-origin.y)/(v.scale*unit)))};
}
void paintMap(HDC dc){
    auto& v=apilPage==Routes?routeMap:tripMap;
    if(v.scale<=0){
        v={360,1650,std::min(float(mapRect.right-mapRect.left)/9000,float(mapRect.bottom-mapRect.top)/13500)};
    }
    const int saved=SaveDC(dc);
    IntersectClipRect(dc,mapRect.left,mapRect.top,mapRect.right,mapRect.bottom);
    FillRect(dc,&mapRect,ui::panel());
    if(fallbackMap){
        const auto a=projectMap(v,-117.25f*48.5f,172.53125f*48.5f);
        const auto b=projectMap(v,(256-117.25f)*48.5f,(172.53125f-256)*48.5f);
        HDC src=CreateCompatibleDC(dc);auto old=SelectObject(src,fallbackMap);
        StretchBlt(dc,a.x,a.y,b.x-a.x,b.y-a.y,src,0,0,768,768,SRCCOPY);
        SelectObject(src,old);DeleteDC(src);
    }
    const int z=mapZoom(v);
    const float unit=9000.0f/ArchiveTile::columns(z);
    auto first=clickedTile(v,{mapRect.left,mapRect.top}),last=clickedTile(v,{mapRect.right,mapRect.bottom});
    const auto progress=overlay.mapDownloadProgress();
    for(int y=std::max(0,first.y);y<=std::min(ArchiveTile::rows(z)-1,last.y);++y)
        for(int x=std::max(0,first.x);x<=std::min(ArchiveTile::columns(z)-1,last.x);++x){
            ArchiveTile tile{z,x,y};const auto a=projectMap(v,-4140+x*unit,8400-y*unit),b=projectMap(v,-4140+(x+1)*unit,8400-(y+1)*unit);
            if(auto image=overlay.mapImage(tile))drawBitmap(dc,image->data(),256,256,a.x,a.y,b.x-a.x,b.y-a.y);
            else for(const auto& transfer:progress.active)if(transfer.key==tile.key()){
                const int barWidth=std::min(72,int(b.x-a.x)-8);if(barWidth<6)break;
                const int bx=(a.x+b.x-barWidth)/2,by=(a.y+b.y)/2;
                RECT track{bx,by,bx+barWidth,by+5};FillRect(dc,&track,static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
                const auto ratio=transfer.total?std::min(1.0,double(transfer.received)/transfer.total):.25;
                const int fill=int(barWidth*ratio);
                const int offset=transfer.total?0:int((barWidth-fill)*(GetTickCount64()%900)/900);
                RECT bar{bx+offset,by,bx+offset+fill,by+5};FillRect(dc,&bar,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));break;
            }
        }
    auto drawPath=[&](const auto& points,COLORREF color,int thick,auto starts){
        if(points.empty())return;
        auto pen=CreatePen(PS_SOLID,thick,color);auto old=SelectObject(dc,pen);
        for(size_t i=0;i<points.size();++i){
            const auto p=projectMap(v,points[i].x,points[i].y);
            if(i==0||starts(points[i]))MoveToEx(dc,p.x,p.y,nullptr);else LineTo(dc,p.x,p.y);
        }
        SelectObject(dc,old);DeleteObject(pen);
    };
    if(apilPage==Routes){
        const auto color=currentRouteColor();
        drawPath(route,RGB(color>>16,(color>>8)&255,color&255),3,[](const Point&){return false;});
        const auto selected=SendMessageW(objectList,LB_GETCURSEL,0,0);
        for(size_t i=0;i<waitingMarkers.entries.size();++i){
            const auto& m=waitingMarkers.entries[i];const auto p=projectMap(v,m.pose.x,m.pose.y);
            ui::rounded(dc,{p.x-5,p.y-5,p.x+6,p.y+6},selected==LRESULT(i)?RGB(255,234,135):RGB(240,156,78),RGB(32,32,32),8);
        }
    }else drawPath(tripView,RGB(117,192,248),2,[](const TripPoint& p){return p.flags&TripHistory::NewTrip;});
    if(haveSample){const auto p=projectMap(v,sample.position.x,sample.position.y);ui::rounded(dc,{p.x-4,p.y-4,p.x+5,p.y+5},RGB(152,247,204),RGB(32,32,32),8);}
    if(saved)RestoreDC(dc,saved);
}
void foldRouteMap(bool fold){
    if(apilPage!=Routes||foldedMap==fold)return;
    foldedMap=fold;textIfChanged(routeFold,fold?L"Развернуть":L"Свернуть");
    layoutApil();
}
LRESULT CALLBACK routeListProc(HWND child,UINT msg,WPARAM wp,LPARAM lp){
    const auto old=reinterpret_cast<WNDPROC>(GetPropW(child,L"ApilRouteListProc"));
    if(msg==WM_MOUSEWHEEL&&apilPage==Routes){
        const int delta=GET_WHEEL_DELTA_WPARAM(wp);
        if(delta<0&&!foldedMap){foldRouteMap(true);return 0;}
        if(delta>0&&foldedMap&&SendMessageW(child,LB_GETTOPINDEX,0,0)<=0){foldRouteMap(false);return 0;}
    }
    if(msg==WM_NCDESTROY)RemovePropW(child,L"ApilRouteListProc");
    return old?CallWindowProcW(old,child,msg,wp,lp):DefWindowProcW(child,msg,wp,lp);
}
void buildApilUi(HWND w){
    window=w;
    font=CreateFontW(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    titleFont=CreateFontW(-28,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    smallFont=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    auto brand=field(Sidebar,L"STATIC",L"apil",SS_CENTERIMAGE,21,18,165,40);
    SendMessageW(brand,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
    versionLabel=field(Sidebar,L"STATIC",apilVersionWide,0,22,61,164,20);
    updateLabel=field(Sidebar,L"STATIC",L"Проверка обновлений…",0,22,84,165,49);
    for(HWND label:{versionLabel,updateLabel})SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
    button(Sidebar,L"Главная",13,150,171,300);
    button(Sidebar,L"Маршруты",13,192,171,301);
    button(Sidebar,L"Диагностика",13,234,171,302);
    button(Sidebar,L"Настройки",13,276,171,303);

    connectButton=button(Home,L"Подключить игру",218,65,235,100);
    connectionLabel=field(Home,L"STATIC",L"Ожидание District",0,218,108,670,24);
    button(Home,L"Запись / стоп · Ins",218,137,166,101);
    button(Home,L"Сохранить",395,137,130,102);
    button(Home,L"Пуск / стоп · Del",536,137,163,103);
    button(Home,L"Стоп",710,137,117,104);
    statusLabel=field(Home,L"STATIC",L"",0,218,175,700,50,501);
    statsLabel=field(Home,L"STATIC",L"",0,218,232,700,58,502);
    sceneLabel=field(Home,L"STATIC",L"",0,218,296,700,29);

    routeList=field(Routes,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,218,66,500,280,401);
    button(Routes,L"Настроить",218,65,116,408);
    button(Routes,L"Весь маршрут",218,101,140,409);
    routeFold=button(Routes,L"Свернуть",369,101,137,410);
    button(Routes,L"Удалить",517,101,120,411);
    mapRect={218,145,995,420};
    field(Routes,L"STATIC",L"Секторы маршрута",0,218,450,310,25,403);
    field(Routes,L"STATIC",L"Объекты ожидания",0,560,450,310,25,404);
    sectorList=field(Routes,L"LISTBOX",L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_BORDER|WS_TABSTOP,218,479,305,115,402);
    objectList=field(Routes,L"LISTBOX",L"",LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_VSCROLL|WS_BORDER|WS_TABSTOP,560,479,315,115,405);
    for(HWND list:{sectorList,objectList}){
        const auto old=SetWindowLongPtrW(list,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(routeListProc));
        SetPropW(list,L"ApilRouteListProc",reinterpret_cast<HANDLE>(old));
    }
    button(Routes,L"Изменить секторы",218,604,245,406);
    button(Routes,L"Настроить объект",560,604,245,407);

    button(Diagnostics,L"Сохранить отчёт",218,66,181,105);
    button(Diagnostics,L"Записи поездок",409,66,181,106);
    button(Diagnostics,L"Папка данных",600,66,175,107);

    autoConnectBox=button(Settings,L"Автоматическое подключение",218,67,305,601,true);
    gpsBox=button(Settings,L"Навигация по GPS",218,106,305,602,true);
    radarBox=button(Settings,L"Проверять препятствия",218,145,305,603,true);
    overlayBox=button(Settings,L"Радар и метки поверх игры",218,184,305,604,true);
    sceneBox=button(Settings,L"Панель навигации в игре",218,223,305,605,true);
    mapBox=button(Settings,L"Спутниковая карта в панели",218,262,305,606,true);
    loopBox=button(Settings,L"Повторять маршрут по кругу",218,301,305,607,true);
    hornBox=button(Settings,L"Предупреждать сигналом",218,340,305,608,true);
    gpsPreviewBox=button(Settings,L"Показывать линию GPS",218,379,305,609,true);
    const wchar_t* captions[]={L"Скорость заранее",L"Устойчивый объезд",L"Автовосстановление",L"Прогноз трафика",L"Журнал поездок"};
    for(size_t i=0;i<improvementBindings.size();++i){
        const auto& binding=improvementBindings[i];
        setCheck(button(Settings,captions[i],536,67+int(i)*39,284,620+int(i),true),improvements.*binding.member);
    }
    button(Settings,L"Скорости и размеры панели",536,280,284,610);
    button(Settings,L"Правило модели объекта",536,319,284,611);
    field(Settings,L"STATIC",L"Целевая скорость, км/ч",0,536,373,280,24);
    speedBox=field(Settings,L"EDIT",L"30",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,536,402,125,30,612);
    SendMessageW(speedBox,EM_SETLIMITTEXT,12,0);
    const auto pref=[&](const wchar_t* section,const wchar_t* key,int fallback){return GetPrivateProfileIntW(section,key,fallback,settingsFile.c_str())!=0;};
    autoConnectEnabled=pref(L"connection",L"automatic",1);
    loopEnabled=pref(L"playback",L"loop",0);
    setCheck(autoConnectBox,autoConnectEnabled);setCheck(gpsBox,gpsEnabled);
    setCheck(radarBox,pref(L"playback",L"radar",1));setCheck(overlayBox,pref(L"playback",L"overlay",1));
    setCheck(sceneBox,pref(L"scene",L"enabled",1));setCheck(mapBox,navigatorSettings.mapEnabled);
    setCheck(loopBox,loopEnabled);setCheck(hornBox,hornEnabled);setCheck(gpsPreviewBox,gpsPreview);
    wchar_t value[64]{};GetPrivateProfileStringW(L"playback",L"speed",L"30",value,64,settingsFile.c_str());SetWindowTextW(speedBox,value);
    fallbackMap=static_cast<HBITMAP>(LoadImageW(nullptr,(appDir.parent_path()/L"assets/map/navigator-atlas.bmp").c_str(),IMAGE_BITMAP,0,0,LR_LOADFROMFILE|LR_CREATEDIBSECTION));
    showPage(Home);updateApilUi();
}
LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:buildApilUi(w);return 0;
    case WM_SIZE:if(updateLabel)layoutApil();return 0;
    case WM_GETMINMAXINFO:{
        auto* limits=reinterpret_cast<MINMAXINFO*>(lp);limits->ptMinTrackSize={880,680};return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);
        RECT rc{};GetClientRect(w,&rc);FillRect(dc,&rc,ui::background());
        const RECT side{0,0,198,rc.bottom};FillRect(dc,&side,ui::panel());
        if(apilPage==Routes||apilPage==Diagnostics){
            HDC buffer=CreateCompatibleDC(dc);
            auto bitmap=CreateCompatibleBitmap(dc,rc.right,rc.bottom);
            if(buffer&&bitmap){
                auto old=SelectObject(buffer,bitmap);paintMap(buffer);
                BitBlt(dc,mapRect.left,mapRect.top,mapRect.right-mapRect.left,mapRect.bottom-mapRect.top,
                    buffer,mapRect.left,mapRect.top,SRCCOPY);SelectObject(buffer,old);
            }
            if(bitmap)DeleteObject(bitmap);
            if(buffer)DeleteDC(buffer);
        }
        if(apilPage==Home&&apilConnected()&&haveSample){
            const auto& shown=mode==Mode::Record?recording:gpsDriving?activeRoute:route;
            if(const auto* frame=overlay.navigatorPreview(sample,sceneBoxes,shown,currentRouteColor(),speed))
                drawBitmap(dc,frame->pixels.data(),frame->width,frame->height,218,328,384,278);
        }
        auto old=SelectObject(dc,titleFont);SetTextColor(dc,RGB(245,245,247));SetBkMode(dc,TRANSPARENT);
        const wchar_t* name=apilPage==Home?L"Главная":apilPage==Routes?L"Маршруты":apilPage==Diagnostics?L"Диагностика":L"Настройки";
        TextOutW(dc,218,18,name,int(wcslen(name)));SelectObject(dc,old);
        EndPaint(w,&ps);return 0;
    }
    case WM_DRAWITEM:if(ui::drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(lp),font))return TRUE;break;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{
        HDC dc=reinterpret_cast<HDC>(wp);HWND child=reinterpret_cast<HWND>(lp);
        const bool sidebar=std::any_of(views.begin(),views.end(),[&](const ViewControl& c){return c.handle==child&&c.page==Sidebar;});
        const bool fill=sidebar||msg!=WM_CTLCOLORSTATIC;
        SetTextColor(dc,child==updateLabel?RGB(164,218,199):RGB(235,235,235));
        SetBkColor(dc,fill?RGB(29,29,29):RGB(17,17,19));
        return reinterpret_cast<LRESULT>(fill?ui::panel():ui::background());
    }
    case WM_MOUSEWHEEL:{
        if(apilPage!=Routes&&apilPage!=Diagnostics)break;
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(w,&p);
        const auto delta=GET_WHEEL_DELTA_WPARAM(wp);
        if(!PtInRect(&mapRect,p)){
            if(apilPage==Routes)foldRouteMap(delta<0);
            return 0;
        }
        auto& v=apilPage==Routes?routeMap:tripMap;
        if(v.scale<=0)return 0;
        const auto before=v.scale;v.scale=std::clamp(before*std::pow(1.25f,float(delta)/120),.014f,14.0f);
        v.x+=(p.x-(mapRect.left+mapRect.right)*.5f)*(1/before-1/v.scale);
        v.y-=(p.y-(mapRect.top+mapRect.bottom)*.5f)*(1/before-1/v.scale);
        InvalidateRect(w,&mapRect,FALSE);return 0;
    }
    case WM_LBUTTONDOWN:case WM_MBUTTONDOWN:{
        POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        if((apilPage==Routes||apilPage==Diagnostics)&&PtInRect(&mapRect,p)){
            panning=true;movedMap=false;mapDown=p;beforePan=apilPage==Routes?routeMap:tripMap;SetCapture(w);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if(panning&&beforePan.scale>0){
            const int dx=GET_X_LPARAM(lp)-mapDown.x,dy=GET_Y_LPARAM(lp)-mapDown.y;
            if(movedMap||std::abs(dx)+std::abs(dy)>=5){
                movedMap=true;auto& v=apilPage==Routes?routeMap:tripMap;
                v.x=std::clamp(beforePan.x-dx/v.scale,-4140.0f,4860.0f);
                v.y=std::clamp(beforePan.y+dy/v.scale,-5100.0f,8400.0f);
                InvalidateRect(w,&mapRect,FALSE);
            }
        }return 0;
    case WM_LBUTTONUP:case WM_MBUTTONUP:
        if(panning){
            panning=false;ReleaseCapture();
            if(!movedMap&&msg==WM_LBUTTONUP&&beforePan.scale>0){
                if(apilPage==Diagnostics){
                    const auto tile=clickedTile(tripMap,mapDown);
                    if(tile.valid()&&!overlay.mapImage(tile))overlay.restartMapTile(tile);
                }else if(apilPage==Routes){
                    for(size_t i=0;i<waitingMarkers.entries.size();++i){
                        const auto& m=waitingMarkers.entries[i];const auto p=projectMap(routeMap,m.pose.x,m.pose.y);
                        if(std::abs(p.x-mapDown.x)<=8&&std::abs(p.y-mapDown.y)<=8){
                            SendMessageW(objectList,LB_SETCURSEL,i,0);editWaitObject();break;
                        }
                    }
                }
            }
        }return 0;
    case WM_CAPTURECHANGED:panning=false;return 0;
    case WM_ACTIVATE:
        if(LOWORD(wp)!=WA_INACTIVE&&mode!=Mode::Idle&&mode!=Mode::Record&&mode!=Mode::Armed)
            stop(L"Игра потеряла фокус. Управление отключено.");
        return 0;
    case WM_COMMAND:{
        const int id=LOWORD(wp),event=HIWORD(wp);
        HWND child=reinterpret_cast<HWND>(lp);
        if(id==401&&event==CBN_SELCHANGE){
            const auto index=SendMessageW(routeList,CB_GETCURSEL,0,0);
            if(mode==Mode::Idle&&index>=0&&size_t(index)<routeCatalog.entries.size()){
                routeManuallySelected=true;selectSavedRoute(size_t(index));refreshRoutes();
            }
            return 0;
        }
        if(id==402||id==405){
            if(event==LBN_DBLCLK){if(id==402)editSector();else editWaitObject();}
            InvalidateRect(w,&mapRect,FALSE);return 0;
        }
        if(id==612&&event==EN_KILLFOCUS){
            auto raw=controlText(speedBox);std::replace(raw.begin(),raw.end(),L',',L'.');
            if(const auto n=numeric(raw);n>=1&&n<=3600)uiCommand(L"value|target|"+raw);
            else {wchar_t value[64]{};GetPrivateProfileStringW(L"playback",L"speed",L"30",value,64,settingsFile.c_str());SetWindowTextW(speedBox,value);}
            return 0;
        }
        if(event!=BN_CLICKED)return 0;
        if(id>=300&&id<=303){showPage(Page(id-300));updateApilUi();return 0;}
        if(id==100){connect();updateApilUi();return 0;}
        if(id>=601&&id<=609&&ui::click(child)){
            if(id==601){
                autoConnectEnabled=getCheck(child);
                WritePrivateProfileStringW(L"connection",L"automatic",autoConnectEnabled?L"1":L"0",settingsFile.c_str());
            }else {
                const wchar_t* keys[]={L"gpsEnabled",L"radar",L"overlay",L"scene",L"panelMap",L"loop",L"horn",L"gpsPreview"};
                uiCommand(std::wstring(L"toggle|")+keys[id-602]+(getCheck(child)?L"|1":L"|0"));
            }
            return 0;
        }
        if(id>=620&&size_t(id-620)<improvementBindings.size()&&ui::click(child)){
            uiCommand(std::wstring(L"toggle|")+improvementBindings[size_t(id-620)].key+(getCheck(child)?L"|1":L"|0"));return 0;
        }
        if(id==610){editNumbers();return 0;}
        if(id==611){editModel();return 0;}
        if(!apilConnected())return 0;
        switch(id){
        case 101:toggleRecord();break;
        case 102:case 104:command(id);break;
        case 103:togglePlayback();break;
        case 105:uiCommand(L"action|report");break;
        case 106:uiCommand(L"action|flights");break;
        case 107:uiCommand(L"action|folder");break;
        case 406:editSector();break;
        case 407:editWaitObject();break;
        case 408:editRouteInfo();break;
        case 409:fitRoute();InvalidateRect(w,&mapRect,FALSE);break;
        case 410:foldRouteMap(!foldedMap);break;
        case 411:
            if(!routeId().empty()&&mode==Mode::Idle&&MessageBoxW(w,L"Удалить выбранный маршрут?",L"APIL",MB_YESNO|MB_ICONQUESTION)==IDYES)
                uiCommand(L"route|delete|"+routeId());
            break;
        }updateApilUi();return 0;
    }
    case WM_APP:{
        auto result=reinterpret_cast<std::wstring*>(lp);message=*result;delete result;
        connecting=false;updateApilUi();return 0;
    }
    case WM_CLOSE:
        if(connecting){MessageBoxW(w,L"Подожди, пока завершится подключение.",L"APIL",MB_OK);return 0;}
        stop(L"Закрытие программы.");DestroyWindow(w);return 0;
    case WM_DESTROY:
        tripHistory.flush(GetTickCount64(),true);overlay.close();detach();
        if(fallbackMap)DeleteObject(fallbackMap);
        DeleteObject(font);DeleteObject(smallFont);DeleteObject(titleFont);
        window=nullptr;PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
