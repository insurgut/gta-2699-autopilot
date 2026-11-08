#pragma once
#include "telemetry.hpp"
#include <vector>
#include "gps_protocol.hpp"
struct GpsChannel{
 HANDLE mapping=nullptr,mutex=nullptr;GpsFrame*shared=nullptr;GpsFrame last{};DWORD owner=0;
 static std::wstring name(DWORD pid){return L"Local\\Lisurgut.District.GPS.v2."+std::to_wstring(pid);}
 void close(){if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);shared=nullptr;mapping=mutex=nullptr;owner=0;last={};}
 bool open(DWORD pid,bool create=false){if(owner==pid&&shared)return true;close();if(!pid)return false;auto key=name(pid);mutex=create?CreateMutexW(nullptr,FALSE,(key+L".mutex").c_str()):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(key+L".mutex").c_str());if(!mutex)return false;mapping=create?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(GpsFrame),key.c_str()):OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,key.c_str());bool fresh=create&&mapping&&GetLastError()!=ERROR_ALREADY_EXISTS;if(mapping)shared=static_cast<GpsFrame*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(GpsFrame)));if(!shared){close();return false;}owner=pid;if(fresh){Lock lock(mutex);if(lock.held){*shared={};shared->pid=pid;}}return true;}
 bool snapshot(){if(!shared)return false;Lock lock(mutex);if(!lock.held)return false;last=*shared;return last.magic==GPS_MAGIC&&last.version==GPS_VERSION&&last.bytes==sizeof(GpsFrame)&&last.pid==owner;}
 void command(uint64_t now,bool enabled,bool draw,float width,const std::vector<GpsPoint>&ribbon){if(!shared)return;Lock lock(mutex);if(!lock.held)return;shared->controllerBeat=now;shared->enabled=enabled;shared->draw=draw;shared->halfWidth=std::clamp(width,.3f,5.0f);shared->ribbonCount=uint32_t(std::min(ribbon.size(),size_t(GPS_MAX_RIBBON)));std::copy_n(ribbon.begin(),shared->ribbonCount,shared->ribbon.begin());}
};
