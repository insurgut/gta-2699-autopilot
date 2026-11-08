#include "scene_geometry.hpp"
#include <cassert>
#include <limits>
inline bool near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
    SceneFootprint out{};
    assert(modelFootprint({10,20,30},{1,0,0},{0,1,0},{0,0,1},{-1,-2,-3},{1,2,3},out));
    assert(near(out.center.x,10)&&near(out.halfWidth,1)&&near(out.halfLength,2)&&near(out.halfHeight,3));
    // Off-center model rotated 90 degrees: translation must use the full basis.
    assert(modelFootprint({10,20,30},{0,-1,0},{1,0,0},{0,0,1},{0,0,0},{2,4,6},out));
    assert(near(out.center.x,12)&&near(out.center.y,19)&&near(out.center.z,33));
    // A vertical forward axis used to reject tilted props outright.
    assert(modelFootprint({0,0,0},{1,0,0},{0,0,1},{0,-1,0},{-1,-2,-3},{1,2,3},out));
    assert(near(out.halfWidth,1)&&near(out.halfLength,3)&&near(out.halfHeight,2));
    float a=std::sqrt(0.5f);
    assert(modelFootprint({0,0,0},{1,0,0},{0,a,a},{0,-a,a},{-1,-2,-3},{1,2,3},out));
    assert(near(out.halfHeight,5*a)&&near(out.halfLength,5*a));
    // Large model, distant pivot but nearby geometry. No tiny-box fallback.
    assert(modelFootprint({200,0,0},{1,0,0},{0,1,0},{0,0,1},{-250,-1,-5},{-150,1,5},out));
    assert(near(out.center.x,0)&&near(out.halfWidth,50));
    assert(near(footprintDistanceSquared(out,{55,0,0}),25));
    assert(modelFootprint({0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,-2,-1},{0,2,1},out));
    assert(out.halfWidth>=0.005f); // thin geometry still has a footprint
    assert(!modelFootprint({0,0,0},{1,0,0},{0,1,0},{0,0,1},{1,0,0},{0,1,1},out));
    assert(!modelFootprint({0,0,0},{1,0,0},{0,1,0},{0,0,1},{0,0,0},{std::numeric_limits<float>::quiet_NaN(),1,1},out));
    struct Ranked {float distance2;uintptr_t entity;unsigned category;};
    std::vector<Ranked> items;for(unsigned i=0;i<300;i++)items.push_back({float(10000+i),i,0});
    items.push_back({1,999,2});keepNearest(items,256);
    assert(items.size()==256&&items.front().entity==999); // props cannot be starved by earlier vehicle entries
}
