#pragma once
#include <array>
#include <cstddef>
template<size_t N> class RisingKeys {
    std::array<bool,N> held{};
public:
    bool pressed(size_t key,bool down,bool active){
        bool fire=active&&down&&!held[key];held[key]=down;return fire;
    }
};
