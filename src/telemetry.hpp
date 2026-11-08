#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
constexpr uint32_t TELEMETRY_MAGIC=0x41503631;
constexpr uint32_t TELEMETRY_VERSION=14;
constexpr uint32_t MAX_OBSTACLES=4096;
struct Vec {float x,y,z;};
struct SceneStats {
    uint32_t capacity[3]{},nonNull[3]{},readFailed[3]{},typeRejected[3]{};
    uint32_t boundsValid[3]{},boundsFallback[3]{},rangeRejected[3]{},heightRejected[3]{};
    uint32_t accepted[3]{},kept[3]{},rawTypes[16]{};
    uint32_t poolReadMask=0,truncated=0,scanMs=0;
    uint32_t state=0,failureStreak=0,reusedAgeMs=0,poolRetries=0;
    uint32_t failureReason=0,membershipMask=0;
    float requiredRange=0,firstDroppedDistance=0;
    uint64_t attempts=0,failures=0,recoveries=0,lastSuccess=0;
};
struct Telemetry {
    uint32_t magic,version,pid,valid;
    uint64_t clientBeat,sampleBeat;
    uint64_t sampleSerial,sceneSerial;
    uint32_t shutdown,reserved;
    uint8_t replayKeys[96]{};
    uint64_t ped,vehicle;
    Vec position,forward;
    float health;
    float ownHalfWidth,ownHalfLength;
    uint32_t ownPaint=0xffffffffu;
    uint32_t radarValid,cameraValid,obstacleCount;
    uint64_t radarBeat;
    float matrix[16];
    struct Obstacle {Vec position,forward;float halfWidth,halfLength,halfHeight;uint32_t kind,modelHash,boundsKnown;uint64_t entity;Vec origin,basisRight,basisForward,basisUp;} obstacles[MAX_OBSTACLES];
    SceneStats scene;
    wchar_t error[160];
};
inline std::wstring ipcName(DWORD pid,bool mutex=false){return L"Local\\Lisurgut.District2699.v14."+std::to_wstring(pid)+(mutex?L".mutex":L"");}
struct Lock {HANDLE h;bool held;explicit Lock(HANDLE v):h(v){DWORD r=WaitForSingleObject(v,0);held=r==WAIT_OBJECT_0||r==WAIT_ABANDONED;}~Lock(){if(held)ReleaseMutex(h);}};
