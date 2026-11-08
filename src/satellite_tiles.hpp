#pragma once
#include "archive_map.hpp"
#include "tile_download_budget.hpp"
#include "navigator_prefetch.hpp"
#include <windows.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

struct MapTileProgress {
    struct Completion {uint64_t serial=0;int key=-1;bool missing=false;};
    struct Transfer {int key=-1;size_t received=0,total=0;};
    std::array<Transfer,TileDownloadBudget::maximum> active{};
    uint64_t serial=0;
    std::array<Completion,64> completed{};
};

class SatelliteTiles {
    static constexpr int size=ArchiveTile::size,zoom=8,radius=2,side=radius*2+1;
    static constexpr int columns=ArchiveTile::columns(zoom),rows=ArchiveTile::rows(zoom);
    static constexpr float scale=ArchiveTile::scale(zoom);
    static constexpr size_t maxBytes=256*1024;
    using Tile=std::array<uint32_t,size*size>;
    using GetInfo=int(*)(const uint8_t*,size_t,int*,int*);
    using Decode=uint8_t*(*)(const uint8_t*,size_t,uint8_t*,size_t,int);
    struct Entry {std::shared_ptr<const Tile> tile;uint64_t used=0,retry=0;bool missing=false;};
    struct InternetHandle {
        HINTERNET handle;
        ~InternetHandle(){if(handle)WinHttpCloseHandle(handle);}
        operator HINTERNET()const{return handle;}
    };
    HMODULE decoder=nullptr;
    GetInfo getInfo=nullptr;
    Decode decode=nullptr;
    std::array<std::thread,TileDownloadBudget::maximum> workers;
    std::mutex mutex;
    mutable std::mutex decoderMutex;
    std::condition_variable signal;
    std::unordered_map<int,Entry> cache;
    std::deque<int> pending,restarts,ahead;
    std::bitset<columns*rows> knownDetailed;
    struct Job {int key=-1;bool interrupted=false;};
    std::array<Job,TileDownloadBudget::maximum> jobs{};
    ArchiveSweep sweep;
    size_t bulkLevel=0;
    TileDownloadBudget budget;
    unsigned running=0,bulkRunning=0,priorityRun=0;
    bool closing=false,bulkExhausted=false,retryPass=false;
    std::filesystem::path root;
    int lastCentre=-1;
    uint64_t nextQueue=0,nextAhead=0;
    MapTileProgress progress;

    enum class TileStatus {Ready,Missing,Retry,Busy};

    static bool exists(const std::filesystem::path& path){
        std::error_code error;
        return std::filesystem::is_regular_file(path,error);
    }
    std::filesystem::path pathFor(ArchiveTile tile)const{
        return root/std::to_wstring(tile.zoom)/std::to_wstring(tile.x)/(std::to_wstring(tile.y)+L".webp");
    }
    static bool writeAtomic(const std::filesystem::path& path,const std::vector<uint8_t>& data){
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(),error);
        if(error)return false;
        auto temp=path;temp+=L".tmp";
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!data.empty())out.write(reinterpret_cast<const char*>(data.data()),data.size());
        out.close();
        if(out&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING))return true;
        std::filesystem::remove(temp,error);
        return false;
    }
    bool validWebp(const std::vector<uint8_t>& data)const{
        std::lock_guard<std::mutex> lock(decoderMutex);
        int w=0,h=0;
        return getInfo(data.data(),data.size(),&w,&h)&&w==size&&h==size;
    }
    std::shared_ptr<Tile> readTile(const std::filesystem::path& path)const{
        std::error_code error;
        const auto bytes=std::filesystem::file_size(path,error);
        if(error||!bytes||bytes>maxBytes)return {};
        std::vector<uint8_t> data(static_cast<size_t>(bytes));
        std::ifstream in(path,std::ios::binary);
        if(!in.read(reinterpret_cast<char*>(data.data()),data.size())||!validWebp(data))return {};
        auto tile=std::make_shared<Tile>();
        std::lock_guard<std::mutex> lock(decoderMutex);
        if(!decode(data.data(),data.size(),reinterpret_cast<uint8_t*>(tile->data()),sizeof(Tile),size*4))return {};
        return tile;
    }
    void recordCompletion(int key,bool missing){
        const auto serial=++progress.serial;
        progress.completed[(serial-1)%progress.completed.size()]={serial,key,missing};
    }
    bool isLoading(int key)const{
        return std::any_of(jobs.begin(),jobs.end(),[&](const Job& job){return job.key==key;});
    }
    bool reportDownload(unsigned slot,size_t received,size_t total){
        std::lock_guard<std::mutex> lock(mutex);
        if(closing||jobs[slot].interrupted)return false;
        progress.active[slot]={jobs[slot].key,received,total};
        return true;
    }
    TileStatus fetch(HINTERNET connection,ArchiveTile tile,unsigned slot,std::vector<uint8_t>& data,uint64_t& retryDelay){
        if(!connection)return TileStatus::Retry;
        if(!reportDownload(slot,0,0))return TileStatus::Retry;
        const auto url=L"/maptiles/hd/gta5/"+std::to_wstring(tile.zoom)+L"/"+
                       std::to_wstring(tile.x)+L"/"+std::to_wstring(tile.y)+L".webp?v=20261101";
        InternetHandle request{WinHttpOpenRequest(connection,L"GET",url.c_str(),nullptr,
                                                  WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
        if(!request||!WinHttpSendRequest(request,WINHTTP_NO_ADDITIONAL_HEADERS,0,nullptr,0,0,0)||
           !WinHttpReceiveResponse(request,nullptr))return TileStatus::Retry;
        DWORD status=0,statusSize=sizeof(status);
        if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,&status,&statusSize,WINHTTP_NO_HEADER_INDEX))return TileStatus::Retry;
        if(status==429||status==503){
            DWORD seconds=0,bytes=sizeof(seconds);
            if(WinHttpQueryHeaders(request,WINHTTP_QUERY_RETRY_AFTER|WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX,&seconds,&bytes,WINHTTP_NO_HEADER_INDEX))
                retryDelay=std::max(retryDelay,uint64_t(seconds)*1000);
            return TileStatus::Busy;
        }
        if(status==404)return TileStatus::Missing;
        if(status!=200)return TileStatus::Retry;
        DWORD length=0,lengthSize=sizeof(length);
        if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,&length,&lengthSize,WINHTTP_NO_HEADER_INDEX))length=0;
        if(length>maxBytes)return TileStatus::Retry;
        if(!reportDownload(slot,0,length))return TileStatus::Retry;
        std::array<uint8_t,8192> buffer;
        DWORD received=0;
        do{
            if(!WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&received)||
               data.size()+received>maxBytes)return TileStatus::Retry;
            data.insert(data.end(),buffer.begin(),buffer.begin()+received);
            if(!reportDownload(slot,data.size(),length))return TileStatus::Retry;
        }while(received);
        return validWebp(data)?TileStatus::Ready:TileStatus::Retry;
    }

    void run(unsigned slot){
        InternetHandle session{WinHttpOpen(L"APIL/0.2.00",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                           WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
        if(session)WinHttpSetTimeouts(session,2000,2000,2000,2500);
        InternetHandle connection{session?WinHttpConnect(session,L"archive-media.l7y.ca",INTERNET_DEFAULT_HTTPS_PORT,0):nullptr};
        for(;;){
            std::unique_lock<std::mutex> lock(mutex);
            ArchiveTile tile;
            bool restart=false,urgent=false,prefetch=false;
            for(;;){
                if(closing)return;
                const auto now=GetTickCount64();
                if(budget.retryAt()>now){
                    signal.wait_for(lock,std::chrono::milliseconds(budget.retryAt()-now));
                    continue;
                }
                if(running>=budget.parallelism()){signal.wait(lock);continue;}
                if(!bulkRunning&&(bulkExhausted||sweep.level!=bulkLevel)){
                    if(retryPass){
                        sweep={bulkLevel,0,0};
                        bulkExhausted=false;
                        retryPass=false;
                    }else bulkLevel=sweep.level;
                }
                auto manual=std::find_if(restarts.begin(),restarts.end(),[&](int key){return !isLoading(key);});
                while(!ahead.empty()){
                    const auto tile=ArchiveTile::fromKey(ahead.front());
                    if(!isLoading(tile.key())&&!knownDetailed.test(tile.y*columns+tile.x))break;
                    ahead.pop_front();
                }
                const bool bulkReady=!bulkExhausted&&sweep.level==bulkLevel&&!isLoading(sweep.tile().key());
                const bool priorityReady=!bulkReady||priorityRun<4;
                if(manual!=restarts.end()){
                    restart=urgent=true;
                    tile=ArchiveTile::fromKey(*manual);
                    restarts.erase(manual);
                }else if(!pending.empty()&&priorityReady){
                    urgent=true;
                    tile=ArchiveTile::fromKey(pending.front());
                    pending.pop_front();
                    ++priorityRun;
                }else if(!ahead.empty()&&priorityReady){
                    urgent=prefetch=true;
                    tile=ArchiveTile::fromKey(ahead.front());
                    ahead.pop_front();
                    ++priorityRun;
                }else if(bulkReady){
                    tile=sweep.tile();
                    pending.erase(std::remove(pending.begin(),pending.end(),tile.key()),pending.end());
                    bulkExhausted=!sweep.advance();
                    ++bulkRunning;
                    priorityRun=0;
                }else{
                    signal.wait(lock);
                    continue;
                }
                break;
            }
            jobs[slot]={tile.key(),false};
            ++running;
            lock.unlock();

            const auto path=pathFor(tile);
            auto missing=path;missing+=L".missing";
            auto status=TileStatus::Ready;
            std::shared_ptr<Tile> decoded;
            bool onDisk=exists(path),attempted=false;
            uint64_t retryDelay=3000;
            if(urgent&&!prefetch&&onDisk&&!restart){
                auto checked=readTile(path);
                onDisk=bool(checked);
                decoded=std::move(checked);
                if(!onDisk){std::error_code error;std::filesystem::remove(path,error);}
            }
            if(!onDisk||restart){
                if(!restart&&exists(missing))status=TileStatus::Missing;
                else{
                    std::vector<uint8_t> bytes;
                    attempted=true;
                    status=fetch(connection,tile,slot,bytes,retryDelay);
                    if(status==TileStatus::Ready&&!writeAtomic(path,bytes))status=TileStatus::Retry;
                    if(status==TileStatus::Missing&&onDisk)status=TileStatus::Ready;
                    else if(status==TileStatus::Missing&&!writeAtomic(missing,{}))status=TileStatus::Retry;
                    if(status==TileStatus::Ready&&urgent&&!prefetch)decoded=readTile(path);
                }
            }
            lock.lock();
            const bool interrupted=jobs[slot].interrupted;
            const auto now=GetTickCount64();
            const bool absent=status==TileStatus::Missing;
            const bool complete=status==TileStatus::Ready||absent;
            if(status==TileStatus::Busy)budget.busy(now,retryDelay);
            else if(attempted&&!interrupted){
                if(complete)budget.success();
                else budget.failed(now);
            }
            if(!interrupted&&tile.zoom==zoom)knownDetailed.set(tile.y*columns+tile.x,complete||onDisk);
            if(!interrupted&&(attempted||(urgent&&!prefetch))&&complete)recordCompletion(tile.key(),absent);
            if(urgent&&!prefetch&&!interrupted){
                cache[tile.key()]={std::move(decoded),now,now+(absent?300000:15000),absent};
                if(cache.size()>96){
                    auto oldest=std::min_element(cache.begin(),cache.end(),[](const auto& a,const auto& b){
                        return a.second.used<b.second.used;
                    });
                    cache.erase(oldest);
                }
            }
            if(!urgent){
                retryPass=retryPass||!complete||interrupted;
                --bulkRunning;
            }
            jobs[slot]={};
            progress.active[slot]={};
            --running;
            signal.notify_all();
        }
    }
    void enqueue(int key,bool first=false){
        if(isLoading(key)||std::find(pending.begin(),pending.end(),key)!=pending.end()||
           std::find(restarts.begin(),restarts.end(),key)!=restarts.end())return;
        if(pending.size()>=256){
            if(!first)return;
            pending.pop_back();
        }
        if(first)pending.push_front(key);else pending.push_back(key);
        signal.notify_one();
    }
public:
    struct Snapshot {
        int left=0,top=0;
        std::array<std::shared_ptr<const Tile>,side*side> tiles{};
        bool sample(float x,float y,uint32_t& color)const{
            const float u=(x+4140)*scale-.5f,v=(8400-y)*scale-.5f;
            if(!(u>=0&&u<columns*size&&v>=0&&v<rows*size))return false;
            const int ix=int(u),iy=int(v);
            const int tx=ix/size-left,ty=iy/size-top;
            if(tx<0||tx>=side||ty<0||ty>=side)return false;
            const auto& tile=tiles[size_t(ty)*side+tx];
            if(!tile)return false;
            const int px=ix%size,py=iy%size;
            const int x1=std::min(px+1,size-1),y1=std::min(py+1,size-1);
            const float a=u-ix,b=v-iy;
            const int nearestX=a<.5f?px:x1,nearestY=b<.5f?py:y1;
            if(((*tile)[nearestY*size+nearestX]>>24)<128)return false;
            const uint32_t p=(*tile)[py*size+px],q=(*tile)[py*size+x1];
            const uint32_t r=(*tile)[y1*size+px],s=(*tile)[y1*size+x1];
            color=0;
            for(unsigned shift:{0u,8u,16u}){
                const float upper=((p>>shift)&255)*(1-a)+((q>>shift)&255)*a;
                const float lower=((r>>shift)&255)*(1-a)+((s>>shift)&255)*a;
                color|=uint32_t(upper+(lower-upper)*b+.5f)<<shift;
            }
            return true;
        }
    };
    ~SatelliteTiles(){
        {std::lock_guard<std::mutex> lock(mutex);closing=true;signal.notify_all();}
        for(auto& worker:workers)if(worker.joinable())worker.join();
        if(decoder)FreeLibrary(decoder);
    }
    void load(const std::filesystem::path& file){
        decoder=LoadLibraryExW(file.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!decoder)return;
        getInfo=reinterpret_cast<GetInfo>(GetProcAddress(decoder,"WebPGetInfo"));
        decode=reinterpret_cast<Decode>(GetProcAddress(decoder,"WebPDecodeBGRAInto"));
    }
    void start(const std::filesystem::path& directory){
        if(!getInfo||!decode||workers.front().joinable())return;
        std::error_code error;
        std::filesystem::create_directories(directory,error);
        if(error)return;
        root=directory;
        for(unsigned slot=0;slot<workers.size();++slot){
            try{workers[slot]=std::thread([this,slot]{run(slot);});}
            catch(const std::system_error&){break;}
        }
    }
    MapTileProgress downloadProgress(){
        std::lock_guard<std::mutex> lock(mutex);
        return progress;
    }
    void restart(ArchiveTile tile){
        if(!workers.front().joinable()||!tile.valid())return;
        std::lock_guard<std::mutex> lock(mutex);
        const int key=tile.key();
        for(size_t slot=0;slot<jobs.size();++slot)if(jobs[slot].key==key){
            jobs[slot].interrupted=true;
            progress.active[slot]={};
            break;
        }
        cache.erase(key);
        pending.erase(std::remove(pending.begin(),pending.end(),key),pending.end());
        restarts.erase(std::remove(restarts.begin(),restarts.end(),key),restarts.end());
        if(restarts.size()>=16)restarts.pop_back();
        restarts.push_front(key);
        signal.notify_one();
    }
    void prioritize(ArchiveTile tile){
        if(!workers.front().joinable()||!tile.valid())return;
        std::lock_guard<std::mutex> lock(mutex);
        const auto entry=cache.find(tile.key());
        if(entry==cache.end()||(!entry->second.tile&&GetTickCount64()>=entry->second.retry))enqueue(tile.key());
        else if(entry->second.missing)recordCompletion(tile.key(),true);
    }
    std::shared_ptr<const Tile> image(ArchiveTile tile){
        if(!workers.front().joinable()||!tile.valid())return {};
        std::lock_guard<std::mutex> lock(mutex);
        const auto now=GetTickCount64();
        const auto entry=cache.find(tile.key());
        if(entry!=cache.end()&&entry->second.tile){entry->second.used=now;return entry->second.tile;}
        if(entry==cache.end()||now>=entry->second.retry)enqueue(tile.key());
        return {};
    }
    Snapshot snapshot(float x,float y,float fx,float fy,float speed){
        Snapshot view;
        const float u=(x+4140)*scale/size,v=(8400-y)*scale/size;
        if(!workers.front().joinable()||!(u>=0&&u<columns&&v>=0&&v<rows))return view;
        const int cx=int(u),cy=int(v),centre=ArchiveTile{zoom,cx,cy}.key();
        view.left=cx-radius;view.top=cy-radius;
        const auto now=GetTickCount64();
        std::lock_guard<std::mutex> lock(mutex);
        if(centre!=lastCentre||now>=nextAhead){
            ahead.clear();
            for(const auto& tile:navigatorPrefetch(x,y,fx,fy,speed,radius)){
                if(!knownDetailed.test(tile.y*columns+tile.x)&&!isLoading(tile.key()))
                    ahead.push_back(tile.key());
            }
            nextAhead=now+1800;
            if(!ahead.empty())signal.notify_all();
        }
        const bool queue=centre!=lastCentre||now>=nextQueue;
        if(queue){lastCentre=centre;nextQueue=now+300;}
        for(int ring=radius;ring>=0;--ring)
            for(int ty=cy-ring;ty<=cy+ring;++ty)for(int tx=cx-ring;tx<=cx+ring;++tx){
                if(std::max(std::abs(tx-cx),std::abs(ty-cy))!=ring||tx<0||ty<0||tx>=columns||ty>=rows)continue;
                const int key=ArchiveTile{zoom,tx,ty}.key();
                const auto entry=cache.find(key);
                if(entry!=cache.end()&&entry->second.tile){
                    entry->second.used=now;
                    view.tiles[size_t(ty-view.top)*side+tx-view.left]=entry->second.tile;
                }else if(queue&&(entry==cache.end()||now>=entry->second.retry))enqueue(key,true);
            }
        return view;
    }
};
