#pragma once
#include "apil_release.hpp"
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <array>
#include <algorithm>
#include <string_view>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace apil {
struct Internet {
    HINTERNET h=nullptr;
    ~Internet(){if(h)WinHttpCloseHandle(h);}
};
inline std::vector<uint8_t> download(const wchar_t* path,size_t limit){
    Internet session{WinHttpOpen(L"APIL/" L"0.2.00",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,nullptr,nullptr,0)};
    if(!session.h)return {};
    WinHttpSetTimeouts(session.h,3000,3000,3000,4000);
    Internet connection{WinHttpConnect(session.h,L"raw.githubusercontent.com",443,0)};
    if(!connection.h)return {};
    Internet request{WinHttpOpenRequest(connection.h,L"GET",path,nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH)};
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if(!request.h||!WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect))||
       !WinHttpSendRequest(request.h,nullptr,0,nullptr,0,0,0)||!WinHttpReceiveResponse(request.h,nullptr))return {};
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)||status!=200)return {};
    std::vector<uint8_t> body;
    std::array<uint8_t,32768> buf{};
    for(;;){
        DWORD bytes=0;
        if(!WinHttpReadData(request.h,buf.data(),DWORD(buf.size()),&bytes)||body.size()+bytes>limit)return {};
        if(!bytes)return body;
        body.insert(body.end(),buf.begin(),buf.begin()+bytes);
    }
}
inline std::string sha256(std::vector<uint8_t>& data){
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    DWORD length=0,copied=0;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&copied,0)<0){
        BCryptCloseAlgorithmProvider(algorithm,0);return {};
    }
    std::vector<uint8_t> storage(length);
    BCRYPT_HASH_HANDLE hash=nullptr;
    std::array<uint8_t,32> digest{};
    const bool ok=BCryptCreateHash(algorithm,&hash,storage.data(),length,nullptr,0,0)>=0&&
        BCryptHashData(hash,data.data(),ULONG(data.size()),0)>=0&&
        BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok)return {};
    static constexpr char hex[]="0123456789abcdef";
    std::string out;
    for(auto byte:digest){out+=hex[byte>>4];out+=hex[byte&15];}
    return out;
}
inline bool parseVersion(const std::string& value,std::array<unsigned,3>& version){
    std::istringstream in(value);
    char a=0,b=0;
    if(!(in>>version[0]>>a>>version[1]>>b>>version[2])||a!='.'||b!='.'||!in.eof())return false;
    return std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||c=='.';});
}
inline std::filesystem::path executable(){
    wchar_t path[32768]{};
    const auto n=GetModuleFileNameW(nullptr,path,DWORD(sizeof(path)/sizeof(path[0])));
    return n&&n<sizeof(path)/sizeof(path[0])?std::filesystem::path(path):std::filesystem::path{};
}
inline bool launch(const std::filesystem::path& file,std::wstring args){
    std::wstring command=L"\""+file.wstring()+L"\""+args;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
    if(!CreateProcessW(file.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process))return false;
    CloseHandle(process.hThread);CloseHandle(process.hProcess);return true;
}
inline bool applyUpdateArgument(){
    int argc=0;
    auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(!argv)return false;
    const bool apply=argc==4&&std::wstring_view(argv[1])==L"--apil-apply";
    if(!apply){LocalFree(argv);return false;}
    const DWORD pid=wcstoul(argv[2],nullptr,10);
    const std::filesystem::path target=argv[3];
    LocalFree(argv);
    if(!pid||!target.is_absolute()||target.extension()!=L".exe")return true;
    if(HANDLE old=OpenProcess(SYNCHRONIZE,FALSE,pid)){
        const auto result=WaitForSingleObject(old,30000);CloseHandle(old);
        if(result!=WAIT_OBJECT_0)return true;
    }
    auto staged=target;staged+=L".apil-"+std::to_wstring(GetCurrentProcessId())+L".tmp";
    const auto source=executable();
    bool replaced=false;
    if(!source.empty()&&CopyFileW(source.c_str(),staged.c_str(),TRUE)){
        SetFileAttributesW(staged.c_str(),FILE_ATTRIBUTE_HIDDEN);
        if(MoveFileExW(staged.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
            replaced=true;SetFileAttributesW(target.c_str(),FILE_ATTRIBUTE_NORMAL);
        }
        else DeleteFileW(staged.c_str());
    }
    if(!replaced&&!source.empty()){auto failure=source;failure+=L".failed";std::ofstream(failure).put('1');}
    launch(target,{});
    return true;
}
class Updater {
    std::thread worker;
    std::atomic<bool> running=false;
    std::mutex mutex;
    std::wstring status=L"Проверка обновлений…";
    std::filesystem::path staged;
    uint64_t next=0;

    void set(std::wstring text){std::lock_guard<std::mutex> lock(mutex);status=std::move(text);}
    void check(const std::filesystem::path& dir){
        constexpr size_t maxBinary=32*1024*1024;
        set(L"Проверка обновлений…");
        auto bytes=download(L"/insurgut/gta-2699-autopilot/main/dist/update.txt",512);
        std::istringstream in(std::string(bytes.begin(),bytes.end()));
        std::string version,hash;
        size_t size=0;
        std::array<unsigned,3> remote{},current{};
        if(!(in>>version>>size>>hash)||size<1024||size>maxBinary||
           hash.size()!=64||hash.find_first_not_of("0123456789abcdef")!=std::string::npos||
           !parseVersion(version,remote)||!parseVersion(apilVersion,current)){
            set(L"Проверка недоступна");return;
        }
        if(remote<=current){set(L"Обновлений нет");return;}
        const std::wstring label(version.begin(),version.end());
        const auto file=dir/(L"APIL-"+label+L".exe");
        auto failure=file;failure+=L".failed";
        std::error_code failureCheck;
        if(std::filesystem::exists(failure,failureCheck)){
            set(L"Нет доступа для замены EXE");return;
        }
        set(L"Загрузка "+label+L"…");
        auto image=download(L"/insurgut/gta-2699-autopilot/main/dist/APIL.exe",size);
        if(image.size()!=size||image[0]!='M'||image[1]!='Z'||sha256(image)!=hash){
            set(L"Проверка файла не прошла");return;
        }
        std::error_code ec;std::filesystem::create_directories(dir,ec);
        if(ec){set(L"Папка обновлений недоступна");return;}
        auto temp=file;temp+=L".tmp";
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        out.write(reinterpret_cast<const char*>(image.data()),std::streamsize(image.size()));out.close();
        if(!out||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING)){
            set(L"Ошибка сохранения обновления");return;
        }
        std::lock_guard<std::mutex> lock(mutex);
        staged=file;status=L"Готово: "+label+L". Жду паузу.";
    }
public:
    ~Updater(){if(worker.joinable())worker.join();}
    std::wstring text(){std::lock_guard<std::mutex> lock(mutex);return status;}
    void tick(uint64_t now,const std::filesystem::path& dir){
        if(running||now<next)return;
        if(worker.joinable())worker.join();
        {std::lock_guard<std::mutex> lock(mutex);if(!staged.empty())return;}
        next=now+15*60*1000;
        running=true;
        try{
            worker=std::thread([this,dir]{
                try{check(dir);}catch(...){set(L"Проверка недоступна");}
                running=false;
            });
        }catch(...){running=false;set(L"Проверка недоступна");}
    }
    bool apply(){
        std::lock_guard<std::mutex> lock(mutex);
        if(staged.empty())return false;
        const auto target=executable();
        if(target.empty()||!launch(staged,L" --apil-apply "+std::to_wstring(GetCurrentProcessId())+L" \""+target.wstring()+L"\"")){
            staged.clear();status=L"Не удалось запустить обновление";return false;
        }
        return true;
    }
};
}
