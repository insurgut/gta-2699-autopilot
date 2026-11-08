#pragma once
#include <array>
#include <cstdint>
// Latches short physical taps across an arbitrarily delayed controller tick.
class PhysicalPresses {
 std::array<bool,256> held{};std::array<uint8_t,256> pending{};
public:
 bool observe(unsigned key,bool down,bool eligible,bool injected){if(injected||key>=held.size())return false;bool edge=down&&!held[key];held[key]=down;if(edge&&eligible&&pending[key]<32)++pending[key];return edge&&eligible;}
 bool take(unsigned key,bool eligible){if(key>=pending.size())return false;if(!eligible){pending[key]=0;return false;}if(!pending[key])return false;--pending[key];return true;}
 void discard(unsigned key){if(key<pending.size())pending[key]=0;}
};
