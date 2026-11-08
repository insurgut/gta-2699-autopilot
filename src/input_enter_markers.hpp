#pragma once
#include "input_pose.hpp"
// Recover geometric action markers from older timed recordings without rewriting
// their input stream or synthesizing timings for legacy coordinate-only routes.
inline std::vector<Point> inputEnterMarkers(const std::vector<Point>&source,const RouteInputs&input){
 if(source.size()<2||!input.available())return source;
 size_t recorded=0,tagged=0;for(const auto&e:input.events)if(e.down&&(e.key==0x1c||e.key==0x11c||e.key==(512|0x0d)))++recorded;for(const auto&p:source)tagged+=p.enterCount;if(tagged>=recorded)return source;
 const size_t needed=recorded-tagged;
 std::vector<float> arcs(source.size(),0),anchorArcs(input.anchors.size(),0);for(size_t i=1;i<source.size();i++)arcs[i]=arcs[i-1]+distance(source[i-1],source[i]);for(size_t i=1;i<input.anchors.size();i++)anchorArcs[i]=anchorArcs[i-1]+distance(input.anchors[i-1].pose,input.anchors[i].pose);
 struct Marker{size_t segment;float t,arc;Point pose;};std::vector<Marker> missing;std::vector<unsigned> used(source.size(),0);float previousArc=0;
 for(const auto&e:input.events){if(!e.down||(e.key!=0x1c&&e.key!=0x11c&&e.key!=(512|0x0d)))continue;auto pose=inputPose(input,e.ms).pose;
  auto at=std::lower_bound(input.anchors.begin(),input.anchors.end(),e.ms,[](const auto&a,uint64_t ms){return a.ms<ms;});size_t ai=size_t(at-input.anchors.begin());float expected=anchorArcs.back();if(ai<input.anchors.size()){expected=anchorArcs[ai];if(ai&&input.anchors[ai].ms>input.anchors[ai-1].ms){float t=float(e.ms-input.anchors[ai-1].ms)/float(input.anchors[ai].ms-input.anchors[ai-1].ms);expected=anchorArcs[ai-1]+(anchorArcs[ai]-anchorArcs[ai-1])*t;}}if(anchorArcs.back()>.01f)expected*=arcs.back()/anchorArcs.back();
  float score=1e30f,bestArc=0,bestT=0,bestDistance=1e30f;size_t best=0;
  for(size_t i=0;i+1<source.size();i++){auto a=source[i],b=source[i+1];float dx=b.x-a.x,dy=b.y-a.y,dz=b.z-a.z,n=dx*dx+dy*dy+dz*dz,t=n>.0001f?std::clamp(((pose.x-a.x)*dx+(pose.y-a.y)*dy+(pose.z-a.z)*dz)/n,0.0f,1.0f):0,arc=arcs[i]+(arcs[i+1]-arcs[i])*t;if(arc+.5f<previousArc)continue;float d=distance(pose,{a.x+dx*t,a.y+dy*t,a.z+dz*t,0}),rank=d*d+.0001f*(arc-expected)*(arc-expected);if(rank<score){score=rank;best=i;bestT=t;bestArc=arc;bestDistance=d;}}
  if(bestDistance>3)continue;
  previousArc=bestArc;bool exists=false;for(size_t i=0;i<source.size();i++)if(used[i]<source[i].enterCount&&std::abs(arcs[i]-bestArc)<3&&distance(source[i],pose)<3){++used[i];exists=true;break;}if(!exists&&missing.size()<needed)missing.push_back({best,bestT,bestArc,pose});
 }
 if(missing.empty()||source.size()+missing.size()>100000)return source;
 std::vector<unsigned> additions(source.size(),0);std::vector<Marker> interior;for(const auto&m:missing){if(m.t<=.001f)++additions[m.segment];else if(m.t>=.999f)++additions[m.segment+1];else interior.push_back(m);}
 std::sort(interior.begin(),interior.end(),[](const auto&a,const auto&b){return a.segment==b.segment?a.t<b.t:a.segment<b.segment;});std::vector<Point> merged;merged.reserve(source.size()+interior.size());size_t at=0;
 for(size_t i=0;i<source.size();i++){auto p=source[i];p.enterCount=std::min(255u,p.enterCount+additions[i]);merged.push_back(p);while(at<interior.size()&&interior[at].segment==i){auto action=interior[at++].pose;action.speedLimit=source[std::min(i+1,source.size()-1)].speedLimit;action.ignoreObstacles=source[std::min(i+1,source.size()-1)].ignoreObstacles;action.waitMs=0;action.enterCount=1;if(distance(merged.back(),action)<.05f)merged.back().enterCount=std::min(255u,merged.back().enterCount+1);else merged.push_back(action);}}
 return merged;
}
