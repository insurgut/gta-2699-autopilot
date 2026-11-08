#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
enum class NoticeIcon {Speed,Error,Route,Section,Play,Pause,Check,Finish,Recovery};
struct NavigatorNotice{std::wstring key,title,detail;NoticeIcon icon=NoticeIcon::Route;uint64_t time=0,until=0;bool critical=false;};
struct NavigatorNoticeState{bool connected=false,playing=false,recording=false,finished=false,error=false,paused=false,offRoute=false,detour=false,recovering=false,waiting=false;float speed=0;std::wstring message,sectionKey,sectionText;};
class NavigatorNotices{
 std::vector<NavigatorNotice> rows;NavigatorNoticeState previous{};bool initialized=false;
public:
 void push(uint64_t now,const std::wstring&key,NoticeIcon icon,const std::wstring&title,const std::wstring&detail=L"",bool critical=false){rows.erase(std::remove_if(rows.begin(),rows.end(),[&](const auto&r){return r.key==key||now>=r.until;}),rows.end());NavigatorNotice n{key,title,detail.substr(0,200),icon,now,now+(critical?9000:5000),critical};if(critical)rows.insert(rows.begin(),n);else{auto it=std::find_if(rows.begin(),rows.end(),[](const auto&r){return !r.critical;});rows.insert(it,n);}if(rows.size()>4)rows.resize(4);}
 std::vector<NavigatorNotice> visible(uint64_t now)const{std::vector<NavigatorNotice> out;for(const auto&r:rows)if(now<r.until&&out.size()<3)out.push_back(r);return out;}
 void observe(uint64_t now,const NavigatorNoticeState&s){
  if(!initialized){previous=s;initialized=true;return;}
  if(s.connected!=previous.connected)push(now,L"connection",s.connected?NoticeIcon::Check:NoticeIcon::Error,s.connected?L"Игра подключена":L"Связь с игрой потеряна",L"",!s.connected);
  if(std::isfinite(s.speed)&&std::abs(s.speed-previous.speed)>=.5f)push(now,L"speed",NoticeIcon::Speed,L"Скорость изменена",std::to_wstring(int(std::lround(s.speed)))+L" км/ч");
  if(s.error&&(!previous.error||s.message!=previous.message))push(now,L"error",NoticeIcon::Error,L"Ошибка",s.message,true);
  if(s.recording!=previous.recording)push(now,L"recording",s.recording?NoticeIcon::Play:NoticeIcon::Check,s.recording?L"Запись начата":L"Запись завершена");
  if(s.playing&&!previous.playing)push(now,L"playback",NoticeIcon::Play,L"Автопилот включён");
  if(!s.playing&&previous.playing&&!s.finished&&!previous.finished)push(now,L"playback",NoticeIcon::Pause,L"Управление отключено");
  if(s.finished&&!previous.finished)push(now,L"finish",NoticeIcon::Finish,L"Маршрут завершён",L"Ручник включён до ручного управления.");
  if(s.paused!=previous.paused&&s.playing)push(now,L"data",s.paused?NoticeIcon::Pause:NoticeIcon::Check,s.paused?L"Проверка данных":L"Данные восстановлены",s.paused?s.message:L"Продолжаю проезд.");
  if(s.offRoute&&!previous.offRoute)push(now,L"off-route",NoticeIcon::Route,L"Отклонение от маршрута",L"Возвращаюсь к выбранному пути.");
  if(s.detour&&!previous.detour)push(now,L"detour",NoticeIcon::Route,L"Построен объезд",L"Следую найденной траектории.");
  if(s.recovering&&!previous.recovering)push(now,L"recovery",NoticeIcon::Recovery,L"Выезд из препятствия",L"Выполняю манёвр освобождения.");
  if(s.waiting&&!previous.waiting)push(now,L"waiting",NoticeIcon::Pause,L"Ожидание объекта",L"Продолжу после движения объекта и проверки проезда.");
  if(!s.waiting&&previous.waiting&&s.playing)push(now,L"waiting",NoticeIcon::Check,L"Проезд открыт");
  if(s.playing&&!s.sectionKey.empty()&&s.sectionKey!=previous.sectionKey)push(now,L"section",NoticeIcon::Section,L"Смена секции маршрута",s.sectionText);
  previous=s;
 }
};
