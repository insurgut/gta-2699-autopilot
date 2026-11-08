#pragma once
#include "route.hpp"
#include <random>
#include <cstdint>
#include <sstream>
#include <string_view>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif
struct SavedRoute {std::string id,name;uint32_t color=0xffffff;std::vector<Point> points;RouteInputs input;};
inline bool replaceRouteFile(const std::filesystem::path&from,const std::filesystem::path&to){
#ifdef _WIN32
 return MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
 std::error_code ec;std::filesystem::rename(from,to,ec);return !ec;
#endif
}
inline std::string routeHex(uint32_t c){std::ostringstream s;s<<'#'<<std::hex<<std::uppercase<<std::setfill('0')<<std::setw(6)<<(c&0xffffff);return s.str();}
inline bool parseRouteColor(std::wstring_view text,uint32_t& color){
    if(text.size()!=7||text.front()!=L'#')return false;
    uint32_t value=0;
    for(wchar_t ch:text.substr(1)){
        unsigned digit;
        if(ch>=L'0'&&ch<=L'9')digit=unsigned(ch-L'0');
        else if(ch>=L'a'&&ch<=L'f')digit=unsigned(ch-L'a'+10);
        else if(ch>=L'A'&&ch<=L'F')digit=unsigned(ch-L'A'+10);
        else return false;
        value=(value<<4)|digit;
    }
    color=value;
    return true;
}
inline uint32_t randomRouteColor(){static std::mt19937 gen(std::random_device{}());std::uniform_int_distribution<unsigned>d(75,235);return (d(gen)<<16)|(d(gen)<<8)|d(gen);}
class RouteCatalog {
 std::filesystem::path dir;
 bool metadata(const std::string& id,const std::string& name,uint32_t color){auto file=dir/(id+".meta"),temp=file;temp+=".tmp";std::ofstream f(temp);f<<color<<' '<<std::quoted(name)<<'\n';f.close();return bool(f)&&replaceRouteFile(temp,file);}
 bool writeSettings(size_t index,const std::vector<Point>& points){
    auto target=file(index),temp=target;
    temp+=".tmp";
    return writeRoute(temp,points,&entries[index].input)&&replaceRouteFile(temp,target);
 }
public:
 std::vector<SavedRoute> entries;
 void open(const std::filesystem::path&directory){dir=directory;std::error_code ec;std::filesystem::create_directories(dir,ec);entries.clear();if(ec)return;for(const auto&e:std::filesystem::directory_iterator(dir,ec)){if(e.path().extension()!=".txt")continue;SavedRoute r;r.id=e.path().stem().string();if(!loadRoute(e.path(),r.points,&r.input))continue;std::ifstream m(dir/(r.id+".meta"));if(!(m>>r.color>>std::quoted(r.name))||r.color>0xffffff||r.name.empty()||r.name.size()>160){r.color=randomRouteColor();r.name=u8"Маршрут "+r.id;metadata(r.id,r.name,r.color);}entries.push_back(std::move(r));}std::sort(entries.begin(),entries.end(),[](const auto&a,const auto&b){return a.id<b.id;});}
 bool add(const std::vector<Point>&points,const std::string&name="",uint32_t color=0,const RouteInputs*input=nullptr){SavedRoute r;r.id=std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());while(std::filesystem::exists(dir/(r.id+".txt")))r.id+='1';r.name=name.empty()?u8"Маршрут "+std::to_string(entries.size()+1):name;r.color=color?color:randomRouteColor();while(std::any_of(entries.begin(),entries.end(),[&](const auto&e){return e.color==r.color;}))r.color=randomRouteColor();r.points=points;if(input)r.input=*input;auto file=dir/(r.id+".txt"),temp=file;temp+=".tmp";if(!writeRoute(temp,points,input)||!metadata(r.id,r.name,r.color)||!replaceRouteFile(temp,file))return false;entries.push_back(std::move(r));return true;}
 std::filesystem::path file(size_t index)const{return index<entries.size()?dir/(entries[index].id+".txt"):std::filesystem::path{};}
 static constexpr float launchRadius=15.0f;
 static bool canLaunch(Point position,const std::vector<Point>& points){
  return points.size()>=2&&distance(position,points.front())<=launchRadius;
 }
 size_t nearestLaunch(Point position)const{
  size_t index=nearestStart(position);
  return index<entries.size()&&canLaunch(position,entries[index].points)?index:SIZE_MAX;
 }
 size_t nearestStart(Point p,size_t selected=SIZE_MAX,float hysteresis=0)const{size_t best=SIZE_MAX;float gap=1e30f;for(size_t i=0;i<entries.size();i++){if(entries[i].points.empty())continue;float d=distance(p,entries[i].points.front());if(d<gap){gap=d;best=i;}}if(selected<entries.size()&&!entries[selected].points.empty()&&distance(p,entries[selected].points.front())<=gap+hysteresis)return selected;return best;}
 bool hasHistory()const{return !entries.empty()||std::filesystem::exists(dir/".initialized");}
 bool erase(size_t index){
    if(index>=entries.size())return false;
    // Keep a retired copy outside the active .txt namespace; no partial file is loaded.
    std::ofstream marker(dir/".initialized",std::ios::trunc);marker<<"1\n";marker.close();if(!marker)return false;
    auto path=file(index),retired=path;retired+=".deleted";if(!replaceRouteFile(path,retired))return false;
    for(const char*suffix:{".meta",".txt.observed",".txt.waiting"}){auto side=dir/(entries[index].id+suffix),backup=side;backup+=".deleted";std::error_code ec;if(std::filesystem::exists(side,ec))replaceRouteFile(side,backup);}
    entries.erase(entries.begin()+index);return true;
 }
 bool update(size_t index,const std::vector<Point>& points){
    if(index>=entries.size())return false;
    auto& saved=entries[index];
    const auto oldPace=saved.input.pace;
    if(oldPace!=RecordedPace::Unavailable)saved.input.pace=RecordedPace::Disabled;
    if(!writeSettings(index,points)){
        saved.input.pace=oldPace;
        return false;
    }
    saved.points=points;
    return true;
 }
 bool setRecordedPace(size_t index,bool enabled){
    if(index>=entries.size())return false;
    auto& saved=entries[index];
    const auto oldPace=saved.input.pace;
    if(oldPace==RecordedPace::Unavailable)return false;
    saved.input.pace=enabled?RecordedPace::Enabled:RecordedPace::Disabled;
    if(writeSettings(index,saved.points))return true;
    saved.input.pace=oldPace;
    return false;
 }
 bool rename(size_t index,const std::string&name){if(index>=entries.size()||name.empty()||name.size()>160||name.find_first_of("\r\n")!=std::string::npos)return false;auto& r=entries[index];if(!metadata(r.id,name,r.color))return false;r.name=name;return true;}
 bool setColor(size_t index,uint32_t color){
    if(index>=entries.size()||color>0xffffff)return false;
    auto& route=entries[index];
    if(!metadata(route.id,route.name,color))return false;
    route.color=color;
    return true;
 }
};
