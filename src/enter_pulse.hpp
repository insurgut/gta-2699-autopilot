#pragma once
#include <cstdint>
// Permission gates only the start; a queued press survives a transient pause and
// an already-started key is always released on its deadline.
inline int enterPulse(uint64_t now,bool mayStart,unsigned&queued,bool held,uint64_t&deadline){if(held&&now>=deadline){deadline=now+100;return -1;}if(!held&&queued&&mayStart&&now>=deadline){--queued;deadline=now+150;return 1;}return 0;}
