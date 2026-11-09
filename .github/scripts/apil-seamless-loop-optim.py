#!/usr/bin/env python3
"""APIL 0.2.08: avoid copying route vectors on every tick during seamless loop checks."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import os

ARCHIVE=Path("source/APIL-source.zip")
def replace(source,old,new,name):
    count=source.count(old)
    if count!=1:raise RuntimeError(f"Expected one anchor in {name}, got {count}: {old[:80]}")
    return source.replace(old,new,1)

with ZipFile(ARCHIVE) as z:
    if z.testzip():raise RuntimeError("Bad source ZIP")
    files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}

version=files["src/apil_release.hpp"].decode()
if 'apilVersion[]="0.2.08"' in version:
    print("APIL 0.2.08 already patched")
else:
    if 'apilVersion[]="0.2.07"' not in version:raise RuntimeError("Expected 0.2.07")
    path="src/seamless_loop.hpp"
    s=files[path].decode()
    s=replace(s,
'''inline bool seamlessLoopMayWrap(const SeamlessLoopPath&lap,size_t segment,Point car){
    if(lap.seam==SIZE_MAX||lap.points.size()<lap.seam+3||segment<lap.seam)return false;
    if(distance(car,lap.points[lap.seam+1])>24.0f)return false;
    return segment>=lap.seam+2 || distance(car,lap.points[lap.seam+1])<=5.0f;
}''',
'''inline bool seamlessLoopMayWrap(const std::vector<Point>&points,size_t seam,size_t segment,Point car){
    if(seam==SIZE_MAX||points.size()<seam+3||segment<seam)return false;
    if(distance(car,points[seam+1])>24.0f)return false;
    return segment>=seam+2 || distance(car,points[seam+1])<=5.0f;
}''',path)
    s=replace(s,
        '    result.seam=result.points.size()-1;',
        '    result.seam=result.points.size()-1;\\n    result.points[result.seam].waitMs=0;\\n    result.points[result.seam].enterCount=0;',path)
    files[path]=s.encode()
    path="src/controller.cpp"
    s=files[path].decode("utf-8-sig")
    s=replace(s,
'''        SeamlessLoopPath observation{activeRoute,seamlessLapEnd};
        if(seamlessLoopMayWrap(observation,segment,point())){''',
'''        if(seamlessLoopMayWrap(activeRoute,seamlessLapEnd,segment,point())){''',path)
    files[path]=s.encode()
    path="src/seamless_loop_tests.cpp"
    s=files[path].decode()
    for old,new in [
      ("seamlessLoopMayWrap(path,0,trimmed.front())","seamlessLoopMayWrap(path.points,path.seam,0,trimmed.front())"),
      ("seamlessLoopMayWrap(path,path.seam,trimmed.front())","seamlessLoopMayWrap(path.points,path.seam,path.seam,trimmed.front())"),
      ("seamlessLoopMayWrap(path,path.seam+2,trimmed[1])","seamlessLoopMayWrap(path.points,path.seam,path.seam+2,trimmed[1])")
    ]:s=replace(s,old,new,path)
    files[path]=s.encode()
    for path in ("src/apil_release.hpp","ui/app.js","ui/index.html"):
        s=files[path].decode()
        if path=="src/apil_release.hpp":
            s=replace(s,'apilVersion[]="0.2.07"','apilVersion[]="0.2.08"',path)
            s=replace(s,'apilVersionWide[]=L"0.2.07"','apilVersionWide[]=L"0.2.08"',path)
        else:
            s=replace(s,"0.2.07","0.2.08",path)
        files[path]=s.encode()
    tmp=ARCHIVE.with_suffix(".optimized.tmp")
    with ZipFile(tmp,"w",compression=ZIP_DEFLATED,compresslevel=6) as out:
        for path,body in sorted(files.items()):out.writestr(path,body)
    with ZipFile(tmp) as verified:
        if verified.testzip():raise RuntimeError("Bad final ZIP")
        assert "0.2.08" in verified.read("src/apil_release.hpp").decode()
        assert "SeamlessLoopPath observation" not in verified.read("src/controller.cpp").decode()
    os.replace(tmp,ARCHIVE)
    print("APIL 0.2.08 seamless loop optimization, archive bytes",ARCHIVE.stat().st_size)
