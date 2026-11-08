#pragma once
#include <string>
inline bool hudErrorMessage(const std::wstring&s){for(const wchar_t*p:{L"Ошибка",L"Не удалось",L"Не найден",L"Нет подтвержд",L"Нет телеметрии",L"Неправиль",L"Обнаружен скачок",L"Обработка остановлена",L"Не совпал"})if(s.rfind(p,0)==0)return true;return false;}
inline std::wstring hudStatus(bool recording,bool playing,bool fault,const std::wstring&message){if(fault||(!recording&&!playing&&hudErrorMessage(message)))return L"Ошибка: "+message.substr(0,240);if(recording)return L"Запись";if(playing)return L"Воспроизведение";return {};}
