#include "driving.hpp"
#include <cassert>
#include <iostream>
float simulate(std::vector<Point> r,Point pos,float yaw,bool requireArrival=true){
    float speed=0,maxError=0;size_t segment=0;bool arrived=false;
    for(int n=0;n<20000;n++){
        auto c=driveOutput(r,segment,pos,std::sin(yaw),std::cos(yaw),speed,8);segment=c.segment;
        maxError=std::max(maxError,c.error);if(c.arrived){arrived=true;break;}
        float acceleration=std::clamp((c.targetSpeed-speed)*2,-5.0f,2.5f);speed=std::max(0.0f,speed+acceleration*0.02f);
        yaw+=speed/2.8f*std::tan(c.steering*0.45f)*0.02f;
        pos.x+=std::sin(yaw)*speed*0.02f;pos.y+=std::cos(yaw)*speed*0.02f;
    }
    if(requireArrival)assert(arrived);
    return maxError;
}
int main(){
    std::vector<Point> straight;for(int i=0;i<=60;i++)straight.push_back({0,float(i*2),0,6});
    assert(simulate(straight,{0,0,0,0},0)<0.01f);
    assert(simulate(straight,{2,0,0,0},0)<3);
    std::vector<Point> curve;for(int i=0;i<=60;i++){float a=float(i)/60*1.57079633f;curve.push_back({30*(1-std::cos(a)),30*std::sin(a),0,6});}
    assert(simulate(curve,{0,0,0,0},0)<4);
    std::reverse(straight.begin(),straight.end());
    float reverseError=simulate(straight,{0,120,0,0},0);
    assert(reverseError<12);
    auto right=driveOutput({{0,0,0,4},{20,20,0,4}},0,{0,0,0,0},0,1,3,8);assert(right.steering>0);
    auto left=driveOutput({{0,0,0,4},{-20,20,0,4}},0,{0,0,0,0},0,1,3,8);assert(left.steering<0);
    auto lost=driveOutput(straight,0,{50,120,0,0},0,1,3,8);assert(lost.error>12);
    constexpr float rad=3.14159265359f/180;
    for(float deg:{0.0f,1.0f,5.0f,9.0f,10.0f})assert(std::abs(bendSpeedLimit(deg*rad,12,25,0)-25)<.001f);
    assert(std::abs(bendSpeedLimit(20*rad,12,25,0)-std::sqrt(4/(2*std::sin(20*rad)/12)))<.001f);
    assert(bendSpeedLimit(45*rad,12,25,0)<7);assert(bendSpeedLimit(5*rad,12,25,2)<25);
    assert(std::abs(bendSpeedLimit(10.01f*rad,12,25,0)-bendSpeedLimit(9.99f*rad,12,25,0))<.001f);
    auto tiny=driveOutput({{0,0,0,25},{20,200,0,25}},0,{0,0,0,0},0,1,15,25);assert(tiny.targetSpeed==25&&tiny.steering>0);
    auto big=driveOutput({{0,0,0,25},{150,150,0,25}},0,{0,0,0,0},0,1,15,25);assert(big.targetSpeed<10);
    auto finish=driveOutput({{0,0,0,25},{0,10,0,25}},0,{0,0,0,0},0,1,15,25);assert(finish.targetSpeed<10);
    auto explicitStop=driveOutput({{0,0,0,25},{0,10,0,25},{0,200,0,25}},0,{0,0,0,0},0,1,15,25,1);assert(explicitStop.targetSpeed<10);
    std::cout<<"Driving simulation passed: straight, recovery, curve, U-turn, steering polarity, off-route detection. U-turn max error="<<reverseError<<"\n";
}
