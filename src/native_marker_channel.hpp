#pragma once
#include "telemetry.hpp"
#include "native_marker_protocol.hpp"
class NativeMarkerChannel {
    HANDLE map=nullptr,mutex=nullptr;NativeMarkerState*shared=nullptr;DWORD attachedPid=0;bool ready=false;const wchar_t*suffix;
    bool valid()const{return shared&&shared->magic==MARKER_MAGIC&&shared->version==MARKER_VERSION&&shared->bytes==sizeof(NativeMarkerState)&&shared->pid==attachedPid;}
public:
    NativeMarkerState last{};DWORD error=0;
    explicit NativeMarkerChannel(const wchar_t*slot=L""):suffix(slot){}
    bool open(DWORD pid){
        if(!pid)return false;
        if(!shared||attachedPid!=pid){
            close();auto name=L"Local\\Lisurgut.District2699.NativeMarkers.v1."+std::to_wstring(pid)+suffix;
            mutex=CreateMutexW(nullptr,FALSE,(name+L".mutex").c_str());
            if(mutex)map=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(NativeMarkerState),name.c_str());
            if(map)shared=static_cast<NativeMarkerState*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(NativeMarkerState)));
            if(!mutex||!map||!shared){DWORD saved=GetLastError();close();error=saved?saved:ERROR_INVALID_HANDLE;return false;}
            attachedPid=pid;
        }
        if(ready)return true;
        Lock lock(mutex);if(!lock.held)return false; // retry on next tick, preserve handles/cache
        if(!shared->magic){*shared=NativeMarkerState{};shared->pid=pid;}
        if(!valid()){error=ERROR_REVISION_MISMATCH;last={};last.magic=0;return false;}
        last=*shared;error=0;ready=true;return true;
    }
    void close(){if(shared)UnmapViewOfFile(shared);if(map)CloseHandle(map);if(mutex)CloseHandle(mutex);shared=nullptr;map=mutex=nullptr;attachedPid=0;ready=false;error=0;last={};}
    bool snapshot(){if(!shared)return false;Lock lock(mutex);if(!lock.held)return false;if(!valid()){error=ERROR_REVISION_MISMATCH;ready=false;last={};last.magic=0;return false;}last=*shared;error=0;return true;}
    void publish(DWORD pid,uint64_t now,bool enabled,float x,float y,float z,float heading,uint64_t revision){
        if(!open(pid))return;
        Lock lock(mutex);if(!lock.held)return;
        if(!valid()){ready=false;error=ERROR_REVISION_MISMATCH;return;}
        shared->controllerBeat=now;shared->enabled=enabled?1:0;shared->x=x;shared->y=y;shared->z=z;shared->heading=heading;shared->revision=revision;last=*shared;error=0;
    }
    void status(uint32_t value,bool frame=false){if(!shared)return;Lock lock(mutex);if(!lock.held||!valid())return;shared->status=value;if(!shared->loadedBeat)shared->loadedBeat=GetTickCount64();if(frame)shared->scriptBeat=GetTickCount64();if(value==4)++shared->drawFrames;last=*shared;}
};
