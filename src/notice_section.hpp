#pragma once
#include "route.hpp"
#include <string>
struct NoticeSection{std::wstring key,text;};
// The same incoming-edge convention as the route editor and speed controller.
inline NoticeSection noticeSection(const std::vector<Point>&r,size_t segment,size_t routeId){
 if(r.size()<2)return {};
 size_t at=std::min(segment+1,r.size()-1),first=at,last=at;
 auto same=[&](size_t i){return r[i].speedLimit==r[at].speedLimit&&r[i].ignoreObstacles==r[at].ignoreObstacles;};
 while(first>1&&same(first-1))--first;
 while(last+1<r.size()&&same(last+1))++last;
 auto speed=r[at].speedLimit>0?std::to_wstring(int(std::lround(r[at].speedLimit*3.6f)))+L" км/ч":L"из настроек";
 return {std::to_wstring(routeId)+L":"+std::to_wstring(first)+L":"+std::to_wstring(last)+L":"+std::to_wstring(r[at].speedLimit)+L":"+(r[at].ignoreObstacles?L"1":L"0"),L"Точки "+std::to_wstring(first)+L"–"+std::to_wstring(last+1)+L" · "+speed+L" · препятствия: "+(r[at].ignoreObstacles?L"игнорировать":L"учитывать")};
}
