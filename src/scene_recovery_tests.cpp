#include "scene_recovery.hpp"
#include "radar_pause.hpp"
#include <cassert>
#include <limits>
#include <iostream>
int main(){
    SceneSpeed fast,slow;
    for(uint64_t t=1000;t<=5000;t++)fast.update(t,float(t-1000)*0.015f,0);
    for(uint64_t t=1000;t<=5000;t+=16)slow.update(t,float(t-1000)*0.015f,0);
    assert(std::abs(fast.value-15)<0.01f&&std::abs(slow.value-15)<0.01f);
    SceneRecovery p;
    assert(p.observe(100,false,1,2,0,0,0)==SceneState::Refreshing);
    assert(p.observe(200,true,1,2,0,0,0)==SceneState::Fresh);
    assert(p.observe(300,false,1,2,0,0,0)==SceneState::Retained);
    assert(p.lastSuccess==200); // failures must not refresh old obstacle timestamps
    assert(p.state(550,1,2,4,0,0)==SceneState::Retained);
    assert(p.state(551,1,2,0,0,0)==SceneState::Refreshing);
    assert(p.state(300,1,2,4.01f,0,0)==SceneState::Refreshing);
    assert(p.state(300,9,2,0,0,0)==SceneState::Refreshing);
    assert(p.state(300,1,9,0,0,0)==SceneState::Refreshing);
    assert(p.state(199,1,2,0,0,0)==SceneState::Refreshing);
    auto far=std::numeric_limits<float>::infinity();
    assert(keptCriticalScene(true,far,180));
    assert(keptCriticalScene(true,200*200,60));
    assert(!keptCriticalScene(true,30*30,60));
    assert(!keptCriticalScene(false,far,60));
    assert(samePool({123,256},{123,256}));
    assert(!samePool({123,256},{124,256}));
    assert(!samePool({123,256},{123,128}));
    assert(readerWatchdog(30000,0,1000,true,false)==ReaderWatchdog::ReleaseAndKeep);
    assert(readerWatchdog(72ull*3600000,0,1000,true,false)==ReaderWatchdog::ReleaseAndKeep);
    assert(readerWatchdog(30000,0,29900,true,false)==ReaderWatchdog::Keep);
    assert(readerWatchdog(30000,0,29900,false,false)==ReaderWatchdog::Exit);
    assert(readerWatchdog(30000,0,29900,true,true)==ReaderWatchdog::Exit);
    assert(readerWatchdog(16000,0,0,true,false)==ReaderWatchdog::Exit);
    SceneRecovery days;FreshSceneGate gate;
    uint64_t resumed=0,paused=0;bool waiting=false;uint64_t total=0;
    const uint64_t hours72=72ull*60*60*1000;
    for(uint64_t now=1000;now<1000+hours72;now+=100){
        uint64_t cycle=(now-1000)%600000;
        bool good=!(cycle>=10000&&cycle<10300)&&!(cycle>=300000&&cycle<360000);
        auto state=days.observe(now,good,1,2,0,0,0);++total;
        bool usable=state==SceneState::Fresh||state==SceneState::Retained;
        if(!usable&&!waiting){waiting=true;gate.reset();++paused;}
        if(waiting&&gate.update(now,days.lastSuccess,usable)){waiting=false;++resumed;}
        assert(days.failureStreak?(days.retryDelayMs()>=50&&days.retryDelayMs()<=250):days.retryDelayMs()==0);
        if(!good&&cycle>=300400&&cycle<360000)assert(!usable);
    }
    assert(days.attempts==total&&days.failures>0&&days.recoveries>0);
    assert(paused==432&&resumed==432&&!waiting);
    const uint64_t oldUptime=1ull<<35;
    assert(days.observe(oldUptime,true,1,2,0,0,0)==SceneState::Fresh);
    assert(days.observe(oldUptime+100,false,1,2,0,0,0)==SceneState::Retained);
    assert(days.state(oldUptime+1000,1,2,0,0,0)==SceneState::Refreshing);
    std::cout<<"72 virtual hours passed: "<<total<<" scans, "<<paused<<" one-minute outages and automatic recoveries; retained frames expire honestly.\n";
}
