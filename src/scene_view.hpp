#pragma once
#include "radar.hpp"
#include "route.hpp"
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace sceneview {
struct V3 {float x=0,y=0,z=0;};
inline V3 operator+(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline V3 operator-(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline V3 operator*(V3 a,float b){return {a.x*b,a.y*b,a.z*b};}
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline V3 normalized(V3 a){float n=std::sqrt(dot(a,a));return n>1e-6f?a*(1/n):V3{0,0,1};}
inline bool finite(V3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
struct Material {std::string name;V3 color{.73f,.75f,.77f};};
struct Triangle {std::array<int,3> vertex{},normal{};unsigned material=0;};
struct Mesh {
    std::vector<V3> vertices,normals;std::vector<Triangle> triangles;std::vector<Material> materials;
    V3 lo{},hi{};
    bool load(const std::filesystem::path&path){
        *this={};std::ifstream file(path);if(!file)return false;
        std::unordered_map<std::string,unsigned> ids;unsigned material=0;materials.push_back({"default",{.73f,.75f,.77f}});
        std::string line;auto index=[](const std::string&s,size_t n){if(s.empty())return -1;try{int i=std::stoi(s);return i>0?i-1:int(n)+i;}catch(...){return -1;}};
        while(std::getline(file,line)){
            std::istringstream in(line);std::string tag;in>>tag;
            if(tag=="mtllib"){
                std::string name;in>>name;std::ifstream mtl(path.parent_path()/std::filesystem::path(name).filename());std::string ml;unsigned current=0;
                while(std::getline(mtl,ml)){std::istringstream values(ml);std::string type;values>>type;
                    if(type=="newmtl"){std::string n;values>>n;current=unsigned(materials.size());ids[n]=current;materials.push_back({n,{.73f,.75f,.77f}});}
                    else if(type=="Kd"&&current<materials.size()){auto&v=materials[current].color;if(!(values>>v.x>>v.y>>v.z)||!finite(v))v={.73f,.75f,.77f};}
                }
            }else if(tag=="usemtl"){std::string name;in>>name;auto found=ids.find(name);material=found==ids.end()?0:found->second;}
            else if(tag=="v"||tag=="vn"){V3 v;if(!(in>>v.x>>v.y>>v.z)||!finite(v))return false;if(tag=="v")vertices.push_back(v);else normals.push_back(normalized(v));}
            else if(tag=="f"){
                std::vector<std::pair<int,int>> polygon;std::string item;
                while(in>>item){auto slash=item.find('/'),second=slash==std::string::npos?slash:item.find('/',slash+1);int v=index(item.substr(0,slash),vertices.size()),n=second==std::string::npos?-1:index(item.substr(second+1),normals.size());if(v<0||size_t(v)>=vertices.size()||(n>=0&&size_t(n)>=normals.size()))return false;polygon.push_back({v,n});}
                for(size_t i=1;i+1<polygon.size();i++)triangles.push_back({{polygon[0].first,polygon[i].first,polygon[i+1].first},{polygon[0].second,polygon[i].second,polygon[i+1].second},material});
            }
            if(vertices.size()>100000||triangles.size()>200000)return false;
        }
        if(vertices.empty()||triangles.empty())return false;
        lo=hi=vertices.front();for(auto v:vertices){lo={std::min(lo.x,v.x),std::min(lo.y,v.y),std::min(lo.z,v.z)};hi={std::max(hi.x,v.x),std::max(hi.y,v.y),std::max(hi.z,v.z)};}
        return hi.x-lo.x>.01f&&hi.y-lo.y>.01f&&hi.z-lo.z>.01f;
    }
};
enum class VehicleShape {Car,Truck,Semi,Bus};
inline VehicleShape shapeFor(float halfWidth,float halfLength,float height=0){
    if(halfLength*2>=13.5f)return VehicleShape::Semi;
    if(halfLength*2>=9.5f)return VehicleShape::Bus;
    if(halfWidth*2>=2.3f||halfLength*2>=6.2f||height>=2.5f)return VehicleShape::Truck;
    return VehicleShape::Car;
}
struct Pose {V3 center;float fx=0,fy=1,halfWidth=1.05f,halfLength=2.5f;};
struct Item {RadarBox box;unsigned kind=5;uint64_t id=0;};
struct Frame {
    int width=0,height=0;std::vector<uint32_t> pixels;std::vector<float> depth;
    void reset(int w,int h){width=w;height=h;pixels.resize(size_t(w)*h);depth.assign(size_t(w)*h,0);for(int y=0;y<h;y++){float t=float(y)/std::max(1,h-1);uint32_t r=unsigned(29-5*t),g=r,b=r;std::fill_n(pixels.begin()+size_t(y)*w,w,(r<<16)|(g<<8)|b);}}
};
struct Screen {float x,y,invZ;};
struct Camera {
    int width,height;V3 eye{0,-16,14},right{1,0,0},forward,up;float focal;
    Camera(int w,int h):width(w),height(h),forward(normalized(V3{0,3,0}-eye)),up(cross(right,forward)),focal(h*.95f){}
    V3 camera(V3 p)const{p=p-eye;return {dot(p,right),dot(p,up),dot(p,forward)};}
    bool project(V3 p,Screen&v)const{auto q=camera(p);if(q.z<.4f||!finite(q))return false;v={width*.5f+focal*q.x/q.z,height*.5f-focal*q.y/q.z,1/q.z};return true;}
};
struct GroundAtlas {
    int width=0,height=0;
    std::vector<uint8_t> pixels;

    void draw(Frame& frame,Pose car) const{
        draw(frame,car,[](float,float,uint32_t&){return false;});
    }
    template<class SatelliteSample>
    void draw(Frame& frame,Pose car,SatelliteSample sample) const{
        if(!finite(car.center))return;
        const Camera camera(frame.width,frame.height);
        const float headingLength=std::hypot(car.fx,car.fy);
        const float fx=headingLength>.01f?car.fx/headingLength:0;
        const float fy=headingLength>.01f?car.fy/headingLength:1;
        // The dashboard map uses the same frame. Ground z=0 is the vehicle's contact plane.
        constexpr float scale=1.0f/48.5f;
        const float uScale=scale*width/256,vScale=scale*height/256;
        const float originU=(117.25f+car.center.x*scale)*width/256-.5f;
        const float originV=(172.53125f-car.center.y*scale)*height/256-.5f;
        for(int y=0;y<frame.height;++y){
            const auto ray=camera.forward+camera.up*((frame.height*.5f-y-.5f)/camera.focal);
            if(ray.z>=-.001f)continue;
            const float distance=-camera.eye.z/ray.z;
            const float localY=camera.eye.y+ray.y*distance;
            const float step=distance/camera.focal;
            const float localX=(.5f-frame.width*.5f)*step;
            const float u0=originU+(localX*fy+localY*fx)*uScale;
            const float v0=originV+(localX*fx-localY*fy)*vScale;
            const float du=step*fy*uScale,dv=step*fx*vScale;
            for(int x=0;x<frame.width;++x){
                const auto at=size_t(y)*frame.width+x;
                if(frame.depth[at]>0)continue;
                const float local=localX+x*step;
                uint32_t satellite=0;
                if(sample(car.center.x+local*fy+localY*fx,car.center.y-local*fx+localY*fy,satellite)){
                    frame.pixels[at]=satellite;frame.depth[at]=-1;
                    continue;
                }
                const float u=u0+x*du,v=v0+x*dv;
                if(pixels.empty()||!(u>=0&&u<width-1&&v>=0&&v<height-1))continue;
                const int ix=int(u),iy=int(v);
                const auto index=size_t(iy)*width+ix;
                const float a=u-ix,b=v-iy;
                const float top=pixels[index]+(pixels[index+1]-pixels[index])*a;
                const float bottom=pixels[index+width]+(pixels[index+width+1]-pixels[index+width])*a;
                frame.pixels[at]=uint32_t(top+(bottom-top)*b+.5f)*0x010101;
                frame.depth[at]=-1; // Ground under every piece of scene geometry.
            }
        }
    }
};
inline float edge(Screen a,Screen b,float x,float y){return (x-a.x)*(b.y-a.y)-(y-a.y)*(b.x-a.x);}
inline uint32_t rgb(V3 color){auto c=[](float v){return uint32_t(std::clamp(v,0.0f,1.0f)*255+.5f);};return (c(color.x)<<16)|(c(color.y)<<8)|c(color.z);}
inline void triangle(Frame&f,Screen a,Screen b,Screen c,V3 color){
    float area=edge(a,b,c.x,c.y);if(std::abs(area)<.01f)return;
    int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),x1=std::min(f.width-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
    int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),y1=std::min(f.height-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
    uint32_t value=rgb(color);for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
        float w0=edge(b,c,x+.5f,y+.5f)/area,w1=edge(c,a,x+.5f,y+.5f)/area,w2=1-w0-w1;if(w0<-.00001f||w1<-.00001f||w2<-.00001f)continue;
        float depth=w0*a.invZ+w1*b.invZ+w2*c.invZ;size_t at=size_t(y)*f.width+x;if(depth>f.depth[at]){f.depth[at]=depth;f.pixels[at]=value;}
    }
}
inline void segment(Frame&f,const Camera&camera,V3 a,V3 b,V3 color,int thickness=1){
    Screen p{},q{};if(!camera.project(a,p)||!camera.project(b,q))return;
    int steps=int(std::ceil(std::max(std::abs(p.x-q.x),std::abs(p.y-q.y))));if(steps>f.width*8+f.height*8)return;
    for(int i=0;i<=steps;i++){float t=steps?float(i)/steps:0,depth=p.invZ+(q.invZ-p.invZ)*t;int x=int(p.x+(q.x-p.x)*t),y=int(p.y+(q.y-p.y)*t);for(int oy=0;oy<thickness;oy++)for(int ox=0;ox<thickness;ox++){int xx=x+ox,yy=y+oy;if(xx<0||yy<0||xx>=f.width||yy>=f.height)continue;size_t at=size_t(yy)*f.width+xx;if(depth>=f.depth[at]-.0001f){f.pixels[at]=rgb(color);f.depth[at]=depth;}}}
}
inline std::array<V3,4> footprint(float x,float y,float fx,float fy,float hw,float hl,float z){
    float n=std::hypot(fx,fy);if(n<.01f){fx=0;fy=1;}else{fx/=n;fy/=n;}
    std::array<V3,4> p{};int i=0;for(auto corner:std::array<std::pair<float,float>,4>{{{-hw,-hl},{hw,-hl},{hw,hl},{-hw,hl}}}){p[size_t(i++)]={x+fy*corner.first+fx*corner.second,y-fx*corner.first+fy*corner.second,z};}return p;
}
class Renderer {
    std::array<Mesh,4> meshes;bool loaded=false;
    V3 local(V3 p,Pose car)const{auto d=p-car.center;float n=std::hypot(car.fx,car.fy);float fx=n>.01f?car.fx/n:0,fy=n>.01f?car.fy/n:1;return {d.x*fy-d.y*fx,d.x*fx+d.y*fy,d.z};}
    void band(Frame&f,const Camera&camera,float x,float y,float fx,float fy,float hw,float hl,float z,V3 color){
        auto bottom=footprint(x,y,fx,fy,hw,hl,z),top=footprint(x,y,fx,fy,hw,hl,z+.24f);
        std::array<Screen,4> a{},b{};for(size_t i=0;i<4;i++)if(!camera.project(bottom[i],a[i])||!camera.project(top[i],b[i]))return;
        triangle(f,b[0],b[1],b[2],color);triangle(f,b[0],b[2],b[3],color);
        for(size_t i=0;i<4;i++){size_t j=(i+1)%4;triangle(f,a[i],a[j],b[j],color*.63f);triangle(f,a[i],b[j],b[i],color*.63f);}
    }
    void vehicle(Frame&f,const Camera&camera,V3 center,float fx,float fy,float hw,float hl,float height,bool own,uint32_t paint=0xffffffffu){
        auto shape=shapeFor(hw,hl,height);const auto&m=meshes[size_t(shape)];if(m.vertices.empty())return;
        float n=std::hypot(fx,fy);if(n<.01f){fx=0;fy=1;}else{fx/=n;fy/=n;}
        float sx=2*hw/(m.hi.x-m.lo.x),sy=2*hl/(m.hi.y-m.lo.y),sz=height>0?height/(m.hi.z-m.lo.z):std::clamp(sx,.65f,1.6f);
        V3 mid{(m.hi.x+m.lo.x)/2,(m.hi.y+m.lo.y)/2,m.lo.z};std::vector<Screen> projected(m.vertices.size());std::vector<bool> valid(m.vertices.size());
        for(size_t i=0;i<m.vertices.size();i++){auto p=m.vertices[i]-mid;p={p.x*sx,p.y*sy,p.z*sz};V3 q{center.x+fy*p.x+fx*p.y,center.y-fx*p.x+fy*p.y,center.z+p.z};valid[i]=camera.project(q,projected[i]);}
        const V3 light=normalized({-.35f,-.45f,1});
        for(const auto&t:m.triangles){if(!valid[size_t(t.vertex[0])]||!valid[size_t(t.vertex[1])]||!valid[size_t(t.vertex[2])])continue;
            V3 normal{};bool known=true;for(int index:t.normal){if(index<0){known=false;break;}normal=normal+m.normals[size_t(index)];}
            if(!known)normal=cross(m.vertices[size_t(t.vertex[1])]-m.vertices[size_t(t.vertex[0])],m.vertices[size_t(t.vertex[2])]-m.vertices[size_t(t.vertex[0])]);
            normal=normalized({normal.x/sx,normal.y/sy,normal.z/sz});normal={fy*normal.x+fx*normal.y,-fx*normal.x+fy*normal.y,normal.z};float shade=.43f+.57f*std::max(0.0f,dot(normal,light));
            const auto&material=m.materials[t.material];
            const float gray=(material.color.x+material.color.y+material.color.z)/3;
            V3 color{gray,gray,gray};
            if(own&&(material.name=="body_gray"||material.name=="roof_gray"||material.name=="cargo_gray")){
                color=paint<=0xffffffu
                    ? V3{float((paint>>16)&255)/255,float((paint>>8)&255)/255,float(paint&255)/255}
                    : V3{.94f,.94f,.94f};
                // Retain subtle reflections on black paint and the existing directional shadows.
                color=color*.97f+V3{.025f,.025f,.025f};
            }
            float inv=(projected[size_t(t.vertex[0])].invZ+projected[size_t(t.vertex[1])].invZ+projected[size_t(t.vertex[2])].invZ)/3;
            float fog=std::clamp((1/inv-35)/90,0.0f,.6f);color=color*(shade*(1-fog))+V3{.12f,.12f,.12f}*fog;
            triangle(f,projected[size_t(t.vertex[0])],projected[size_t(t.vertex[1])],projected[size_t(t.vertex[2])],color);
        }
    }
public:
    bool load(const std::filesystem::path&directory){const char*names[]={"01_passenger_car.obj","02_rigid_truck.obj","03_semi_truck_trailer.obj","04_bus.obj"};loaded=true;for(size_t i=0;i<4;i++)loaded=meshes[i].load(directory/names[i])&&loaded;return loaded;}
    bool ready()const{return loaded;}
    const Mesh&mesh(VehicleShape shape)const{return meshes[size_t(shape)];}
    void render(Frame&frame,int width,int height,Pose car,const std::vector<Item>&items,const std::vector<Point>&route,bool valid=true,uint32_t routeColor=0xffffff,uint32_t paint=0xffffffffu){
        frame.reset(width,height);if(!valid||!loaded)return;Camera camera(width,height);car.center.z-=.68f;
        // Only a recorded path is drawn: there is no invented lane or map data.
        for(size_t i=0;i+1<route.size();i++){V3 a=local({route[i].x,route[i].y,route[i].z-.65f},car),b=local({route[i+1].x,route[i+1].y,route[i+1].z-.65f},car);if(std::hypot(a.x,a.y)<70||std::hypot(b.x,b.y)<70)segment(frame,camera,a,b,{float((routeColor>>16)&255)/255,float((routeColor>>8)&255)/255,float(routeColor&255)/255},2);}
        unsigned vehicles=0,solids=0;
        for(const auto&item:items){const auto&b=item.box;if(passableRule(b.passability))continue;auto p=local({b.x,b.y,b.z-b.halfHeight},car);if(!finite(p)||std::hypot(p.x,p.y)>70||p.y< -18||p.y>70||std::abs(p.z)>4)continue;
            float n=std::hypot(car.fx,car.fy),cf=n>.01f?car.fx/n:0,cg=n>.01f?car.fy/n:1,fx=b.fx*cg-b.fy*cf,fy=b.fx*cf+b.fy*cg;
            if(item.kind==3){if(vehicles++>=48)continue;vehicle(frame,camera,{p.x,p.y,std::max(-.1f,p.z)},fx,fy,b.halfWidth,b.halfLength,2*b.halfHeight,false);}
            else if(item.kind!=4){if(solids++>=128)continue;band(frame,camera,p.x,p.y,fx,fy,b.halfWidth,b.halfLength,std::max(.01f,p.z),b.passability==WAIT_OBJECT?V3{.85f,.85f,.85f}:V3{.56f,.56f,.56f});}
        }
        vehicle(frame,camera,{0,0,0},0,1,car.halfWidth,car.halfLength,0,true,paint);
    }
};
struct PanelRect {int x=0,y=0,width=0,height=0;};
inline PanelRect panelRect(int width,int height){if(width<480||height<420)return {};int h=std::min(520,std::max(260,int(height*.56f))),y=std::max(160,(height-h)/2);if(y+h>height-24)h=height-y-24;return {16,y,int(h*.679f),h};}
}
