#include "native_marker_protocol.hpp"
#include <cassert>
#include <limits>
int main(){
 NativeMarkerState s;s.enabled=1;s.controllerBeat=1000;s.x=123.25f;s.y=-456.5f;s.z=30;s.heading=90;
 assert(markerFresh(s,1000)&&markerFresh(s,1750));assert(!markerFresh(s,1751)&&!markerFresh(s,999));
 s.enabled=0;assert(!markerFresh(s,1000));s.enabled=1;s.x=std::numeric_limits<float>::quiet_NaN();assert(!markerFresh(s,1000));s.x=123.25f;
 auto args=markerArguments(s,1,29.3f,5,5,.18f,110);assert(args.size()==24&&args[0]==1&&args[13]==70&&args[16]==110&&args[19]==2&&args[21]==0&&args[22]==0&&args[23]==0);
 for(unsigned i:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u})assert((args[i]>>32)==0);
 float x=0,y=0;uint32_t b=uint32_t(args[1]);std::memcpy(&x,&b,4);b=uint32_t(args[2]);std::memcpy(&y,&b,4);assert(x==123.25f&&y==-456.5f);
 NativeMarkerState loaded;std::memcpy(&loaded,&s,sizeof(s));assert(markerFresh(loaded,1000));loaded.version=2;assert(!markerFresh(loaded,1000));loaded=s;loaded.bytes=0;assert(!markerFresh(loaded,1000));
}
