#include "gps_memory.hpp"
#include <cassert>
#include <limits>
#include <map>
int main(){
 uint64_t base=0x10000000,ptr=0x40000000;std::map<uint64_t,std::vector<uint8_t>> memory;
 auto put=[&](uint64_t at,const void*p,size_t n){memory[at]=std::vector<uint8_t>(static_cast<const uint8_t*>(p),static_cast<const uint8_t*>(p)+n);};
 uint8_t code[]={0x48,0x8d,0x05,0x63,0x55,0x43,0x01,0x48,0x69,0xc9,0xb0,0,0,0,0x48,0x03,0xc8,0x83,0x79,0x74,0,0x74,0x38,0x83,0x79,0x44,0};assert(gpsSlotInstruction(code));put(base+GPS_SLOT_CODE_RVA,code,sizeof(code));
 uint8_t header[0xb0]{};uint32_t count=3,active=8;GpsPoint goal{2674,1644.45f,23.625f},mirror{goal.x,goal.y,0};std::memcpy(header,&ptr,8);std::memcpy(header+0x20,&goal,12);std::memcpy(header+0x44,&count,4);std::memcpy(header+0x74,&active,4);put(base+GPS_SLOT_RVA,header,sizeof(header));put(base+GPS_MIRROR_RVA,&mirror,12);
 std::vector<GpsRawPoint> raw{{2706.96143f,1644.69971f,23.65625f,0},{2689.47949f,1644.45f,23.625f,0},{2674,1644.45f,23.625f,0}};put(ptr,raw.data(),raw.size()*sizeof(raw[0]));
 auto read=[&](uint64_t at,void*out,size_t n){auto it=memory.find(at);if(it==memory.end()||it->second.size()!=n)return false;std::memcpy(out,it->second.data(),n);return true;};
 GpsFrame frame{};GpsPoint car{raw.front().x,raw.front().y,raw.front().z+1},forward{-1,0,0};gpsReadMemory(read,base,9,1000,car,forward,frame);assert(frame.provider==1&&frame.status==4&&frame.reason==0&&gpsFresh(frame,9,1200));for(unsigned i=1;i<frame.count;i++)assert(gpsDistance(frame.points[i-1],frame.points[i])<=5.01f);
 auto h=gpsHeader(header,mirror);
 int reads=0;auto torn=[&](uint64_t at,void*out,size_t n){bool ok=read(at,out,n);if(at==ptr&&++reads==2&&ok)static_cast<uint8_t*>(out)[0]^=1;return ok;};gpsReadMemory(torn,base,9,1000,car,forward,frame);assert(frame.status==10&&frame.reason==10&&!gpsFresh(frame,9,1100));
GpsFrame normalized{};assert(gpsNormalize(h,raw,{0,0,0},forward,normalized)==7);auto bad=raw;bad[1].x=std::numeric_limits<float>::quiet_NaN();assert(gpsNormalize(h,bad,car,forward,normalized)==4);auto wrong=h;wrong.goal.x=2000;wrong.mirror.x=2000;assert(gpsNormalize(wrong,raw,car,forward,normalized)==6);wrong=h;wrong.mirror.x=2000;assert(gpsNormalize(wrong,raw,car,forward,normalized)==2);
 h.goal={64000,64000,64000};h.mirror={0,0,0};assert(gpsEmpty(h));std::memcpy(header+0x20,&h.goal,12);put(base+GPS_SLOT_RVA,header,sizeof(header));put(base+GPS_MIRROR_RVA,&h.mirror,12);gpsReadMemory(read,base,9,1001,car,forward,frame);assert(frame.status==2&&!gpsFresh(frame,9,1002));

 auto normalize=[](const std::vector<GpsRawPoint>& road) {
    const auto& goal=road.back();
    GpsMemoryHeader header{};
    header.count=static_cast<uint32_t>(road.size());
    header.active=1;
    header.goal={goal.x,goal.y,goal.z};
    header.mirror={goal.x,goal.y,0};
    GpsFrame route{};
    assert(gpsNormalize(header,road,{0,0,0},{1,0,0},route)==0);
    assert(gpsDistance(route.points[route.count-1],header.goal)<.01f);
    return route;
 };
 for(const auto& road : std::vector<std::vector<GpsRawPoint>>{
    {{0,0,0,0},{8,0,0,0},{10,2,0,0},{14,0,0,0},{20,0,0,0}},
    {{0,0,0,0},{8,0,0,0},{9,2,0,0},{11,-2,0,0},{14,0,0,0},{20,0,0,0}},
    {{0,0,0,0},{8,0,0,0},{10,3,0,0},{12,6,0,0},{11,3,0,0},{14,0,0,0},{20,0,0,0}},
    {{0,0,0,0},{8,0,0,0},{7,4,0,0},{9,-2,0,0},{14,0,0,0},{20,0,0,0}}
 }) {
    const auto route=normalize(road);
    for(unsigned i=0;i<route.count;++i)assert(std::abs(route.points[i].y)<.001f);
 }
 for(const auto& road : std::vector<std::vector<GpsRawPoint>>{
    {{0,0,0,0},{8,0,0,0},{10,2,0,0},{10,8,0,0},{10,14,0,0}},
    {{0,0,0,0},{8,0,0,0},{10,9,0,0},{14,0,0,0},{20,0,0,0}},
    {{0,0,0,0},{8,0,0,0},{10,2,1.1f,0},{14,0,0,0},{20,0,0,0}},
    {{0,0,0,0},{8,0,0,0},{10,2,0,0},{12,2,0,0},{14,2,0,0},{16,0,0,0},{22,0,0,0}}
 }) {
    const auto route=normalize(road);
    bool keptMiddle=false;
    for(unsigned i=0;i<route.count;++i)keptMiddle|=route.points[i].y>=std::max(.5f,road[2].y-3.001f);
    assert(keptMiddle);
 }

 const auto arc=normalize({{0,0,0,0},{25,0,0,0},{45,0,0,0},{50,0,0,0},{50,25,0,0}});
 bool startedBeforeCorner=false;
 for(unsigned i=1;i+1<arc.count;++i) {
    const auto a=arc.points[i-1],b=arc.points[i],c=arc.points[i+1];
    const float ax=b.x-a.x,ay=b.y-a.y,bx=c.x-b.x,by=c.y-b.y;
    assert(std::abs(std::atan2(ax*by-ay*bx,ax*bx+ay*by))<.16f);
    assert(std::min(std::abs(50-b.x),std::abs(b.y))<=3.001f);
    startedBeforeCorner|=b.y>.01f&&b.x<48;
 }
 assert(startedBeforeCorner);

 // The noisy straight contains more points than the short-spike filter handles.
 const auto quiet=normalize({
    {0,0,0,0},{5,0,0,0},{9,.14f,0,0},{13,.43f,0,0},{17,.59f,0,0},
    {21,.39f,0,0},{25,-.12f,0,0},{29,-.56f,0,0},{33,-.45f,0,0},
    {37,-.14f,0,0},{41,0,0,0},{46,0,0,0}
 });
 for(unsigned i=0;i<quiet.count;++i)assert(std::abs(quiet.points[i].y)<.001f);

 // A gentle but consistent change of direction survives, even inside the tolerance.
 std::vector<GpsPoint> gentle{{0,0,0},{5,0,0},{10,.1f,0},{15,.3f,0},{20,.6f,0},{25,1,0}};
 gpsStraightenShallowWiggles(gentle);
 assert(gentle.size()==6);
 std::vector<GpsPoint> crest{{0,0,0},{5,0,0},{10,.2f,.5f},{15,0,0},{20,0,0}};
 gpsStraightenShallowWiggles(crest);
 assert(crest.size()==5);
}
