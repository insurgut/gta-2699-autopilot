#include "scene_view.hpp"
#include "vehicle_palette.hpp"
#include <cstring>
#include <cassert>
#include <iostream>
#include <chrono>
using namespace sceneview;
int main(){
 // Exercise the exact reader chain and byte order captured in District.
 constexpr uintptr_t base=0x100000,carPtr=0x200000,draw=0x300000,render=0x301000;
 uintptr_t vtable=base+0x100;
 std::array<uint8_t,4> paint{0x00,0x00,0xff,0xff};
 auto read=[&](uintptr_t address,auto& out){
    auto copy=[&](const auto& value){
        if(sizeof(value)!=sizeof(out))return false;
        std::memcpy(&out,&value,sizeof(value));
        return true;
    };
    if(address==carPtr+0x48)return copy(draw);
    if(address==draw)return copy(vtable);
    if(address==draw+0x20)return copy(render);
    if(address==render+0xa4)return copy(paint);
    if(address==draw+0x388)return copy(uint8_t(0));
    return false;
 };
 assert(districtPrimaryPaint(read,base,0x10000,carPtr)==0xff0000);
 paint={0xff,0x00,0xb7,0xff};
 assert(districtPrimaryPaint(read,base,0x10000,carPtr)==0xb700ff);
 paint={0,0,0,0xff};
 assert(districtPrimaryPaint(read,base,0x10000,carPtr)==0);
 paint[3]=0;
 assert(districtPrimaryPaint(read,base,0x10000,carPtr)==vehiclePaintColor(0));
 vtable=base-1;
 assert(districtPrimaryPaint(read,base,0x10000,carPtr)==0xffffffffu);
 Renderer renderer;assert(renderer.load("assets/vehicles"));assert(renderer.mesh(VehicleShape::Car).vertices.size()==308);assert(renderer.mesh(VehicleShape::Car).triangles.size()==592);assert(std::abs(renderer.mesh(VehicleShape::Car).hi.x-renderer.mesh(VehicleShape::Car).lo.x-1.88f)<.001f);
 assert(shapeFor(.94f,2.29f)==VehicleShape::Car);assert(shapeFor(1.27f,3.75f)==VehicleShape::Truck);assert(shapeFor(1.31f,7.825f)==VehicleShape::Semi);assert(shapeFor(1.33f,5.6f)==VehicleShape::Bus);
 auto wall=footprint(0,0,0,1,.15f,8,0);assert(std::abs(wall[1].x-wall[0].x-.3f)<.001f);assert(std::abs(wall[2].y-wall[1].y-16)<.001f);
 auto turn=footprint(0,0,1,0,.15f,8,0);assert(std::abs(turn[1].y-turn[0].y+.3f)<.001f);assert(std::abs(turn[2].x-turn[1].x-16)<.001f);
 Pose car{{0,0,.68f},0,1,1.05f,2.5f};Frame empty,hidden,scene;renderer.render(empty,294,433,car,{},{});
 std::vector<Item> items{{{0,10,.8f,0,1,1,2.3f,.8f,1},3,1}};renderer.render(hidden,294,433,car,items,{});assert(empty.pixels==hidden.pixels);
 items[0].box.passability=0;items.push_back({{5,10,1,0,1,.25f,12,1,0},5,2});renderer.render(scene,294,433,car,items,{});assert(scene.pixels!=empty.pixels);
 size_t light=0;for(auto c:empty.pixels){assert(((c>>16)&255)==((c>>8)&255)&&((c>>8)&255)==(c&255));if((c&255)>120)++light;}assert(light>200);
 Frame red,black;
 renderer.render(red,294,433,car,{}, {},true,0xffffff,0xa31923);
 renderer.render(black,294,433,car,{}, {},true,0xffffff,0x080808);
 size_t painted=0,dark=0;
 for(size_t i=0;i<red.pixels.size();++i)if(red.depth[i]>0){
    if(((red.pixels[i]>>16)&255)>((red.pixels[i]>>8)&255)+25)++painted;
    if((black.pixels[i]&255)<30)++dark;
 }
 assert(painted>200&&dark>200&&red.depth==black.depth);
 // Depth is order independent for intersecting projected triangles.
 Frame f1,f2;f1.reset(40,40);f2.reset(40,40);Screen a{5,5,.1f},b{35,5,.1f},c{20,35,.1f},d{5,5,.2f},e{35,5,.2f},g{20,35,.2f};triangle(f1,a,b,c,{0,1,0});triangle(f1,d,e,g,{1,0,0});triangle(f2,d,e,g,{1,0,0});triangle(f2,a,b,c,{0,1,0});assert(f1.pixels==f2.pixels);
 // Rotating the whole world and car together keeps the vehicle-relative view.
 std::vector<Item> rotated=items;for(auto&i:rotated){auto&b=i.box;float x=b.x;b.x=b.y;b.y=-x;float fx=b.fx;b.fx=b.fy;b.fy=-fx;}Frame east;renderer.render(east,294,433,{{0,0,.68f},1,0,1.05f,2.5f},rotated,{});assert(east.pixels==scene.pixels);
 // The atlas projects onto the same ground plane as the scene and leaves meshes intact.
 GroundAtlas atlas;
 atlas.width=atlas.height=512;
 atlas.pixels.resize(size_t(atlas.width)*atlas.height);
 for(int y=0;y<atlas.height;++y)for(int x=0;x<atlas.width;++x)
    atlas.pixels[size_t(y)*atlas.width+x]=uint8_t((x*7+y*3)%256);
 Frame ground=scene,groundEast=scene,groundMoved=scene;
 atlas.draw(ground,car);
 atlas.draw(groundEast,{{0,0,.68f},1,0});
 atlas.draw(groundMoved,{{25,0,.68f},0,1});
 assert(ground.pixels!=groundEast.pixels&&ground.pixels!=groundMoved.pixels);
 for(size_t i=0;i<ground.pixels.size();++i){
    if(scene.depth[i]>0)assert(ground.pixels[i]==scene.pixels[i]&&ground.depth[i]==scene.depth[i]);
    else assert(ground.depth[i]<0);
 }
 Frame tiled=scene;
 atlas.draw(tiled,car,[](float x,float,uint32_t& color){
    if(x<0)return false;
    color=0x994422;
    return true;
 });
 assert(tiled.pixels[0]==ground.pixels[0]&&tiled.pixels[ground.width-1]==0x994422);
 for(size_t i=0;i<tiled.pixels.size();++i)if(scene.depth[i]>0)
    assert(tiled.pixels[i]==scene.pixels[i]&&tiled.depth[i]==scene.depth[i]);
 Frame outside=scene;atlas.draw(outside,{{1e8f,1e8f,.68f},0,1});
 assert(outside.pixels==scene.pixels&&outside.depth==scene.depth);
 Camera camera(ground.width,ground.height);Screen projected{};
 assert(camera.project({-3,8,0},projected));
 const auto at=size_t(projected.y)*ground.width+size_t(projected.x);
 assert(ground.depth[at]<0);
 constexpr float scale=1.0f/48.5f;
 const float u=(117.25f-3*scale)*2-.5f,v=(172.53125f-8*scale)*2-.5f;
 const int expected=(int(u)*7+int(v)*3)%256;
 const float value=expected+(u-int(u))*7+(v-int(v))*3;
 assert(std::abs(float(ground.pixels[at]&255)-value)<1.5f);
 for(auto size:std::array<std::pair<int,int>,3>{{{800,600},{1280,720},{1920,1080}}}){auto p=panelRect(size.first,size.second);assert(p.x>=0&&p.y>=0&&p.x+p.width<=size.first&&p.y+p.height<=size.second);}
 std::vector<Item> preview={{{-1.1f,9,.8f,0,1,1,2.3f,.8f,0},3,1},{{2.5f,13,1.7f,0,1,1.27f,3.75f,1.7f,0},3,2},{{-2.2f,-7,.8f,0,1,1,2.3f,.8f,0},3,3},{{4.5f,-7,1.64f,0,1,1.33f,5.6f,1.64f,0},3,4},{{-5.5f,27,1.85f,0,1,1.31f,7.8f,1.85f,0},3,5},{{8,12,1,0,1,.15f,18,1,0},5,6}};
 renderer.render(scene,294,433,car,preview,{{0,-20,.68f,0},{0,50,.68f,0}});
 std::ofstream image("test-bin/scene-preview.ppm",std::ios::binary);image<<"P6\n"<<scene.width<<" "<<scene.height<<"\n255\n";for(auto p:scene.pixels){char c[3]={char(p>>16),char(p>>8),char(p)};image.write(c,3);}image.close();
 auto started=std::chrono::steady_clock::now();for(int i=0;i<40;i++)renderer.render(scene,320,470,car,items,{});std::cout<<"Scene models, wall thickness, passable filtering, depth, rotation and panel bounds passed; "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()/40.0<<" ms/frame\n";
}
