#!/usr/bin/env python3
"""APIL 0.2.06: one frameless finish-flag blip and visible interactive popup."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from tempfile import TemporaryDirectory
import os, subprocess, shutil

ZIP=Path("source/APIL-source.zip")
def replace_once(s,a,b,label):
    n=s.count(a)
    if n!=1:raise RuntimeError(f"{label}: expected unique anchor, found {n}: {a[:65]}")
    return s.replace(a,b,1)

def apply(files):
    path="ui/route-editor.js"
    s=files[path].decode("utf-8-sig")
    a="  const endpointButtons=['start','finish'].map(kind=>{"
    b="  function updateEndpointVisual(){"
    if s.count(a)!=1 or s.count(b)!=1:raise RuntimeError("Missing endpoint editor anchors")
    i=s.index(a);j=s.index(b,i)
    s=s[:i]+r'''  // A single common control for both route endpoints: no duplicate start/finish blips.
  const endpointButton=document.createElement('button');
  endpointButton.type='button';
  endpointButton.className='endpoint-blip';
  endpointButton.setAttribute('aria-label','Настройки старта и финиша');
  endpointButton.title='Старт и финиш маршрута';
  endpointButton.innerHTML='<img src="map/finish-blip.png" alt="">';
  endpointButton.addEventListener('pointerdown',event=>{
      event.stopPropagation(); // map canvas drag must never steal this click
  });
  endpointButton.addEventListener('pointerup',event=>event.stopPropagation());
  endpointButton.addEventListener('click',event=>{
      event.preventDefault();event.stopPropagation();
      const state=getState();
      if(!state.selectedRoute||state.path.length<2)return;
      popup.hidden=true;waitPopup.hidden=true;
      $('endpoint-label').textContent='Старт и финиш маршрута';
      $('endpoint-skip').value=state.finishIgnoreSeconds??0;
      $('endpoint-pause').value=state.finishPauseSeconds??0;
      const disabled=state.recording||state.playing;
      $('endpoint-fields').disabled=disabled;
      $('endpoint-save').disabled=disabled;
      $('endpoint-feedback').textContent=disabled?'Останови запись или проезд, чтобы изменить параметры.':'';
      $('endpoint-feedback').dataset.pending='0';
      updateEndpointVisual();
      placePopup(endpointPopup,event.clientX+14,event.clientY+14);
      endpointPopup.scrollTop=0;
  });
  endpointLayer.append(endpointButton);
'''+s[j:]
    a="  function drawEndpoints(){"
    b="document.addEventListener('keydown',event=>{"
    if s.count(a)!=1 or s.count(b)!=1:raise RuntimeError("Missing drawEndpoints anchors")
    i=s.index(a);j=s.index(b,i)
    s=s[:i]+r'''  function drawEndpoints(){
      const state=getState(),points=state.path;
      if(!state.selectedRoute||state.gpsDriving||state.recording||points.length<2){
          endpointButton.hidden=true;return;
      }
      // One flag at the route's finish, configuring start + finish together.
      // This also avoids two overlapping markers on a closed loop.
      const pos=project(points[points.length-1]);
      endpointButton.style.left=pos[0]+'px';
      endpointButton.style.top=pos[1]+'px';
      endpointButton.hidden=pos[0]<-26||pos[0]>width+26||pos[1]<-26||pos[1]>height+26;
  }
'''+s[j:]
    assert "endpointButtons" not in s and "drawEndpoints();" in s
    files[path]=s.encode("utf-8")
    path="ui/css/district.css"
    s=files[path].decode("utf-8-sig")
    s+="""
/* APIL 0.2.06: the flag is ONLY the icon, with an invisible clickable hit area. */
.endpoint-marker-layer{pointer-events:none;z-index:30}
.endpoint-blip,.endpoint-blip:hover,.endpoint-blip:active,.endpoint-blip:focus{
  appearance:none!important;-webkit-appearance:none!important;
  display:flex!important;align-items:center!important;justify-content:center!important;
  width:38px!important;height:38px!important;padding:3px!important;
  border:0!important;border-radius:0!important;background:transparent!important;
  box-shadow:none!important;filter:none!important;
  outline:0!important;cursor:pointer!important;pointer-events:auto!important;
  transform:translate(-50%,-80%)!important;
}
.endpoint-blip[hidden]{display:none!important}
.endpoint-blip img{width:32px!important;height:32px!important;background:transparent!important;
  border:none!important;box-shadow:none!important;object-fit:contain!important;
  image-rendering:pixelated!important;pointer-events:none!important}
/* Fix: previously this dialog was statically positioned below the full-screen application. */
.endpoint-popup{
  position:fixed!important;z-index:10020!important;
  box-sizing:border-box;width:min(385px,calc(100vw - 20px))!important;
  max-width:calc(100vw - 20px)!important;min-width:0!important;
  max-height:calc(100vh - 40px);overflow-y:auto;
  background:var(--bg,#252525);border:1px solid var(--border,#484848);
  border-radius:12px;padding:16px;
  box-shadow:0 12px 36px #0008;
}
.endpoint-popup[hidden]{display:none!important}
.endpoint-title-icon{background:transparent!important;border:0!important;padding:0!important}
"""
    files[path]=s.encode("utf-8")
    path="ui/app.js"
    s=files[path].decode("utf-8-sig")
    s=replace_once(s,"version:'0.2.05'","version:'0.2.06'",path)
    files[path]=s.encode("utf-8")
    path="ui/index.html"
    s=files[path].decode("utf-8-sig")
    s=replace_once(s,'<span data-text="version">0.2.03</span>','<span data-text="version">0.2.06</span>',path)
    files[path]=s.encode("utf-8")
    path="src/apil_release.hpp"
    s=files[path].decode("utf-8-sig")
    s=replace_once(s,'apilVersion[]="0.2.05"','apilVersion[]="0.2.06"',path)
    s=replace_once(s,'apilVersionWide[]=L"0.2.05"','apilVersionWide[]=L"0.2.06"',path)
    files[path]=s.encode("utf-8")
    assert "position:fixed!important" in files["ui/css/district.css"].decode()
    assert "endpointButton.addEventListener('click'" in files["ui/route-editor.js"].decode()
    assert files["ui/route-editor.js"].decode().count("const endpointButton=document.createElement")==1
    with TemporaryDirectory() as td:
        js=Path(td)/"route-editor.js"
        js.write_bytes(files["ui/route-editor.js"])
        if shutil.which("node"):
            subprocess.run(["node","--check",str(js)],check=True)
            js.write_bytes(files["ui/app.js"])
            subprocess.run(["node","--check",str(js)],check=True)
    return files

def main():
    with ZipFile(ZIP) as z:
        files={a.filename:z.read(a) for a in z.infolist() if not a.is_dir()}
    version=files["src/apil_release.hpp"].decode()
    if 'apilVersion[]="0.2.06"' in version:
        print("Already fixed to 0.2.06")
        return
    if 'apilVersion[]="0.2.05"' not in version:
        raise RuntimeError("Expected 0.2.05 source archive")
    files=apply(files)
    tmp=ZIP.with_suffix(".fix.tmp")
    with ZipFile(tmp,"w",compression=ZIP_DEFLATED,compresslevel=6) as z:
        for name,data in sorted(files.items()):z.writestr(name,data)
    with ZipFile(tmp) as z:
        if z.testzip() is not None:raise RuntimeError("ZIP CRC failed")
        assert "0.2.06" in z.read("src/apil_release.hpp").decode()
    os.replace(tmp,ZIP)
    print("APIL 0.2.06: one frameless flag, popup click fix, archive bytes:",ZIP.stat().st_size)

if __name__=="__main__":main()
