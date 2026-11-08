#include "trip_history.hpp"
#include <cassert>
#include <chrono>
int main(){
    const auto path=std::filesystem::temp_directory_path()/
        ("district-trip-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    {
        TripHistory history;
        history.open(path);
        history.observe(1000,1,{0,0,0},false);
        history.observe(1200,1,{10,0,0},false);
        history.observe(1400,1,{20,0,0},true);
        history.observe(1600,1,{400,0,0},true);
        history.observe(1800,2,{420,0,0},false);
        assert(history.flush(2000,true));
        assert(history.size()==5);
        const auto part=history.read(1,3);
        assert(part.size()==3&&part[0].flags==0&&part[1].flags==TripHistory::Auto);
        assert(part[2].flags==(TripHistory::Auto|TripHistory::NewTrip));
        assert(std::abs(history.distanceKm()-.02)<1e-6);
    }
    {
        std::ofstream tail(path,std::ios::app|std::ios::binary);
        tail.put('x');
    }
    {
        TripHistory reopened;
        reopened.open(path);
        assert(reopened.size()==5&&std::abs(reopened.distanceKm()-.02)<1e-6);
        reopened.observe(1000,2,{500,0,0},false);
        assert(reopened.flush(1000,true));
        const auto next=reopened.read(5,4096);
        assert(next.size()==1&&(next[0].flags&TripHistory::NewTrip));
        assert(std::abs(reopened.distanceKm()-.02)<1e-6);
    }
    std::filesystem::remove(path);
}
