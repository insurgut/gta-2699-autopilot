#pragma once
#include "telemetry.hpp"
#include <algorithm>
class AcrylicHud {
    HWND background=nullptr,foreground=nullptr;HFONT font=nullptr;int backdrop=0,lastW=0,lastH=0;std::wstring lastText;
    struct Margins {int left,right,top,bottom;};
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b){
        auto self=reinterpret_cast<AcrylicHud*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){auto c=reinterpret_cast<CREATESTRUCTW*>(b);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(c->lpCreateParams));return TRUE;}
        if(m==WM_NCHITTEST)return HTTRANSPARENT;
        if(m==WM_MOUSEACTIVATE)return MA_NOACTIVATE;
        if(m==WM_ERASEBKGND)return 1;
        if(m==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);bool textLayer=self&&w==self->foreground;auto brush=CreateSolidBrush(textLayer?RGB(255,0,255):(self&&self->backdrop?RGB(0,0,0):RGB(29,29,29)));FillRect(dc,&r,brush);DeleteObject(brush);if(textLayer&&self->font){auto of=SelectObject(dc,self->font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,self->lastText.rfind(L"Ошибка",0)==0?RGB(235,235,235):RGB(248,248,248));RECT area{16,12,r.right-16,r.bottom-10};DrawTextW(dc,self->lastText.c_str(),int(self->lastText.size()),&area,DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,of);}EndPaint(w,&ps);return 0;}
        return DefWindowProcW(w,m,a,b);
    }
    bool create(){
        if(background&&foreground)return true;
        WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"DistrictAcrylicHUD15";RegisterClassW(&c);
        DWORD ex=WS_EX_TOPMOST|WS_EX_NOACTIVATE|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW;
        background=CreateWindowExW(ex,c.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,c.hInstance,this);
        foreground=CreateWindowExW(ex|WS_EX_LAYERED,c.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,c.hInstance,this);
        if(!background||!foreground){close();return false;}
        SetLayeredWindowAttributes(foreground,RGB(255,0,255),255,LWA_COLORKEY);
        auto dwm=LoadLibraryW(L"dwmapi.dll");
        if(dwm){using Set=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);using Extend=HRESULT(WINAPI*)(HWND,const Margins*);auto set=reinterpret_cast<Set>(GetProcAddress(dwm,"DwmSetWindowAttribute"));auto extend=reinterpret_cast<Extend>(GetProcAddress(dwm,"DwmExtendFrameIntoClientArea"));
            if(set){BOOL dark=TRUE;set(background,20,&dark,sizeof(dark));int transient=3;if(SUCCEEDED(set(background,38,&transient,sizeof(transient))))backdrop=1;int corners=2;set(background,33,&corners,sizeof(corners));}
            if(extend){Margins margins{-1,-1,-1,-1};extend(background,&margins);}FreeLibrary(dwm);
        }
        if(!backdrop){struct Accent {int state,flags;DWORD color;int animation;};struct Data {int attribute;PVOID data;SIZE_T bytes;};using Set=BOOL(WINAPI*)(HWND,Data*);auto set=reinterpret_cast<Set>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetWindowCompositionAttribute"));if(set){Accent accent{4,2,0xA01D1D1D,0};Data data{19,&accent,sizeof(accent)};if(set(background,&data))backdrop=2;else{accent.state=3;if(set(background,&data))backdrop=2;}}}
        if(!backdrop){SetWindowLongPtrW(background,GWL_EXSTYLE,GetWindowLongPtrW(background,GWL_EXSTYLE)|WS_EX_LAYERED);SetLayeredWindowAttributes(background,0,185,LWA_ALPHA);}
        font=CreateFontW(-20,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");return true;
    }
public:
    int mode()const{return backdrop;}
    void hide(){if(background)ShowWindow(background,SW_HIDE);if(foreground)ShowWindow(foreground,SW_HIDE);}
    void close(){if(background)DestroyWindow(background);if(foreground)DestroyWindow(foreground);if(font)DeleteObject(font);background=foreground=nullptr;font=nullptr;lastW=lastH=0;lastText.clear();backdrop=0;}
    void show(POINT origin,int availableWidth,const std::wstring&text){
        if(text.empty()){hide();return;}if(!create())return;
        HDC dc=GetDC(foreground);if(!dc)return;auto old=SelectObject(dc,font);RECT measured{0,0,std::max(120,std::min(580,availableWidth-72)),0};DrawTextW(dc,text.c_str(),int(text.size()),&measured,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);
        int w=int(std::max<LONG>(200,measured.right+32)),h=int(std::clamp<LONG>(measured.bottom+26,52,150));
        bool redraw=w!=lastW||h!=lastH||text!=lastText;
        if(w!=lastW||h!=lastH){HRGN clip=CreateRoundRectRgn(0,0,w+1,h+1,18,18);if(!SetWindowRgn(background,clip,TRUE))DeleteObject(clip);InvalidateRect(background,nullptr,TRUE);}
        SetWindowPos(background,HWND_TOPMOST,origin.x+16,origin.y+16,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);SetWindowPos(foreground,HWND_TOPMOST,origin.x+16,origin.y+16,w,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        if(redraw){auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,w,h);auto ob=SelectObject(memory,bitmap);RECT r{0,0,w,h};auto transparent=CreateSolidBrush(RGB(255,0,255));FillRect(memory,&r,transparent);DeleteObject(transparent);auto of=SelectObject(memory,font);SetBkMode(memory,TRANSPARENT);SetTextColor(memory,text.rfind(L"Ошибка",0)==0?RGB(235,235,235):RGB(248,248,248));RECT area{16,12,w-16,h-10};DrawTextW(memory,text.c_str(),int(text.size()),&area,DT_WORDBREAK|DT_NOPREFIX);BitBlt(dc,0,0,w,h,memory,0,0,SRCCOPY);SelectObject(memory,of);SelectObject(memory,ob);DeleteObject(bitmap);DeleteDC(memory);lastText=text;lastW=w;lastH=h;}
        ReleaseDC(foreground,dc);
    }
};
