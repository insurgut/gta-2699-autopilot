#pragma once
#include "gps_protocol.hpp"
#include <vector>
#include <cstring>
constexpr uint32_t GPS_SLOT_RVA=0x20f9730,GPS_PRIMARY_RVA=0x20f9750,GPS_MIRROR_RVA=0x20e6e40,GPS_SLOT_CODE_RVA=0xcc41c6;
struct GpsRawPoint{float x,y,z,pad;};
struct GpsMemoryHeader{uint64_t pointer=0;uint32_t count=0,active=0;GpsPoint goal{},mirror{};};
inline GpsMemoryHeader gpsHeader(const uint8_t*b,GpsPoint mirror){GpsMemoryHeader h{};std::memcpy(&h.pointer,b,8);std::memcpy(&h.goal,b+0x20,12);std::memcpy(&h.count,b+0x44,4);std::memcpy(&h.active,b+0x74,4);h.mirror=mirror;return h;}
inline bool gpsHeaderSame(const GpsMemoryHeader&a,const GpsMemoryHeader&b){return a.pointer==b.pointer&&a.count==b.count&&a.active==b.active&&gpsDistance(a.goal,b.goal)<.01f&&gpsDistance(a.mirror,b.mirror)<.01f;}
inline bool gpsEmpty(const GpsMemoryHeader&h){return h.goal.x==64000&&h.goal.y==64000&&h.goal.z==64000&&std::abs(h.mirror.x)<.01&&std::abs(h.mirror.y)<.01;}
inline bool gpsGoalValid(const GpsMemoryHeader&h){return gpsPointValid(h.goal)&&gpsPointValid(h.mirror)&&std::abs(h.goal.x)<10000&&std::abs(h.goal.y)<15000&&std::hypot(h.goal.x-h.mirror.x,h.goal.y-h.mirror.y)<.1f;}
inline bool gpsSlotInstruction(const uint8_t*b){int32_t d=0;std::memcpy(&d,b+3,4);return b[0]==0x48&&b[1]==0x8d&&b[2]==5&&int64_t(GPS_SLOT_CODE_RVA)+7+d==GPS_SLOT_RVA&&b[7]==0x48&&b[8]==0x69&&b[9]==0xc9&&b[10]==0xb0&&b[17]==0x83&&b[18]==0x79&&b[19]==0x74&&b[23]==0x83&&b[24]==0x79&&b[25]==0x44;}
inline bool gpsLocalSpike(GpsPoint before, GpsPoint start, const std::vector<GpsPoint>& points,
                          size_t first, size_t end) {
    const auto finish = points[end], after = points[end + 1];
    const float dx = finish.x - start.x, dy = finish.y - start.y;
    const float span = std::hypot(dx, dy);
    if (span < 1 || span > 50) return false;
    const float ux = dx / span, uy = dy / span;
    auto aligned = [&](float x, float y) {
        const float length = std::hypot(x, y);
        return length >= .3f && x * ux + y * uy >= .951f * length &&
               std::abs(x * uy - y * ux) * span <= 2 * length;
    };
    if (!aligned(start.x - before.x, start.y - before.y) ||
        !aligned(after.x - finish.x, after.y - finish.y)) return false;

    float previousAlong = 0, maxOffset = 0, traveled = 0;
    float incomingX = start.x - before.x, incomingY = start.y - before.y;
    bool sharp = false;
    bool reversal = false;
    auto previous = start;
    for (size_t i = first; i <= end; ++i) {
        const auto p = points[i];
        const float x = p.x - start.x, y = p.y - start.y;
        const float along = x * ux + y * uy;
        const float offset = std::abs(x * uy - y * ux);
        if (along < previousAlong - 4 || along < -4 || along > span + 4 || offset > 8 ||
            std::abs(p.z - (start.z + (finish.z - start.z) * along / span)) > .8f) return false;
        maxOffset = std::max(maxOffset, offset);
        const float stepX = p.x - previous.x, stepY = p.y - previous.y;
        const float stepLength = std::hypot(stepX, stepY);
        traveled += stepLength;
        sharp |= stepX * ux + stepY * uy < .966f * stepLength;
        reversal |= stepX * incomingX + stepY * incomingY <
                    .5f * stepLength * std::hypot(incomingX, incomingY);
        incomingX = stepX;
        incomingY = stepY;
        previousAlong = std::max(previousAlong, along);
        previous = p;
    }
    if (maxOffset < .35f || !sharp) return false;
    const bool smallSpike = end - first <= 2 && maxOffset <= 3 && traveled - span <= 8;
    const bool triangle = reversal && span <= 32 && traveled <= 44 && traveled - span >= 1;
    return smallSpike || triangle;
}
inline void gpsRemoveLocalSpikes(std::vector<GpsPoint>& points) {
    if (points.size() < 5) return;
    std::vector<GpsPoint> filtered{points[0], points[1]};
    filtered.reserve(points.size());
    for (size_t i = 2; i < points.size(); ++i) {
        // A short triangle can contain intermediate waypoints on both legs.
        for (size_t end = i + 1; end <= i + 4 && end + 1 < points.size(); ++end) {
            if (gpsLocalSpike(filtered[filtered.size() - 2], filtered.back(), points, i, end)) {
                i = end;
                break;
            }
        }
        filtered.push_back(points[i]);
    }
    points.swap(filtered);
}
inline bool gpsShallowWiggle(const std::vector<GpsPoint>& points, size_t start, size_t end) {
    const auto a = points[start], b = points[end];
    const float dx = b.x-a.x, dy = b.y-a.y;
    const float span = std::hypot(dx,dy);
    if (span < 2 || span > 60) return false;
    const float ux = dx/span, uy = dy/span;
    const auto before = points[start ? start-1 : start];
    const auto after = points[std::min(end+1,points.size()-1)];
    const auto first = start ? a : points[start+1];
    const auto last = end+1 < points.size() ? b : points[end-1];
    auto slope = [&](GpsPoint from, GpsPoint to) {
        const float x = to.x-from.x, y = to.y-from.y;
        const float along = x*ux+y*uy;
        return along > .001f ? (y*ux-x*uy)/along : 1e6f;
    };
    const float inHeading = slope(before,first), outHeading = slope(last,after);
    if (std::abs(inHeading) > .10f || std::abs(outHeading) > .10f ||
        std::abs(inHeading-outHeading) > .10f) return false;

    float left = 0, right = 0, previousAlong = 0, previousHeading = inHeading;
    float maxOffset = 0;
    for (size_t i = start+1; i <= end; ++i) {
        const auto p = points[i], prev = points[i-1];
        const float along = (p.x-a.x)*ux+(p.y-a.y)*uy;
        const float offset = std::abs((p.y-a.y)*ux-(p.x-a.x)*uy);
        if (along < previousAlong || along > span+.001f || offset > .9f ||
            std::abs(p.z-(a.z+(b.z-a.z)*along/span)) > .20f) return false;
        const float heading = slope(prev,p);
        if (std::abs(heading) > .68f) return false;
        const float turn = heading-previousHeading;
        left += std::max(0.0f,turn);
        right += std::max(0.0f,-turn);
        previousAlong = along;
        previousHeading = heading;
        maxOffset = std::max(maxOffset,offset);
    }
    const float exitTurn = outHeading-previousHeading;
    left += std::max(0.0f,exitTurn);
    right += std::max(0.0f,-exitTurn);
    // Both directions must be present. A genuine bend can fit inside a narrow
    // corridor too, but its headings progress consistently in one direction.
    return maxOffset > .025f && std::min(left,right) > .02f;
}
inline void gpsStraightenShallowWiggles(std::vector<GpsPoint>& points) {
    if (points.size() < 3) return;
    std::vector<GpsPoint> filtered{points.front()};
    filtered.reserve(points.size());
    for (size_t start = 0; start+1 < points.size();) {
        size_t next = start+1;
        float traveled = 0;
        for (size_t end = start+1; end < points.size() && end <= start+48; ++end) {
            traveled += gpsDistance(points[end-1],points[end]);
            if (traveled > 64) break;
            if (end > start+1 && gpsShallowWiggle(points,start,end)) next = end;
        }
        filtered.push_back(points[next]);
        start = next;
    }
    points.swap(filtered);
}
inline std::vector<GpsPoint> gpsRoundRoad(const std::vector<GpsPoint>& points) {
    if (points.size() < 3) return points;
    std::vector<GpsPoint> corners{points.front()};
    corners.reserve(points.size());
    for (size_t i = 1; i + 1 < points.size(); ++i) {
        const auto a = corners.back(), b = points[i], c = points[i + 1];
        const float length = gpsDistance(a, c);
        const float ab = gpsDistance(a, b), bc = gpsDistance(b, c);
        // Merge collinear samples so they don't shorten the approach to a turn.
        if (length > .1f && length <= 120 && ab + bc - length < .002f) {
            const float t = ab / (ab + bc);
            const GpsPoint onLine{a.x + (c.x-a.x)*t, a.y + (c.y-a.y)*t, a.z + (c.z-a.z)*t};
            if (gpsDistance(onLine, b) < .08f) continue;
        }
        corners.push_back(b);
    }
    corners.push_back(points.back());

    std::vector<GpsPoint> rounded{corners.front()};
    rounded.reserve(corners.size());
    for (size_t i = 1; i + 1 < corners.size(); ++i) {
        const auto a = corners[i - 1], b = corners[i], c = corners[i + 1];
        const float inLength = std::hypot(b.x-a.x,b.y-a.y);
        const float outLength = std::hypot(c.x-b.x,c.y-b.y);
        if (inLength < .15f || outLength < .15f) {
            rounded.push_back(b);
            continue;
        }
        const float ix = (b.x-a.x)/inLength, iy = (b.y-a.y)/inLength;
        const float ox = (c.x-b.x)/outLength, oy = (c.y-b.y)/outLength;
        const float sweep = std::atan2(ix*oy-iy*ox, ix*ox+iy*oy);
        const float angle = std::abs(sweep);
        if (angle < .07f || angle > 2.97f) {
            rounded.push_back(b);
            continue;
        }
        const float half = angle*.5f;
        const float tangent = std::tan(half);
        // Bound corner cut and split shared edges so neighboring arcs cannot overlap.
        const float cutLimitedRadius = 3.0f / (1 / std::cos(half) - 1);
        const float trim = std::min({20.0f, inLength*.49f, outLength*.49f, cutLimitedRadius*tangent});
        if (trim < .08f) {
            rounded.push_back(b);
            continue;
        }
        const float radius = trim/tangent;
        const GpsPoint entry{b.x-ix*trim,b.y-iy*trim,b.z+(a.z-b.z)*trim/inLength};
        const GpsPoint exit{b.x+ox*trim,b.y+oy*trim,b.z+(c.z-b.z)*trim/outLength};
        const float direction = std::copysign(1.0f,sweep);
        const float rx = iy*radius*direction, ry = -ix*radius*direction;
        const unsigned steps = unsigned(std::ceil(std::max(angle/.14f,radius*angle/1.5f)));
        rounded.push_back(entry);
        for (unsigned j = 1; j < steps; ++j) {
            const float t = float(j)/steps;
            const float cs = std::cos(t*sweep), sn = std::sin(t*sweep);
            rounded.push_back({entry.x+rx*(cs-1)-ry*sn,entry.y+rx*sn+ry*(cs-1),entry.z+(exit.z-entry.z)*t});
        }
        rounded.push_back(exit);
    }
    rounded.push_back(corners.back());
    return rounded;
}
inline unsigned gpsFrameFromRoad(const std::vector<GpsPoint>& points,GpsPoint car,GpsPoint forward,GpsFrame&out){
 out.count=0;
 size_t best=0;float bestT=0,score=1e30f,bestDistance=1e30f;
 for(size_t i=0;i+1<points.size();i++){auto a=points[i],b=points[i+1];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=dx*dx+dy*dy+dz*dz,t=n>.001f?std::clamp(((car.x-a.x)*dx+(car.y-a.y)*dy+(car.z-a.z)*dz)/n,0.0f,1.0f):0;GpsPoint q{a.x+t*dx,a.y+t*dy,a.z+t*dz};float d=gpsDistance(car,q),dot=(dx*forward.x+dy*forward.y)/std::max(.01f,std::hypot(dx,dy)),rank=d+std::max(0.0f,-dot)*2;if(rank<score){score=rank;bestDistance=d;best=i;bestT=t;}}
 if(bestDistance>12)return 7;
 auto a=points[best],b=points[best+1];GpsPoint previous{a.x+(b.x-a.x)*bestT,a.y+(b.y-a.y)*bestT,a.z+(b.z-a.z)*bestT};out.points[out.count++]=previous;
 for(size_t i=best+1;i<points.size();i++){auto q=points[i];float gap=gpsDistance(previous,q);if(gap<.05f)continue;unsigned steps=unsigned(std::ceil(gap/5));if(out.count+steps>GPS_MAX_POINTS){out.count=0;return 8;}for(unsigned j=1;j<=steps;j++){float t=float(j)/steps;out.points[out.count++]={previous.x+(q.x-previous.x)*t,previous.y+(q.y-previous.y)*t,previous.z+(q.z-previous.z)*t};}previous=q;}
 if(out.count==1){out.points[1]=out.points[0];out.count=2;}
 return 0;
}
// Read-only parser: refuse unknown/stale/incomplete layouts rather than invent a connector.
inline unsigned gpsNormalize(const GpsMemoryHeader&h,const std::vector<GpsRawPoint>&raw,GpsPoint car,GpsPoint forward,GpsFrame&out){
 out.count=0;out.waypoint=h.goal;if(!gpsGoalValid(h))return 2;if(h.count<2||h.count>8192||raw.size()!=h.count||!h.active||h.active>16)return 3;
 std::vector<GpsPoint> points;points.reserve(raw.size());float total=0;
 for(const auto&p:raw){GpsPoint q{p.x,p.y,p.z};if(!gpsPointValid(q))return 4;if(points.empty()){points.push_back(q);continue;}float gap=gpsDistance(points.back(),q);if(gap>350)return 5;if(gap>.05f){total+=gap;if(total>100000)return 5;points.push_back(q);}}
 if(points.size()<2)return 3;
 auto end=points.back();if(std::hypot(end.x-h.goal.x,end.y-h.goal.y)>60)return 6;
 gpsRemoveLocalSpikes(points);
 gpsStraightenShallowWiggles(points);
 const unsigned result=gpsFrameFromRoad(gpsRoundRoad(points),car,forward,out);
 return result==0 ? 0 : gpsFrameFromRoad(points,car,forward,out);
}
template<class Read>void gpsReadMemory(Read read,uint64_t base,uint32_t pid,uint64_t now,GpsPoint car,GpsPoint forward,GpsFrame&out){
 out={};out.pid=pid;out.beat=now;out.serial=now;out.provider=1;out.status=10;
 uint8_t code[27]{},header[0xb0]{},after[0xb0]{};GpsPoint mirror{},mirrorAfter{};
 if(!read(base+GPS_SLOT_CODE_RVA,code,sizeof(code))||!gpsSlotInstruction(code)){out.reason=1;return;}
 if(!read(base+GPS_SLOT_RVA,header,sizeof(header))||!read(base+GPS_MIRROR_RVA,&mirror,sizeof(mirror))){out.reason=9;return;}
 auto h=gpsHeader(header,mirror);out.waypoint=h.goal;out.rawPointer=h.pointer;out.rawCount=h.count;out.rawActive=h.active;out.mirror=h.mirror;
 if(gpsEmpty(h)){out.status=2;return;}if(!gpsGoalValid(h)){out.reason=2;return;}if(h.count<2||h.count>8192||h.pointer<0x10000||h.pointer>=0x800000000000ull){out.reason=3;return;}
 std::vector<GpsRawPoint> raw(h.count),again(h.count);size_t bytes=raw.size()*sizeof(GpsRawPoint);
 if(!read(h.pointer,raw.data(),bytes)||!read(h.pointer,again.data(),bytes)||!read(base+GPS_SLOT_RVA,after,sizeof(after))||!read(base+GPS_MIRROR_RVA,&mirrorAfter,sizeof(mirrorAfter))){out.reason=9;return;}
 if(!gpsHeaderSame(h,gpsHeader(after,mirrorAfter))||std::memcmp(raw.data(),again.data(),bytes)){out.reason=10;return;}
 out.rawFirst={raw.front().x,raw.front().y,raw.front().z};out.rawLast={raw.back().x,raw.back().y,raw.back().z};
 out.reason=gpsNormalize(h,raw,car,forward,out);out.status=out.reason?10:4;
}
