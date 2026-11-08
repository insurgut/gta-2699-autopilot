#include "selection_arrow.hpp"
#include <cassert>
#include <iostream>
int main(){
 assert(selectionArrowColor(0xff8040)==0xcc6633);
 std::vector<ThreadSegment> lines={{0,0,0,100,0,0,10,1,2},{1,100,0,100,100,10,10,2,2},{5,300,0,400,0,20,10,1,1}};
 auto p=selectionArrowChains(lines,5,15);assert(p.size()==1&&p[0].size()==3);assert(std::abs(p[0][0].x-66.6667f)<.01f);assert(p[0].back().y==50);
 auto reverse=selectionArrowChains(lines,15,5);assert(reverse[0].front().x==p[0].front().x&&reverse[0].back().y==50);
 auto curves=roundedArrowPath(p[0]);assert(curves.size()==4&&curves[2].curve);assert(curves.front().end.x==p[0].front().x&&curves.back().end.y==50);assert(curves[1].end.x>=92&&curves[2].end.y<=8);
 assert(selectionArrowChains(lines,0,30).size()==2);assert(selectionArrowChains(lines,5,5).empty());assert(roundedArrowPath({}).empty());
 std::cout<<"Arrow perspective, clipping, reverse selection and rounded corners passed\n";
}
