#include "native_marker_extra.hpp"
#include <cassert>
#include <limits>
int main(){
 NativeMarkerState s;s.x=10;s.y=20;s.z=30;
 MarkerGroundCache cache;assert(cache.due(s,1000));cache.update(s,1000,true,28.2f);assert(cache.ground==28.2f&&!cache.due(s,1999));assert(cache.due(s,2000));
 cache.update(s,2000,false,0);assert(cache.ground==28.2f);cache.update(s,3000,true,std::numeric_limits<float>::quiet_NaN());assert(cache.ground==28.2f);
 s.x+=10;assert(cache.due(s,3000));cache.update(s,3000,false,0);assert(std::abs(cache.ground-29.3f)<.001f);cache.update(s,4000,true,1000);assert(std::abs(cache.ground-29.3f)<.001f);
 NativeMarkerTest test;test.request();assert(!test.begin(1000,true,false,20)&&test.armed);assert(!test.begin(2000,false,true,20));assert(test.begin(3000,true,true,20));assert(test.active(12999)&&!test.finished(12999));assert(test.finished(13000)&&!test.active(13000)&&test.baselineFrames==20);test.reset();assert(!test.finished(14000));
 auto orange=markerArguments(s,1,29,3,3,.18f,110,255,160,70);assert(orange[13]==255&&orange[14]==160&&orange[15]==70);
}
