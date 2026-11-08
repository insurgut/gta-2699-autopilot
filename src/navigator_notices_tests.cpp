#include "archive_map.hpp"
#include "navigator_prefetch.hpp"
#include "navigator_notices.hpp"
#include "navigator_group.hpp"
#include "notice_section.hpp"
#include <cassert>
#include <iostream>
int main(){
 ArchiveSweep sweep;
 std::array<int,3> counts{};
 int previousZoom=8;
 do{
    const auto tile=sweep.tile(), decoded=ArchiveTile::fromKey(tile.key());
    assert(tile.valid()&&tile.zoom==decoded.zoom&&tile.x==decoded.x&&tile.y==decoded.y);
    assert(tile.zoom<=previousZoom);
    previousZoom=tile.zoom;
    ++counts[(tile.zoom-4)/2];
 }while(sweep.advance());
 assert((counts==std::array<int,3>{96,1536,24576}));
 assert(sweep.tile().zoom==8);
 constexpr float tileUnit=9000.0f/128;
 auto north=navigatorPrefetch(0,0,0,1,0,2);
 auto fast=navigatorPrefetch(0,0,0,1,45,2);
 auto south=navigatorPrefetch(0,0,0,-1,0,2);
 assert(!north.empty()&&fast.size()>north.size()&&!south.empty());
 auto centerY=[&](ArchiveTile tile){return 8400-(tile.y+.5f)*tileUnit;};
 for(const auto& tile:north)assert(tile.valid()&&centerY(tile)>0&&centerY(tile)<=1000);
 for(const auto& tile:fast)assert(tile.valid()&&centerY(tile)>0&&centerY(tile)<=1800);
 for(const auto& tile:south)assert(tile.valid()&&centerY(tile)<0&&centerY(tile)>=-1000);
 assert(centerY(north.back())>900&&centerY(fast.back())>1700);
 assert(navigatorPrefetch(0,0,0,0,0,2).empty());
 assert(navigatorPrefetch(-1e20f,0,0,1,0,2).empty());
 for(const auto& tile:navigatorPrefetch(4850,8380,1,1,40,2))assert(tile.valid());

 assert((!ArchiveTile{8,128,0}.valid()&&!ArchiveTile{7,0,0}.valid()));
 const auto group=navigatorGroup(1920,1080,NavigatorSettings{},2);
 const int x=group.nav.x,y=group.nav.y,center=x+group.nav.width/2;
 assert(!navigatorPixel(group,x,y)&&navigatorPixel(group,center,y));
 assert(navigatorPixel(group,center,y+group.nav.height-1));
 assert(!navigatorPixel(group,center,y+group.nav.height));
 assert(navigatorPixel(group,center,y+group.rowTop(0)));
 assert(!navigatorPixel(group,center,y+group.totalHeight));
 assert(!navigatorPixel(NavigatorGroup{},center,y));

 NavigatorNotices q;NavigatorNoticeState s;s.connected=true;s.speed=70;q.observe(0,s);assert(q.visible(0).empty());
 s.speed=75;q.observe(10,s);s.speed=80;q.observe(20,s);auto v=q.visible(20);assert(v.size()==1&&v[0].detail==L"80 км/ч");
 s.error=true;s.message=L"fault";q.observe(30,s);s.speed=90;q.observe(40,s);v=q.visible(40);assert(v.size()==2&&v[0].critical&&v[1].key==L"speed");
 q.observe(50,s);assert(q.visible(50).size()==2);assert(q.visible(5040).size()==1);assert(q.visible(9030).empty());
 s.error=false;s.playing=true;s.sectionKey=L"a";s.sectionText=L"section";q.observe(10000,s);v=q.visible(10000);assert(v.size()==2&&v[0].key==L"section");
 s.sectionKey=L"b";q.observe(10010,s);assert(q.visible(10010).size()==2);s.playing=false;s.finished=true;q.observe(10020,s);v=q.visible(10020);assert(v[0].key==L"finish");
 // Finishing must not also announce a manual stop.
 for(const auto&n:v)assert(n.key!=L"playback"||n.icon!=NoticeIcon::Pause);
 for(int h:{240,300,480,720,1080})for(int count=0;count<4;count++){auto g=navigatorGroup(1280,h,{360,480,85},count);assert(g.nav.y>=16&&g.nav.y+g.totalHeight<=h-16);assert(g.count<=count);assert(g.totalHeight==g.nav.height+g.count*65);for(int i=0;i<g.count;i++)assert(g.rowTop(i)==g.nav.height+3+i*65);}
 assert(!groupPixelInside(0,0,360,480,14,4));assert(groupPixelInside(4,479,360,480,14,4));assert(!groupPixelInside(4,479,360,480,14,14));assert(!groupPixelInside(-1,4,360,480,14,4));
 std::vector<Point> r(6);r[2].speedLimit=r[3].speedLimit=20/3.6f;r[4].ignoreObstacles=r[5].ignoreObstacles=true;
 auto a=noticeSection(r,1,0),b=noticeSection(r,2,0),c=noticeSection(r,3,0);assert(a.key==b.key&&a.key!=c.key);assert(a.text.find(L"2–4")!=std::wstring::npos);assert(c.text.find(L"игнорировать")!=std::wstring::npos);assert(noticeSection(r,1,1).key!=a.key);
 std::cout<<"Navigator queue, grouped geometry and section transitions passed\n";
}
