#include "route.hpp"
#include <cassert>
#include <limits>
int main(){
    auto file=std::filesystem::temp_directory_path()/"gta-autopilot-route-test.txt";
    std::vector<Point> route{{0,0,0,0},{4,0,0,5},{8,0,0,6},{12,0,0,7},{16,0,0,8},{20,0,0,9},{24,0,0,10},{28,0,0,0}};
    assert(nextGoal(route,0,1)==6);assert(nextGoal(route,7,-1)==1);
    assert(nextGoal(route,7,1)==7);assert(nextGoal(route,0,-1)==0);
    assert(writeRoute(file,route));std::vector<Point> out;assert(loadRoute(file,out));assert(out.size()==route.size());
    assert(out[6].x==24&&out[6].speed==10);
    std::ofstream(file)<<"GTA_AUTOPILOT 1 999999999\n";assert(!loadRoute(file,out));assert(out.size()==route.size());
    std::ofstream(file)<<"GTA_AUTOPILOT 1 2\n0 0 0 0\n";assert(!loadRoute(file,out));
    std::ofstream(file)<<"GTA_AUTOPILOT 1 2\n0 0 0 0\n1 1 1 0\nextra";assert(!loadRoute(file,out));
    route[0].x=std::numeric_limits<float>::infinity();assert(!writeRoute(file,route));
    std::filesystem::remove(file);
}
