#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

struct TripPoint {
    float x=0, y=0, z=0;
    uint32_t flags=0;
};
static_assert(sizeof(TripPoint)==16);
class TripHistory {
    std::filesystem::path file;
    std::vector<TripPoint> pending;
    TripPoint previous{};
    uint64_t vehicle=0, beat=0, lastFlush=0, persisted=0;
    bool writable=false;
    double meters=0;
    static float gap(const TripPoint& a,const TripPoint& b) {
        return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);
    }
public:
    enum : uint32_t { NewTrip=1, Auto=2 };
    void open(const std::filesystem::path& path) {
        file=path;
        std::error_code ec;
        const bool exists=std::filesystem::exists(path,ec);
        if(ec)return;
        const auto bytes=exists?std::filesystem::file_size(path,ec):0;
        if(ec)return;
        persisted=bytes/sizeof(TripPoint);
        if(bytes%sizeof(TripPoint))std::filesystem::resize_file(path,persisted*sizeof(TripPoint),ec);
        if(ec)return;
        if(exists){
            std::ifstream input(path,std::ios::binary);
            TripPoint point{}, prior{};
            uint64_t read=0;
            while(input.read(reinterpret_cast<char*>(&point),sizeof(point))){
                if(read&&!(point.flags&NewTrip)){
                    const float length=gap(prior,point);
                    if(std::isfinite(length)&&length<=100)meters+=length;
                }
                prior=point;
                ++read;
            }
            if(read!=persisted)return;
        }
        writable=true;
    }
    void observe(uint64_t now,uint64_t entity,TripPoint point,bool automatic) {
        if(!entity||!std::isfinite(point.x)||!std::isfinite(point.y)||!std::isfinite(point.z)){
            vehicle=0;
            return;
        }
        if(entity==vehicle&&now>=beat&&now-beat<120)return;
        const float distance=gap(previous,point);
        const bool split=entity!=vehicle||now<beat||now-beat>1500||
            distance>std::min(100.0f,8.0f+float(now-beat)*.13f);
        point.flags=(split?uint32_t(NewTrip):0u)|(automatic?uint32_t(Auto):0u);
        beat=now;
        vehicle=entity;
        if(!split&&distance<1.25f&&((previous.flags^point.flags)&Auto)==0)return;
        if(!split)meters+=distance;
        previous=point;
        if(writable&&pending.size()<8192)pending.push_back(point);
        else if(writable)vehicle=0;
    }
    bool flush(uint64_t now,bool force=false) {
        if(!writable||pending.empty())return writable;
        if(!force&&now-lastFlush<2000)return true;
        lastFlush=now;
        std::ofstream output(file,std::ios::binary|std::ios::app);
        output.write(reinterpret_cast<const char*>(pending.data()),std::streamsize(pending.size()*sizeof(TripPoint)));
        output.close();
        if(!output){
            std::error_code ec;
            if(std::filesystem::exists(file,ec))std::filesystem::resize_file(file,persisted*sizeof(TripPoint),ec);
            if(ec)writable=false;
            return false;
        }
        persisted+=pending.size();
        pending.clear();
        return true;
    }
    uint64_t size()const{return persisted;}
    double distanceKm()const{return meters/1000;}
    std::vector<TripPoint> read(uint64_t offset,size_t limit)const {
        if(offset>=persisted)return {};
        std::vector<TripPoint> out(size_t(std::min<uint64_t>(limit,persisted-offset)));
        std::ifstream input(file,std::ios::binary);
        input.seekg(std::streamoff(offset*sizeof(TripPoint)));
        input.read(reinterpret_cast<char*>(out.data()),std::streamsize(out.size()*sizeof(TripPoint)));
        out.resize(size_t(input.gcount())/sizeof(TripPoint));
        return out;
    }
};
