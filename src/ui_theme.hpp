#pragma once
#include <windows.h>
namespace ui {
inline HBRUSH background(){static HBRUSH b=CreateSolidBrush(RGB(17,17,19));return b;}
inline HBRUSH panel(){static HBRUSH b=CreateSolidBrush(RGB(29,29,29));return b;}
inline LRESULT CALLBACK buttonProc(HWND w,UINT m,WPARAM a,LPARAM b){
    auto old=reinterpret_cast<WNDPROC>(GetPropW(w,L"DistrictButtonProc"));
    bool check=GetPropW(w,L"DistrictCheck")!=nullptr;
    if(check&&m==BM_GETCHECK)return GetPropW(w,L"DistrictChecked")?BST_CHECKED:BST_UNCHECKED;
    if(check&&m==BM_SETCHECK){if(a==BST_CHECKED)SetPropW(w,L"DistrictChecked",HANDLE(1));else RemovePropW(w,L"DistrictChecked");InvalidateRect(w,nullptr,FALSE);return 0;}
    if(m==WM_MOUSEMOVE){SetPropW(w,L"DistrictHover",HANDLE(1));TRACKMOUSEEVENT t{sizeof(t),TME_LEAVE,w,0};TrackMouseEvent(&t);InvalidateRect(w,nullptr,FALSE);}
    if(m==WM_MOUSELEAVE){RemovePropW(w,L"DistrictHover");InvalidateRect(w,nullptr,FALSE);}
    if(m==WM_NCDESTROY){RemovePropW(w,L"DistrictButtonProc");RemovePropW(w,L"DistrictCheck");RemovePropW(w,L"DistrictChecked");RemovePropW(w,L"DistrictHover");}
    return old?CallWindowProcW(old,w,m,a,b):DefWindowProcW(w,m,a,b);
}
inline void attach(HWND w,bool check){if(check)SetPropW(w,L"DistrictCheck",HANDLE(1));auto old=SetWindowLongPtrW(w,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(buttonProc));SetPropW(w,L"DistrictButtonProc",reinterpret_cast<HANDLE>(old));}
inline bool click(HWND w){if(!w||!GetPropW(w,L"DistrictCheck"))return false;SendMessageW(w,BM_SETCHECK,SendMessageW(w,BM_GETCHECK,0,0)==BST_CHECKED?BST_UNCHECKED:BST_CHECKED,0);return true;}
inline void rounded(HDC dc,RECT r,COLORREF fill,COLORREF edge,int radius=12){auto brush=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,1,edge);auto ob=SelectObject(dc,brush),op=SelectObject(dc,pen);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(brush);DeleteObject(pen);}
inline void glyph(HDC dc,int id,int x,int y){
    auto pen=CreatePen(PS_SOLID,2,RGB(235,235,235));auto op=SelectObject(dc,pen),ob=SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
    if(id==101){Ellipse(dc,x,y,x+12,y+12);}
    else if(id==103){POINT p[]={{x+1,y},{x+12,y+6},{x+1,y+12},{x+1,y}};Polyline(dc,p,4);}
    else if(id==104){Rectangle(dc,x,y,x+12,y+12);}
    else if(id==105){MoveToEx(dc,x,y+3,nullptr);LineTo(dc,x+4,y+3);LineTo(dc,x+6,y+5);LineTo(dc,x+14,y+5);LineTo(dc,x+14,y+13);LineTo(dc,x,y+13);LineTo(dc,x,y+3);}
    else if(id==107){Rectangle(dc,x,y,x+14,y+14);MoveToEx(dc,x+7,y,nullptr);LineTo(dc,x+7,y+14);MoveToEx(dc,x,y+7,nullptr);LineTo(dc,x+14,y+7);}
    else if(id==100){Ellipse(dc,x,y+2,x+9,y+11);Ellipse(dc,x+6,y+2,x+15,y+11);}
    else{Rectangle(dc,x,y,x+12,y+14);MoveToEx(dc,x+3,y+5,nullptr);LineTo(dc,x+9,y+5);MoveToEx(dc,x+3,y+9,nullptr);LineTo(dc,x+9,y+9);}
    SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(pen);
}
inline bool drawButton(const DRAWITEMSTRUCT&d,HFONT font){if(d.CtlType!=ODT_BUTTON)return false;
    bool enabled=!(d.itemState&ODS_DISABLED),pressed=d.itemState&ODS_SELECTED,hover=GetPropW(d.hwndItem,L"DistrictHover")!=nullptr,check=GetPropW(d.hwndItem,L"DistrictCheck")!=nullptr;
    FillRect(d.hDC,&d.rcItem,panel());
    unsigned shade=!enabled?27:pressed?59:hover?48:39,edge=!enabled?39:65;
    rounded(d.hDC,d.rcItem,RGB(shade,shade,shade),RGB(edge,edge,edge));
    wchar_t text[256]{};GetWindowTextW(d.hwndItem,text,256);RECT t=d.rcItem;
    auto old=SelectObject(d.hDC,font);SetBkMode(d.hDC,TRANSPARENT);SetTextColor(d.hDC,enabled?RGB(235,235,235):RGB(110,110,110));
    if(check){RECT c{t.left+10,t.top+6,t.left+26,t.top+22};rounded(d.hDC,c,RGB(29,29,29),RGB(130,130,130),4);if(SendMessageW(d.hwndItem,BM_GETCHECK,0,0)==BST_CHECKED){auto p=CreatePen(PS_SOLID,2,RGB(235,235,235));auto op=SelectObject(d.hDC,p);MoveToEx(d.hDC,c.left+3,c.top+8,nullptr);LineTo(d.hDC,c.left+7,c.top+12);LineTo(d.hDC,c.left+13,c.top+4);SelectObject(d.hDC,op);DeleteObject(p);}t.left+=36;DrawTextW(d.hDC,text,-1,&t,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
    else{glyph(d.hDC,int(d.CtlID),t.left+14,(t.top+t.bottom)/2-7);t.left+=30;t.right-=5;DrawTextW(d.hDC,text,-1,&t,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
    if(d.itemState&ODS_FOCUS){RECT f=d.rcItem;InflateRect(&f,-4,-4);DrawFocusRect(d.hDC,&f);}SelectObject(d.hDC,old);return true;
}
}
