#include "input_keys.hpp"
#include "gps_channel.hpp"
#include "gps_memory.hpp"
#include "attach.hpp"
#include <memory>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "scene_geometry.hpp"
#include "vehicle_palette.hpp"
#include "scene_recovery.hpp"
namespace {
HANDLE remoteProcess=nullptr;
bool readBytes(uintptr_t address,void*out,size_t size){
    SIZE_T n=0;
    return address>0x10000&&address<0x800000000000ull&&size<=1024*1024&&ReadProcessMemory(remoteProcess,reinterpret_cast<void*>(address),out,size,&n)&&n==size;
}
template<class T> bool read(uintptr_t address,T&out){return readBytes(address,&out,sizeof(T));}
uint32_t readPrimaryPaint(uintptr_t base,uintptr_t vehicle){
    return districtPrimaryPaint([](uintptr_t address,auto& value){return read(address,value);},
                                base,DISTRICT_IMAGE_SIZE,vehicle);
}
bool vecValid(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<100000&&std::abs(p.y)<100000&&std::abs(p.z)<20000;}
struct PoolEntry {uintptr_t ptr;uint64_t handle;};
static_assert(sizeof(PoolEntry)==16);
struct Pool {std::vector<PoolEntry> entries;};
bool readPool(uintptr_t interface,uintptr_t offset,Pool&out,uint32_t&retries){
    if(interface<0x10000)return false;
    for(unsigned attempt=0;attempt<2;attempt++){
        if(attempt)++retries;
        PoolIdentity before{},after{};uint8_t header[12]{};
        if(!readBytes(interface+offset,header,sizeof(header)))continue;
        std::memcpy(&before.list,header,8);std::memcpy(&before.capacity,header+8,4);
        if(before.capacity>32768||(before.capacity&&before.list<0x10000))continue;
        out.entries.resize(before.capacity);
        if(before.capacity&&!readBytes(before.list,out.entries.data(),before.capacity*sizeof(PoolEntry)))continue;
        if(!readBytes(interface+offset,header,sizeof(header)))continue;
        std::memcpy(&after.list,header,8);std::memcpy(&after.capacity,header+8,4);
        if(samePool(before,after))return true;
    }
    return false;
}
bool contains(const Pool&p,uintptr_t item){return item&&std::any_of(p.entries.begin(),p.entries.end(),[&](const auto&e){return e.ptr==item;});}
struct EntityHead {
    uintptr_t vtable=0,model=0;uint8_t type=0;Vec position{},forward{},right{},up{};
};
bool entityHead(uintptr_t ptr,EntityHead&out,bool allowTilt=false){
    uint8_t b[0xA0]{};if(!readBytes(ptr,b,sizeof(b)))return false;
    std::memcpy(&out.vtable,b,8);std::memcpy(&out.model,b+0x20,8);out.type=b[0x28]&15;
    std::memcpy(&out.right,b+0x60,sizeof(Vec));std::memcpy(&out.forward,b+0x70,sizeof(Vec));std::memcpy(&out.position,b+0x90,sizeof(Vec));
    std::memcpy(&out.up,b+0x80,sizeof(Vec));
    float norm=std::hypot(out.forward.x,out.forward.y);
    bool basis=allowTilt?(usableBasis({out.right.x,out.right.y,out.right.z})&&usableBasis({out.forward.x,out.forward.y,out.forward.z})&&usableBasis({out.up.x,out.up.y,out.up.z})):(norm>0.5f&&norm<1.5f);
    return out.vtable>0x10000&&vecValid(out.position)&&vecValid(out.forward)&&vecValid(out.right)&&vecValid(out.up)&&basis;
}
struct SceneReader {
    uint64_t lastAttempt=0,cachedVehicle=0,cachedPed=0;uint32_t cachedCount=0;
    SceneSpeed sceneSpeed;float speedEstimate=0;
    SceneRecovery recovery;
    uint64_t sceneSerial=0;
    float cachedFirstDropped=std::numeric_limits<float>::infinity();
    Telemetry::Obstacle cached[MAX_OBSTACLES]{};
    SceneStats lastStats{};
    Pool vehicles,peds,objects;
    struct Model {Vec lo{},hi{};uint32_t hash=0;bool readable=false;uint64_t beat=0;};
    std::unordered_map<uintptr_t,Model> models;
    struct RuleCache {unsigned value;uint64_t beat;};std::unordered_map<uint32_t,RuleCache> rules;
    std::filesystem::path ruleFile;
    SceneReader(){wchar_t folder[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",folder,32768);if(n&&n<32768)ruleFile=std::filesystem::path(folder)/L"Lisurgut"/L"DistrictAutopilot2699"/L"settings.ini";}
    bool ignoredModel(uint32_t model,uint64_t now){
        if(!model||ruleFile.empty())return false;
        auto it=rules.find(model);if(it!=rules.end()&&now-it->second.beat<1000)return it->second.value==1||it->second.value==2;
        unsigned value=unsigned(GetPrivateProfileIntW(L"objects",std::to_wstring(model).c_str(),0,ruleFile.c_str()));if(rules.size()>4096)rules.clear();rules[model]={value,now};return value==1||value==2;
    }

    Model modelInfo(uintptr_t model,uint64_t now){
        auto it=models.find(model);if(it!=models.end()&&now-it->second.beat<1000)return it->second;
        Model m;m.beat=now;uint8_t bytes[0x50]{};
        if(readBytes(model,bytes,sizeof(bytes))){
            std::memcpy(&m.hash,bytes+0x18,4);std::memcpy(&m.lo,bytes+0x30,sizeof(Vec));std::memcpy(&m.hi,bytes+0x40,sizeof(Vec));m.readable=true;
        }
        if(models.size()>4096)models.clear();
        models[model]=m;return m;
    }
    void publish(uint64_t now,Telemetry&s){
        auto state=recovery.state(now,s.vehicle,s.ped,s.position.x,s.position.y,s.position.z);
        float movement=std::hypot(s.position.x-recovery.x,s.position.y-recovery.y);
        if(std::isfinite(cachedFirstDropped)&&cachedFirstDropped-movement<=requiredSceneRange(speedEstimate))state=SceneState::Refreshing;
        s.radarValid=state==SceneState::Fresh||state==SceneState::Retained;
        s.radarBeat=recovery.lastSuccess;s.obstacleCount=s.radarValid?cachedCount:0;
        s.sceneSerial=sceneSerial;
        if(s.radarValid)std::copy_n(std::begin(cached),cachedCount,std::begin(s.obstacles));
        s.scene=lastStats;s.scene.state=uint32_t(state);s.scene.failureStreak=recovery.failureStreak;
        s.scene.attempts=recovery.attempts;s.scene.failures=recovery.failures;s.scene.recoveries=recovery.recoveries;s.scene.lastSuccess=recovery.lastSuccess;
        s.scene.reusedAgeMs=recovery.lastSuccess?uint32_t(std::min<uint64_t>(now-recovery.lastSuccess,0xffffffffu)):0;
        s.scene.requiredRange=requiredSceneRange(speedEstimate);
    }
    void recoverException(Telemetry&s){
        auto now=GetTickCount64();lastAttempt=now;lastStats.failureReason=16;models.clear();
        recovery.observe(now,false,s.vehicle,s.ped,s.position.x,s.position.y,s.position.z);publish(now,s);
    }
    void update(uintptr_t base,Telemetry&s){
        uintptr_t viewport=0;
        if(read(base+VIEWPORT_RVA,viewport)&&read(viewport+VIEWPORT_MATRIX_OFFSET,s.matrix)){
            bool good=true;float sum=0;for(float f:s.matrix){good=good&&std::isfinite(f)&&std::abs(f)<1e7f;sum+=std::abs(f);}s.cameraValid=good&&sum>1;
        }
        if(!s.valid)return;
        auto now=GetTickCount64();
        if(cachedVehicle!=s.vehicle||cachedPed!=s.ped){
            recovery.lastSuccess=0;recovery.failureStreak=0;cachedCount=0;lastAttempt=0;sceneSpeed={};speedEstimate=0;cachedFirstDropped=std::numeric_limits<float>::infinity();
            cachedVehicle=s.vehicle;cachedPed=s.ped;
        }
        speedEstimate=sceneSpeed.update(now,s.position.x,s.position.y);
        if(lastAttempt&&now-lastAttempt<recovery.retryDelayMs()){publish(now,s);return;}
        uintptr_t replay=0,vi=0,pi=0,oi=0;
        bool roots=false,vp=false,pp=false,op=false,ok=false;
        for(unsigned attempt=0;attempt<2;attempt++){
            if(attempt)++s.scene.poolRetries;
            bool root=read(base+REPLAY_RVA,replay)&&replay>0x10000;
            bool vr=root&&read(replay+0x10,vi),pr=root&&read(replay+0x18,pi),orr=root&&read(replay+0x28,oi);
            roots=root&&vr&&pr&&orr;
            vp=vr&&readPool(vi,0x180,vehicles,s.scene.poolRetries);
            pp=pr&&readPool(pi,0x100,peds,s.scene.poolRetries);
            op=orr&&readPool(oi,0x158,objects,s.scene.poolRetries);
            s.scene.membershipMask=unsigned(vp&&contains(vehicles,s.vehicle))|(unsigned(pp&&contains(peds,s.ped))<<1);
            uintptr_t after=0;bool stable=read(base+REPLAY_RVA,after)&&after==replay;
            ok=readyPools(roots,vp,pp,op,stable,uint32_t(vehicles.entries.size()),uint32_t(peds.entries.size()));
            if(ok)break;
        }
        s.scene.failureReason=ok?0u:(!roots?1u:(!(vp&&pp&&op)?2u:4u));
        s.scene.poolReadMask=unsigned(vp)|(unsigned(pp)<<1)|(unsigned(op)<<2);
        s.scene.capacity[0]=uint32_t(vehicles.entries.size());s.scene.capacity[1]=uint32_t(peds.entries.size());s.scene.capacity[2]=uint32_t(objects.entries.size());
        if(ok){
            s.radarValid=1;s.radarBeat=now;
            const Pool*pools[]={&vehicles,&peds,&objects};const unsigned types[]={3,4,5};
            struct Candidate {Telemetry::Obstacle obstacle;float distance2;uintptr_t entity;unsigned category;};
            std::vector<Candidate> candidates;candidates.reserve(vehicles.entries.size()+peds.entries.size()+objects.entries.size());
            std::unordered_set<uintptr_t> seen;
            for(unsigned category=0;category<3;category++){
                for(const auto&entry:pools[category]->entries){
                    if(!entry.ptr||entry.ptr==s.vehicle||entry.ptr==s.ped)continue;
                    ++s.scene.nonNull[category];if(!seen.insert(entry.ptr).second)continue;
                    EntityHead h;if(!entityHead(entry.ptr,h,category==2)){++s.scene.readFailed[category];continue;}
                    ++s.scene.rawTypes[h.type];
                    if(h.type!=types[category]){++s.scene.typeRejected[category];continue;}
                    Model model=modelInfo(h.model,now);if(ignoredModel(model.hash,now))continue;SceneFootprint box{};
                    auto convert=[](Vec v){return ScenePoint{v.x,v.y,v.z};};
                    bool bounds=model.readable&&modelFootprint(convert(h.position),convert(h.right),convert(h.forward),convert(h.up),convert(model.lo),convert(model.hi),box);
                    if(bounds&&category==2&&!modelFootprintSlice(convert(h.position),convert(h.right),convert(h.forward),convert(h.up),convert(model.lo),convert(model.hi),s.position.z-0.1f,s.position.z+1.7f,box)){++s.scene.heightRejected[category];continue;}
                    if(bounds)++s.scene.boundsValid[category];
                    else{
                        ++s.scene.boundsFallback[category];
                        float w=category==0?1.2f:0.6f,l=category==0?2.6f:0.6f,z=category==0?1.3f:1.5f;
                        if(!modelFootprint(convert(h.position),convert(h.right),convert(h.forward),convert(h.up),{-w,-l,-z},{w,l,z},box)){++s.scene.readFailed[category];continue;}
                    }
                    float distance2=footprintDistanceSquared(box,convert(s.position));
                    if(distance2>180*180){++s.scene.rangeRejected[category];continue;}
                    if(box.center.z+box.halfHeight<s.position.z-2||box.center.z-box.halfHeight>s.position.z+6){++s.scene.heightRejected[category];continue;}
                    ++s.scene.accepted[category];
                    candidates.push_back({{{box.center.x,box.center.y,box.center.z},{box.forward.x,box.forward.y,0},box.halfWidth,box.halfLength,box.halfHeight,types[category],model.hash,unsigned(bounds),entry.ptr,h.position,h.right,h.forward,h.up},distance2,entry.ptr,category});
                }
            }
            if(candidates.size()>MAX_OBSTACLES)s.scene.truncated=uint32_t(candidates.size()-MAX_OBSTACLES);
            float firstDropped=keepNearest(candidates,MAX_OBSTACLES);
            s.scene.firstDroppedDistance=std::isfinite(firstDropped)?std::sqrt(firstDropped):-1;
            ok=keptCriticalScene(true,firstDropped,requiredSceneRange(speedEstimate));
            if(!ok)s.scene.failureReason=8;
            for(const auto&candidate:candidates){s.obstacles[s.obstacleCount++]=candidate.obstacle;++s.scene.kept[candidate.category];}
        }
        auto finished=GetTickCount64();lastAttempt=finished;
        s.scene.scanMs=uint32_t(finished-now);lastStats=s.scene;
        recovery.observe(finished,ok,s.vehicle,s.ped,s.position.x,s.position.y,s.position.z);
        if(ok){++sceneSerial;cachedCount=s.obstacleCount;std::copy_n(std::begin(s.obstacles),cachedCount,std::begin(cached));cachedFirstDropped=s.scene.firstDroppedDistance<0?std::numeric_limits<float>::infinity():s.scene.firstDroppedDistance;}
        publish(finished,s);
    }
};
uint8_t replayOwned[96]{};
void releaseRecordedKeys(){INPUT keys[INPUT_KEY_COUNT]{};unsigned count=0;for(unsigned key=0;key<INPUT_KEY_COUNT;key++)if(validInputKey(key)&&(replayOwned[key/8]&(1u<<(key%8)))){auto&input=keys[count++];input.type=INPUT_KEYBOARD;input.ki.dwFlags=KEYEVENTF_KEYUP;if(key>=512)input.ki.wVk=WORD(key&255);else{input.ki.wScan=WORD(key&255);input.ki.dwFlags|=KEYEVENTF_SCANCODE|((key&256)?KEYEVENTF_EXTENDEDKEY:0);}}if(count)SendInput(count,keys,sizeof(INPUT));}
void releaseKeys(){releaseRecordedKeys();
    INPUT keys[7]{};const WORD scans[]={0x11,0x1f,0x1e,0x20,0x39,0x1c,0x12};
    for(unsigned i=0;i<7;i++){keys[i].type=INPUT_KEYBOARD;keys[i].ki.wScan=scans[i];keys[i].ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;}
    SendInput(7,keys,sizeof(INPUT));
}
void watchdogBrake(DWORD pid){releaseRecordedKeys();
    DWORD active=0;GetWindowThreadProcessId(GetForegroundWindow(),&active);
    if(active!=pid){releaseKeys();return;}
    INPUT keys[6]{};const WORD scans[]={0x11,0x1f,0x1e,0x20,0x1c,0x12};
    for(unsigned i=0;i<6;i++){keys[i].type=INPUT_KEYBOARD;keys[i].ki.wScan=scans[i];keys[i].ki.dwFlags=KEYEVENTF_SCANCODE|KEYEVENTF_KEYUP;}
    SendInput(6,keys,sizeof(INPUT));
    if(!(GetAsyncKeyState(VK_SPACE)&0x8000)){INPUT key{};key.type=INPUT_KEYBOARD;key.ki.wScan=0x39;key.ki.dwFlags=KEYEVENTF_SCANCODE;SendInput(1,&key,sizeof(key));}
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR args,int){
    wchar_t*end=nullptr;unsigned long parsed=wcstoul(args,&end,10);
    if(!parsed||!end)return 2;
    while(std::iswspace(*end))++end;
    wchar_t*controllerStart=end;unsigned long controllerPid=wcstoul(controllerStart,&end,10);
    if(!controllerPid||end==controllerStart)return 2;
    while(std::iswspace(*end))++end;
    if(*end)return 2;
    DWORD pid=DWORD(parsed);
    if(battleye())return 3;
    WinHandle single(CreateMutexW(nullptr,FALSE,(ipcName(pid)+L".reader").c_str()));
    if(!single.h||GetLastError()==ERROR_ALREADY_EXISTS)return 6;
    WinHandle process(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!process.h)return 3;
    WinHandle controller(OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(controllerPid)));
    if(!controller.h)return 3;
    remoteProcess=process.h;
    uintptr_t base=moduleBase(pid,L"District9_b2699_GameProcess.exe");
    if(!profileMatches(process,base))return 4;
    WinHandle mutex(CreateMutexW(nullptr,FALSE,ipcName(pid,true).c_str()));
    WinHandle mapping(CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Telemetry),ipcName(pid).c_str()));
    if(!mutex.h||!mapping.h)return 5;
    auto shared=static_cast<Telemetry*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Telemetry)));
    if(!shared)return 5;
    {Lock lock(mutex);if(!lock.held){UnmapViewOfFile(shared);return 5;}*shared={};shared->magic=TELEMETRY_MAGIC;shared->version=TELEMETRY_VERSION;shared->pid=pid;}
    GpsChannel gps;gps.open(pid,true);uint64_t lastGps=0;
    auto scene=std::make_unique<SceneReader>();auto started=GetTickCount64();bool outputHeld=false;uint64_t sampleSerial=0;
    while(WaitForSingleObject(process,0)==WAIT_TIMEOUT){
        if(WaitForSingleObject(controller,0)!=WAIT_TIMEOUT)break;
        Telemetry sample{};sample.magic=TELEMETRY_MAGIC;sample.version=TELEMETRY_VERSION;sample.pid=pid;sample.sampleBeat=GetTickCount64();
        uintptr_t factory=0,ped=0,car=0,driver=0;float health=0;EntityHead ph,ch;
        if(!read(base+WORLD_RVA,factory)||!read(factory+8,ped)||!entityHead(ped,ph)||ph.type!=4||!read(ped+0x280,health)||!std::isfinite(health)||health<=0||health>10000)
            std::wcscpy(sample.error,L"Персонаж недоступен. Войди в игровой мир.");
        else{
            sample.ped=ped;sample.health=health;sample.position=ph.position;
            if(!read(ped+PED_VEHICLE_OFFSET,car)||!entityHead(car,ch)||ch.type!=3||!read(car+ACTIVE_DRIVER_OFFSET,driver)||!driverMatches(ped,driver)
                ||std::hypot(ch.position.x-ph.position.x,ch.position.y-ph.position.y)>7||std::abs(ch.position.z-ph.position.z)>7)
                std::wcscpy(sample.error,L"Сядь именно за руль. Машина из предыдущей поездки не считается активной.");
            else{
                sample.valid=1;sample.vehicle=car;sample.position=ch.position;sample.forward=ch.forward;sample.ownHalfWidth=1.05f;sample.ownHalfLength=2.5f;
                sample.ownPaint=readPrimaryPaint(base,car);
                Vec lo{},hi{};
                if(read(ch.model+0x30,lo)&&read(ch.model+0x40,hi)&&vecValid(lo)&&vecValid(hi)){
                    float w=(hi.x-lo.x)/2,l=(hi.y-lo.y)/2;if(w>0.3f&&w<5&&l>0.6f&&l<15){sample.ownHalfWidth=w;sample.ownHalfLength=l;}
                }
            }
        }
        try{scene->update(base,sample);}catch(...){scene->recoverException(sample);}
        sample.sampleBeat=GetTickCount64();
        sample.sampleSerial=++sampleSerial;
        if(sample.sampleBeat-lastGps>=200){lastGps=sample.sampleBeat;GpsFrame frame{};gpsReadMemory([](uint64_t address,void*out,size_t n){return readBytes(uintptr_t(address),out,n);},base,pid,lastGps,{sample.position.x,sample.position.y,sample.position.z},{sample.forward.x,sample.forward.y,sample.forward.z},frame);if(gps.shared){Lock lock(gps.mutex);if(lock.held){auto old=*gps.shared;*gps.shared=frame;gps.shared->controllerBeat=old.controllerBeat;gps.shared->enabled=old.enabled;gps.shared->draw=old.draw;gps.shared->halfWidth=old.halfWidth;gps.shared->ribbonCount=old.ribbonCount;gps.shared->ribbon=old.ribbon;}}}
        bool finish=false;
        {Lock lock(mutex);if(lock.held){auto beat=shared->clientBeat;auto shutdown=shared->shutdown;auto control=shared->reserved;std::copy(std::begin(shared->replayKeys),std::end(shared->replayKeys),std::begin(replayOwned));
            *shared=sample;shared->clientBeat=beat;shared->shutdown=shutdown;shared->reserved=control;std::copy(std::begin(replayOwned),std::end(replayOwned),std::begin(shared->replayKeys));outputHeld=(control&1)!=0;
            auto action=readerWatchdog(sample.sampleBeat,started,beat,true,shutdown!=0);
            if(action==ReaderWatchdog::ReleaseAndKeep&&(control&3u)){watchdogBrake(pid);shared->reserved=2;outputHeld=true;}
            finish=action==ReaderWatchdog::Exit;}}
        if(finish)break;
        SwitchToThread();
    }
    if(outputHeld)releaseKeys();
    gps.close();UnmapViewOfFile(shared);return 0;
}
