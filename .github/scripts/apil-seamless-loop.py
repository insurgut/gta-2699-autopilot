#!/usr/bin/env python3
"""APIL 0.2.07: seamless lap handoff and route-color finish blip."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from tempfile import TemporaryDirectory
import os, shutil, subprocess

ARCHIVE=Path("source/APIL-source.zip")
def change(source, old, new, name):
    times=source.count(old)
    if times!=1:
        raise RuntimeError(f"{name}: expected 1 matching anchor, found {times}: {old[:110]!r}")
    return source.replace(old,new,1)

def patch(files):
    files["src/seamless_loop.hpp"]=r'''#pragma once
#include "route.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

// A short connector to the actual recorded start is permitted ONLY when looping,
// ignored-tail is enabled and the delay is zero. The ordinary brakes and all
// collision / object-wait rules remain active outside this specific handoff.
struct SeamlessLoopPath {
    std::vector<Point> points;
    size_t seam=SIZE_MAX; // last point of current lap; following points are preview of next lap
};
inline float seamlessPolylineMeters(const std::vector<Point>&points){
    float total=0;
    for(size_t i=1;i<points.size();++i) total+=distance(points[i-1],points[i]);
    return total;
}
inline SeamlessLoopPath makeSeamlessLoopPath(
    const std::vector<Point>&current,const std::vector<Point>&canonical,
    unsigned ignoreSeconds,unsigned pauseSeconds,bool repeat,bool reverse){
    SeamlessLoopPath result{current,SIZE_MAX};
    if(!repeat||!ignoreSeconds||pauseSeconds||reverse||current.size()<12||canonical.size()<12)return result;
    if(current.size()>100000||canonical.size()>100000)return result;
    if(seamlessPolylineMeters(canonical)<40.0f)return result;
    if(distance(current.back(),canonical.front())>15.0f)return result;
    if(std::fabs(current.back().z-canonical.front().z)>2.5f)return result;
    if(current.back().direction!=1||canonical.front().direction!=1||canonical[1].direction!=1)return result;
    // Avoid a severe instant reversal at the lap joint.
    const Point&prev=current[current.size()-2];
    const Point&last=current.back();
    const Point&first=canonical.front();
    const Point&next=canonical[1];
    const float ax=last.x-prev.x,ay=last.y-prev.y,bx=next.x-first.x,by=next.y-first.y;
    const float an=std::hypot(ax,ay),bn=std::hypot(bx,by);
    if(an>0.1f&&bn>0.1f&&(ax*bx+ay*by)/(an*bn)<-0.25f)return result;
    result.seam=result.points.size()-1;
    // Append a look-ahead tail. The steering/speed controller otherwise brakes
    // for its final waypoint even though this is actually a lap boundary.
    float preview=0;
    Point prior=current.back();
    unsigned appended=0;
    for(int cycle=0;cycle<20 && preview<900.0f && appended<14000;++cycle){
        for(size_t i=0;i<canonical.size() && preview<900.0f && appended<14000;++i){
            Point p=canonical[i];
            // Virtual preview must never generate fake Enter presses or timed stops.
            p.waitMs=0;p.enterCount=0;
            if(cycle==0 && i==0 && distance(prior,p)<0.05f)continue;
            preview+=distance(prior,p);
            result.points.push_back(p);
            prior=p;
            ++appended;
        }
    }
    if(preview<90.0f){
        result.points=current;
        result.seam=SIZE_MAX;
    }
    return result;
}
inline bool seamlessLoopMayWrap(const SeamlessLoopPath&lap,size_t segment,Point car){
    if(lap.seam==SIZE_MAX||lap.points.size()<lap.seam+3||segment<lap.seam)return false;
    if(distance(car,lap.points[lap.seam+1])>24.0f)return false;
    return segment>=lap.seam+2 || distance(car,lap.points[lap.seam+1])<=5.0f;
}
inline size_t seamlessResumeSegment(size_t segment,size_t seam){
    if(segment<=seam+1)return 0;
    return segment-seam-1;
}
'''.encode()
    files["src/seamless_loop_tests.cpp"]=r'''#include "seamless_loop.hpp"
#include "driving.hpp"
#include <cassert>
#include <cmath>
#include <vector>
int main(){
    std::vector<Point> circle;
    for(int i=0;i<=120;++i){
        const float theta=6.28318530718f*i/120.0f;
        Point p{25*std::cos(theta),25*std::sin(theta),0,4};
        p.direction=1;circle.push_back(p);
    }
    const auto trimmed=trimEndpointTail(circle,2);
    assert(distance(trimmed.back(),trimmed.front())<15.0f);
    auto path=makeSeamlessLoopPath(trimmed,trimmed,2,0,true,false);
    assert(path.seam==trimmed.size()-1);
    assert(path.points.size()>trimmed.size());
    assert(seamlessPolylineMeters(path.points)>900);
    assert(!seamlessLoopMayWrap(path,0,trimmed.front()));
    assert(seamlessLoopMayWrap(path,path.seam,trimmed.front()));
    assert(seamlessLoopMayWrap(path,path.seam+2,trimmed[1]));
    assert(seamlessResumeSegment(path.seam+3,path.seam)==2);
    assert(makeSeamlessLoopPath(trimmed,trimmed,2,3,true,false).seam==SIZE_MAX);
    assert(makeSeamlessLoopPath(trimmed,trimmed,0,0,true,false).seam==SIZE_MAX);
    assert(makeSeamlessLoopPath(trimmed,trimmed,2,0,false,false).seam==SIZE_MAX);
    assert(makeSeamlessLoopPath(trimmed,trimmed,2,0,true,true).seam==SIZE_MAX);
    auto far=trimmed;far.front().x+=300;
    assert(makeSeamlessLoopPath(trimmed,far,2,0,true,false).seam==SIZE_MAX);
    auto reverse=trimmed;reverse[0].direction=-1;
    assert(makeSeamlessLoopPath(trimmed,reverse,2,0,true,false).seam==SIZE_MAX);
    // The controller must no longer see the cut point as its final brake point.
    const auto pos=trimmed[trimmed.size()-3];
    auto out=driveOutput(path.points,path.seam-4,pos,
                         0.8f,-0.6f,7.0f,12.0f,SIZE_MAX,0,true);
    assert(!out.arrived);
    assert(out.targetSpeed>1.0f);
}
'''.encode()
    # Connect both regular and join-start initial playback to the same canonical loop.
    path="src/controller.cpp"
    src=files[path].decode("utf-8-sig")
    src=change(src,'#include "endpoint_settings.hpp"',
               '#include "endpoint_settings.hpp"\n#include "seamless_loop.hpp"',path)
    src=change(src,'FinishHold finishHold;uint64_t endpointRestartAt=0;',
'''FinishHold finishHold;uint64_t endpointRestartAt=0;
size_t seamlessLapEnd=SIZE_MAX,seamlessCanonicalEnd=SIZE_MAX;
std::vector<Point> seamlessCanonicalRoute;''',path)
    src=change(src,'endpointRestartAt=0;trafficYield.reset();',
               'endpointRestartAt=0;seamlessLapEnd=seamlessCanonicalEnd=SIZE_MAX;seamlessCanonicalRoute.clear();trafficYield.reset();',path)
    src=change(src,
'''    activeRoute=initial?*initial:playbackRoute;
    if(!gpsDriving&&selectedRoute<routeCatalog.entries.size())
        activeRoute=trimEndpointTail(activeRoute,routeCatalog.entries[selectedRoute].endpoints.ignoreFinalSeconds);
    routeActions.reset();segment=0;pendingStop=nextStop(activeRoute,1);''',
'''    activeRoute=initial?*initial:playbackRoute;
    seamlessLapEnd=seamlessCanonicalEnd=SIZE_MAX;
    seamlessCanonicalRoute.clear();
    if(!gpsDriving&&selectedRoute<routeCatalog.entries.size()){
        const auto config=routeCatalog.entries[selectedRoute].endpoints;
        activeRoute=trimEndpointTail(activeRoute,config.ignoreFinalSeconds);
        if(!joinStaging){
            auto canonical=trimEndpointTail(playbackRoute,config.ignoreFinalSeconds);
            auto plan=makeSeamlessLoopPath(activeRoute,canonical,config.ignoreFinalSeconds,
                                           config.restartDelaySeconds,loopEnabled,resumeReverse);
            if(plan.seam!=SIZE_MAX){
                seamlessLapEnd=plan.seam;
                activeRoute=std::move(plan.points);
                auto full=makeSeamlessLoopPath(canonical,canonical,config.ignoreFinalSeconds,
                                               config.restartDelaySeconds,loopEnabled,resumeReverse);
                if(full.seam!=SIZE_MAX){
                    seamlessCanonicalEnd=full.seam;
                    seamlessCanonicalRoute=std::move(full.points);
                }else seamlessLapEnd=SIZE_MAX;
            }
        }
    }
    routeActions.reset();segment=0;pendingStop=nextStop(activeRoute,1);''',path)
    # Avoid a stale virtual seam if a fallback join requires a new path.
    src=change(src,'    requireNearbyStart=nearbyStart;',
'''    requireNearbyStart=nearbyStart;
    seamlessLapEnd=seamlessCanonicalEnd=SIZE_MAX;seamlessCanonicalRoute.clear();''',path)
    anchor='    RoutePick selected;bool selectedReverse=false;'
    insert='''    // Seamless lap handoff: no Brake, Parked, handbrake or key release.
    // Follow the short connector and then continue from the original start.
    if(valid&&mode==Mode::Drive&&seamlessLapEnd!=SIZE_MAX&&
       seamlessCanonicalEnd!=SIZE_MAX&&!enterQueued&&!enterHeld&&!forwardDetour.active()){
        SeamlessLoopPath observation{activeRoute,seamlessLapEnd};
        if(seamlessLoopMayWrap(observation,segment,point())){
            size_t restart=seamlessResumeSegment(segment,seamlessLapEnd);
            activeRoute=seamlessCanonicalRoute;
            seamlessLapEnd=seamlessCanonicalEnd;
            segment=std::min(restart,activeRoute.size()-2);
            routeActions.reset();
            routeActions.cursor=segment;
            pendingStop=nextStop(activeRoute,segment+1);
            detour.clear();
            waitCycle.reset();
            lastMotion=now;motionOrigin=point();
            if(completedLaps<UINT64_MAX)++completedLaps;
            WritePrivateProfileStringW(L"statistics",L"laps",
                                       std::to_wstring(completedLaps).c_str(),settingsFile.c_str());
            message=L"Следующий круг: проезд без финишной остановки.";
            appendEvent(message.c_str());
        }
    }
'''
    src=change(src,anchor,insert+anchor,path)
    files[path]=src.encode("utf-8")
    # Recolor the existing single flag with the same palette as wait-object blips.
    path="ui/route-editor.js"
    src=files[path].decode("utf-8-sig")
    src=change(src,'endpointButton.innerHTML=\'<img src="map/finish-blip.png" alt="">\';',
'''endpointButton.innerHTML='<span class="endpoint-blip-outline" aria-hidden="true"><span class="endpoint-blip-symbol"></span></span>';''',path)
    src=change(src,'<div class="mini-heading"><img class="endpoint-title-icon" src="map/finish-blip.png" alt=""><div>',
'''<div class="mini-heading"><span class="endpoint-title-icon endpoint-blip-outline" aria-hidden="true"><span class="endpoint-blip-symbol"></span></span><div>''',path)
    src=change(src,'  function updateEndpointVisual(){',
'''  function updateEndpointVisual(){
      endpointPopup.querySelector('.endpoint-title-icon').style.setProperty('--blip-color',getState().routeColor||'#FFFFFF');''',path)
    src=change(src,
'''      const state=getState(),points=state.path;
      if(!state.selectedRoute''',
'''      const state=getState(),points=state.path;
      endpointButton.style.setProperty('--blip-color',state.routeColor||'#FFFFFF');
      if(!state.selectedRoute''',path)
    files[path]=src.encode("utf-8")
    files["ui/map/finish-blip-mask.svg"]=r'''<svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 24 24"><path fill="#fff" d="M2 1h3v22H2zm3 1h5v5H5zm10 1h5v5h-5zm-5 5h5v5h-5zm-5 5h5v5H5zm10 0h5v5h-5z"/></svg>
'''.encode()
    path="ui/css/district.css"
    css=files[path].decode("utf-8-sig")
    css+="""
/* 0.2.07: identical route-color tint and black silhouette as object wait clock blips.
   Transparent hit area only: no tile, circle, fill, shadow or background. */
.endpoint-blip{--blip-color:#fff;pointer-events:auto!important}
.endpoint-blip-outline{display:block;position:relative;width:32px;height:32px;
  flex:none;pointer-events:none;
  filter:drop-shadow(1px 0 0 #000) drop-shadow(-1px 0 0 #000)
         drop-shadow(0 1px 0 #000) drop-shadow(0 -1px 0 #000)}
.endpoint-blip-symbol{display:block;width:32px;height:32px;pointer-events:none;
  background-color:var(--blip-color,#fff);
  -webkit-mask:url('../map/finish-blip-mask.svg') center/contain no-repeat;
  mask:url('../map/finish-blip-mask.svg') center/contain no-repeat;}
.endpoint-title-icon.endpoint-blip-outline{display:inline-block;background:transparent!important;
  padding:0!important;border:0!important;border-radius:0!important;}
.endpoint-title-icon .endpoint-blip-symbol{background-color:var(--blip-color,#fff)}
"""
    files[path]=css.encode("utf-8")
    path="ui/app.js"
    src=files[path].decode("utf-8-sig")
    src=change(src,"version:'0.2.06'","version:'0.2.07'",path)
    files[path]=src.encode("utf-8")
    path="ui/index.html"
    src=files[path].decode("utf-8-sig")
    src=change(src,'<span data-text="version">0.2.06</span>',
               '<span data-text="version">0.2.07</span>',path)
    files[path]=src.encode("utf-8")
    path="src/apil_release.hpp"
    src=files[path].decode("utf-8-sig")
    src=change(src,'apilVersion[]="0.2.06"','apilVersion[]="0.2.07"',path)
    src=change(src,'apilVersionWide[]=L"0.2.06"','apilVersionWide[]=L"0.2.07"',path)
    files[path]=src.encode("utf-8")
    with TemporaryDirectory() as td:
        root=Path(td)
        for js in ('ui/route-editor.js','ui/app.js'):
            target=root/Path(js).name
            target.write_bytes(files[js])
            if shutil.which("node"):subprocess.run(["node","--check",str(target)],check=True)
    return files

def main():
    with ZipFile(ARCHIVE) as z:
        if z.testzip() is not None:raise RuntimeError("Corrupt input ZIP")
        files={entry.filename:z.read(entry) for entry in z.infolist() if not entry.is_dir()}
    version=files["src/apil_release.hpp"].decode("utf-8-sig")
    if 'apilVersion[]="0.2.07"' in version:
        print("Already 0.2.07")
        return
    if 'apilVersion[]="0.2.06"' not in version:
        raise RuntimeError("Expected APIL v0.2.06 archive")
    files=patch(files)
    temp=ARCHIVE.with_suffix(".seamless.tmp")
    with ZipFile(temp,"w",compression=ZIP_DEFLATED,compresslevel=6) as out:
        for name,body in sorted(files.items()):out.writestr(name,body)
    with ZipFile(temp) as verified:
        if verified.testzip() is not None:raise RuntimeError("Corrupt output ZIP")
        assert "0.2.07" in verified.read("src/apil_release.hpp").decode()
        assert b"seamlessLoopMayWrap" in verified.read("src/controller.cpp")
    os.replace(temp,ARCHIVE)
    print("Patched APIL 0.2.07, archive size:",ARCHIVE.stat().st_size)

if __name__=="__main__":main()
