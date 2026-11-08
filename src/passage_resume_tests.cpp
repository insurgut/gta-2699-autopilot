#include "observed_passage.hpp"
#include "resume_policy.hpp"
#include "voxels.hpp"
#include <cassert>
#include <iostream>
std::vector<PassageObservation> run(float lateral,bool stopped,bool turn=false){
    PassageLearner learner;PassageObject o{{10,lateral,0.8f,1,0,0.6f,0.6f,0.7f,0},12};std::vector<PassageObservation> learned;
    for(int i=0;i<=150;i++){float x=i*0.1f;auto r=learner.update({x,0,0,turn&&i>60?0.0f:1.0f,turn&&i>60?1.0f:0.0f,stopped&&i>=85&&i<=115?0.0f:2.0f,2.5f,uint64_t(1000+i*50),1},{o});learned.insert(learned.end(),r.begin(),r.end());}return learned;
}
int main(){assert(run(0,false).size()==1);assert(run(2,false).empty());assert(run(0,true).empty());assert(run(0,false,true).empty());
    PassageLearner reverse;std::vector<PassageObservation> found;PassageObject o{{-10,0,0.8f,1,0,.6f,.6f,.7f,0},12};for(int i=0;i<=150;i++){auto r=reverse.update({-i*.1f,0,0,1,0,2,2.5f,uint64_t(1000+i*50),-1},{o});found.insert(found.end(),r.begin(),r.end());}assert(found.size()==1);
    assert(!observedMatches(found[0],{{-10,2,.8f,1,0,.6f,.6f,.7f,0},12}));
    SlowdownConfirmation slowdown;assert(!slowdown.update(1000,true));assert(!slowdown.update(1100,false));assert(!slowdown.update(1200,true));assert(slowdown.update(1700,true));
    assert(canResumeBlocked(true,true,true,0));assert(!canResumeBlocked(true,false,true,0));assert(!canResumeBlocked(true,true,false,0));
    assert(setDrawRadius(1)&&setDrawRadius(100)&&!setDrawRadius(.5f)&&!setDrawRadius(101));
    VoxelGrid grid;RadarBox b{4,0,.8f,1,0,.6f,.6f,.7f,3};grid.rebuild(&b,1,0,0,0,1000);auto c=cellAt(4,0,0);assert(grid.columnState(c.x,c.y,0)==5);RadarBox boxes[]={b,b};boxes[1].passability=0;grid.rebuild(boxes,2,0,0,0,1100);assert(grid.columnState(c.x,c.y,0)==3);
    std::cout<<"Passage/resume tests passed\n";
}
