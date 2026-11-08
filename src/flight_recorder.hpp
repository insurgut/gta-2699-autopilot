#pragma once
#include "forward_navigation.hpp"
#include <deque>
#include <fstream>
#include <iomanip>
#include <sstream>
struct FlightFrame {
 uint64_t now=0;NavPose car{};float speed=0,target=0,steer=0,error=0,ttc=0;int mode=0;size_t segment=0;uint64_t riskEntity=0;std::string reason,route;
 std::vector<RadarBox> objects;std::vector<NavPose> planned;
};
inline float flightNumber(float value){return std::isfinite(value)?value:0;}
inline std::string flightJson(const std::string&value){std::ostringstream out;out<<'"';for(unsigned char c:value){if(c=='"'||c=='\\')out<<'\\'<<char(c);else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;else out<<char(c);}out<<'"';return out.str();}
class FlightRecorder {
 std::deque<FlightFrame> frames;uint64_t lastSample=0,incidentAt=0,lastSaved=0;std::string incident;
public:
 void reset(){*this={};}
 void push(FlightFrame frame){if(lastSample&&frame.now>=lastSample&&frame.now-lastSample<200)return;lastSample=frame.now;frames.push_back(std::move(frame));while(frames.size()>120)frames.pop_front();}
 void mark(uint64_t now,const std::string&reason){if(!incidentAt&&(!lastSaved||now-lastSaved>=30000)){incidentAt=now;incident=reason;}}
 bool due(uint64_t now)const{return incidentAt&&now>=incidentAt+2000;}
 size_t size()const{return frames.size();}
 bool save(const std::filesystem::path&directory,uint64_t now){
  if(!due(now))return false;
  std::error_code ec;std::filesystem::create_directories(directory,ec);if(ec){incidentAt=0;return false;}
  auto path=directory/("flight-"+std::to_string(now)+".jsonl");auto temp=path;temp+=".tmp";
  std::ofstream f(temp);f<<std::setprecision(9)<<"{\"schema\":1,\"incident\":"<<flightJson(incident)<<"}\n";
  for(const auto&r:frames){f<<"{\"ms\":"<<r.now<<",\"mode\":"<<r.mode<<",\"segment\":"<<r.segment<<",\"route\":"<<flightJson(r.route)<<",\"reason\":"<<flightJson(r.reason)<<",\"pose\":["<<flightNumber(r.car.x)<<','<<flightNumber(r.car.y)<<','<<flightNumber(r.car.z)<<','<<flightNumber(r.car.yaw)<<"],\"speed\":"<<flightNumber(r.speed)<<",\"target\":"<<flightNumber(r.target)<<",\"steer\":"<<flightNumber(r.steer)<<",\"error\":"<<flightNumber(r.error)<<",\"ttc\":"<<flightNumber(r.ttc)<<",\"riskEntity\":"<<r.riskEntity<<",\"objects\":[";
   bool comma=false;for(const auto&b:r.objects){if(comma)f<<',';f<<'['<<flightNumber(b.x)<<','<<flightNumber(b.y)<<','<<flightNumber(b.z)<<','<<flightNumber(b.fx)<<','<<flightNumber(b.fy)<<','<<flightNumber(b.halfWidth)<<','<<flightNumber(b.halfLength)<<','<<flightNumber(b.halfHeight)<<','<<b.passability<<']';comma=true;}f<<"],\"planned\":[";comma=false;for(const auto&p:r.planned){if(comma)f<<',';f<<'['<<flightNumber(p.x)<<','<<flightNumber(p.y)<<','<<flightNumber(p.z)<<','<<flightNumber(p.yaw)<<']';comma=true;}f<<"]}\n";
  }f.close();if(!f){incidentAt=0;return false;}
  std::filesystem::rename(temp,path,ec);incidentAt=0;if(ec)return false;lastSaved=now;
  std::vector<std::filesystem::path> files;for(const auto&e:std::filesystem::directory_iterator(directory,ec))if(e.path().filename().string().rfind("flight-",0)==0&&e.path().extension()==".jsonl")files.push_back(e.path());std::sort(files.begin(),files.end(),[](const auto&a,const auto&b){std::error_code error;return std::filesystem::last_write_time(a,error)<std::filesystem::last_write_time(b,error);});while(files.size()>10){std::filesystem::remove(files.front(),ec);files.erase(files.begin());}return true;
 }
};
