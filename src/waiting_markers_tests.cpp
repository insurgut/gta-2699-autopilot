#include "waiting_markers.hpp"
#include <cassert>
int main(){
 WaitingMarkers m;std::vector<Point> path{{0,0,0,0},{100,0,0,0}};assert(!m.observe(1,22,{50,40,0,0},path));assert(m.observe(1,22,{50,2,0,0},path));assert(!m.observe(1,22,{80,2,0,0},path));assert(!m.observe(2,22,{51,2,0,0},path));assert(m.observe(3,22,{90,2,0,0},path));assert(m.entries.size()==2);
 auto file=std::filesystem::temp_directory_path()/"district-waiting-markers-test.txt";assert(m.setDelay(0,2.5,22));assert(!m.setDelay(0,-1,22));assert(!m.setDelay(0,7,99));assert(m.delayFor(1,22,{90,2,0,0})==2500);WaitApproach profile{1.5f,8,20,{9,17,33}};assert(m.setApproach(1,profile));assert(!m.setApproach(0,{4,3,8,{9,17,33}}));assert(m.save(file));WaitingMarkers reloaded;assert(reloaded.load(file,[](uint32_t model){return model==22;}));assert(reloaded.entries.size()==2&&reloaded.entries[0].pose.x==50&&reloaded.entries[0].delayMs==2500);assert(reloaded.entries[0].approach.stop==.5f);const auto* marker=reloaded.find(0,22,{91,2,0,0});assert(marker&&marker->approach.stop==1.5f&&marker->approach.kmh==profile.kmh);assert(profile.speed(1.5f)==0&&profile.speed(6)==2.5f&&profile.speed(19)>4.7f&&profile.speed(60)>9.1f);assert(reloaded.load(file,[](uint32_t){return false;}));assert(reloaded.entries.empty());
 std::ofstream(file)<<"WAIT_POINTS 1 1\n50 2 0 22\n";assert(reloaded.load(file,[](uint32_t){return true;}));assert(reloaded.entries[0].delayMs==5000);
 std::ofstream(file)<<"WAIT_POINTS 1 9000\n";assert(!reloaded.load(file,[](uint32_t){return true;}));std::filesystem::remove(file);
}
