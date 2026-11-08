#pragma once
#include "radar.hpp"
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <vector>
#include <cstdint>
inline float CELL=1.25f;
inline float DRAW_RADIUS=20.0f;
inline bool setDrawRadius(float value){if(!std::isfinite(value)||value<1||value>100)return false;DRAW_RADIUS=value;return true;}
inline bool setCellSize(float value){if(!std::isfinite(value)||value<0.10f||value>5.0f)return false;CELL=value;return true;}
struct Cell {int x,y,z;bool operator==(const Cell&c)const{return x==c.x&&y==c.y&&z==c.z;}};
struct CellHash {size_t operator()(const Cell&c)const{return size_t(uint32_t(c.x)*73856093u)^size_t(uint32_t(c.y)*19349663u)^size_t(uint32_t(c.z)*83492791u);}};
inline Cell cellAt(float x,float y,float z){return {int(std::floor(x/CELL)),int(std::floor(y/CELL)),int(std::floor(z/CELL))};}
struct GridPoint {float x,y,z;};
class VoxelGrid {
public:
    std::unordered_set<Cell,CellHash> occupied,contact,passable,observed,waiting;
    bool complete=true;float halfWidth=1.05f;
    std::vector<uint32_t> prefix;int ox=0,oy=0,nx=0,ny=0;size_t cachedOccupied=0,cachedContact=0;
    void clear(){occupied.clear();contact.clear();passable.clear();observed.clear();waiting.clear();prefix.clear();contactTime=0;complete=true;}
    void buildPrefix(float px,float py){
        auto lo=cellAt(px-33,py-33,0),hi=cellAt(px+33,py+33,0);ox=lo.x;oy=lo.y;nx=hi.x-lo.x+1;ny=hi.y-lo.y+1;
        prefix.assign(size_t(nx+1)*(ny+1),0);
        for(int y=0;y<ny;y++){uint32_t row=0;for(int x=0;x<nx;x++){
            Cell c{x+ox,y+oy,0};row+=occupied.count(c)||contact.count(c);
            prefix[size_t(y+1)*(nx+1)+x+1]=prefix[size_t(y)*(nx+1)+x+1]+row;
        }}cachedOccupied=occupied.size();cachedContact=contact.size();
    }
    bool rectangleBlocked(int x0,int y0,int x1,int y1)const{
        if(!prefix.empty()&&cachedOccupied==occupied.size()&&cachedContact==contact.size()){
            if(x0<ox||y0<oy||x1>=ox+nx||y1>=oy+ny)return true;
            int a=x0-ox,b=y0-oy,c=x1-ox+1,d=y1-oy+1;
            return prefix[size_t(d)*(nx+1)+c]+prefix[size_t(b)*(nx+1)+a]>prefix[size_t(b)*(nx+1)+c]+prefix[size_t(d)*(nx+1)+a];
        }
        for(int x=x0;x<=x1;x++)for(int y=y0;y<=y1;y++)if(occupied.count({x,y,0})||contact.count({x,y,0}))return true;
        return false;
    }
    uint64_t contactTime=0;
    void rebuild(const RadarBox*boxes,size_t count,float px,float py,float pz,uint64_t now,bool makePrefix=true){
        occupied.clear();passable.clear();observed.clear();waiting.clear();prefix.clear();complete=true;
        if(contactTime&&now-contactTime>60000){contact.clear();contactTime=0;}
        for(size_t i=0;i<count;i++){
            const auto&b=boxes[i];if(b.z+b.halfHeight<pz+0.1f||b.z-b.halfHeight>pz+1.6f)continue;
            float norm=std::hypot(b.fx,b.fy);if(norm<0.5f)continue;float fx=b.fx/norm,fy=b.fy/norm;
            float ex=std::abs(fy)*b.halfWidth+std::abs(fx)*b.halfLength,ey=std::abs(fx)*b.halfWidth+std::abs(fy)*b.halfLength;
            auto lo=cellAt(std::max(b.x-ex,px-32),std::max(b.y-ey,py-32),0);
            auto hi=cellAt(std::min(b.x+ex,px+32),std::min(b.y+ey,py+32),0);
            for(int x=lo.x;x<=hi.x;x++)for(int y=lo.y;y<=hi.y;y++){
                float dx=(x+0.5f)*CELL-b.x,dy=(y+0.5f)*CELL-b.y;
                float projection=(std::abs(fx)+std::abs(fy))*CELL/2;
                if(std::abs(dx*fx+dy*fy)>b.halfLength+projection||std::abs(dx*fy-dy*fx)>b.halfWidth+projection)continue;
                if(std::abs(dx)>ex+CELL/2||std::abs(dy)>ey+CELL/2)continue;
                if(passableRule(b.passability)){passable.insert({x,y,0});if(b.passability==3)observed.insert({x,y,0});}else occupied.insert({x,y,0});
                if(b.passability==WAIT_OBJECT||b.passability==WAIT_PASS)waiting.insert({x,y,0});
                if(occupied.size()+passable.size()>300000){complete=false;if(makePrefix)buildPrefix(px,py);return;}
            }
        }if(makePrefix)buildPrefix(px,py);
    }
    void markContact(float x,float y,float,uint64_t now){auto c=cellAt(x,y,0);contact.insert(c);contactTime=now;}
    bool blocked(int x,int y,float)const{return occupied.count({x,y,0})||contact.count({x,y,0});}
    int columnState(int x,int y,float)const{
        Cell c{x,y,0};if(waiting.count(c))return 6;if(occupied.count(c))return 3;if(contact.count(c))return 2;if(observed.count(c))return 5;if(passable.count(c))return 4;return 1;
    }
    bool traversable(int x,int y,float z)const{
        float cx=(x+0.5f)*CELL,cy=(y+0.5f)*CELL;
        auto lo=cellAt(cx-halfWidth,cy-halfWidth,z),hi=cellAt(cx+halfWidth,cy+halfWidth,z);
        return !rectangleBlocked(lo.x,lo.y,hi.x,hi.y);
    }
    std::vector<GridPoint> plan(GridPoint start,GridPoint goal)const{
        Cell a=cellAt(start.x,start.y,start.z),b=cellAt(goal.x,goal.y,start.z);b.z=a.z;
        int stride=std::max(1,int(std::ceil(0.75f/CELL)));
        b.x=a.x+int(std::round(float(b.x-a.x)/stride))*stride;b.y=a.y+int(std::round(float(b.y-a.y)/stride))*stride;
        if(!traversable(b.x,b.y,start.z))return {};
        struct Node {Cell c;float g,f;bool operator<(const Node&n)const{return f>n.f;}};
        std::priority_queue<Node> open;std::unordered_map<Cell,float,CellHash> costs;std::unordered_map<Cell,Cell,CellHash> parent;
        auto heuristic=[&](Cell c){return std::hypot(float(c.x-b.x),float(c.y-b.y))/stride;};
        costs[a]=0;open.push({a,0,heuristic(a)});unsigned expanded=0;bool found=false;
        while(!open.empty()&&expanded++<15000){auto n=open.top();open.pop();if(n.g>costs[n.c]+0.001f)continue;if(n.c==b){found=true;break;}
            for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++){
                if(!dx&&!dy)continue;
                Cell c{n.c.x+dx*stride,n.c.y+dy*stride,a.z};
                if(std::abs(c.x-a.x)*CELL>32||std::abs(c.y-a.y)*CELL>32||!traversable(c.x,c.y,start.z))continue;
                if(dx&&dy&&(!traversable(n.c.x+dx*stride,n.c.y,start.z)||!traversable(n.c.x,n.c.y+dy*stride,start.z)))continue;
                if(!pathClear({(n.c.x+0.5f)*CELL,(n.c.y+0.5f)*CELL,start.z},{(c.x+0.5f)*CELL,(c.y+0.5f)*CELL,start.z}))continue;
                float g=n.g+(dx&&dy?1.41421356f:1.0f);auto old=costs.find(c);if(old!=costs.end()&&old->second<=g)continue;
                costs[c]=g;parent[c]=n.c;open.push({c,g,g+heuristic(c)});
            }
        }
        if(!found)return {};
        std::vector<GridPoint> path;Cell c=b;
        while(!(c==a)){path.push_back({(c.x+0.5f)*CELL,(c.y+0.5f)*CELL,start.z});auto it=parent.find(c);if(it==parent.end())return {};c=it->second;}
        std::reverse(path.begin(),path.end());return path;
    }
    bool pathClear(GridPoint start,GridPoint end)const{
        float d=std::hypot(end.x-start.x,end.y-start.y);int steps=std::max(1,int(std::ceil(d/std::min(0.5f,CELL/2))));
        for(int i=0;i<=steps;i++){float t=float(i)/steps;float x=start.x+(end.x-start.x)*t,y=start.y+(end.y-start.y)*t;
            auto lo=cellAt(x-halfWidth,y-halfWidth,start.z),hi=cellAt(x+halfWidth,y+halfWidth,start.z);
            if(rectangleBlocked(lo.x,lo.y,hi.x,hi.y))return false;
        }return true;
    }
};
