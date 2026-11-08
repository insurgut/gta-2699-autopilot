#include "traffic_prediction.hpp"
#include "horn.hpp"
#include <cassert>
#include <iostream>
int main(){
 HornPulse horn;assert(horn.update(1000,true,true)==1);assert(horn.update(1099,true,true)==0);assert(horn.update(1100,true,true)==-1);assert(horn.update(1250,true,true)==1);assert(horn.update(1350,true,true)==-1);assert(horn.update(12000,true,true)==0);assert(horn.update(12001,true,false)==0);assert(horn.update(12002,true,true)==1);assert(horn.update(12003,false,true)==-1);assert(!horn.held);assert(horn.update(12004,false,true)==0);
 std::vector<Point> route{{0,0,0,5},{0,80,0,5}};NavPose car{0,0,0,0};TrafficPredictor prediction;
 prediction.observe(1000,{{1,42,{8,6,1,0,1,1,2.3f,1,0}}});prediction.observe(1200,{{1,42,{6,6,1,0,1,1,2.3f,1,0}}});auto crossing=prediction.risk(1200,car,5,1.05f,2.5f,route,0);assert(crossing.entity==1&&crossing.cap<5);
 TrafficPredictor fast;for(unsigned i=0;i<=8;i++)fast.observe(1000+i*25,{{1,42,{8-float(i)*.25f,6,1,0,1,1,2.3f,1,0}}});assert(fast.risk(1200,car,5,1.05f,2.5f,route,0).entity==1);
 assert(!prediction.risk(3000,car,5,1.05f,2.5f,route,0).entity);
 prediction.reset();prediction.observe(1000,{{2,42,{5,7,1,0,1,1,2.3f,1,0}}});prediction.observe(1200,{{2,42,{5,8,1,0,1,1,2.3f,1,0}}});assert(!prediction.risk(1200,car,5,1.05f,2.5f,route,0).entity);
 prediction.reset();prediction.observe(1000,{{3,42,{0,10,1,0,1,1,2.3f,1,0}}});prediction.observe(1200,{{3,42,{0,13,1,0,1,1,2.3f,1,0}}});assert(!prediction.risk(1200,car,5,1.05f,2.5f,route,0).entity);
 prediction.observe(1400,{{3,42,{100,13,1,0,1,1,2.3f,1,0}}});assert(!prediction.risk(1400,car,5,1.05f,2.5f,route,0).entity);
 auto approach=[&](float x,float y,float vy,float z=1){
    TrafficPredictor traffic;
    traffic.observe(1000,{{7,42,{x,y,z,0,1,1,2.3f,1,0}}});
    traffic.observe(1200,{{7,42,{x,y+vy*.2f,z,0,1,1,2.3f,1,0}}});
    return traffic;
 };
 assert(approach(3.1f,45,-11).approaching(1200,car,6,1.05f,2.5f,0)==7);
 assert(approach(-3,-32,16).approaching(1200,car,6,1.05f,2.5f,0)==7);
 assert(approach(1,-23,6).approaching(1200,car,6,1.05f,2.5f,0)==0);
 assert(approach(1,22,7).approaching(1200,car,6,1.05f,2.5f,0)==0);
 assert(approach(1,22,0).approaching(1200,car,6,1.05f,2.5f,0)==0);
 assert(approach(10,22,-10).approaching(1200,car,6,1.05f,2.5f,0)==0);
 assert(approach(1,22,-10,5).approaching(1200,car,6,1.05f,2.5f,0)==0);
 assert(approach(2,4,8).approaching(1200,car,0,1.05f,2.5f,7)==7);
 assert(approach(1,22,-10).approaching(1700,car,6,1.05f,2.5f,0)==0);
 TrafficYield yield;
 assert(yield.update(1200,7));
 assert(yield.update(1850,7));
 assert(yield.update(2499,0));
 assert(!yield.update(2500,0));
 assert(yield.update(3000,7));
 assert(!yield.update(11000,7));
 assert(!yield.update(11500,7));
 assert(!yield.update(12000,0));
 assert(yield.update(12100,7));
 std::cout<<"Double horn timing/cancel/rearm, crossing risk, parallel traffic, moving-away traffic, stale tracks and teleport rejection passed\n";
}
