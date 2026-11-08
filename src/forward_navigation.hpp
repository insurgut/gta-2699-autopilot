#pragma once
#include "radar.hpp"
#include "route.hpp"
#include "speed_profile.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <limits>
#include <cstdint>
#include <chrono>
constexpr float NAV_PI=3.14159265359f;
inline float navAngle(float a){return std::remainder(a,2*NAV_PI);}
struct NavPose {float x=0,y=0,z=0,yaw=0;};
class NavigationScene {
    struct Shape {RadarBox b;float fx,fy,ex,ey;};std::vector<Shape> shapes;
public:
    float halfWidth=1.05f,halfLength=2.5f,margin=.12f;
    NavigationScene(const RadarBox*boxes,size_t count,float z,float w,float l,float cx=0,float cy=0,bool localOnly=false):halfWidth(w>0?w:1.05f),halfLength(l>0?l:2.5f){
        shapes.reserve(count);
        for(size_t i=0;i<count;i++){auto b=boxes[i];float n=std::hypot(b.fx,b.fy);
            if(passableRule(b.passability)||b.z+b.halfHeight<z+.1f||b.z-b.halfHeight>z+1.6f||n<.5f)continue;
            float fx=b.fx/n,fy=b.fy/n,ex=std::abs(fy)*b.halfWidth+std::abs(fx)*b.halfLength,ey=std::abs(fx)*b.halfWidth+std::abs(fy)*b.halfLength;if(localOnly&&(std::abs(b.x-cx)>ex+55||std::abs(b.y-cy)>ey+55))continue;shapes.push_back({b,fx,fy,ex,ey});
        }
    }
    bool clear(NavPose p,float extra=0)const{
        float fx=std::sin(p.yaw),fy=std::cos(p.yaw),w=halfWidth+margin+extra,l=halfLength+margin+extra;
        float ex=std::abs(fy)*w+std::abs(fx)*l,ey=std::abs(fx)*w+std::abs(fy)*l;
        for(const auto&s:shapes){float dx=s.b.x-p.x,dy=s.b.y-p.y;if(std::abs(dx)>ex+s.ex||std::abs(dy)>ey+s.ey)continue;
            float dot=fx*s.fx+fy*s.fy,cross=fx*s.fy-fy*s.fx;
            if(std::abs(dx*fy-dy*fx)>=w+std::abs(dot)*s.b.halfWidth+std::abs(cross)*s.b.halfLength)continue;
            if(std::abs(dx*fx+dy*fy)>=l+std::abs(cross)*s.b.halfWidth+std::abs(dot)*s.b.halfLength)continue;
            if(std::abs(dx*s.fy-dy*s.fx)>=s.b.halfWidth+std::abs(dot)*w+std::abs(cross)*l)continue;
            if(std::abs(dx*s.fx+dy*s.fy)>=s.b.halfLength+std::abs(cross)*w+std::abs(dot)*l)continue;
            return false;
        }return true;
    }
    float overlap(NavPose p,const Shape&s)const{
        float fx=std::sin(p.yaw),fy=std::cos(p.yaw),w=halfWidth+margin,l=halfLength+margin;
        float dx=s.b.x-p.x,dy=s.b.y-p.y,dot=fx*s.fx+fy*s.fy,cross=fx*s.fy-fy*s.fx;
        return std::max(0.0f,std::min({w+std::abs(dot)*s.b.halfWidth+std::abs(cross)*s.b.halfLength-std::abs(dx*fy-dy*fx),l+std::abs(cross)*s.b.halfWidth+std::abs(dot)*s.b.halfLength-std::abs(dx*fx+dy*fy),s.b.halfWidth+std::abs(dot)*w+std::abs(cross)*l-std::abs(dx*s.fy-dy*s.fx),s.b.halfLength+std::abs(cross)*w+std::abs(dot)*l-std::abs(dx*s.fx+dy*s.fy)}));
    }
    bool escapeStep(NavPose origin,NavPose previous,NavPose next)const{
        for(const auto&s:shapes){float d=overlap(next,s);if(d<=0)continue;if(overlap(origin,s)<=0||d>overlap(previous,s)+.01f)return false;}return true;
    }
    bool edge(NavPose a,NavPose b)const{
        float d=std::hypot(b.x-a.x,b.y-a.y),turn=navAngle(b.yaw-a.yaw);int steps=std::max(1,int(std::ceil(std::max(d/.25f,std::abs(turn)/.06f))));
        for(int i=0;i<=steps;i++){float t=float(i)/steps;if(!clear({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z,navAngle(a.yaw+turn*t)}))return false;}return true;
    }
    float freeDistance(NavPose car,float x,float y)const{
        float dx=x-car.x,dy=y-car.y,d=std::hypot(dx,dy),heading=d>.001f?std::atan2(dx,dy):car.yaw,last=0;
        int steps=std::max(1,int(std::ceil(d/.1f)));
        for(int i=0;i<=steps;i++){float at=d*float(i)/steps,t=d>.001f?at/d:0;NavPose p{car.x+dx*t,car.y+dy*t,car.z,navAngle(car.yaw+navAngle(heading-car.yaw)*std::min(1.0f,at/3))};if(!clear(p))return last;last=at;}return d;
    }
    std::pair<float,bool> routeDistance(NavPose car,const std::vector<Point>&r,size_t segment,size_t limit)const{
        float used=0;if(r.size()<2)return {0,true};limit=std::min(limit,r.size()-1);
        for(size_t i=segment+1;i<=limit;i++){float dx=r[i].x-car.x,dy=r[i].y-car.y,d=std::hypot(dx,dy);if(d<.001f)continue;float heading=std::atan2(dx,dy),last=0;int steps=std::max(1,int(std::ceil(d/.15f)));for(int j=0;j<=steps;j++){float t=float(j)/steps;NavPose p{car.x+dx*t,car.y+dy*t,car.z,navAngle(car.yaw+navAngle(heading-car.yaw)*t)};if(!clear(p))return {used+last,true};last=d*t;}used+=d;car={r[i].x,r[i].y,car.z,heading};}
        return {used,false};
    }
    bool routeClear(NavPose car,const std::vector<Point>&r,size_t segment,size_t limit,float horizon)const{
        if(r.size()<2||segment+1>=r.size())return false;
        limit=std::min(limit,r.size()-1);float used=0;
        for(size_t i=segment+1;i<=limit;i++){
            float dx=r[i].x-car.x,dy=r[i].y-car.y,d=std::hypot(dx,dy);if(d<.02f)continue;
            float take=std::min(d,horizon-used),t=take/d;
            NavPose end{car.x+dx*t,car.y+dy*t,car.z,std::atan2(dx,dy)};
            if(!edge(car,end))return false;
            used+=take;if(used>=horizon-.01f)return true;car=end;
        }return true;
    }
};
struct NavKey {int x,y,a;bool operator==(const NavKey&o)const{return x==o.x&&y==o.y&&a==o.a;}};
struct NavKeyHash {size_t operator()(const NavKey&k)const{return uint32_t(k.x)*73856093u^uint32_t(k.y)*19349663u^uint32_t(k.a)*83492791u;}};
inline std::vector<NavPose> planForward(const NavigationScene&scene,NavPose start,NavPose goal,unsigned budget=9000,float radius=34,unsigned timeBudgetMs=0){
    const auto started=std::chrono::steady_clock::now();
    if(!scene.clear(start)||!scene.clear(goal))return {};
    auto safe=[&](NavPose p){return scene.clear(p,std::min(.4f,std::hypot(p.x-start.x,p.y-start.y)*.15f));};
    struct Node {NavPose p;float cost;int parent;float steer;};struct Open {float f;int id;bool operator<(const Open&o)const{return f>o.f;}};
    std::vector<Node> nodes;nodes.reserve(budget*2);nodes.push_back({start,0,-1,0});std::priority_queue<Open> open;open.push({0,0});std::unordered_map<NavKey,float,NavKeyHash> costs;
    auto key=[&](NavPose p){return NavKey{int(std::floor((p.x-start.x)/.65f)),int(std::floor((p.y-start.y)/.65f)),int(std::floor((navAngle(p.yaw)+NAV_PI)/(2*NAV_PI)*48))%48};};costs[key(start)]=0;
    float wheelbase=std::clamp(scene.halfLength*1.15f,2.2f,4.5f);int found=-1;unsigned expanded=0;
    while(!open.empty()&&expanded++<budget){if(timeBudgetMs&&expanded%32==0&&std::chrono::steady_clock::now()-started>=std::chrono::milliseconds(timeBudgetMs))break;auto top=open.top();open.pop();Node n=nodes[top.id];auto current=costs.find(key(n.p));if(current!=costs.end()&&n.cost>current->second+.001f)continue;
        float dist=std::hypot(n.p.x-goal.x,n.p.y-goal.y);
        if(dist<1.5f&&std::abs(navAngle(n.p.yaw-goal.yaw))<.55f&&scene.edge(n.p,goal)){found=top.id;break;}
        for(float steer:{0.0f,-.28f,.28f,-.55f,.55f}){
            NavPose p=n.p;bool valid=true;float step=.23f;
            for(int j=0;j<6;j++){float mid=p.yaw+std::tan(steer)/wheelbase*step/2;p.x+=std::sin(mid)*step;p.y+=std::cos(mid)*step;p.yaw=navAngle(p.yaw+std::tan(steer)/wheelbase*step);if(!safe(p)){valid=false;break;}}
            if(!valid||std::hypot(p.x-start.x,p.y-start.y)>radius)continue;
            float cost=n.cost+1.38f+.12f*std::abs(steer)+.15f*std::abs(steer-n.steer);auto k=key(p);auto prior=costs.find(k);if(prior!=costs.end()&&prior->second<=cost)continue;
            costs[k]=cost;int id=int(nodes.size());nodes.push_back({p,cost,top.id,steer});float h=std::hypot(p.x-goal.x,p.y-goal.y)+.55f*std::abs(navAngle(p.yaw-goal.yaw));open.push({cost+h,id});
        }
    }
    if(found<0)return {};
    std::vector<NavPose> path;for(int i=found;i>=0;i=nodes[i].parent)path.push_back(nodes[i].p);std::reverse(path.begin(),path.end());path.push_back(goal);return path;
}
struct DetourFollow {float steering=0,error=0,speedCap=0;bool valid=false,arrived=false;};
struct ForwardDetour {
    std::vector<NavPose> path;size_t cursor=0,routeGoal=0;NavPose goal{};uint64_t lastTry=0,invalidSince=0;unsigned failures=0;
    bool active()const{return path.size()>1;}
    bool replaceInvalid(uint64_t now,bool valid,float error,bool retain=true){if(valid){invalidSince=0;return false;}if(!invalidSince)invalidSince=now;return !retain||error>3.0f||now-invalidSince>=1400;}
    void reset(){*this={};}
    void set(std::vector<NavPose> p,size_t index,NavPose g){path=std::move(p);cursor=0;routeGoal=index;goal=g;failures=0;invalidSince=0;}
    DetourFollow follow(NavPose car,float speed,const NavigationScene&scene,bool anticipate=true){
        DetourFollow out;if(!active())return out;
        float best=std::numeric_limits<float>::infinity(),bestT=0;size_t bestI=cursor;
        for(size_t i=cursor;i+1<path.size()&&i<cursor+6;i++){auto a=path[i],b=path[i+1];float dx=b.x-a.x,dy=b.y-a.y,n=dx*dx+dy*dy,t=n>.001f?std::clamp(((car.x-a.x)*dx+(car.y-a.y)*dy)/n,0.0f,1.0f):0;float d=std::hypot(car.x-a.x-dx*t,car.y-a.y-dy*t);if(d<best){best=d;bestT=t;bestI=i;}}
        out.error=best;if(best>3.0f)return out;cursor=bestI;
        auto a=path[bestI],b=path[bestI+1];NavPose p{a.x+(b.x-a.x)*bestT,a.y+(b.y-a.y)*bestT,car.z,navAngle(a.yaw+navAngle(b.yaw-a.yaw)*bestT)};
        float look=std::clamp(3.0f+speed*.55f,3.0f,8.0f);NavPose projected=p;bool shortcutClear=false;
        for(unsigned attempt=0;attempt<7;attempt++){
            p=projected;float left=look;
            for(size_t i=bestI+1;i<path.size();i++){auto q=path[i];float d=std::hypot(q.x-p.x,q.y-p.y);if(d>=left&&d>.001f){float t=left/d;p={p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,car.z,navAngle(p.yaw+navAngle(q.yaw-p.yaw)*t)};break;}left-=d;p=q;}
            if(scene.edge(car,p)){shortcutClear=true;break;}look=std::max(1.5f,look*.72f);
        }
        if(!shortcutClear)return out;
        // Validate the curved corridor ahead, not a shortcut to every later waypoint.
        NavPose prev=car;float checked=0;for(size_t i=bestI+1;i<path.size();i++){auto q=path[i];float d=std::hypot(q.x-prev.x,q.y-prev.y);if(!scene.edge(prev,q))return out;checked+=d;if(checked>=std::max(6.0f,speed*1.4f))break;prev=q;}
        float angle=navAngle(std::atan2(p.x-car.x,p.y-car.y)-car.yaw);
        float signedCurvature=2*std::sin(angle)/std::max(2.0f,std::hypot(p.x-car.x,p.y-car.y));
        float wb=std::clamp(scene.halfLength*1.15f,2.2f,4.5f);out.steering=std::clamp(std::atan(wb*signedCurvature)/.55f,-1.0f,1.0f);float curvature=std::abs(signedCurvature);out.speedCap=std::min({30.0f/3.6f,std::sqrt(3.2f/std::max(.01f,curvature)),std::max(2.3f,look*1.6f)});
        if(anticipate){std::vector<Point> preview;preview.reserve(path.size()-bestI);preview.push_back({car.x,car.y,car.z,0});for(size_t i=bestI+1;i<path.size();i++)preview.push_back({path[i].x,path[i].y,path[i].z,0});out.speedCap=std::min(out.speedCap,anticipatedSpeed(preview,0,preview.front(),speed,30.0f/3.6f));}if(std::abs(angle)>.8f)out.speedCap=std::min(out.speedCap,2.0f);
        out.valid=true;out.arrived=std::hypot(car.x-goal.x,car.y-goal.y)<1.8f&&std::abs(navAngle(car.yaw-goal.yaw))<.65f;return out;
    }
};
