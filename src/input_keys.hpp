#pragma once
#include <array>
#include <cstdint>
#include <chrono>
constexpr unsigned INPUT_KEY_COUNT=768;
using InputState=std::array<bool,INPUT_KEY_COUNT>;
constexpr std::array<unsigned,7> recordedScans{{0x11,0x1f,0x1e,0x20,0x39,0x1c,0x12}};
inline bool validInputKey(unsigned key){return key<INPUT_KEY_COUNT&&(key&255)!=0;}
inline unsigned encodeInputKey(unsigned scan,bool extended,unsigned vk){return scan&&vk!=0x13?(scan&255)|(extended?256u:0u):512u|(vk&255);}
inline uint64_t inputClock(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
