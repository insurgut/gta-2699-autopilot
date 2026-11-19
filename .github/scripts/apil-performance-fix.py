#!/usr/bin/env python3
"""APIL 0.2.10: cap runaway loops and reduce overlay/diagnostics CPU load.

The read-only telemetry remains responsive (~60-100Hz depending on Windows'
scheduler); driving safety checks and watchdogs stay enabled.
"""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import os

ARCHIVE=Path("source/APIL-source.zip")
def change(src,old,new,label):
    n=src.count(old)
    if n!=1:raise RuntimeError(f"{label}: expected one anchor, found {n}: {old[:125]!r}")
    return src.replace(old,new,1)

with ZipFile(ARCHIVE) as input_zip:
    assert input_zip.testzip() is None, "Corrupt APIL source archive"
    files={i.filename:input_zip.read(i) for i in input_zip.infolist() if not i.is_dir()}

version=files["src/apil_release.hpp"].decode("utf-8-sig")
if 'apilVersion[]="0.2.10"' in version:
    print("APIL 0.2.10 already optimized")
    raise SystemExit(0)
assert 'apilVersion[]="0.2.09"' in version, "Expected APIL 0.2.09"

path="src/external_reader.cpp"
s=files[path].decode("utf-8-sig")
# A busy loop continuously scanned memory, copied ~0.5 MB IPC frames and
# saturated a CPU core despite the obstacle radar's own temporal gating.
s=change(s,
'''        if(finish)break;
        SwitchToThread();''',
'''        if(finish)break;
        // ~60Hz telemetry (at typical Windows timer granularity), not an
        // unbounded spin loop. Input watchdog still runs on every sample.
        Sleep(8);''',path)
files[path]=s.encode("utf-8")

path="src/controller.cpp"
s=files[path].decode("utf-8-sig")
s=change(s,
'''        if(!foreground())MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);else SwitchToThread();''',
'''        // Cap control-loop polling while the game is active. Overlay itself
        // has an independent 40fps gate; no CPU-burning yield spin is needed.
        if(!foreground())MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);
        else Sleep(8);''',path)
s=change(s,
'''        // Offline connection report: no telemetry or running game required.''',
'''        // Offline probes are useful without a game; skip process enumeration
        // and memory-profile reads in periodic reports when already connected.
        if(full||!haveSample){
        // Offline connection report: no telemetry or running game required.''',path)
s=change(s,
'''        SYSTEMTIME generated{};GetLocalTime(&generated);''',
'''        }
        SYSTEMTIME generated{};GetLocalTime(&generated);''',path)
s=change(s,
'''    if(now-last>=1000){last=now;writeDiagnostic(L"report-auto.txt");}}catch(...){}''',
'''    // Periodic full text reports are expensive (large obstacle arrays and
    // filesystem writes). Keep emergency updates frequent, idle writes sparse.
    const uint64_t reportInterval=(mode==Mode::Emergency)?1000u:(mode==Mode::Idle?5000u:2000u);
    if(now-last>=reportInterval){last=now;writeDiagnostic(L"report-auto.txt");}}catch(...){}''',path)
files[path]=s.encode("utf-8")

path="src/overlay.hpp"
s=files[path].decode("utf-8-sig")
s=change(s,
'''            std::sort(coloured.begin(),coloured.end(),[&](Cell a,Cell b){auto d=[&](Cell c){return std::hypot(float(c.x-center.x),float(c.y-center.y));};return d(a)<d(b);});''',
'''            // Squared distance preserves ordering while removing 2*O(n log n)
            // square roots each overlay frame.
            std::sort(coloured.begin(),coloured.end(),[&](Cell a,Cell b){
                const float ax=float(a.x-center.x),ay=float(a.y-center.y);
                const float bx=float(b.x-center.x),by=float(b.y-center.y);
                return ax*ax+ay*ay<bx*bx+by*by;
            });''',path)
s=change(s,
'''if(!clipThreadRadius(p,q,{s.position.x,s.position.y,s.position.z,0},70,a,b))continue;line(''',
'''if((p.x<s.position.x-71&&q.x<s.position.x-71)||(p.x>s.position.x+71&&q.x>s.position.x+71)||
               (p.y<s.position.y-71&&q.y<s.position.y-71)||(p.y>s.position.y+71&&q.y>s.position.y+71)||
               !clipThreadRadius(p,q,{s.position.x,s.position.y,s.position.z,0},70,a,b))continue;line(''',path)
s=change(s,
'''            if(!clipThreadRadius(originalA,originalB,{s.position.x,s.position.y,s.position.z,0},70,from,to)){threadArc+=length;continue;}''',
'''            // Cheap AABB rejection before the expensive segment-circle clip.
            if((originalA.x<s.position.x-71&&originalB.x<s.position.x-71)||
               (originalA.x>s.position.x+71&&originalB.x>s.position.x+71)||
               (originalA.y<s.position.y-71&&originalB.y<s.position.y-71)||
               (originalA.y>s.position.y+71&&originalB.y>s.position.y+71)||
               !clipThreadRadius(originalA,originalB,{s.position.x,s.position.y,s.position.z,0},70,from,to)){
                threadArc+=length;continue;
            }''',path)
files[path]=s.encode("utf-8")

path="src/apil_release.hpp"
s=files[path].decode("utf-8-sig")
s=change(s,'apilVersion[]="0.2.09"','apilVersion[]="0.2.10"',path)
s=change(s,'apilVersionWide[]=L"0.2.09"','apilVersionWide[]=L"0.2.10"',path)
files[path]=s.encode("utf-8")

path="ui/app.js"
s=files[path].decode("utf-8-sig")
s=change(s,"version:'0.2.09'","version:'0.2.10'",path)
files[path]=s.encode("utf-8")

path="ui/index.html"
s=files[path].decode("utf-8-sig")
s=change(s,'<span data-text="version">0.2.09</span>',
           '<span data-text="version">0.2.10</span>',path)
files[path]=s.encode("utf-8")

assert b"Sleep(8)" in files["src/external_reader.cpp"]
assert b"if(full||!haveSample)" in files["src/controller.cpp"]
assert b"Squared distance" in files["src/overlay.hpp"]
tmp=ARCHIVE.with_suffix(".performance.tmp")
with ZipFile(tmp,"w",compression=ZIP_DEFLATED,compresslevel=6) as out:
    for path,body in sorted(files.items()):out.writestr(path,body)
with ZipFile(tmp) as verified:
    assert verified.testzip() is None, "Corrupt resulting archive"
    assert b'0.2.10' in verified.read("src/apil_release.hpp")
os.replace(tmp,ARCHIVE)
print("Optimized APIL 0.2.10 source archive, size",ARCHIVE.stat().st_size)
