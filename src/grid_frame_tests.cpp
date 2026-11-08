#include "grid_frame.hpp"
#include "voxels.hpp"
#include "frame_limit.hpp"
#include <cassert>
int main(){
 auto f=GridFrame::make(100,200,3,1,0);auto p=f.world(2,4);assert(p.x==104&&p.y==198);auto q=f.local(p.x,p.y);assert(q.x==2&&q.y==4);
 RadarBox b{104,198,3,1,0,.5f,.5f,1,WAIT_OBJECT};auto local=f.localBox(b);assert(local.x==2&&local.y==4&&local.fx==0&&local.fy==1);
 VoxelGrid grid;grid.rebuild(&local,1,0,0,3,1000,false);auto c=cellAt(2,4,3);assert(grid.blocked(c.x,c.y,3)&&grid.columnState(c.x,c.y,3)==6&&grid.prefix.empty());
 std::vector<Point> r{{100,200,3,0},{100,205,3,2}};r[0].facingX=1;r[0].facingY=0;r[0].startHalfWidth=1;r[0].startHalfLength=2.5f;
 auto start=startFrame(r);assert(start.fx==1&&start.fy==0);auto frozen=displayFrame(r,GridFrame::make(101,200,3,0,-1));assert(frozen.x==100&&frozen.fx==1&&frozen.fy==0);
 auto moving=displayFrame(r,GridFrame::make(110,200,3,0,-1));assert(moving.x==110&&moving.fy==-1);
 auto above=displayFrame(r,GridFrame::make(100,200,33,0,-1));assert(above.z==33&&above.fy==-1);
 r[1].enterCount=1;assert(startFrame(r).x==100); // oldest point remains start, never nearest/action marker
 r[0].facingX=r[0].facingY=0;r[1].direction=-1;assert(startFrame(r).fy==-1); // legacy reverse recording
 auto file=std::filesystem::temp_directory_path()/"district-grid-heading-test.txt";r[0].facingX=1;assert(writeRoute(file,r));std::vector<Point> loaded;assert(loadRoute(file,loaded));assert(loaded[0].facingX==1&&loaded[0].startHalfLength==2.5f);std::filesystem::remove(file);
 FrameLimit40 clock;assert(clock.due(0));assert(!clock.due(24999999));assert(clock.due(25000000));assert(!clock.due(25000000));assert(clock.due(1000000000));assert(!clock.due(1000000001));
 clock.reset();unsigned frames=0;for(uint64_t t=0;t<1000000000;t+=1000000)frames+=clock.due(t);assert(frames==40);
}
