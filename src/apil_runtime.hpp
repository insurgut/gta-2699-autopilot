#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>

inline bool unpackApilRuntime(const std::filesystem::path& root){
    const wchar_t* files[]={
        L"bin/DistrictTelemetry.exe",L"bin/DistrictWebP.dll",L"assets/map/navigator-atlas.bmp",
        L"assets/vehicles/01_passenger_car.obj",L"assets/vehicles/02_rigid_truck.obj",
        L"assets/vehicles/03_semi_truck_trailer.obj",L"assets/vehicles/04_bus.obj",
        L"assets/vehicles/vehicle_materials.mtl"
    };
    const auto exe=GetModuleHandleW(nullptr);
    for(unsigned i=0;i<sizeof(files)/sizeof(files[0]);++i){
        const auto resource=FindResourceW(exe,MAKEINTRESOURCEW(101+i),MAKEINTRESOURCEW(10));
        const auto bytes=resource?SizeofResource(exe,resource):0;
        const auto loaded=resource?LoadResource(exe,resource):nullptr;
        const auto data=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
        if(!bytes||!data)return false;
        const auto file=root/files[i];
        std::error_code ec;
        if(std::filesystem::file_size(file,ec)==bytes&&!ec)continue;
        std::filesystem::create_directories(file.parent_path(),ec);
        if(ec)return false;
        auto temp=file;temp+=L".tmp";
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        out.write(data,bytes);out.close();
        if(!out||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING))return false;
    }
    return true;
}
