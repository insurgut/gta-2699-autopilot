#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
enum class SceneState : uint32_t {Empty=0,Fresh=1,Retained=2,Refreshing=3};
struct SceneSpeed {
    uint64_t beat=0;float x=0,y=0,value=0;
    float update(uint64_t now,float px,float py){
        if(!beat){beat=now;x=px;y=py;return value;}
        if(now>beat&&now-beat>=50){
            float dt=float(now-beat),instant=std::hypot(px-x,py-y)*1000/dt;
            if(std::isfinite(instant))value=std::max(std::min(instant,200.0f),value*std::exp(-dt/1000));
            beat=now;x=px;y=py;
        }
        return value;
    }
};
struct SceneRecovery {
    uint64_t lastSuccess=0,attempts=0,failures=0,recoveries=0;
    uint32_t failureStreak=0;
    uint64_t vehicle=0,ped=0;
    float x=0,y=0,z=0;
    SceneState observe(uint64_t now,bool complete,uint64_t car,uint64_t player,float px,float py,float pz){
        ++attempts;
        if(complete){
            if(failureStreak)++recoveries;
            failureStreak=0;lastSuccess=now;vehicle=car;ped=player;x=px;y=py;z=pz;return SceneState::Fresh;
        }
        ++failures;if(failureStreak<0xffffffffu)++failureStreak;
        return state(now,car,player,px,py,pz);
    }
    SceneState state(uint64_t now,uint64_t car,uint64_t player,float px,float py,float pz)const{
        float dx=px-x,dy=py-y;
        bool usable=lastSuccess&&now>=lastSuccess&&now-lastSuccess<=350&&car==vehicle&&player==ped
            &&std::isfinite(dx)&&std::isfinite(dy)&&std::isfinite(pz)&&dx*dx+dy*dy<=16&&std::abs(pz-z)<=2.5f;
        if(!usable)return SceneState::Refreshing;
        return failureStreak?SceneState::Retained:SceneState::Fresh;
    }
    uint32_t retryDelayMs()const{return failureStreak>100?250u:(failureStreak?50u:0u);}
};
struct PoolIdentity {uint64_t list=0;uint32_t capacity=0;};
inline bool samePool(PoolIdentity a,PoolIdentity b){return a.list==b.list&&a.capacity==b.capacity;}
inline bool readyPools(bool roots,bool vehicles,bool peds,bool objects,bool stable,uint32_t vehicleSlots,uint32_t pedSlots){
    return roots&&vehicles&&peds&&objects&&stable&&vehicleSlots>0&&pedSlots>0;
}
inline float requiredSceneRange(float speed){
    if(!std::isfinite(speed)||speed<0)return 180;
    return std::clamp(25+speed*1.8f+speed*speed/8,40.0f,160.0f)+20;
}
inline bool keptCriticalScene(bool poolsComplete,float firstDroppedDistance2,float range){
    return poolsComplete&&firstDroppedDistance2>range*range;
}
enum class ReaderWatchdog {Keep,ReleaseAndKeep,Exit};
inline ReaderWatchdog readerWatchdog(uint64_t now,uint64_t started,uint64_t beat,bool controllerAlive,bool shutdown){
    if(!controllerAlive||shutdown||(!beat&&now-started>15000))return ReaderWatchdog::Exit;
    return beat&&now>=beat&&now-beat>2500?ReaderWatchdog::ReleaseAndKeep:ReaderWatchdog::Keep;
}
