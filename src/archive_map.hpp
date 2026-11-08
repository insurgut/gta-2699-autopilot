#pragma once
#include <array>
#include <cstddef>

inline constexpr wchar_t archiveCacheFolder[]=L"ArchiVe-20261101";

struct ArchiveTile {
    int zoom=0,x=0,y=0;
    static constexpr int size=256;
    static constexpr std::array<int,3> levels{8,6,4};
    static constexpr int columns(int z){return 1<<(z-1);}
    static constexpr int rows(int z){return columns(z)*3/2;}
    static constexpr float scale(int z){return float(columns(z)*size)/9000;}
    bool valid()const{
        if(zoom!=4&&zoom!=6&&zoom!=8)return false;
        return x>=0&&y>=0&&x<columns(zoom)&&y<rows(zoom);
    }
    int key()const{return (zoom<<16)|(y<<8)|x;}
    static ArchiveTile fromKey(int key){return {key>>16,key&255,(key>>8)&255};}
};
struct ArchiveSweep {
    std::size_t level=0;
    int x=0,y=0;
    ArchiveTile tile()const{return {ArchiveTile::levels[level],x,y};}
    bool advance(){
        const int zoom=ArchiveTile::levels[level];
        if(++x<ArchiveTile::columns(zoom))return true;
        x=0;
        if(++y<ArchiveTile::rows(zoom))return true;
        y=0;
        if(++level<ArchiveTile::levels.size())return true;
        level=0;return false;
    }
};
