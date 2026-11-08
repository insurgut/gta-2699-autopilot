#pragma once
#include <algorithm>
#include <cstdint>

class TileDownloadBudget {
    unsigned successes=0,failures=0,limit=5;
    uint64_t resumeAt=0;
public:
    static constexpr unsigned maximum=15;
    unsigned parallelism()const{return limit;}
    uint64_t retryAt()const{return resumeAt;}
    void success(){
        failures=0;
        if(++successes>=12){limit=std::min(maximum,limit+1);successes=0;}
    }
    void failed(uint64_t now){
        successes=0;
        if(++failures>=3){
            limit=std::max(5u,limit-2);
            resumeAt=std::max(resumeAt,now+1000);
            failures=0;
        }
    }
    void busy(uint64_t now,uint64_t delay){
        limit=5;
        successes=failures=0;
        resumeAt=std::max(resumeAt,now+delay);
    }
};
