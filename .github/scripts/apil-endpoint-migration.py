#!/usr/bin/env python3
"""One-time APIL 0.2.05 source ZIP migration. Never touches users' stored routes."""
from __future__ import annotations
import base64
import os
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ARCHIVE=Path("source/APIL-source.zip")
VERSION="0.2.05"
def change(path, old, new):
    p=Path(path)
    s=p.read_text(encoding="utf-8-sig")
    count=s.count(old)
    if count!=1:raise RuntimeError(f"{path}: expected 1 anchor, got {count}: {old[:100]!r}")
    p.write_text(s.replace(old,new,1),encoding="utf-8")

def append(path,body):
    p=Path(path)
    with p.open("a",encoding="utf-8") as f:f.write("\n"+body+"\n")

def install():
    Path("src/endpoint_settings.hpp").write_text(r'''#pragma once
#include "route.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Route-local options stored in a small sidecar file, leaving GTA_AUTOPILOT v8 compatible.
struct EndpointSettings {
    unsigned ignoreFinalSeconds=0;
    unsigned restartDelaySeconds=0;
};
inline bool validEndpointSettings(const EndpointSettings& s){
    return s.ignoreFinalSeconds<=60 && s.restartDelaySeconds<=300;
}
// Route recordings only store positions and speeds, not an exact timestamp for each point.
// Estimate the ignored tail by distance / recorded speed, retaining at least two points.
// The original route is never destructively rewritten by this operation.
inline std::vector<Point> trimEndpointTail(const std::vector<Point>& source,unsigned ignoreSeconds){
    auto route=source;
    if(ignoreSeconds==0 || route.size()<3)return route;
    double remaining=double(std::min(ignoreSeconds,60u));
    for(size_t i=route.size()-1;i>1;--i){
        const Point a=route[i-1],b=route[i];
        const double meters=distance(a,b);
        const double velocity=std::clamp((double(a.speed)+double(b.speed))*.5,1.5,90.0);
        const double secs=meters/velocity;
        if(!std::isfinite(secs)||secs<=.0001){route.resize(i);continue;}
        if(remaining>=secs){
            remaining-=secs;
            route.resize(i);
            continue;
        }
        const float t=float(std::clamp(1.0-remaining/secs,0.0,1.0));
        Point end=a;
        end.x=a.x+(b.x-a.x)*t;
        end.y=a.y+(b.y-a.y)*t;
        end.z=a.z+(b.z-a.z)*t;
        end.speed=a.speed+(b.speed-a.speed)*t;
        end.facingX=a.facingX+(b.facingX-a.facingX)*t;
        end.facingY=a.facingY+(b.facingY-a.facingY)*t;
        end.waitMs=0;end.enterCount=0;
        route[i]=end;
        route.resize(i+1);
        return route;
    }
    return route;
}
''',encoding="utf-8")
    Path("src/endpoint_settings_tests.cpp").write_text(r'''#include "endpoint_settings.hpp"
#include <cassert>
int main(){
    Point a{0,0,0,10},b{100,0,0,10},c{200,0,0,10};
    const std::vector<Point> r{a,b,c};
    assert(trimEndpointTail(r,0).size()==3);
    const auto cut=trimEndpointTail(r,5);
    assert(cut.size()==3 && std::abs(cut.back().x-150)<.001);
    assert(trimEndpointTail(r,60).size()==2);
    assert(validEndpointSettings({60,300}));
    assert(!validEndpointSettings({61,0}));
}
''',encoding="utf-8")
    change("src/route_catalog.hpp",'#include "route.hpp"','#include "route.hpp"\n#include "endpoint_settings.hpp"')
    change("src/route_catalog.hpp",'RouteInputs input;};','RouteInputs input;EndpointSettings endpoints{};};')
    change("src/route_catalog.hpp",'entries.push_back(std::move(r));}std::sort(entries.begin(),entries.end()',
'''{std::ifstream fin(dir/(r.id+".finish"));unsigned skip=0,delay=0;
if(fin>>skip>>delay){EndpointSettings candidate{skip,delay};if(validEndpointSettings(candidate))r.endpoints=candidate;}}
entries.push_back(std::move(r));}std::sort(entries.begin(),entries.end()''')
    change("src/route_catalog.hpp",'{".meta",".txt.observed",".txt.waiting"}',
           '{".meta",".txt.observed",".txt.waiting",".finish"}')
    change("src/route_catalog.hpp",' bool rename(size_t index,const std::string&name)',
''' bool setEndpointSettings(size_t index,EndpointSettings value){
    if(index>=entries.size()||!validEndpointSettings(value))return false;
    auto target=dir/(entries[index].id+".finish"),temp=target;
    temp+=".tmp";
    std::ofstream f(temp,std::ios::trunc);
    if(!(f<<value.ignoreFinalSeconds<<' '<<value.restartDelaySeconds<<'\\n'))return false;
    f.close();
    if(!f||!replaceRouteFile(temp,target))return false;
    entries[index].endpoints=value;
    return true;
 }
 bool rename(size_t index,const std::string&name)''')
    change("src/controller.cpp",'#include "finish_hold.hpp"','#include "finish_hold.hpp"\n#include "endpoint_settings.hpp"')
    change("src/controller.cpp",'FinishHold finishHold;','FinishHold finishHold;uint64_t endpointRestartAt=0;')
    change("src/controller.cpp",'release();finishHold.reset();sceneGate.reset();objectClearGate.reset();enterStall.reset();',
          'release();finishHold.reset();endpointRestartAt=0;sceneGate.reset();objectClearGate.reset();enterStall.reset();')
    change("src/controller.cpp",'activeRoute=initial?*initial:playbackRoute;routeActions.reset();segment=0;pendingStop=nextStop(activeRoute,1);',
'''activeRoute=initial?*initial:playbackRoute;
    if(!gpsDriving&&selectedRoute<routeCatalog.entries.size())
        activeRoute=trimEndpointTail(activeRoute,routeCatalog.entries[selectedRoute].endpoints.ignoreFinalSeconds);
    routeActions.reset();segment=0;pendingStop=nextStop(activeRoute,1);''')
    change("src/controller.cpp",'resumeRequested=false;joinStaging=false;requireNearbyStart=false;',
           'resumeRequested=false;joinStaging=false;requireNearbyStart=false;endpointRestartAt=0;')
    brake_start='    if(mode==Mode::Brake){\n        if(!finishHold.active)finishHold.begin();'
    brake_end='    if(mode==Mode::Parked)holdHandbrake();'
    c=Path("src/controller.cpp")
    s=c.read_text(encoding="utf-8-sig")
    assert s.count(brake_start)==1 and s.count(brake_end)==1
    p=s.index(brake_start)
    q=s.index(brake_end,p)
    chunk=s[p:q]
    assert chunk.count('if(speed<0.45f&&!enterQueued&&!enterHeld){')==1
    chunk=chunk.replace('if(speed<0.45f&&!enterQueued&&!enterHeld){',
'''if(speed<0.45f&&!enterQueued&&!enterHeld){
            const unsigned delaySeconds=(!gpsDriving&&loopEnabled&&selectedRoute<routeCatalog.entries.size())
                ?routeCatalog.entries[selectedRoute].endpoints.restartDelaySeconds:0u;
            if(!endpointRestartAt)endpointRestartAt=now+uint64_t(delaySeconds)*1000;
            if(now<endpointRestartAt){
                wchar_t text[150]{};
                swprintf(text,150,L"Финиш. Ручник включён. Следующий проезд через %.1f с",
                         double(endpointRestartAt-now)/1000.0);
                message=text;
            }else{''',1)
    suffix='        }\n    }\n'
    assert chunk.endswith(suffix),repr(chunk[-60:])
    chunk=chunk[:-len(suffix)]+'            }\n'+suffix
    s=s[:p]+chunk+s[q:]
    c.write_text(s,encoding="utf-8")
    # Supply settings in the existing state message (no new WebView channel).
    change("src/controller.cpp",'snapshot.insert(pathAt,flags.str());',
'''flags<<L",\\"finishIgnoreSeconds\\":"<<(selectedRoute<routeCatalog.entries.size()?routeCatalog.entries[selectedRoute].endpoints.ignoreFinalSeconds:0u)
          <<L",\\"finishPauseSeconds\\":"<<(selectedRoute<routeCatalog.entries.size()?routeCatalog.entries[selectedRoute].endpoints.restartDelaySeconds:0u);
          snapshot.insert(pathAt,flags.str());''')
    change("src/controller.cpp",'}else if(parts.size()==11&&parts[0]==L"waiting"){',
''' }else if(parts.size()==4&&parts[0]==L"endpoint"){
            if(mode!=Mode::Idle||overlay.selecting()||selectedRoute>=routeCatalog.entries.size()||
               parts[1]!=std::wstring(routeCatalog.entries[selectedRoute].id.begin(),routeCatalog.entries[selectedRoute].id.end())){
                message=L"Останови проезд и выбери маршрут.";updateWebState();return;
            }
            if(parts[2].empty()||parts[3].empty()||
                parts[2].find_first_not_of(L"0123456789")!=std::wstring::npos||
                parts[3].find_first_not_of(L"0123456789")!=std::wstring::npos||
                parts[2].size()>3||parts[3].size()>3){
                message=L"Недопустимое время финиша.";updateWebState();return;
            }
            EndpointSettings value{unsigned(std::stoul(parts[2])),unsigned(std::stoul(parts[3]))};
            if(!routeCatalog.setEndpointSettings(selectedRoute,value)){
                message=L"Не удалось сохранить настройки финиша.";updateWebState();return;
            }
            endpointRestartAt=0;++routeVisualRevision;message=L"Настройки финиша сохранены.";
        }else if(parts.size()==11&&parts[0]==L"waiting"){''')
    change("src/apil_release.hpp",'apilVersion[]="0.2.04";','apilVersion[]="0.2.05";')
    change("src/apil_release.hpp", 'L"0.2.04"', 'L"0.2.05"')
    app=Path("ui/app.js")
    code=app.read_text(encoding="utf-8-sig")
    assert "version:'0.2.03'" in code or "version:'0.2.04'" in code
    code=code.replace("version:'0.2.03'","version:'0.2.05'",1).replace("version:'0.2.04'","version:'0.2.05'",1)
    code=code.replace("selectedRoute:'',routes:[]","selectedRoute:'',finishIgnoreSeconds:0,finishPauseSeconds:0,routes:[]",1)
    app.write_text(code,encoding="utf-8")
    change("ui/route-editor.js",
'''const markerLayer=document.createElement('div');markerLayer.className='wait-marker-layer';stage.append(markerLayer);''',
'''const markerLayer=document.createElement('div');markerLayer.className='wait-marker-layer';stage.append(markerLayer);
  const endpointLayer=document.createElement('div');endpointLayer.className='endpoint-marker-layer';stage.append(endpointLayer);''')
    # Attach another small editor alongside the unchanged waiting-object and section editors.
    popup=r'''
  const endpointPopup=document.createElement('div');
  endpointPopup.className='mini-settings endpoint-popup';
  endpointPopup.hidden=true;endpointPopup.setAttribute('role','dialog');
  endpointPopup.setAttribute('aria-label','Параметры начала и финиша маршрута');
  endpointPopup.innerHTML=[
    '<button type="button" class="button button-small popup-close" id="endpoint-close" aria-label="Закрыть"><span aria-hidden="true">×</span></button>',
    '<div class="mini-heading"><img class="endpoint-title-icon" src="map/finish-blip.png" alt=""><div><strong>Старт и финиш</strong><p id="endpoint-label">Параметры маршрута</p></div></div>',
    '<div class="endpoint-visual" aria-hidden="true"><span>Старт</span><div class="endpoint-line"><i id="endpoint-ignored"></i></div><span>Финиш</span></div>',
    '<form id="endpoint-form"><fieldset id="endpoint-fields">',
    '<label class="endpoint-field">Игнорировать последние секунды перед финишем',
    '<input id="endpoint-skip" type="number" min="0" max="60" step="1" required inputmode="numeric"></label>',
    '<div class="mini-presets endpoint-presets" id="endpoint-presets">',
    '<button type="button" data-endpoint-skip="0">0 с</button>',
    '<button type="button" data-endpoint-skip="3">3 с</button>',
    '<button type="button" data-endpoint-skip="5">5 с</button>',
    '<button type="button" data-endpoint-skip="10">10 с</button></div>',
    '<label class="endpoint-field">Стоять перед запуском следующего маршрута',
    '<input id="endpoint-pause" type="number" min="0" max="300" step="1" required inputmode="numeric"></label>',
    '<div class="endpoint-timebar"><i id="endpoint-pausebar"></i></div>',
    '<p class="small-copy endpoint-hint" id="endpoint-hint">Пауза действует при включённом повторе маршрута. Игнорируемые секунды оцениваются по записанной скорости.</p>',
    '</fieldset><div class="action-row"><button class="button button-primary" id="endpoint-save" type="submit">Сохранить</button></div>',
    '<p class="small-copy" id="endpoint-feedback" role="status"></p></form>'
  ].join('');
  document.body.append(endpointPopup);
  const endpointButtons=['start','finish'].map(kind=>{
     const button=document.createElement('button');button.type='button';
     button.className='endpoint-blip endpoint-blip--'+kind;
     button.title=kind==='start'?'Настройки старта':'Настройки финиша';
     button.setAttribute('aria-label',button.title);
     button.innerHTML='<img src="map/finish-blip.png" alt="">';
     button.addEventListener('pointerdown',e=>e.stopPropagation());
     button.addEventListener('pointerup',e=>e.stopPropagation());
     button.onclick=e=>{
        e.stopPropagation();
        const state=getState();if(!state.selectedRoute||state.path.length<2)return;
        popup.hidden=true;waitPopup.hidden=true;
        $('endpoint-label').textContent=kind==='start'?'Точка старта':'Точка финиша';
        $('endpoint-skip').value=state.finishIgnoreSeconds||0;
        $('endpoint-pause').value=state.finishPauseSeconds||0;
        const disabled=state.recording||state.playing;
        $('endpoint-fields').disabled=disabled;
        $('endpoint-save').disabled=disabled;
        $('endpoint-feedback').textContent=disabled?'Останови запись или проезд, чтобы изменить параметры.':'';
        $('endpoint-feedback').dataset.pending='0';
        updateEndpointVisual();
        placePopup(endpointPopup,e.clientX+12,e.clientY+12);
     };
     endpointLayer.append(button);
     return button;
  });
  function updateEndpointVisual(){
     const skip=Number($('endpoint-skip').value)||0;
     const pause=Number($('endpoint-pause').value)||0;
     $('endpoint-ignored').style.width=(Math.max(0,Math.min(60,skip))/60*100)+'%';
     $('endpoint-pausebar').style.width=(Math.max(0,Math.min(300,pause))/300*100)+'%';
     endpointPopup.querySelectorAll('[data-endpoint-skip]').forEach(b=>{
        b.setAttribute('aria-pressed',String(Number(b.dataset.endpointSkip)===skip));
     });
  }
  $('endpoint-close').onclick=()=>endpointPopup.hidden=true;
  $('endpoint-form').addEventListener('input',updateEndpointVisual);
  endpointPopup.querySelectorAll('[data-endpoint-skip]').forEach(b=>{
      b.onclick=()=>{$('endpoint-skip').value=b.dataset.endpointSkip;updateEndpointVisual();};
  });
  $('endpoint-form').onsubmit=e=>{
      e.preventDefault();
      const state=getState(),skip=Number($('endpoint-skip').value),pause=Number($('endpoint-pause').value);
      if(!state.selectedRoute||state.recording||state.playing)return;
      if(!Number.isInteger(skip)||skip<0||skip>60||!Number.isInteger(pause)||pause<0||pause>300){
          $('endpoint-feedback').textContent='Укажи целые секунды: 0–60 и 0–300.';
          return;
      }
      send(['endpoint',state.selectedRoute,skip,pause].join('|'));
      $('endpoint-feedback').textContent='Сохранение...';
      $('endpoint-feedback').dataset.pending='1';
  };
'''
    change("ui/route-editor.js","const project=q=>",popup+"const project=q=>")
    change("ui/route-editor.js",'   drawWaitMarkers();',
           '   drawWaitMarkers();drawEndpoints();')
    change("ui/route-editor.js",
           "   if(screen.length){ctx.fillStyle=state.routeColor;ctx.strokeStyle='#111';ctx.lineWidth=2;ctx.beginPath();ctx.arc(...screen[0],5,0,Math.PI*2);ctx.fill();}",
           "   // Endpoints are clickable flag blips above the canvas.")
    markers=r'''
  function drawEndpoints(){
      const state=getState(),points=state.path;
      if(!state.selectedRoute||state.gpsDriving||state.recording||points.length<2){
         endpointButtons.forEach(b=>b.hidden=true);return;
      }
      const first=project(points[0]),last=project(points[points.length-1]);
      const overlap=Math.hypot(first[0]-last[0],first[1]-last[1])<32;
      [first,last].forEach((pos,i)=>{
          const b=endpointButtons[i];
          b.style.left=(pos[0]+(overlap?(i===0?-12:12):0))+'px';
          b.style.top=(pos[1]-(overlap?4:0))+'px';
          b.hidden=pos[0]<-24||pos[0]>width+24||pos[1]<-24||pos[1]>height+24;
      });
  }
'''
    change("ui/route-editor.js","  document.addEventListener('keydown',event=>{",
           markers+"  document.addEventListener('keydown',event=>{")
    change("ui/route-editor.js",
           "if(!waitPopup.hidden)waitPopup.hidden=true;",
           "if(!endpointPopup.hidden)endpointPopup.hidden=true;\n     else if(!waitPopup.hidden)waitPopup.hidden=true;")
    change("ui/route-editor.js",
           "popup.hidden=true;waitPopup.hidden=true;selectedWait=null;pendingFit=true;",
           "popup.hidden=true;waitPopup.hidden=true;endpointPopup.hidden=true;selectedWait=null;pendingFit=true;")
    change("ui/route-editor.js",
           "   scheduleDock();renderSections();",
           """   if($('endpoint-feedback').dataset.pending==='1'&&(state.message==='Настройки финиша сохранены.'||state.message.indexOf('Не удалось сохранить настройки финиша.')===0)){
      $('endpoint-feedback').textContent=state.message;$('endpoint-feedback').dataset.pending='0';
   }
   $('endpoint-fields').disabled=state.playing||state.recording;
   $('endpoint-save').disabled=state.playing||state.recording||!state.selectedRoute;
   scheduleDock();renderSections();""")
    change("ui/route-editor.js",
           "if(!waitPopup.contains(e.target)&&!e.target.closest('.wait-blip'))waitPopup.hidden=true;",
           "if(!waitPopup.contains(e.target)&&!e.target.closest('.wait-blip'))waitPopup.hidden=true;if(!endpointPopup.contains(e.target)&&!e.target.closest('.endpoint-blip'))endpointPopup.hidden=true;")
    change("ui/route-editor.js",
           "window.addEventListener('resize',()=>{popup.hidden=true;waitPopup.hidden=true;});",
           "window.addEventListener('resize',()=>{popup.hidden=true;waitPopup.hidden=true;endpointPopup.hidden=true;});")
    change("ui/route-editor.js",
           "stage.addEventListener('pointerdown',e=>{if(e.button!==0&&e.button!==1)return;popup.hidden=true;waitPopup.hidden=true;",
           "stage.addEventListener('pointerdown',e=>{if(e.button!==0&&e.button!==1)return;popup.hidden=true;waitPopup.hidden=true;endpointPopup.hidden=true;")
    append("ui/css/district.css",r'''
/* APIL 0.2.05 route endpoint blips and compact settings */
.endpoint-marker-layer{position:absolute;inset:0;z-index:9;pointer-events:none}
.endpoint-blip{position:absolute;display:grid;place-items:center;width:34px;height:34px;padding:3px;border:1px solid rgba(255,255,255,.72);border-radius:11px;background:#111;box-shadow:0 2px 8px #0009;transform:translate(-50%,-80%);pointer-events:auto;cursor:pointer;transition:transform .14s,border-color .14s,box-shadow .14s}
.endpoint-blip:hover{transform:translate(-50%,-85%) scale(1.09);border-color:#fff;box-shadow:0 0 0 3px #ffffff2b,0 4px 12px #000b}
.endpoint-blip:focus-visible{outline:2px solid white;outline-offset:3px}
.endpoint-blip img{display:block;width:26px;height:26px;object-fit:contain;image-rendering:pixelated}
.endpoint-blip--start{border-color:#b6d2bb}
.endpoint-blip--finish{border-color:#f1eee7}
.endpoint-popup{z-index:220!important;max-width:min(385px,calc(100vw - 18px));min-width:310px}
.endpoint-title-icon{width:26px;height:26px;object-fit:contain;image-rendering:pixelated;background:#111;border-radius:6px;padding:2px}
.endpoint-visual{display:flex;align-items:center;gap:10px;color:#aeb5b7;font-size:12px;margin:12px 0 16px}
.endpoint-line{position:relative;height:8px;border-radius:8px;background:linear-gradient(90deg,#667f72,#7e8c86);flex:1;overflow:hidden}
.endpoint-line>i{display:block;position:absolute;right:0;top:0;bottom:0;background:repeating-linear-gradient(45deg,#db8b79 0,#db8b79 5px,#5a3935 5px,#5a3935 10px);min-width:0;transition:width .15s}
.endpoint-field{display:flex;flex-direction:column;gap:7px;margin:10px 0;font-size:13px;color:var(--text,#eee)}
.endpoint-field input{width:100%;box-sizing:border-box;margin-top:3px}
.endpoint-presets{margin:4px 0 14px;display:flex;gap:6px;flex-wrap:wrap}
.endpoint-presets button{border:1px solid #ffffff26;border-radius:8px;background:#ffffff10;color:#eee;padding:5px 10px;cursor:pointer}
.endpoint-presets button[aria-pressed="true"]{background:#ffffff2e;border-color:#ffffff7a}
.endpoint-timebar{height:5px;border-radius:5px;background:#ffffff1a;margin:4px 0 12px;overflow:hidden}
.endpoint-timebar i{display:block;height:100%;width:0;background:#c5d1c9;transition:width .15s}
.endpoint-hint{line-height:1.45;opacity:.72;margin:7px 0 12px}
''')
    Path("ui/map/finish-blip.png").write_bytes(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAABEVBMVEUAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAASExKYm5gAAAAqKio5OTkAAAAIBwgHBwcAAgAAAAAAAAAAAAAIBwgAAAAoKCjMzMwGBgYAAAAAAAAAAAAHBwcAAAAAAAAAAAAAAACfop+foZ+fn58pKCkAAABPTk+fnJ9fYF/29/b////6+/o9QT0ABACDhYP2+/b29vb6+vo+QT72+vaDg4P4+PgCAQL5+flGQkZFQkU9QD11d3WgoqCkoKSgoKBeXV5BP0EmKCYIBAj/+///+v8ICAj8/fxaXVqtqq1VVVV7fXt7eXtaWVpgXWBfXV9oaGh7e3tMS0z9/f1eX14WFxYtLy1aW1rV1dXoeSKRAAAAJHRSTlMAAgMBF6u9vLdZRP39tvv5tfySf3+DOPkl/v74I93199QRBQcZ+1CpAAAAAWJLR0QAiAUdSAAAAAlwSFlzAAAAYAAAAGAA8GtCzwAAAAd0SU1FB+ILCxcUJtJDQWQAAAEVSURBVDjLY2AgBjAyYQXMMHkWVjZ2LICDE6aAi5tHRRUE1dTUNUBAUwvE0+Zlging09GFAD19A5ACQyMQR4UdocAYqsDEFKLADF0BmgnmYL4aqgl6JkBoYWpgCQTmxiCeGooJVtY2tkBgZ+/g4Ojo5Axiu6CYYOzqagAE9sZu7kZmTmC2BooJOh4gyy0d3ICWe3qBXaKBagJUgTt2BSATPFyBwBtkgq4PmI1qgp6vrx8Q+PsHBAYEBgWD2CHsNAxJiAloIekJBKFwE0A8tJAMC/MCgnCw/yIiQWx+AZRwEBQSBgIRMBAGA1ExBuRwEJfAnSTBJuBXMCxMkJSKipKWwa1ARlZOXl5BEbcCBiVlZWUlBjIBALnoVYLdL/8iAAAAJXRFWHRkYXRlOmNyZWF0ZQAyMDE4LTExLTExVDIzOjM1OjIzKzAxOjAwsG/noAAAACV0RVh0ZGF0ZTptb2RpZnkAMjAxOC0xMS0xMVQyMzoyMDozOCswMTowMCg4H/wAAAAASUVORK5CYII="))
    # Schema and local sanity checks. Main GitHub workflow compiles the Win64 binary.
    assert "endpoint|".replace("endpoint|","endpoint") in Path("src/controller.cpp").read_text()
    assert "finishIgnoreSeconds" in Path("ui/route-editor.js").read_text()
    if shutil.which("node"):
        subprocess.run(["node","--check","ui/route-editor.js"],check=True)
        subprocess.run(["node","--check","ui/app.js"],check=True)

def main():
    with zipfile.ZipFile(ARCHIVE) as z:
        names=set(z.namelist())
        if "src/apil_release.hpp" not in names:raise RuntimeError("Missing release header")
        release=z.read("src/apil_release.hpp").decode("utf-8")
        if VERSION in release:
            print("Already patched to",VERSION)
            return
        if "0.2.04" not in release:raise RuntimeError("Unexpected starting version: "+release)
        with tempfile.TemporaryDirectory() as td:
            dst=Path(td)
            for info in z.infolist():
                target=dst/info.filename
                if info.is_dir():target.mkdir(parents=True,exist_ok=True);continue
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(z.read(info))
            cwd=Path.cwd()
            os.chdir(dst)
            try:
                install()
                updated={}
                for p in dst.rglob("*"):
                    if p.is_file():updated[p.relative_to(dst).as_posix()]=p.read_bytes()
            finally:os.chdir(cwd)
        # Data already collected; replace ZIP atomically after all checks pass.
        tempzip=ARCHIVE.with_suffix(".tmp")
        with zipfile.ZipFile(tempzip,"w",compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as out:
            for name,data in sorted(updated.items()):
                out.writestr(name,data)
        with zipfile.ZipFile(tempzip) as checked:
            assert checked.testzip() is None
            assert VERSION in checked.read("src/apil_release.hpp").decode()
            assert "endpoint-blip" in checked.read("ui/route-editor.js").decode()
        os.replace(tempzip,ARCHIVE)
    print("Patched",ARCHIVE,ARCHIVE.stat().st_size,"bytes, version",VERSION)

if __name__=="__main__":
    main()
