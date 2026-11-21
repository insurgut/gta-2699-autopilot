#!/usr/bin/env python3
"""APIL 0.2.13: hard-lock APIL-generated braking keys on marked route edges."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import os
archive=Path("source/APIL-source.zip")
with ZipFile(archive) as z:
    assert z.testzip() is None
    files={i.filename:z.read(i) for i in z.infolist() if not i.is_dir()}
if b'apilVersion[]="0.2.13"' in files['src/apil_release.hpp']:
    print('APIL 0.2.13 already applied')
    raise SystemExit(0)
assert b'apilVersion[]="0.2.12"' in files['src/apil_release.hpp']
def replace(path,before,after):
    value=files[path].decode('utf-8-sig')
    count=value.count(before)
    if count!=1:raise RuntimeError(f'{path} anchor {count}: {before[:170]}')
    files[path]=value.replace(before,after,1).encode('utf-8')
files['src/no_braking_key_policy.hpp']=r'''#pragma once
#include <cstddef>
// The five virtual driving keys are indexed W=0, S=1, A=2, D=3, Space=4.
// This policy applies only to APIL-generated key-downs. Key-ups must ALWAYS
// pass through to avoid sticking a brake after switching sections.
// On reverse sections S is the accelerator, so W becomes the brake key.
inline bool blockAutopilotBrakingKey(bool markedSection,int direction,
                                    std::size_t key,bool down) noexcept {
    if(!markedSection||!down)return false;
    return key==4||(direction<0?key==0:key==1);
}
'''
replace('src/controller.cpp',
'''bool outputKey(size_t i,bool down){
    if(keyState[i]==down)return true;''',
'''bool outputKey(size_t i,bool down){
    const bool markedSection=!gpsDriving&&mode!=Mode::Idle&&mode!=Mode::Record&&
        activeRoute.size()>1&&
        activeRoute[std::min(segment+1,activeRoute.size()-1)].noRecordedBraking;
    // Last line of defence: no APIL-generated Space/brake on marked sections.
    // Explicit releases and the reverse-gear accelerator remain available.
    if(blockAutopilotBrakingKey(markedSection,driveDirection,i,down))down=false;
    if(keyState[i]==down)return true;''')
replace('src/controller.cpp',
'''#include "route_sections.hpp"''',
'''#include "route_sections.hpp"
#include "no_braking_key_policy.hpp"''')
replace('ui/route-editor.js',
'''Торможение перед препятствиями и опасными поворотами остаётся активным.''',
'''На этом участке APIL вообще не будет нажимать Space или тормозную клавишу S (при заднем ходе — W), даже перед препятствиями. Записанные ранее торможения и остановки тоже игнорируются. Ручное управление клавиатурой не блокируется.''')
replace('ui/app.js',
'''Не учитывать записанные вручную торможения и остановки.''',
'''Не нажимать тормозные клавиши в автопилоте, в том числе перед препятствиями.''')
replace('src/apil_release.hpp','apilVersion[]="0.2.12"','apilVersion[]="0.2.13"')
replace('src/apil_release.hpp','apilVersionWide[]=L"0.2.12"','apilVersionWide[]=L"0.2.13"')
replace('ui/app.js',"version:'0.2.12'","version:'0.2.13'")
replace('ui/index.html','<span data-text="version">0.2.12</span>','<span data-text="version">0.2.13</span>')
files['src/no_braking_key_policy_tests.cpp']=r'''#include "no_braking_key_policy.hpp"
#include <cassert>
int main(){
  for(int direction:{-1,1}){
    for(std::size_t key=0;key<5;++key){
      assert(!blockAutopilotBrakingKey(false,direction,key,true));
      assert(!blockAutopilotBrakingKey(true,direction,key,false));
      assert(blockAutopilotBrakingKey(true,direction,key,true)==
             (key==4||key==(direction<0?0u:1u)));
    }
  }
  assert(blockAutopilotBrakingKey(true,1,1,true));   // S brake
  assert(blockAutopilotBrakingKey(true,1,4,true));   // Space brake
  assert(!blockAutopilotBrakingKey(true,1,0,true));  // W gas
  assert(!blockAutopilotBrakingKey(true,-1,1,true)); // S reverse gas
  assert(blockAutopilotBrakingKey(true,-1,0,true));  // W reverse brake
  assert(!blockAutopilotBrakingKey(true,-1,4,false)); // Space key-up
}
'''.encode('utf-8')
assert b'if(blockAutopilotBrakingKey(markedSection,driveDirection,i,down))down=false;' in files['src/controller.cpp']
assert b'activeRoute[std::min(segment+1,activeRoute.size()-1)].noRecordedBraking' in files['src/controller.cpp']
tmp=archive.with_suffix('.hard-no-braking.tmp')
with ZipFile(tmp,'w',compression=ZIP_DEFLATED,compresslevel=6) as out:
    for name,content in sorted(files.items()):out.writestr(name,content)
with ZipFile(tmp) as z:assert z.testzip() is None and b'0.2.13' in z.read('src/apil_release.hpp')
os.replace(tmp,archive)
print('APIL 0.2.13: all controller brake keydown paths blocked on marked route edges')
