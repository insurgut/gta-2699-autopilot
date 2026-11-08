#include "native_marker_channel.hpp"
#include "native_marker_extra.hpp"
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
    void marker(const NativeMarkerState&s,int type,float z,float sx,float sy,float sz,int alpha,bool finish=false){init(0x28477EC23D892089ull);for(auto arg:markerArguments(s,type,z,sx,sy,sz,alpha,finish?255:70,finish?160:220,finish?70:165))push(arg);call();}
    bool ground(const NativeMarkerState&s,float&z){init(0xC906A7DAB05C8D2Bull);push(nativeFloat(s.x));push(nativeFloat(s.y));push(nativeFloat(s.z+3));push(reinterpret_cast<uint64_t>(&z));push(0);push(0);auto r=call();return r&&(*r&0xffffffffu)!=0&&std::isfinite(z)&&std::abs(z-s.z)<8;}
} api;
NativeMarkerChannel channel,finishChannel{L".finish"};bool registered=false;
void renderMarker(NativeMarkerChannel&link,MarkerGroundCache&cache,uint64_t now,bool finish){
    link.open(GetCurrentProcessId());link.snapshot();
    auto state=link.last; // retain last fresh position during a brief mutex contention
    link.status(2,true);
    if(!markerFresh(state,now))return;
    if(cache.due(state,now)){float ground=0;bool found=api.ground(state,ground);cache.update(state,now,found,ground);}
    api.marker(state,1,cache.ground+.03f,finish?3.5f:5.0f,finish?3.5f:5.0f,.18f,110,finish);
    api.marker(state,2,cache.ground+2.3f,.65f,.65f,.65f,220,finish);
    link.status(4,true);
}
void scriptMain(){
    MarkerGroundCache startGround,finishGround;
    for(;;){uint64_t now=GetTickCount64();renderMarker(channel,startGround,now,false);renderMarker(finishChannel,finishGround,now,true);api.wait(0);}
}
}
extern "C" __declspec(dllexport) unsigned DistrictMarkerVersion(){return MARKER_VERSION;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved){
    if(reason==DLL_PROCESS_ATTACH){
        DisableThreadLibraryCalls(module);channel.open(GetCurrentProcessId());finishChannel.open(GetCurrentProcessId());
        if(api.load()){channel.status(1);finishChannel.status(1);api.reg(module,scriptMain);registered=true;}else{channel.status(3);finishChannel.status(3);}
    }else if(reason==DLL_PROCESS_DETACH){if(!reserved&&registered&&api.unreg)api.unreg(module);channel.close();finishChannel.close();}
    return TRUE;
}
