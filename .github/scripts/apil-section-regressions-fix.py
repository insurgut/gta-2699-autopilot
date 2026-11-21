#!/usr/bin/env python3
"""APIL 0.2.12: atomic section removal and recorded-braking boundary fixes."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from tempfile import TemporaryDirectory
import os,subprocess,shutil
archive=Path("source/APIL-source.zip")
with ZipFile(archive) as z:
    assert z.testzip() is None
    files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
if b'0.2.12' in files['src/apil_release.hpp']:
    print('APIL 0.2.12 already applied')
    raise SystemExit(0)
assert b'apilVersion[]="0.2.11"' in files['src/apil_release.hpp']
def replace(path, before, after):
    source=files[path].decode('utf-8-sig')
    matches=source.count(before)
    if matches!=1:raise RuntimeError(f'{path}: expected 1 anchor, got {matches}: {before[:130]}')
    files[path]=source.replace(before,after,1).encode('utf-8')
replace('src/route_catalog.hpp',
'''  bool setRecordedPace(size_t index,bool enabled){''',
'''  // Clear only selected policies; recorded wait/pace source data is kept.
  bool clearSectionSettings(size_t index,size_t from,size_t to){
    if(index>=entries.size()||from>to||to+1>=entries[index].points.size())return false;
    auto edited=entries[index].points;
    for(size_t i=from+1;i<=to+1;++i){
      edited[i].speedLimit=0;
      edited[i].ignoreObstacles=false;
      edited[i].noRecordedBraking=false;
    }
    if(!writeSettings(index,edited))return false;
    entries[index].points=std::move(edited);
    return true;
  }
  bool setRecordedPace(size_t index,bool enabled){''')
replace('src/controller.cpp',
'''        }else if(parts.size()==7&&parts[0]==L"section"&&parts[1]==L"braking"){''',
'''        }else if(parts.size()==6&&parts[0]==L"section"&&parts[1]==L"clear"){
            if(mode!=Mode::Idle||overlay.selecting()||selectedRoute>=routeCatalog.entries.size()||
               parts[2]!=std::wstring(routeCatalog.entries[selectedRoute].id.begin(),routeCatalog.entries[selectedRoute].id.end())){
                message=L"Сначала останови проезд и выбери маршрут.";updateWebState();return;
            }
            size_t a=0,b=0;float unusedSpeed=0;bool unusedIgnore=false;
            if(!parseSectionEdit(parts[3],parts[4],L"0",L"0",parts[5],route.size(),
                                 a,b,unusedSpeed,unusedIgnore)){
                message=L"Недопустимые параметры участка.";updateWebState();return;
            }
            if(!routeCatalog.clearSectionSettings(selectedRoute,a,b)){
                message=L"Не удалось удалить настройки участка.";updateWebState();return;
            }
            route=routeCatalog.entries[selectedRoute].points;
            applyRecordedPace();
            ++routeVisualRevision;
            message=L"Настройки участка удалены.";
        }else if(parts.size()==7&&parts[0]==L"section"&&parts[1]==L"braking"){''')
replace('ui/route-editor.js',
'''send(`section|${state.selectedRoute}|${from}|${to}|0|0|${p.length}`);''',
'''send(`section|clear|${state.selectedRoute}|${from}|${to}|${p.length}`);''')
replace('ui/route-editor.js',
'''return;}const raw=$('section-speed').value,kmh=reset?0:Number(raw),ignore=reset?'0':$('section-obstacles').value;''',
'''return;}if(reset){send(`section|clear|${s.selectedRoute}|${selection.from}|${selection.to}|${s.path.length}`);$('section-feedback').textContent='Удаление настроек...';$('section-feedback').dataset.pending='1';return;}const raw=$('section-speed').value,kmh=Number(raw),ignore=$('section-obstacles').value;''')
replace('ui/route-editor.js',
'''if(state.message==='Параметры участка сохранены.'){''',
'''if(state.message==='Параметры участка сохранены.'||state.message==='Настройки участка удалены.'){''')
# No early recorded-braking preview from a neighboring unmarked section.
# Explicit custom caps and safety stops still apply.
replace('src/route_sections.hpp',
'''if((j>segment+1||recorded)&&limit<cap){''',
'''if((j>segment+1||recorded)&&limit<cap&&!(b.noRecordedBraking&&recorded)){''')
# Synthetic route rejoin anchors and recovered Enter markers inherit edge policy.
replace('src/route_selection.hpp',
'''v.ignoreObstacles=b.ignoreObstacles;return v;''',
'''v.ignoreObstacles=b.ignoreObstacles;v.noRecordedBraking=b.noRecordedBraking;v.followRecordedSpeed=a.followRecordedSpeed&&b.followRecordedSpeed;return v;''')
replace('src/input_enter_markers.hpp',
'''action.ignoreObstacles=source[std::min(i+1,source.size()-1)].ignoreObstacles;action.waitMs=0;''',
'''action.ignoreObstacles=source[std::min(i+1,source.size()-1)].ignoreObstacles;action.noRecordedBraking=source[std::min(i+1,source.size()-1)].noRecordedBraking;action.waitMs=0;''')
replace('src/apil_release.hpp','apilVersion[]="0.2.11"','apilVersion[]="0.2.12"')
replace('src/apil_release.hpp','apilVersionWide[]=L"0.2.11"','apilVersionWide[]=L"0.2.12"')
replace('ui/app.js',"version:'0.2.11'","version:'0.2.12'")
replace('ui/index.html','<span data-text="version">0.2.11</span>','<span data-text="version">0.2.12</span>')
files['src/section_regressions_tests.cpp']=r'''#include "route_catalog.hpp"
#include "route_selection.hpp"
#include "route_sections.hpp"
#include "stops.hpp"
#include <cassert>
#include <filesystem>
int main(){
  std::vector<Point> r{{0,0,0,30},{10,0,0,30},{20,0,0,0},{30,0,0,30}};
  for(auto& p:r)p.followRecordedSpeed=true;
  r[1].noRecordedBraking=true;
  assert(routeSpeedAheadCap(r,0,{1,0,0,0},30)>=29.99f);
  r[1].noRecordedBraking=false;
  assert(routeSpeedAheadCap(r,0,{1,0,0,0},30)<20);
  r[1].noRecordedBraking=true;
  r[2].speedLimit=8;
  assert(routeSpeedAheadCap(r,0,{1,0,0,0},30)<25);
  r[2].speedLimit=0;
  RoutePick at{0,.5f,0,0,0,true};
  Point anchor=routeAnchor(r,at);
  assert(anchor.noRecordedBraking&&anchor.followRecordedSpeed);
  auto continuation=continueRoute(r,at,false);
  assert(continuation.size()==4&&continuation.front().noRecordedBraking);
  auto dir=std::filesystem::temp_directory_path()/"apil-0.2.12-section-clear-test";
  std::filesystem::remove_all(dir);
  RouteCatalog catalog;catalog.open(dir);
  auto saved=r;saved[1].speedLimit=12;saved[1].ignoreObstacles=true;saved[1].waitMs=2200;
  assert(catalog.add(saved,"Test"));
  assert(catalog.entries[0].points[1].noRecordedBraking);
  assert(catalog.clearSectionSettings(0,0,0));
  const auto& cleared=catalog.entries[0].points[1];
  assert(cleared.speedLimit==0&&!cleared.ignoreObstacles&&!cleared.noRecordedBraking);
  assert(cleared.waitMs==2200);
  assert(!catalog.clearSectionSettings(0,2,3));
  RouteCatalog reopened;reopened.open(dir);
  assert(reopened.entries.size()==1);
  assert(!reopened.entries[0].points[1].noRecordedBraking);
  assert(reopened.entries[0].points[1].waitMs==2200);
  std::filesystem::remove_all(dir);
}
'''.encode('utf-8')
with TemporaryDirectory() as folder:
    for name in ('ui/app.js','ui/route-editor.js'):
        path=Path(folder)/Path(name).name
        path.write_bytes(files[name])
        if shutil.which('node'):subprocess.run(['node','--check',str(path)],check=True)
temporary=archive.with_suffix('.0.2.12.tmp')
with ZipFile(temporary,'w',compression=ZIP_DEFLATED,compresslevel=6) as out:
    for name,content in sorted(files.items()):out.writestr(name,content)
with ZipFile(temporary) as z:
    assert z.testzip() is None
    assert b'0.2.12' in z.read('src/apil_release.hpp')
os.replace(temporary,archive)
print('APIL 0.2.12: section deletion, route joins and recorded-pace boundaries fixed')
