#!/usr/bin/env python3
"""APIL 0.2.11 per-route-edge suppression of recorded braking."""
from zipfile import ZipFile,ZIP_DEFLATED
from pathlib import Path
from tempfile import TemporaryDirectory
import os,subprocess,shutil
archive=Path("source/APIL-source.zip")
with ZipFile(archive) as z:
    assert z.testzip() is None
    f={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
if b'0.2.11' in f['src/apil_release.hpp']:
    print('APIL 0.2.11 already applied')
    raise SystemExit(0)
assert b'apilVersion[]="0.2.10"' in f['src/apil_release.hpp']
def change(path,old,new):
    s=f[path].decode('utf-8-sig')
    count=s.count(old)
    if count!=1:raise RuntimeError(path+' anchor '+str(count)+': '+old[:130])
    f[path]=s.replace(old,new,1).encode()
# A route uses original v7/v8 when no section has the new flag; v9 carries a flag per point.
change('src/route.hpp','bool followRecordedSpeed=false; };','bool followRecordedSpeed=false; bool noRecordedBraking=false; };')
change('src/route.hpp','version<1||version>8','version<1||version>9')
change('src/route.hpp','p.ignoreObstacles=ignore!=0;}if(!valid(p))return false;',
       'p.ignoreObstacles=ignore!=0;}if(version>=9){unsigned v=0;if(!(f>>v)||v>1)return false;p.noRecordedBraking=v!=0;}if(!valid(p))return false;')
change('src/route.hpp','if(!(f>>tag>>enabled)||tag!="PACE"||enabled>1)return false;','if(!(f>>tag>>enabled)||tag!="PACE"||enabled>(version>=9?2u:1u))return false;')
change('src/route.hpp','timing.pace=enabled?RecordedPace::Enabled:RecordedPace::Disabled;',
       'timing.pace=enabled==2?RecordedPace::Unavailable:(enabled?RecordedPace::Enabled:RecordedPace::Disabled);')
change('src/route.hpp','const bool pace=input&&input->pace!=RecordedPace::Unavailable;',
       'const bool pace=input&&input->pace!=RecordedPace::Unavailable;\n    const bool advanced=std::any_of(r.begin(),r.end(),[](const Point&p){return p.noRecordedBraking;});')
change('src/route.hpp','(pace?8:7)', '(advanced?9:(pace?8:7))')
change('src/route.hpp',"<<p.speedLimit<<' '<<p.ignoreObstacles<<'\\n';",
       "<<p.speedLimit<<' '<<p.ignoreObstacles<<(advanced?(p.noRecordedBraking?\" 1\":\" 0\"):\"\")<<'\\n';")
change('src/route.hpp','if(pace)f<<"PACE "<<(input->pace==RecordedPace::Enabled)',
       'if(pace||advanced)f<<"PACE "<<(pace?(input->pace==RecordedPace::Enabled?1:0):2)')
# Ignore only recorded speed dips and manually recorded stationary waits.
change('src/route_sections.hpp','return a.followRecordedSpeed&&b.followRecordedSpeed&&b.speedLimit<=0;',
       'return a.followRecordedSpeed&&b.followRecordedSpeed&&b.speedLimit<=0&&!b.noRecordedBraking;')
change('src/route_sections.hpp','if(p.speedLimit<=0&&!p.ignoreObstacles)return rgb;',
       'if(p.speedLimit<=0&&!p.ignoreObstacles&&!p.noRecordedBraking)return rgb;')
change('src/stops.hpp','if(r[i].waitMs>=500||(i+1<r.size()&&r[i+1].direction!=r[i].direction))return i;',
       'if((r[i].waitMs>=500&&!r[i].noRecordedBraking)||(i+1<r.size()&&r[i+1].direction!=r[i].direction))return i;')
change('src/stops.hpp','out[i].ignoreObstacles=r[r.size()-i].ignoreObstacles;',
       'out[i].ignoreObstacles=r[r.size()-i].ignoreObstacles;out[i].noRecordedBraking=r[r.size()-i].noRecordedBraking;')
change('src/route_catalog.hpp','bool setRecordedPace(size_t index,bool enabled){',
'''  // This setter retains the existing route-wide recorded-pace setting.
  bool setNoRecordedBraking(size_t index,size_t from,size_t to,bool enabled){
    if(index>=entries.size()||from>to||to+1>=entries[index].points.size())return false;
    auto edited=entries[index].points;
    for(size_t i=from+1;i<=to+1;++i)edited[i].noRecordedBraking=enabled;
    if(!writeSettings(index,edited))return false;
    entries[index].points=std::move(edited);
    return true;
  }
  bool setRecordedPace(size_t index,bool enabled){''')
change('src/controller.cpp','<<L","<<(p.ignoreObstacles?1:0)<<L"]";',
       '<<L","<<(p.ignoreObstacles?1:0)<<L","<<(p.noRecordedBraking?1:0)<<L"]";')
change('src/controller.cpp','        }else if(parts.size()==7&&parts[0]==L"section"){',
'''        }else if(parts.size()==7&&parts[0]==L"section"&&parts[1]==L"braking"){
            if(mode!=Mode::Idle||overlay.selecting()||selectedRoute>=routeCatalog.entries.size()||
               parts[2]!=std::wstring(routeCatalog.entries[selectedRoute].id.begin(),routeCatalog.entries[selectedRoute].id.end())){
                message=L"Сначала останови проезд и выбери маршрут.";updateWebState();return;
            }
            size_t a=0,b=0;float ignoredSpeed=0;bool ignoredFlag=false;
            if((parts[5]!=L"0"&&parts[5]!=L"1")||
               !parseSectionEdit(parts[3],parts[4],L"0",L"0",parts[6],route.size(),
                                 a,b,ignoredSpeed,ignoredFlag)){
                message=L"Недопустимые параметры участка.";updateWebState();return;
            }
            const bool enabled=parts[5]==L"1";
            if(!routeCatalog.setNoRecordedBraking(selectedRoute,a,b,enabled)){
                message=L"Не удалось сохранить режим без торможения.";updateWebState();return;
            }
            route=routeCatalog.entries[selectedRoute].points;
            applyRecordedPace();
            ++routeVisualRevision;
            message=enabled?L"Без торможения включено на участке.":L"Без торможения выключено на участке.";
        }else if(parts.size()==7&&parts[0]==L"section"){''')
# Reuse the exact toggle component already used in the settings tab.
D='$'+'{'
markup='<div class="section-no-brake-row">'+D+"toggle('sectionNoBrake','Без торможения','Не учитывать записанные вручную торможения и остановки.').replace('data-toggle=\"sectionNoBrake\"','data-section-toggle=\"noBraking\"')}"+'</div>'
change('ui/app.js','</select></label></div><div class="action-row">',
       '</select></label></div>'+markup+'<div class="action-row">')
change('ui/app.js',"version:'0.2.10'","version:'0.2.11'")
# Add the confirmation overlay to the EXISTING popup, not a separate dialog.
inject=r'''
  const noBrakeSwitch=$('section-editor').querySelector('[data-section-toggle="noBraking"]');
  const brakingConfirm=document.createElement('div');
  brakingConfirm.className='section-braking-confirm';
  brakingConfirm.hidden=true;
  brakingConfirm.setAttribute('role','alertdialog');
  brakingConfirm.setAttribute('aria-modal','true');
  brakingConfirm.setAttribute('aria-label','Подтверждение режима без торможения');
  brakingConfirm.innerHTML='<div class="section-braking-confirm-content"><strong>Без торможения</strong><p>На этом участке будут игнорироваться даже торможения и остановки, которые ты раньше записал вручную. Торможение перед препятствиями и опасными поворотами остаётся активным.</p><button type="button" class="button button-primary" id="braking-confirm-ok">Ок</button></div>';
  popup.append(brakingConfirm);
  let brakingSaving=false;
  const displayBraking=value=>noBrakeSwitch.setAttribute('aria-checked',String(Boolean(value)));
  function dismissBraking(){brakingConfirm.hidden=true;brakingConfirm.classList.remove('revealed');}
  function saveBraking(enabled){
      const s=getState();
      if(brakingSaving||!selection||!s.selectedRoute||s.playing||s.recording)return;
      brakingSaving=true;
      send('section|braking|'+s.selectedRoute+'|'+selection.from+'|'+selection.to+'|'+(enabled?'1':'0')+'|'+s.path.length);
      displayBraking(enabled);
      $('section-feedback').textContent='Сохранение режима...';
      $('section-feedback').dataset.pending='braking';
  }
  noBrakeSwitch.onclick=()=>{
      if(brakingSaving||!selection||getState().playing||getState().recording)return;
      if(noBrakeSwitch.getAttribute('aria-checked')==='true'){saveBraking(false);return;}
      const thumb=noBrakeSwitch.querySelector('.switch-thumb')||noBrakeSwitch;
      const a=thumb.getBoundingClientRect(),b=popup.getBoundingClientRect();
      brakingConfirm.style.setProperty('--cx',(a.left+a.width/2-b.left)+'px');
      brakingConfirm.style.setProperty('--cy',(a.top+a.height/2-b.top)+'px');
      brakingConfirm.hidden=false;
      requestAnimationFrame(()=>requestAnimationFrame(()=>{
          if(!brakingConfirm.hidden)brakingConfirm.classList.add('revealed');
      }));
  };
  brakingConfirm.querySelector('#braking-confirm-ok').onclick=()=>{
      dismissBraking();saveBraking(true);
  };
'''
change('ui/route-editor.js','popup.append(close,editor);document.body.append(popup);',
       'popup.append(close,editor);document.body.append(popup);'+inject)
change('ui/route-editor.js',"$('section-obstacles').value=sameIgnore?(first[3]?'1':'0'):'mixed';$('section-feedback').textContent='';",
       "$('section-obstacles').value=sameIgnore?(first[3]?'1':'0'):'mixed';displayBraking(values.every(q=>Boolean(q[4])===Boolean(first[4]))&&Boolean(first[4]));noBrakeSwitch.disabled=disabled;$('section-feedback').textContent='';")
change('ui/route-editor.js','if(key!==next){key=next;selection=null;',
       'if(key!==next){dismissBraking();brakingSaving=false;key=next;selection=null;')
change('ui/route-editor.js',"if(state.message==='Параметры участка сохранены.'){",
       "if($('section-feedback').dataset.pending==='braking'&&(state.message.startsWith('Без торможения ')||state.message.startsWith('Не удалось'))){brakingSaving=false;showSelection();$('section-feedback').textContent=state.message;$('section-feedback').dataset.pending='0';}\n   if(state.message==='Параметры участка сохранены.'){")
change('ui/route-editor.js','function openPopup(){if(!selection||getState().playing||getState().recording)return;popup.hidden=false;',
       'function openPopup(){if(!selection||getState().playing||getState().recording)return;dismissBraking();popup.hidden=false;')
change('ui/route-editor.js','if(!(p[i][2]>0)&&!p[i][3]){++i;continue;}',
       'if(!(p[i][2]>0)&&!p[i][3]&&!p[i][4]){++i;continue;}')
change('ui/route-editor.js','Boolean(p[j+1][3])===Boolean(p[i][3]))++j;',
       'Boolean(p[j+1][3])===Boolean(p[i][3])&&Boolean(p[j+1][4])===Boolean(p[i][4]))++j;')
anchor=" · игнорировать препятствия':''}"
change('ui/route-editor.js',anchor,anchor+D+"p[i][4]?' · без торможения':''}")
change('ui/route-editor.js',"if(event.key!=='Escape')return;",
       "if(event.key!=='Escape')return; if(!brakingConfirm.hidden){dismissBraking();return;}")
f['ui/css/district.css']+=r'''
/* APIL 0.2.11: circular reveal inside the current section settings window. */
#section-popup{isolation:isolate;overflow-x:hidden}
#section-popup .section-no-brake-row{margin:6px 0 14px;padding-top:9px;border-top:1px solid var(--border)}
#section-popup .section-no-brake-row .toggle-row{width:100%;padding:7px 2px;text-align:left}
#section-popup .section-braking-confirm{position:absolute;inset:0;z-index:5;display:flex;
  align-items:center;justify-content:center;padding:22px;background:var(--bg,#292929);
  clip-path:circle(0px at var(--cx,80%) var(--cy,60%));
  transition:clip-path .4s cubic-bezier(.16,1,.3,1)}
#section-popup .section-braking-confirm[hidden]{display:none!important}
#section-popup .section-braking-confirm.revealed{clip-path:circle(160% at var(--cx,80%) var(--cy,60%))}
.section-braking-confirm-content{display:flex;flex-direction:column;gap:14px;max-width:350px}
.section-braking-confirm-content strong{font-size:17px}
.section-braking-confirm-content p{font-size:12px;line-height:1.55;color:var(--description)}
.section-braking-confirm-content button{align-self:flex-end;min-width:75px;justify-content:center}
@media(prefers-reduced-motion:reduce){#section-popup .section-braking-confirm{transition:none}}
'''.encode()
for path in ('src/apil_release.hpp','ui/index.html'):
    if path.startswith('src/'):
        change(path,'apilVersion[]="0.2.10"','apilVersion[]="0.2.11"')
        change(path,'apilVersionWide[]=L"0.2.10"','apilVersionWide[]=L"0.2.11"')
    else:change(path,'<span data-text="version">0.2.10</span>','<span data-text="version">0.2.11</span>')
# Regression tests: recorded-speed dip, stop, and disk roundtrip.
f['src/no_recorded_braking_tests.cpp']=r'''#include "route.hpp"
#include "route_sections.hpp"
#include "stops.hpp"
#include <cassert>
#include <filesystem>
#include <vector>
int main(){
 std::vector<Point> r{{0,0,0,10},{10,0,0,0},{20,0,0,10}};
 for(auto& p:r)p.followRecordedSpeed=true;
 r[1].waitMs=2000;
 assert(nextStop(r,1)==1&&routeSpeedAt(r[0],r[1],1,20)<2);
 r[1].noRecordedBraking=true;
 assert(nextStop(r,1)==SIZE_MAX&&routeSpeedAt(r[0],r[1],1,20)==20);
 auto tmp=std::filesystem::temp_directory_path()/"apil-no-brake-test.txt";
 assert(writeRoute(tmp,r));
 RouteInputs in;std::vector<Point> back;
 assert(loadRoute(tmp,back,&in));std::filesystem::remove(tmp);
 assert(back[1].noRecordedBraking&&back[1].waitMs==2000);
 assert(in.pace==RecordedPace::Unavailable);
 back[1].noRecordedBraking=false;
 assert(nextStop(back,1)==1);
}
'''.encode()
with TemporaryDirectory() as folder:
    for name in ('ui/app.js','ui/route-editor.js'):
        p=Path(folder)/Path(name).name;p.write_bytes(f[name])
        if shutil.which('node'):subprocess.run(['node','--check',str(p)],check=True)
tmp=archive.with_suffix('.no-braking.tmp')
with ZipFile(tmp,'w',compression=ZIP_DEFLATED,compresslevel=6) as out:
    for name,b in sorted(f.items()):out.writestr(name,b)
with ZipFile(tmp) as z:assert z.testzip() is None and b'0.2.11' in z.read('src/apil_release.hpp')
os.replace(tmp,archive)
print('APIL 0.2.11 section no-recorded-braking source updated')
