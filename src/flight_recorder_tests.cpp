#include "flight_recorder.hpp"
#include <cassert>
#include <iostream>
int main(){
 assert(flightNumber(std::numeric_limits<float>::quiet_NaN())==0);assert(flightJson("a\n\"b\\")=="\"a\\u000a\\\"b\\\\\"");
 auto dir=std::filesystem::temp_directory_path()/("district-flight-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));FlightRecorder recorder;
 for(unsigned i=1;i<=150;i++){FlightFrame f;f.now=i*200;f.reason="test\nstatus";f.route="route1";f.objects.push_back({0,4,1,0,1,1,2,1,0});f.planned.push_back({0,10,0,0});recorder.push(std::move(f));}assert(recorder.size()==120);
 recorder.mark(30000,"test incident");assert(!recorder.due(31999));assert(recorder.due(32000));assert(recorder.save(dir,32000));size_t count=0;for(const auto&e:std::filesystem::directory_iterator(dir)){++count;std::ifstream file(e.path());std::string line;unsigned lines=0;while(std::getline(file,line)){assert(line.front()=='{'&&line.back()=='}');++lines;}assert(lines==121);}assert(count==1);
 for(unsigned i=1;i<=12;i++){uint64_t now=32000+i*33000;recorder.mark(now,"next");assert(recorder.save(dir,now+2000));}count=0;for(const auto&e:std::filesystem::directory_iterator(dir)){(void)e;++count;}assert(count==10);std::filesystem::remove_all(dir);
 std::cout<<"Bounded rolling flight capture, JSON escaping, delayed capture, cooldown and ten-file retention passed\n";
}
