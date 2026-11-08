#pragma once
#include <array>
#include <string>
struct ImprovementSettings {
 bool anticipatorySpeed=true,stableDetour=true,autoRecovery=true,trafficPrediction=true,flightRecording=true;
 bool set(const std::wstring&key,bool enabled);
 template<class Read>void load(Read read);
};
struct ImprovementBinding {const wchar_t*key;bool ImprovementSettings::*member;};
inline constexpr std::array<ImprovementBinding,5> improvementBindings{{
 {L"anticipatorySpeed",&ImprovementSettings::anticipatorySpeed},
 {L"stableDetour",&ImprovementSettings::stableDetour},
 {L"autoRecovery",&ImprovementSettings::autoRecovery},
 {L"trafficPrediction",&ImprovementSettings::trafficPrediction},
 {L"flightRecording",&ImprovementSettings::flightRecording}
}};
inline bool ImprovementSettings::set(const std::wstring&key,bool enabled){for(const auto&b:improvementBindings)if(key==b.key){this->*b.member=enabled;return true;}return false;}
template<class Read>void ImprovementSettings::load(Read read){for(const auto&b:improvementBindings)this->*b.member=read(b.key,true);}
