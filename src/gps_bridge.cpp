#include "gps_channel.hpp"
#include "native_marker_protocol.hpp"
#include <vector>
namespace {
struct ScriptApi {
    void (*reg)(HMODULE,void(*)())=nullptr;void (*unreg)(HMODULE)=nullptr;void (*wait)(DWORD)=nullptr;
    void (*init)(uint64_t)=nullptr;void (*push)(uint64_t)=nullptr;uint64_t*(*call)()=nullptr;
    template<class T>static T resolve(HMODULE m,const char*plain,const char*decorated){auto p=GetProcAddress(m,plain);if(!p)p=GetProcAddress(m,decorated);return reinterpret_cast<T>(p);}
    bool load(){auto h=GetModuleHandleW(L"scripthookv.dll");if(!h)return false;
        reg=resolve<decltype(reg)>(h,"scriptRegister","?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
        unreg=resolve<decltype(unreg)>(h,"scriptUnregister","?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
        wait=resolve<decltype(wait)>(h,"scriptWait","?scriptWait@@YAXK@Z");
        init=resolve<decltype(init)>(h,"nativeInit","?nativeInit@@YAX_K@Z");push=resolve<decltype(push)>(h,"nativePush64","?nativePush64@@YAX_K@Z");call=resolve<decltype(call)>(h,"nativeCall","?nativeCall@@YAPEA_KXZ");
        return reg&&unreg&&wait&&init&&push&&call;
    }

 uint64_t invoke(uint64_t hash,std::initializer_list<uint64_t> args={}){init(hash);for(auto a:args)push(a);auto r=call();return r?(*r&0xffffffffu):0;}
 GpsPoint blip(uint64_t id){init(0xFA7C7F0AADF25D09ull);push(id);auto r=call();GpsPoint p{};if(r){std::memcpy(&p.x,r,4);std::memcpy(&p.y,r+1,4);std::memcpy(&p.z,r+2,4);}return p;}
 bool along(float distance,GpsPoint&out){struct NativeVector{float x;uint32_t pad1;float y;uint32_t pad2;float z;uint32_t pad3;}p{};bool ok=invoke(0xF3162836C28F9DA5ull,{reinterpret_cast<uint64_t>(&p),1,nativeFloat(distance),0})!=0;out={p.x,p.y,p.z};return ok&&gpsPointValid(out);}
 void triangle(GpsPoint a,GpsPoint b,GpsPoint c){invoke(0xAC26716048436851ull,{nativeFloat(a.x),nativeFloat(a.y),nativeFloat(a.z+.12f),nativeFloat(b.x),nativeFloat(b.y),nativeFloat(b.z+.12f),nativeFloat(c.x),nativeFloat(c.y),nativeFloat(c.z+.12f),170,200,230,72});}
}api;
GpsChannel channel;bool registered=false;
void scriptMain(){uint64_t lastRead=0;for(;;){auto now=GetTickCount64();channel.snapshot();auto command=channel.last;
 if(api.invoke(0xB0034A223497FFCBull)){{Lock lock(channel.mutex);if(lock.held){channel.shared->status=6;channel.shared->beat=now;}}api.wait(0);continue;}
 if(now-lastRead>=500){lastRead=now;GpsFrame frame{};frame.pid=GetCurrentProcessId();frame.beat=now;frame.serial=now;auto id=api.invoke(0x1BEDE233E6CD2A1Full,{8});if(int32_t(id)<=0)id=0;frame.status=id?3:2;
  if(id){frame.waypoint=api.blip(id);for(unsigned i=0;i<GPS_MAX_POINTS;i++){GpsPoint p{};if(!api.along(float(i)*5,p))break;if(i&&gpsDistance(frame.points[i-1],p)<.05f)break;if(i&&gpsDistance(frame.points[i-1],p)>30)break;frame.points[frame.count++]=p;if(std::hypot(p.x-frame.waypoint.x,p.y-frame.waypoint.y)<12)break;}if(frame.count>=2&&std::hypot(frame.points[frame.count-1].x-frame.waypoint.x,frame.points[frame.count-1].y-frame.waypoint.y)<60)frame.status=4;}
  Lock lock(channel.mutex);if(lock.held){auto s=channel.shared;s->beat=frame.beat;s->serial=frame.serial;s->status=frame.status;s->waypoint=frame.waypoint;s->count=frame.count;std::copy_n(frame.points.begin(),frame.count,s->points.begin());}
 }
 if(command.enabled&&command.draw&&command.controllerBeat&&now>=command.controllerBeat&&now-command.controllerBeat<500&&command.ribbonCount<=GPS_MAX_RIBBON&&command.halfWidth>.2f&&command.halfWidth<=5){for(unsigned i=0;i+1<command.ribbonCount;i++){auto a=command.ribbon[i],b=command.ribbon[i+1];if(!gpsPointValid(a)||!gpsPointValid(b))break;float dx=b.x-a.x,dy=b.y-a.y,n=std::hypot(dx,dy);if(n<.01f||n>15)continue;dx=dx/n*command.halfWidth;dy=dy/n*command.halfWidth;GpsPoint l{a.x-dy,a.y+dx,a.z},r{a.x+dy,a.y-dx,a.z},ll{b.x-dy,b.y+dx,b.z},rr{b.x+dy,b.y-dx,b.z};api.triangle(l,r,ll);api.triangle(r,rr,ll);api.triangle(ll,r,l);api.triangle(ll,rr,r);}}
 api.wait(0);
}}
}
extern "C" __declspec(dllexport) unsigned DistrictGpsVersion(){return GPS_VERSION;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(module);channel.open(GetCurrentProcessId(),true);if(channel.shared&&api.load()){api.reg(module,scriptMain);registered=true;}else if(channel.shared){Lock lock(channel.mutex);if(lock.held){channel.shared->status=5;channel.shared->beat=GetTickCount64();}}}else if(reason==DLL_PROCESS_DETACH){if(!reserved&&registered&&api.unreg)api.unreg(module);channel.close();}return TRUE;}
