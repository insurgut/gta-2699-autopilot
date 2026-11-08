#pragma once
#include "telemetry.hpp"
#include "profile.hpp"
#include <tlhelp32.h>
#include <filesystem>
#include <vector>
#include <cwchar>
#include <cstring>
#include <sstream>
struct WinHandle {
    HANDLE h=nullptr;
    explicit WinHandle(HANDLE p=nullptr):h(p){}
    ~WinHandle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    operator HANDLE()const{return h;}
    WinHandle(const WinHandle&)=delete;
};
inline std::vector<PROCESSENTRY32W> processList(){
    WinHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));std::vector<PROCESSENTRY32W> list;
    PROCESSENTRY32W e{};e.dwSize=sizeof(e);
    if(snap.h!=INVALID_HANDLE_VALUE&&Process32FirstW(snap,&e)){do{list.push_back(e);}while(Process32NextW(snap,&e));}
    return list;
}
inline bool battleye(){for(auto&e:processList())if(!_wcsicmp(e.szExeFile,L"BEService.exe")||!_wcsicmp(e.szExeFile,L"BEService_x64.exe")||!_wcsicmp(e.szExeFile,L"GTA5_BE.exe"))return true;return false;}
inline DWORD gameProcess(){
    DWORD selected=0;
    for(auto&e:processList())if(!_wcsicmp(e.szExeFile,L"District9_b2699_GameProcess.exe")){
        if(selected)return 0;
        selected=e.th32ProcessID;
    }
    return selected;
}
inline uintptr_t moduleBase(DWORD pid,const wchar_t*name){
    WinHandle snap(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));MODULEENTRY32W e{};e.dwSize=sizeof(e);
    if(snap.h!=INVALID_HANDLE_VALUE&&Module32FirstW(snap,&e)){do{if(!_wcsicmp(e.szModule,name))return reinterpret_cast<uintptr_t>(e.modBaseAddr);}while(Module32NextW(snap,&e));}
    return 0;
}
inline bool profileMatches(HANDLE process,uintptr_t base,std::wstring*reason=nullptr){
    SIZE_T n=0;IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};uint8_t code[7]{};
    auto fail=[&](const std::wstring&text){if(reason)*reason=text;return false;};
    if(!base)return fail(L"Игровой модуль не найден.");
    if(!ReadProcessMemory(process,reinterpret_cast<void*>(base),&dos,sizeof(dos),&n)||n!=sizeof(dos))return fail(L"Не читается заголовок модуля. Windows: "+std::to_wstring(GetLastError()));
    if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0||dos.e_lfanew>0x100000)return fail(L"Неизвестный формат DOS-заголовка.");
    if(!ReadProcessMemory(process,reinterpret_cast<void*>(base+dos.e_lfanew),&nt,sizeof(nt),&n)||n!=sizeof(nt))return fail(L"Не читается PE-заголовок. Windows: "+std::to_wstring(GetLastError()));
    if(nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return fail(L"Модуль не является ожидаемым Windows x64 PE.");
    if(!profileHeaderMatches(nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage)){
        std::wostringstream s;s<<L"Неизвестный заголовок: timestamp=0x"<<std::hex<<nt.FileHeader.TimeDateStamp<<L", image=0x"<<nt.OptionalHeader.SizeOfImage;
        return fail(s.str());
    }
    if(!ReadProcessMemory(process,reinterpret_cast<void*>(base+WORLD_LOAD_RVA),code,sizeof(code),&n)||n!=sizeof(code))return fail(L"Код игры пока не читается. Войди в игровой мир.");
    int32_t relative=0;std::memcpy(&relative,code+3,4);
    if(!worldInstructionMatches(code,relative)){
        std::wostringstream s;s<<L"Не совпала инструкция мира: байты"<<std::hex;for(auto b:code)s<<L" "<<unsigned(b);
        s<<L", target=0x"<<(int64_t(WORLD_LOAD_RVA)+7+relative);return fail(s.str());
    }
    if(reason)reason->clear();
    return true;
}
inline std::wstring attachReader(DWORD pid,const std::filesystem::path&reader){
    if(battleye())return L"Обнаружен BattlEye. Подключение отменено.";
    std::error_code ec;if(!std::filesystem::is_regular_file(reader,ec))return L"Не удалось найти встроенный модуль телеметрии.";
    WinHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    if(!process.h)return L"Не удалось прочитать процесс District. Ошибка Windows: "+std::to_wstring(GetLastError());
    auto base=moduleBase(pid,L"District9_b2699_GameProcess.exe");
    std::wstring reason;
    if(!profileMatches(process,base,&reason))return L"Проверка District 2699: "+reason;
    for(unsigned i=0;i<20;i++){
        WinHandle map(OpenFileMappingW(FILE_MAP_READ,FALSE,ipcName(pid).c_str()));
        if(!map.h)break;
        auto view=static_cast<const Telemetry*>(MapViewOfFile(map,FILE_MAP_READ,0,0,sizeof(Telemetry)));
        bool healthy=view&&view->magic==TELEMETRY_MAGIC&&view->version==TELEMETRY_VERSION&&view->pid==pid&&!view->shutdown&&view->sampleBeat&&GetTickCount64()-view->sampleBeat<500;
        if(view)UnmapViewOfFile(view);
        if(healthy)return L"Внешняя телеметрия уже работает.";
        Sleep(100);
    }
    std::wstring cmd=L"\""+reader.wstring()+L"\" "+std::to_wstring(pid)+L" "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION info{};
    auto cwd=reader.parent_path().wstring();
    if(!CreateProcessW(reader.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&startup,&info))return L"Не удалось запустить телеметрию. Ошибка Windows: "+std::to_wstring(GetLastError());
    WinHandle child(info.hProcess),thread(info.hThread);
    for(unsigned i=0;i<100;i++){
        WinHandle map(OpenFileMappingW(FILE_MAP_READ,FALSE,ipcName(pid).c_str()));
        if(map.h)return L"Телеметрия запущена. Сядь за руль; Ins — запись, Del — пуск или стоп.";
        if(WaitForSingleObject(child,0)==WAIT_OBJECT_0){DWORD code=0;GetExitCodeProcess(child,&code);return L"Телеметрия завершилась при запуске, код "+std::to_wstring(code)+L". Сохрани диагностику.";}
        Sleep(50);
    }
    return L"Ожидание телеметрии истекло. Проверь диагностический файл.";
}
