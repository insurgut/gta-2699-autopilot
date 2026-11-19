#!/usr/bin/env python3
"""APIL 0.2.09: resilient District process discovery and offline diagnostics.

Keeps anti-cheat rejection, GTA b2699 memory-profile validation, and
telemetry's read-only process access. Only changes packaging and UI guards.
"""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import os
import re

ARCHIVE = Path("source/APIL-source.zip")

def once(value, old, new, label):
    count=value.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one anchor, got {count}: {old[:110]!r}")
    return value.replace(old, new, 1)

with ZipFile(ARCHIVE) as input_zip:
    assert input_zip.testzip() is None, "Input archive CRC invalid"
    files = {i.filename: input_zip.read(i) for i in input_zip.infolist() if not i.is_dir()}

version = files["src/apil_release.hpp"].decode("utf-8-sig")
if 'apilVersion[]="0.2.09"' in version:
    print("APIL 0.2.09 already applied")
    raise SystemExit(0)
assert 'apilVersion[]="0.2.08"' in version, "Expected APIL 0.2.08"

name = "src/attach.hpp"
s = files[name].decode("utf-8-sig")
s = once(s, '#include <cwchar>', '#include <cwchar>\n#include <cwctype>', name)
s = once(s, '''inline DWORD gameProcess(){
    DWORD selected=0;
    for(auto&e:processList())if(!_wcsicmp(e.szExeFile,L"District9_b2699_GameProcess.exe")){
        if(selected)return 0;
        selected=e.th32ProcessID;
    }
    return selected;
}''',
'''// District's executable filename may change with launcher/server updates.
// Only select obvious District/FiveM b2699 candidates; never use a generic GTA5.exe.
inline bool districtGameName(const wchar_t*name){
    if(!name||!*name)return false;
    std::wstring text(name);
    for(auto&c:text)c=std::towlower(c);
    if(text.size()<5||text.substr(text.size()-4)!=L".exe")return false;
    if(text==L"district9_b2699_gameprocess.exe"||text==L"b2699_gameproc.exe"||
       text==L"district9_b2699_gameproc.exe")return true;
    const bool district=text.find(L"district")!=std::wstring::npos;
    const bool game=text.find(L"gameproc")!=std::wstring::npos;
    const bool version=text.find(L"b2699")!=std::wstring::npos;
    return (district&&game)||(version&&game);
}
inline DWORD gameProcess(){
    DWORD selected=0;
    for(const auto&e:processList())if(districtGameName(e.szExeFile)){
        if(selected&&selected!=e.th32ProcessID)return 0; // ambiguous clients
        selected=e.th32ProcessID;
    }
    return selected;
}''', name)
s = once(s,
'''    if(snap.h!=INVALID_HANDLE_VALUE&&Module32FirstW(snap,&e)){do{if(!_wcsicmp(e.szModule,name))return reinterpret_cast<uintptr_t>(e.modBaseAddr);}while(Module32NextW(snap,&e));}''',
'''    if(snap.h!=INVALID_HANDLE_VALUE&&Module32FirstW(snap,&e)){do{
        if(!name||!name[0]||!_wcsicmp(e.szModule,name))
            return reinterpret_cast<uintptr_t>(e.modBaseAddr);
    }while(Module32NextW(snap,&e));}''', name)
s = once(s,
'''    if(!profileHeaderMatches(nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage)){
        std::wostringstream s;s<<L"Неизвестный заголовок: timestamp=0x"<<std::hex<<nt.FileHeader.TimeDateStamp<<L", image=0x"<<nt.OptionalHeader.SizeOfImage;
        return fail(s.str());
    }''',
'''    // Timestamp can change when District updates its launcher/PE wrapper.
    // Never accept a different image extent, and always validate the actual
    // WORLD_LOAD instruction against the exact b2699 RVA below.
    if(!profileHeaderMatches(nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage)&&
       nt.OptionalHeader.SizeOfImage!=DISTRICT_IMAGE_SIZE&&
       nt.OptionalHeader.SizeOfImage!=GAME_IMAGE_SIZE){
        std::wostringstream s;s<<L"Неизвестный размер игрового модуля: timestamp=0x"<<std::hex
                              <<nt.FileHeader.TimeDateStamp<<L", image=0x"<<nt.OptionalHeader.SizeOfImage;
        return fail(s.str());
    }''', name)
s = once(s,
'''    auto base=moduleBase(pid,L"District9_b2699_GameProcess.exe");''',
'''    auto base=moduleBase(pid,nullptr); // main executable module, including renamed District builds''',name)
files[name]=s.encode("utf-8")

name="src/external_reader.cpp"
s=files[name].decode("utf-8-sig")
s=once(s,'    uintptr_t base=moduleBase(pid,L"District9_b2699_GameProcess.exe");',
       '    uintptr_t base=moduleBase(pid,nullptr); // same validated module as controller',name)
files[name]=s.encode("utf-8")

name="src/controller.cpp"
s=files[name].decode("utf-8-sig")
s=once(s,
'''    detach();pid=gameProcess();
    if(!pid){message=L"Запусти один клиент District 2699 и войди в игровой мир.";return;}''',
'''    detach();pid=gameProcess();
    if(!pid){message=L"Процесс District 2699 не найден или запущено несколько клиентов. Открой «Диагностика» и сохрани отчёт.";return;}''',name)
s=once(s,
'''    auto args=new ReaderArgs{pid,window,appDir/L"DistrictTelemetry.exe"};''',
'''    auto reader=appDir/L"DistrictTelemetry.exe";
    std::error_code readerError;
    if(!std::filesystem::is_regular_file(reader,readerError))
        reader=dataDir/L"runtime"/apilVersionWide/L"bin"/L"DistrictTelemetry.exe";
    auto args=new ReaderArgs{pid,window,reader};''',name)
s=once(s,
r'''        f<<"\nStatus: "<<utf8(message)<<"\nTelemetry: "<<utf8(std::wstring(sample.error,errorLength))<<"\n";r''',
r'''        f<<"\nStatus: "<<utf8(message)<<"\nTelemetry: "<<utf8(std::wstring(sample.error,errorLength))<<"\n";
        // Offline connection report: no telemetry or running game required.
        f<<"Connection connected="<<haveSample<<" connecting="<<connecting
         <<" PID="<<pid<<" BattlEye="<<battleye()<<"\n";
        const DWORD discoveredPid=gameProcess();
        f<<"Discovered PID="<<discoveredPid<<" (zero = missing or ambiguous)\n";
        for(const auto&entry:processList()){
            std::wstring exe(entry.szExeFile);
            std::wstring low=exe;
            for(auto&c:low)c=std::towlower(c);
            if(districtGameName(entry.szExeFile)||low.find(L"district")!=std::wstring::npos||
               low.find(L"gta5")!=std::wstring::npos||low.find(L"fivem")!=std::wstring::npos)
                f<<"Process candidate PID="<<entry.th32ProcessID<<" exe="<<utf8(exe)
                 <<" eligible="<<districtGameName(entry.szExeFile)<<"\n";
        }
        auto reportReader=appDir/L"DistrictTelemetry.exe";
        std::error_code readerError;
        if(!std::filesystem::is_regular_file(reportReader,readerError))
            reportReader=dataDir/L"runtime"/apilVersionWide/L"bin"/L"DistrictTelemetry.exe";
        f<<"Reader executable="<<utf8(reportReader.wstring())
         <<" present="<<std::filesystem::is_regular_file(reportReader,readerError)<<"\n";
        const DWORD probePid=pid?pid:discoveredPid;
        if(probePid&&!battleye()){
            WinHandle probe(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,probePid));
            const DWORD openError=probe.h?0:GetLastError();
            f<<"OpenProcess success="<<bool(probe.h)<<" winError="<<openError<<"\n";
            if(probe.h){
                uintptr_t base=moduleBase(probePid,nullptr);
                std::wstring profileReason;
                bool profileOK=profileMatches(probe,base,&profileReason);
                f<<"Main module base=0x"<<std::hex<<base<<std::dec
                 <<" b2699 profile="<<profileOK<<" reason="<<utf8(profileReason)<<"\n";
                WinHandle ipc(OpenFileMappingW(FILE_MAP_READ,FALSE,ipcName(probePid).c_str()));
                f<<"Telemetry IPC present="<<bool(ipc.h)<<"\n";
            }
        }r''',name)
s=once(s,
r'''         <<"sampleAgeMs "<<(GetTickCount64()-sample.sampleBeat)<<" radarAgeMs "<<(GetTickCount64()-sample.radarBeat)<<" radarValid "<<sample.radarValid<<" complete "<<grid.complete<<"\n"r''',
r'''         <<"sampleAgeMs "<<(sample.sampleBeat?GetTickCount64()-sample.sampleBeat:0)<<" radarAgeMs "<<(sample.radarBeat?GetTickCount64()-sample.radarBeat:0)<<" radarValid "<<sample.radarValid<<" complete "<<grid.complete<<"\n"r''',name)
files[name]=s.encode("utf-8")

name="ui/app.js"
s=files[name].decode("utf-8-sig")
s=once(s,"function navigate(page){if(page!=='settings'&&!state.connected)return;",
         "function navigate(page){if(page!=='settings'&&page!=='diagnostics'&&!state.connected)return;",name)
s=once(s,"el.disabled=el.dataset.page!=='settings'&&!state.connected;",
         "el.disabled=!['settings','diagnostics'].includes(el.dataset.page)&&!state.connected;",name)
s=once(s,"if(!state.connected&&currentPage!=='settings')navigate('settings');",
         "if(!state.connected&&!['settings','diagnostics'].includes(currentPage))navigate('settings');",name)
s=once(s,"version:'0.2.08'","version:'0.2.09'",name)
files[name]=s.encode("utf-8")

name="ui/index.html"
s=files[name].decode("utf-8-sig")
s=once(s,'<span data-text="version">0.2.08</span>',
         '<span data-text="version">0.2.09</span>',name)
files[name]=s.encode("utf-8")

name="src/apil_release.hpp"
s=files[name].decode("utf-8-sig")
s=once(s,'apilVersion[]="0.2.08"','apilVersion[]="0.2.09"',name)
s=once(s,'apilVersionWide[]=L"0.2.08"','apilVersionWide[]=L"0.2.09"',name)
files[name]=s.encode("utf-8")

assert "districtGameName" in files["src/attach.hpp"].decode()
assert "page!=='diagnostics'" in files["ui/app.js"].decode()
assert "Connection connected=" in files["src/controller.cpp"].decode()
archive_tmp=ARCHIVE.with_suffix(".connection.tmp")
with ZipFile(archive_tmp,"w",compression=ZIP_DEFLATED,compresslevel=6) as out:
    for path,body in sorted(files.items()):
        out.writestr(path,body)
with ZipFile(archive_tmp) as verified:
    assert verified.testzip() is None, "Output archive CRC invalid"
    assert 'apilVersion[]="0.2.09"' in verified.read("src/apil_release.hpp").decode()
os.replace(archive_tmp,ARCHIVE)
print("APIL 0.2.09: updated process matching, offline diagnostics, profile validation, and UI")
