#include <functional>
#include "attach.hpp"
#include "driving.hpp"
#include "stops.hpp"
#include "voxels.hpp"
#include "overlay.hpp"
#include "recovery.hpp"
#include "observed_passage.hpp"
#include "resume_policy.hpp"
#include "progress_policy.hpp"
#include "radar_pause.hpp"
#include "hotkeys.hpp"
#include "waiting_stops.hpp"
#include "wait_cycle.hpp"
#include "finish_hold.hpp"
#include "route_actions.hpp"
#include "ui_theme.hpp"
#include "apil_runtime.hpp"
#include "apil_update.hpp"
#include <windowsx.h>
#include "native_marker_channel.hpp"
#include "grid_frame.hpp"
#include "forward_navigation.hpp"
#include "enter_approach.hpp"
#include "hud_status.hpp"
#include "loop_policy.hpp"
#include "physical_recovery.hpp"
#include "direct_rejoin.hpp"
#include "traffic_prediction.hpp"
#include "horn.hpp"
#include "flight_recorder.hpp"
#include "improvement_settings.hpp"
#include "physical_presses.hpp"
#include "input_enter_markers.hpp"
#include "enter_pulse.hpp"
#include "route_sections.hpp"
#include "waiting_markers.hpp"
#include "gps_channel.hpp"
#include "gps_navigation.hpp"
#include "gps_pace.hpp"
#include "trip_history.hpp"
#include "notice_section.hpp"
#include <shlobj.h>
#include <shellapi.h>
#include <fstream>
#include <sstream>
#include <cwchar>
#include <array>
#include <iterator>
namespace {
GpsChannel gpsChannel;
GpsTurnPace gpsTurnPace;
GpsBrakeModel gpsBrakeModel;
std::vector<GpsPoint> gpsPreviewLine;bool gpsEnabled=true,gpsDriving=false,gpsPreview=true;
bool gpsMarkerSuppressed=false;GpsPoint gpsSuppressedGoal{};float gpsSpeed=30,gpsWheelStep=5;int gpsWheelDelta=0;uint64_t gpsSerial=0;GpsPoint gpsGoal{};std::wstring gpsStatus=L"Внешнее чтение GPS ещё не подключено.";
const wchar_t* gpsReadReason(unsigned reason){switch(reason){case 1:return L"Профиль GPS изменился, нужна диагностика.";case 2:return L"Координаты метки пока не согласованы.";case 3:return L"Ожидание буфера дороги GTA.";case 4:return L"Дорога содержит неверные координаты.";case 5:return L"В считанной дороге найден разрыв.";case 6:return L"Конец считанной дороги пока не совпал с меткой.";case 7:return L"Машина далеко от дороги: подъедь ближе и включи GPS снова.";case 8:return L"Дорога слишком длинная для текущего буфера.";case 9:return L"Данные GPS пока недоступны для чтения.";case 10:return L"GTA перестраивает дорогу, ожидаю стабильные данные.";default:return L"Ожидание проверенного пути GTA.";}}
RouteInputs routeInputs;PhysicalPresses physicalPresses;
bool smartEnabled(bool enabled){return enabled;}
ImprovementSettings improvements;
TrafficPredictor trafficPredictor;TrafficRisk trafficRisk;TrafficYield trafficYield;SpeedDemand speedDemand;HornPulse hornPulse;bool hornHeld=false;uint64_t trafficSerial=0,lastTraffic=0,lastFlight=0;FlightRecorder flightRecorder;float traceTarget=0,traceSteer=0,traceError=0;
void markFlight(uint64_t now,const std::string&reason){if(improvements.flightRecording)flightRecorder.mark(now,reason);}
apil::Updater updater;void updateApilUi();void uiCommand(const std::wstring& command);
HWND window,statusLabel,connectionLabel,statsLabel,sceneLabel,sceneBox,loopBox,speedBox,connectButton,radarBox,overlayBox;
std::filesystem::path settingsFile;std::unordered_map<uint32_t,unsigned> objectRules;
NativeMarkerChannel legacyStartMarkers,legacyFinishMarkers{L".finish"};Overlay overlay;RadarResult radar{{40,40,40,40,40},40};StopRecorder stopRecorder;
VoxelGrid grid;ForwardDetour forwardDetour;EnterStall enterStall;std::vector<GridPoint> detour;uint64_t lastGrid=0,throttleSince=0;float signedSpeed=0;int motionDirection=1,driveDirection=1;
uint64_t plannedAt=0;
StallDetector stallDetector;HoldClock holdClock;
SlowdownConfirmation slowdown;PassageLearner passageLearner;std::vector<PassageObservation> observedPassages;
uint64_t lastLearning=0,lastBlockedCheck=0;bool blockedNeedsReposition=false;Point blockedOrigin{};
EscapePlan recovery;GridPoint recoveryGoal{},recoveryPrevious{},recoveryOrigin{};float recoveryProgress=0;uint64_t recoveryDeadline=0,recoveryLastMotion=0;unsigned recoveryAttempts=0;int resumeDirection=1;
size_t pendingStop=SIZE_MAX;uint64_t holdStarted=0;float lateralShift=0;int avoidanceLane=2;
HFONT font;DWORD pid=0;HANDLE mapping=nullptr,mutex=nullptr;Telemetry*shared=nullptr;
Telemetry sample{};bool haveSample=false,connecting=false;
std::wstring message=L"Подключи игру, затем запиши маршрут.";
std::filesystem::path appDir,dataDir,routeFile,desktopDir;
RouteCatalog routeCatalog;size_t selectedRoute=SIZE_MAX;bool routeManuallySelected=false;uint32_t recordingColor=0xffffff;NavigatorSettings navigatorSettings;
std::vector<Point> route,recording,activeRoute,playbackRoute;WaitingMarkers waitingMarkers;uint64_t routeVisualRevision=1;
enum class Mode {Idle,Record,Armed,Drive,Hold,Brake,Parked,RecoveryBrake,RecoveryMove,RecoverySettle,BlockedWait,Emergency,RadarWait,ObjectWait,ObjectApproach,EnterWait,PickBrake,JoinWait};Mode mode=Mode::Idle;
Mode enterResumeMode=Mode::Drive;
bool hornEnabled=true;bool loopEnabled=false;uint64_t completedLaps=0;
TripHistory tripHistory;
bool autoGpsBaselinePending=false;
RoutePick resumePick;bool resumeReverse=false,resumeRequested=false;uint64_t resumeUntil=0,resumeAttemptAt=0;bool resumeNearest=false,joinStaging=false,requireNearbyStart=false;unsigned joinAttempts=0;NavPose recoveryStart{};ContactBackoffGate contactBackoff;
Mode radarResumeMode=Mode::Idle;uint64_t radarPausedAt=0;FreshSceneGate sceneGate,objectClearGate;float objectWaitRange=0;WaitCycle waitCycle;uint64_t waitObservedSerial=0;
uint64_t lastUi=0,lastRecord=0,deadline=0,lastMotion=0,lastSpeed=0,lastSafety=0;
uint64_t lastSceneSerial=0;bool lastSceneValid=false;std::vector<RadarBox> sceneBoxes;
uint64_t stopRuleScene=0;size_t ignoredTimedStops=0;
Point previous{},motionOrigin{};bool previousValid=false;
float speed=0;size_t segment=0;uint64_t drivingVehicle=0;DWORD safetyPid=0;
bool blocked=false;bool keyState[5]{};constexpr WORD scans[]={0x11,0x1f,0x1e,0x20,0x39}; // W,S,A,D,Space
FinishHold finishHold;HHOOK keyboardHook=nullptr,mouseHook=nullptr;
NavigatorNotices navigatorNotices;
uint32_t currentRouteColor();
std::wstring currentRouteName(){
    if(gpsDriving)return L"Маршрут по GPS-метке";
    if(selectedRoute>=routeCatalog.entries.size())return L"Маршрут";
    const auto& name=routeCatalog.entries[selectedRoute].name;
    const int size=MultiByteToWideChar(CP_UTF8,0,name.data(),int(name.size()),nullptr,0);
    std::wstring out(size_t(std::max(0,size)),L'\0');
    if(size)MultiByteToWideChar(CP_UTF8,0,name.data(),int(name.size()),out.data(),size);
    return out;
}
void updateNavigatorNotices(){
    auto now=GetTickCount64();static uint64_t lastUpdate=0;if(lastUpdate&&now-lastUpdate<50)return;lastUpdate=now;NavigatorNoticeState n;
    n.connected=haveSample&&sample.valid&&sample.sampleBeat&&now>=sample.sampleBeat&&now-sample.sampleBeat<1500;
    n.recording=mode==Mode::Record||(mode==Mode::RadarWait&&radarResumeMode==Mode::Record);
    n.finished=mode==Mode::Parked;
    n.playing=mode!=Mode::Idle&&!n.recording&&!n.finished;
    n.error=mode==Mode::Emergency||hudErrorMessage(message);n.message=message;
    static bool offRoute=false;n.paused=mode==Mode::RadarWait;
    offRoute=n.playing&&mode==Mode::Drive&&traceError>(offRoute?2.0f:4.0f);n.offRoute=offRoute;
    n.detour=n.playing&&forwardDetour.active();n.recovering=mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::RecoverySettle;
    n.waiting=mode==Mode::ObjectWait;
    n.speed=(gpsDriving?gpsSpeed:n.playing&&!activeRoute.empty()?activeRoute[std::min(segment+1,activeRoute.size()-1)].speedLimit*3.6f:0);
    // A policy value is a target; braking and traffic corrections must never produce speed-change spam.
    if(!gpsDriving&&n.speed<=0){wchar_t value[40]{};GetWindowTextW(speedBox,value,40);n.speed=float(_wtof(value));}
    if(n.playing&&!gpsDriving){auto section=noticeSection(activeRoute,segment,selectedRoute);n.sectionKey=section.key;n.sectionText=section.text;}
    navigatorNotices.observe(now,n);
    auto notices=navigatorNotices.visible(now);
    notices.erase(std::remove_if(notices.begin(),notices.end(),[&](const NavigatorNotice& notice){
        return (n.recording&&notice.key==L"recording")||(n.playing&&notice.key==L"playback");
    }),notices.end());
    overlay.notifications(std::move(notices));
    overlay.navigatorActivity(n.recording,n.playing&&!n.error,currentRouteName(),currentRouteColor());
}
Point point();
LRESULT CALLBACK manualKeyboard(int code,WPARAM wp,LPARAM lp){
    if(code==HC_ACTION){auto*k=reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);bool injected=(k->flags&LLKHF_INJECTED)!=0,down=wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN;finishHold.input(injected,down);
        DWORD activePid=0;GetWindowThreadProcessId(GetForegroundWindow(),&activePid);
        bool edge=physicalPresses.observe(k->vkCode,down,pid&&activePid==pid,injected);
        if(edge&&k->vkCode==VK_RETURN&&mode==Mode::Record&&haveSample&&sample.vehicle==drivingVehicle){if(recordEnter(recording,point())){++routeVisualRevision;message=L"Точка Enter сохранена.";}}

    }
    return CallNextHookEx(nullptr,code,wp,lp);
}
LRESULT CALLBACK manualMouse(int code,WPARAM wp,LPARAM lp){
    if(code==HC_ACTION){auto*m=reinterpret_cast<MSLLHOOKSTRUCT*>(lp);DWORD activePid=0;GetWindowThreadProcessId(GetForegroundWindow(),&activePid);if(wp==WM_MOUSEWHEEL&&gpsDriving&&activePid==pid&&!(m->flags&LLMHF_INJECTED)){gpsWheelDelta+=SHORT(HIWORD(m->mouseData));return 1;}finishHold.input((m->flags&LLMHF_INJECTED)!=0,wp==WM_LBUTTONDOWN||wp==WM_RBUTTONDOWN||wp==WM_MBUTTONDOWN||wp==WM_XBUTTONDOWN||wp==WM_MOUSEWHEEL||wp==WM_MOUSEHWHEEL);}
    return CallNextHookEx(nullptr,code,wp,lp);
}
RouteActions routeActions;bool enterHeld=false;unsigned enterQueued=0;uint64_t enterDeadline=0;
bool outputKey(size_t i,bool down){
    if(keyState[i]==down)return true;
    INPUT input{};input.type=INPUT_KEYBOARD;input.ki.wScan=scans[i];input.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);
    if(SendInput(1,&input,sizeof(input))!=1)return false;
    keyState[i]=down;return true;
}
bool enterKey(bool down){INPUT k{};k.type=INPUT_KEYBOARD;k.ki.wScan=0x1c;k.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);if(SendInput(1,&k,sizeof(k))!=1)return false;enterHeld=down;return true;}
bool hornKey(bool down){INPUT k{};k.type=INPUT_KEYBOARD;k.ki.wScan=0x12;k.ki.dwFlags=KEYEVENTF_SCANCODE|(down?0:KEYEVENTF_KEYUP);if(SendInput(1,&k,sizeof(k))!=1)return false;hornHeld=down;return true;}
void cancelHorn(){hornPulse.update(GetTickCount64(),false,false);if(hornHeld)hornKey(false);}
void release(){cancelHorn();speedDemand.reset();for(size_t i=0;i<5;i++)outputKey(i,false);if(enterHeld)enterKey(false);enterQueued=0;enterDeadline=0;}
void appendEvent(const wchar_t*reason)noexcept;
void stop(const wchar_t*text){if(gpsDriving){gpsMarkerSuppressed=true;gpsSuppressedGoal=gpsGoal;++routeVisualRevision;}gpsDriving=false;gpsPreviewLine.clear();overlay.gpsPrediction({},1);gpsSerial=0;gpsChannel.command(GetTickCount64(),false,false,1,{});overlay.cancelSelection();resumeRequested=false;joinStaging=false;requireNearbyStart=false;trafficYield.reset();contactBackoff.reset();finishHold.reset();waitCycle.reset();appendEvent(text);release();mode=Mode::Idle;drivingVehicle=0;throttleSince=0;stallDetector.reset();holdClock.reset();slowdown.reset();passageLearner.reset();detour.clear();forwardDetour.reset();message=text;}
bool foreground(){DWORD f=0;GetWindowThreadProcessId(GetForegroundWindow(),&f);return pid&&f==pid;}
void holdHandbrake(){
    if(!foreground()){release();return;}
    for(unsigned i=0;i<4;i++)outputKey(i,false);
    if(keyState[4]&&!(GetAsyncKeyState(VK_SPACE)&0x8000))keyState[4]=false;
    outputKey(4,true);
}
Point point(){float n=std::hypot(sample.forward.x,sample.forward.y);return {sample.position.x,sample.position.y,sample.position.z,speed,0,motionDirection,0,n>.01f?sample.forward.x/n:0,n>.01f?sample.forward.y/n:0,sample.ownHalfWidth,sample.ownHalfLength};}
void prepareLap(uint64_t now,const std::vector<Point>*initial=nullptr){
    release();finishHold.reset();sceneGate.reset();objectClearGate.reset();enterStall.reset();
    ignoredTimedStops=0;stopRuleScene=0;waitCycle.reset();waitObservedSerial=0;
    activeRoute=initial?*initial:playbackRoute;routeActions.reset();segment=0;pendingStop=nextStop(activeRoute,1);
    lateralShift=0;avoidanceLane=2;throttleSince=0;detour.clear();forwardDetour.reset();
    recovery={};recoveryAttempts=0;recoveryDeadline=0;recoveryLastMotion=0;
    stallDetector.reset();holdClock.reset();holdStarted=0;slowdown.reset();passageLearner.reset();
    plannedAt=0;lastGrid=0;grid.clear();blockedNeedsReposition=false;lastBlockedCheck=0;
    lastMotion=now;motionOrigin=point();enterResumeMode=Mode::Drive;
    mode=Mode::Armed;deadline=now+15000;drivingVehicle=sample.vehicle;
}
void blockedWait(const wchar_t*why,bool contact=false){if(mode==Mode::Drive)resumeDirection=driveDirection;appendEvent(why);blockedNeedsReposition=contact;blockedOrigin=point();release();mode=Mode::BlockedWait;throttleSince=0;slowdown.reset();lastBlockedCheck=0;message=why;}
void emergency(const wchar_t*why){cancelHorn();if(enterHeld)enterKey(false);enterQueued=0;appendEvent(why);mode=Mode::Emergency;holdHandbrake();message=why;}
void detach(){
    legacyStartMarkers.close();legacyFinishMarkers.close();release();trafficPredictor.reset();trafficSerial=0;trafficRisk={};if(shared){Lock lock(mutex);if(lock.held){shared->shutdown=1;shared->reserved=0;}}
    if(shared)UnmapViewOfFile(shared);
    if(mapping)CloseHandle(mapping);
    if(mutex)CloseHandle(mutex);
    shared=nullptr;mapping=mutex=nullptr;haveSample=false;previousValid=false;
}
bool telemetry(){
    if(!shared&&pid){
        mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,ipcName(pid,true).c_str());
        mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,ipcName(pid).c_str());
        if(mapping&&mutex)shared=static_cast<Telemetry*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Telemetry)));
        if(!shared){if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);mapping=mutex=nullptr;return false;}
    }
    if(!shared)return false;
    Lock lock(mutex);if(!lock.held){haveSample=sample.valid&&sample.sampleBeat&&GetTickCount64()-sample.sampleBeat<500;return haveSample;}
    if(shared->magic!=TELEMETRY_MAGIC||shared->version!=TELEMETRY_VERSION||shared->pid!=pid){haveSample=false;return false;}
    if(shared->reserved&2u){release();if(mode!=Mode::Idle&&mode!=Mode::Record)emergency(L"Пауза обработки. Машина удерживается ручником; Del — отменить управление.");}
    std::fill(std::begin(shared->replayKeys),std::end(shared->replayKeys),uint8_t(0));shared->clientBeat=GetTickCount64();shared->reserved=(enterHeld||hornHeld||std::any_of(std::begin(keyState),std::end(keyState),[](bool v){return v;}))?1u:0u;sample=*shared;sample.error[159]=0;
    haveSample=sample.sampleBeat&&GetTickCount64()-sample.sampleBeat<500&&sample.valid;
    return haveSample;
}
float maximum(){if(gpsDriving)return gpsSpeed/3.6f;wchar_t text[64];GetWindowTextW(speedBox,text,64);wchar_t*end=nullptr;double n=wcstod(text,&end)/3.6;return end!=text&&!*end&&std::isfinite(n)&&n>0&&n<3.4e38?float(n):0;}
bool ignoringRouteObstacles(){if(gpsDriving)return true;if(mode==Mode::Idle||mode==Mode::Record)return false;if(resumeRequested&&(mode==Mode::JoinWait||(mode==Mode::RadarWait&&radarResumeMode==Mode::JoinWait)))return route.size()>1&&route[std::min(resumePick.segment+1,route.size()-1)].ignoreObstacles;return !activeRoute.empty()&&activeRoute[std::min(segment+1,activeRoute.size()-1)].ignoreObstacles;}
bool radarEnabled(){return !ignoringRouteObstacles()&&SendMessageW(radarBox,BM_GETCHECK,0,0)==BST_CHECKED;}
unsigned passability(uint32_t model){
    if(!model)return 0;
    auto it=objectRules.find(model);if(it!=objectRules.end())return it->second;
    unsigned rule=unsigned(GetPrivateProfileIntW(L"objects",std::to_wstring(model).c_str(),0,settingsFile.c_str()));
    if(rule>4||rule==3)rule=0;
    objectRules[model]=rule;return rule;
}
unsigned obstacleRule(const Telemetry::Obstacle&o){
    unsigned rule=passability(o.modelHash);if(waitCycle.passing&&o.entity==waitCycle.id)return WAIT_PASS;
    if(!rule&&o.kind==5){PassageObject p{{o.position.x,o.position.y,o.position.z,o.forward.x,o.forward.y,o.halfWidth,o.halfLength,o.halfHeight,0},o.modelHash};for(const auto&v:observedPassages)if(observedMatches(v,p))return 3;}
    return rule;
}
void saveObserved();
uint64_t routeStamp(){uint64_t h=1469598103934665603ull;for(const auto&p:route){std::ostringstream text;text.precision(9);text<<p.x<<' '<<p.y<<' '<<p.z<<' '<<p.speed<<' '<<p.waitMs<<' '<<p.direction;if(p.enterCount)text<<" enter="<<p.enterCount;for(unsigned char c:text.str()){h^=c;h*=1099511628211ull;}}return h;}
void saveObserved(){auto file=routeFile;file+=L".observed";std::ofstream f(file);f.precision(9);f<<routeStamp()<<' '<<observedPassages.size()<<'\n';for(const auto&o:observedPassages){const auto&b=o.box;f<<o.model<<' '<<b.x<<' '<<b.y<<' '<<b.z<<' '<<b.fx<<' '<<b.fy<<' '<<b.halfWidth<<' '<<b.halfLength<<' '<<b.halfHeight<<'\n';}}
void loadObserved(){auto file=routeFile;file+=L".observed";std::ifstream f(file);uint64_t stamp=0;size_t n=0;if(!(f>>stamp>>n)||stamp!=routeStamp()||n>2048)return;std::vector<PassageObservation> loaded;
    for(size_t i=0;i<n;i++){PassageObservation o{};auto&b=o.box;if(!(f>>o.model>>b.x>>b.y>>b.z>>b.fx>>b.fy>>b.halfWidth>>b.halfLength>>b.halfHeight))return;
        if(!o.model||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||std::abs(b.x)>50000||std::abs(b.y)>50000||std::abs(b.z)>50000||!std::isfinite(b.fx)||!std::isfinite(b.fy)||std::hypot(b.fx,b.fy)<0.9f||std::hypot(b.fx,b.fy)>1.1f||!(b.halfWidth>0&&b.halfWidth<=4)||!(b.halfLength>0&&b.halfLength<=4)||!(b.halfHeight>0&&b.halfHeight<100))return;
        b.passability=3;loaded.push_back(o);
    }observedPassages=std::move(loaded);
}
void loadWaitingMarkers(){auto path=routeFile;path+=L".waiting";waitingMarkers.load(path,[](uint32_t model){return passability(model)==WAIT_OBJECT;});}
void saveWaitingMarkers(){if(selectedRoute>=routeCatalog.entries.size())return;auto path=routeFile;path+=L".waiting";waitingMarkers.save(path);}
void applyRecordedPace(){
    const bool enabled=routeInputs.pace==RecordedPace::Enabled;
    for(auto& point:route)point.followRecordedSpeed=enabled;
}
void selectSavedRoute(size_t index){
    if(index>=routeCatalog.entries.size()||selectedRoute==index)return;
    selectedRoute=index;routeInputs=routeCatalog.entries[index].input;route=inputEnterMarkers(routeCatalog.entries[index].points,routeInputs);applyRecordedPace();routeFile=routeCatalog.file(index);loadWaitingMarkers();observedPassages.clear();loadObserved();++routeVisualRevision;
    WritePrivateProfileStringW(L"routes",L"selected",std::wstring(routeCatalog.entries[index].id.begin(),routeCatalog.entries[index].id.end()).c_str(),settingsFile.c_str());
}
uint32_t currentRouteColor(){if(gpsDriving)return GPS_ROUTE_COLOR;return mode==Mode::Record||(mode==Mode::RadarWait&&radarResumeMode==Mode::Record)?recordingColor:selectedRoute<routeCatalog.entries.size()?routeCatalog.entries[selectedRoute].color:0xffffff;}
void selectNearbyStart(){
    if(mode!=Mode::Idle||!haveSample||overlay.selecting()||routeManuallySelected||gpsDriving)return;
    selectSavedRoute(routeCatalog.nearestLaunch(point()));
}
bool persist(){
    RouteInputs settings;
    settings.pace=RecordedPace::Enabled;
    if(!routeCatalog.add(recording,"",recordingColor,&settings))return false;
    auto observations=observedPassages;auto waits=waitingMarkers;selectSavedRoute(routeCatalog.entries.size()-1);waitingMarkers=std::move(waits);saveWaitingMarkers();observedPassages=std::move(observations);saveObserved();return true;
}
void finishRecord(){
    if(mode!=Mode::Record){message=L"Сейчас запись не идёт.";return;}
    if(haveSample){stopRecorder.update(recording,point(),speed,GetTickCount64());if(recording.empty()||distance(point(),recording.back())>0.5f)recording.push_back(point());}
    if(persist()){mode=Mode::Idle;message=L"Новый маршрут сохранён отдельно. Подъедь к его старту и нажми Del.";}
    else message=L"Не удалось сохранить: нужны две точки и доступная папка. Запись остаётся в памяти, Ins повторяет сохранение.";
}
void requestJoin(RoutePick pick,bool reverse,bool nearest,uint64_t now,bool nearbyStart=false);
void rejectDistantStart(bool cancel){
    constexpr auto prompt=L"Подъедь к старту маршрута на 15 м или ближе.";
    if(cancel)stop(prompt);else message=prompt;
    navigatorNotices.push(GetTickCount64(),L"start-range",NoticeIcon::Route,L"Старт маршрута далеко",prompt);
}
void command(int id){
    if(id==108){bool enabled=SendMessageW(sceneBox,BM_GETCHECK,0,0)!=BST_CHECKED;SendMessageW(sceneBox,BM_SETCHECK,enabled?BST_CHECKED:BST_UNCHECKED,0);WritePrivateProfileStringW(L"scene",L"enabled",enabled?L"1":L"0",settingsFile.c_str());return;}
    if(id==104){stop(L"Управление отключено. Машина остаётся под твоим контролем.");return;}
    if(id==102){finishRecord();return;}
    if(mode==Mode::Emergency){message=L"Аварийный тормоз удерживается. Нажми Del и проверь препятствие перед новым запуском.";return;}
    if(mode!=Mode::Idle){message=L"Сначала закончи запись или нажми «Стоп».";return;}
    if(blocked){message=L"Обнаружен BattlEye. Подключение и управление отключены.";return;}
    if(!haveSample){message=L"Нет подтверждённых координат машины. Запусти телеметрию и сядь за руль.";return;}
    if(id==101){
        release();
        do{recordingColor=randomRouteColor();}while(std::any_of(routeCatalog.entries.begin(),routeCatalog.entries.end(),
            [](const SavedRoute& saved){return saved.color==recordingColor;}));
        ++routeVisualRevision;recording.clear();waitingMarkers.clear();recording.push_back(point());stopRecorder={};passageLearner.reset();observedPassages.clear();mode=Mode::Record;lastRecord=GetTickCount64();drivingVehicle=sample.vehicle;
        message=L"Запись идёт. Проедь путь и нажми Ins на финише.";
    }else if(id==103){
        if(maximum()<=0){message=L"Введи положительную скорость в км/ч.";return;}
        if(routeCatalog.entries.empty()){message=L"Сначала запиши маршрут: Ins → проехать → Ins.";return;}
        const auto closest=routeCatalog.nearestLaunch(point());
        if(closest==SIZE_MAX){rejectDistantStart(false);return;}
        if(!keyboardHook||!mouseHook){message=L"Не удалось включить передачу ручного управления. Перезапусти программу.";return;}
        selectSavedRoute(closest);
        routeManuallySelected=false;
        requestJoin(RoutePick{0,0,0,0,0,true},false,false,GetTickCount64(),true);
        if(!foreground())message=L"Вернись в игру в течение 15 секунд. Del — отключить.";
    }
}
void toggleSelection(){
    if(gpsDriving){message=L"Сначала останови GPS-проезд через Del.";return;}
    if(overlay.selecting()||mode==Mode::PickBrake||resumeRequested){stop(L"Выбор продолжения отменён.");return;}
    if(mode==Mode::Record){message=L"Сначала заверши запись через Ins.";return;}
    if(blocked){message=L"Обнаружен BattlEye. Подключение и управление отключены.";return;}
    if(!haveSample||!sample.cameraValid||route.size()<2){message=L"Ошибка: для выбора нити нужны маршрут и координаты камеры.";return;}
    stop(L"Останавливаюсь перед выбором точки на нити.");mode=Mode::PickBrake;drivingVehicle=sample.vehicle;
}
void requestJoin(RoutePick pick,bool reverse,bool nearest,uint64_t now,bool nearbyStart){
    requireNearbyStart=nearbyStart;
        release();finishHold.reset();waitCycle.reset();contactBackoff.reset();resumePick=pick;resumeReverse=reverse;resumeNearest=nearest;resumeRequested=true;resumeAttemptAt=0;joinAttempts=0;resumeUntil=now+15000;joinStaging=false;
    mode=Mode::JoinWait;drivingVehicle=sample.vehicle;
    message=L"Проверяю подъезд к маршруту. Del - стоп.";
}
void startSelectionRoute(uint64_t now){
    resumeAttemptAt=now;
    if(blocked||!keyboardHook||!mouseHook||!sample.vehicle||sample.vehicle!=drivingVehicle){stop(L"Продолжение отменено: автомобиль или состояние управления изменились.");return;}
    if(maximum()<=0){resumeRequested=false;emergency(L"Неправильная скорость. Продолжение отменено.");return;}
    if(requireNearbyStart&&!RouteCatalog::canLaunch(point(),route)){rejectDistantStart(true);return;}
    if(resumeNearest){resumePick=nearestRoutePoint(route,point());resumePick=advanceRoutePick(route,resumePick,float(joinAttempts%4)*4*(resumeReverse?-1:1));}++joinAttempts;
    playbackRoute=resumeReverse?returnRoute(route):route;
    auto tail=continueRoute(route,resumePick,resumeReverse);if(tail.empty()){message=L"Проверяю точки маршрута для подъезда.";return;}
    int gear=tail.size()>1?tail[1].direction:route[std::min(resumePick.segment+1,route.size()-1)].direction*(resumeReverse?-1:1);auto anchor=tail.front();float gx=0,gy=0;
    for(size_t i=1;i<tail.size();i++){gx=tail[i].x-anchor.x;gy=tail[i].y-anchor.y;if(std::hypot(gx,gy)>.1f)break;}
    if(std::hypot(gx,gy)<.1f){auto a=route[resumePick.segment],b=route[resumePick.segment+1];gx=(b.x-a.x)*(resumeReverse?-1:1);gy=(b.y-a.y)*(resumeReverse?-1:1);}
    NavPose car{sample.position.x,sample.position.y,sample.position.z,std::atan2(sample.forward.x*gear,sample.forward.y*gear)};
    NavPose goal{anchor.x,anchor.y,anchor.z,std::atan2(gx,gy)};
    std::vector<RadarBox> boxes;boxes.reserve(sample.obstacleCount);
    for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);i++){const auto&o=sample.obstacles[i];boxes.push_back({o.position.x,o.position.y,o.position.z,o.forward.x,o.forward.y,o.halfWidth,o.halfLength,o.halfHeight,obstacleRule(o)});}
    for(const auto&c:grid.contact){float x=(c.x+.5f)*CELL,y=(c.y+.5f)*CELL;bool ignored=false;for(const auto&b:boxes)if(passableRule(b.passability)&&centerCrosses(b,x,y,x,y)){ignored=true;break;}if(!ignored)boxes.push_back({x,y,sample.position.z+.5f,0,1,.3f,.3f,.7f,0});}
    NavigationScene scene(ignoringRouteObstacles()?nullptr:boxes.data(),ignoringRouteObstacles()?0:boxes.size(),sample.position.z,sample.ownHalfWidth,sample.ownHalfLength);
    float gap=distance(point(),anchor),flat=std::hypot(anchor.x-car.x,anchor.y-car.y);joinStaging=flat>55;
    if(joinStaging){float t=35/flat;goal={car.x+(anchor.x-car.x)*t,car.y+(anchor.y-car.y)*t,car.z,std::atan2(anchor.x-car.x,anchor.y-car.y)};}
    else if(std::abs(anchor.z-car.z)>3){message=L"Проверяю подъезд к маршруту на другом уровне.";return;}
    auto connector=gap<.4f&&tail.size()==1&&scene.clear(car)?std::vector<NavPose>{car,{anchor.x,anchor.y,anchor.z,car.yaw}}:planForward(scene,car,goal,9000,65,100);
    if(connector.empty()){
        message=L"Пробую другой подъезд к маршруту. Ручник включён.";
        if(smartEnabled(improvements.autoRecovery)&&contactBackoff.update(now,!scene.clear(car)||corridorObjectDistance(car.x,car.y,car.z,sample.forward.x*gear,sample.forward.y*gear,boxes.data(),boxes.size(),sample.ownHalfWidth,sample.ownHalfLength,3,false)<1,speed)){
            recoveryGoal={goal.x,goal.y,goal.z};resumeDirection=gear;mode=Mode::RecoveryBrake;recoveryDeadline=now+4000;contactBackoff.reset();
        }return;
    }
    contactBackoff.reset();std::vector<Point> initial;initial.reserve(connector.size()+tail.size());
    const auto joinPolicy=tail.size()>1?tail[1]:tail.front();
    for(const auto&p:connector){Point q{p.x,p.y,p.z,25.0f/3.6f,0,gear};q.facingX=std::sin(p.yaw)*gear;q.facingY=std::cos(p.yaw)*gear;q.startHalfWidth=sample.ownHalfWidth;q.startHalfLength=sample.ownHalfLength;q.speedLimit=joinPolicy.speedLimit;q.ignoreObstacles=joinPolicy.ignoreObstacles;initial.push_back(q);}
    if(!joinStaging){initial.back()=anchor;initial.back().direction=gear;initial.back().speedLimit=joinPolicy.speedLimit;initial.back().ignoreObstacles=joinPolicy.ignoreObstacles;initial.insert(initial.end(),tail.begin()+1,tail.end());}
    resumeRequested=joinStaging;prepareLap(now,&initial);forwardDetour.set(connector,connector.size()-1,connector.back());
    message=joinStaging?L"Поэтапно подъезжаю к маршруту.":resumeReverse?L"Еду к выбранной точке, затем по нити обратно. Del - стоп.":requireNearbyStart?L"Еду от начала маршрута. Del - стоп.":L"Продолжаю маршрут от выбранной точки. Del - стоп.";appendEvent(message.c_str());
}
void toggleRecord(){
    if(overlay.selecting()||resumeRequested){message=L"Сначала отмени выбор продолжения через End.";return;}
    if(mode==Mode::Record)finishRecord();
    else if(mode==Mode::Idle)command(101);
    else message=L"Сначала останови воспроизведение клавишей Del.";
}
void togglePlayback(){
    if(overlay.selecting()||resumeRequested||mode==Mode::PickBrake){stop(L"Продолжение отменено.");return;}
    if(mode==Mode::Record){message=L"Сначала заверши запись клавишей Ins.";return;}
    if(mode==Mode::Idle)command(103);else command(104);
}
std::string utf8(const std::wstring&text){
    if(text.empty())return {};
    int count=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    if(count<=0)return {};
    std::string out(size_t(count),'\0');WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),out.data(),count,nullptr,nullptr);return out;
}
void appendEvent(const wchar_t*reason)noexcept{
    try{
        auto file=dataDir/L"events.log";WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if(GetFileAttributesExW(file.c_str(),GetFileExInfoStandard,&attributes)&&attributes.nFileSizeLow>2*1024*1024){auto old=dataDir/L"events.previous.log";MoveFileExW(file.c_str(),old.c_str(),MOVEFILE_REPLACE_EXISTING);}
        std::ostringstream text;text<<GetTickCount64()<<" mode="<<int(mode)<<" segment="<<segment<<" stop="<<pendingStop<<" speed="<<speed<<" radarValid="<<sample.radarValid<<" radarAge="<<(GetTickCount64()-sample.radarBeat)<<" sampleAge="<<(GetTickCount64()-sample.sampleBeat)<<" keys=";
        for(bool key:keyState)text<<key;
        text<<" reason="<<utf8(reason)<<"\r\n";auto bytes=text.str();
        HANDLE h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(h,bytes.data(),DWORD(bytes.size()),&written,nullptr);CloseHandle(h);}
    }catch(...){}
}
bool writeDiagnostic(const wchar_t*name,bool full=false)noexcept{
    try{
        std::ostringstream f;f.precision(9);
        size_t errorLength=0;while(errorLength<160&&sample.error[errorLength])++errorLength;
        f<<"District 2699 external autopilot 0.2.00\n"<<"Mode "<<int(mode)<<" segment "<<segment<<" pendingStop "<<pendingStop<<" points "<<activeRoute.size()<<"\n"
         <<"Speed "<<speed<<" signed "<<signedSpeed<<" gear "<<driveDirection<<" holdStarted "<<holdStarted<<" throttleSince "<<throttleSince<<"\n"
         <<"sampleAgeMs "<<(GetTickCount64()-sample.sampleBeat)<<" radarAgeMs "<<(GetTickCount64()-sample.radarBeat)<<" radarValid "<<sample.radarValid<<" complete "<<grid.complete<<"\n"
         <<"position "<<sample.position.x<<' '<<sample.position.y<<' '<<sample.position.z<<"\n"
         <<"forward "<<sample.forward.x<<' '<<sample.forward.y<<' '<<sample.forward.z<<"\n"
         <<"vehicle "<<sample.vehicle<<" ped "<<sample.ped<<" occupied "<<grid.occupied.size()<<" contacts "<<grid.contact.size()<<" observed "<<observedPassages.size()<<"\n"
         <<"keys W/S/A/D/Space ";for(bool key:keyState)f<<key<<' ';
        f<<"\nStatus: "<<utf8(message)<<"\nTelemetry: "<<utf8(std::wstring(sample.error,errorLength))<<"\n";
        SYSTEMTIME generated{};GetLocalTime(&generated);
        f<<"GeneratedLocal "<<generated.wYear<<'-'<<generated.wMonth<<'-'<<generated.wDay<<' '<<generated.wHour<<':'<<generated.wMinute<<':'<<generated.wSecond<<"\n";
        f<<"PID "<<pid<<" sampleSerial "<<sample.sampleSerial<<" sceneSerial "<<sample.sceneSerial<<" protocol "<<sample.version<<" sceneScanMs "<<sample.scene.scanMs<<" poolReadMask "<<sample.scene.poolReadMask<<" truncated "<<sample.scene.truncated<<" cameraValid "<<sample.cameraValid<<"\n";
        f<<"Navigator width="<<navigatorSettings.width<<" height="<<navigatorSettings.height<<" opacity="<<navigatorSettings.opacity<<" routeCount="<<routeCatalog.entries.size()<<" vehiclePaint="<<(sample.ownPaint<=0xffffff?routeHex(sample.ownPaint):"unavailable")<<" routeColor="<<routeHex(currentRouteColor())<<"\n";
        f<<"EngineMarkers disabled=1 sceneModelsReady="<<overlay.sceneModelsReady()<<" scenePanel="<<(SendMessageW(sceneBox,BM_GETCHECK,0,0)==BST_CHECKED)<<"\n";
        f<<"ForwardDetour active="<<forwardDetour.active()<<" cursor="<<forwardDetour.cursor<<" goal="<<forwardDetour.routeGoal<<" failures="<<forwardDetour.failures<<" acrylicMode="<<overlay.backdropMode()<<" loop="<<loopEnabled<<" completedLaps="<<completedLaps<<" picking="<<overlay.selecting()<<" resumePending="<<resumeRequested<<" resumeReverse="<<resumeReverse<<" resumeSegment="<<resumePick.segment<<" resumeT="<<resumePick.t<<"\n";
        f<<"RouteActionCursor "<<routeActions.cursor<<" enterQueued "<<enterQueued<<" enterHeld "<<enterHeld<<"\n";
        f<<"GPS enabled="<<gpsEnabled<<" driving="<<gpsDriving<<" bridgeStatus="<<gpsChannel.last.status<<" bridgeBeat="<<gpsChannel.last.beat<<" provider="<<gpsChannel.last.provider<<" reason="<<gpsChannel.last.reason<<" rawCount="<<gpsChannel.last.rawCount<<" rawActive="<<gpsChannel.last.rawActive<<" rawPointer="<<std::hex<<gpsChannel.last.rawPointer<<std::dec<<" count="<<gpsChannel.last.count<<" speedKmh="<<gpsSpeed<<" waypoint="<<gpsChannel.last.waypoint.x<<","<<gpsChannel.last.waypoint.y<<","<<gpsChannel.last.waypoint.z<<" mirror="<<gpsChannel.last.mirror.x<<","<<gpsChannel.last.mirror.y<<","<<gpsChannel.last.mirror.z<<" rawFirst="<<gpsChannel.last.rawFirst.x<<","<<gpsChannel.last.rawFirst.y<<","<<gpsChannel.last.rawFirst.z<<" rawLast="<<gpsChannel.last.rawLast.x<<","<<gpsChannel.last.rawLast.y<<","<<gpsChannel.last.rawLast.z<<"\n";
        f<<"GPSBrake measuredMps2="<<gpsBrakeModel.measured<<" samples="<<gpsBrakeModel.observations<<" turnCapKmh="<<gpsTurnPace.cap*3.6f<<"\n";
        f<<"WaitingEntity "<<waitCycle.id<<" motionStartedMs "<<waitCycle.motionStarted<<" remainingMs "<<waitCycle.remaining(GetTickCount64())<<" passing "<<waitCycle.passing<<"\n";
        f<<"Improvements ";for(const auto&b:improvementBindings)f<<utf8(b.key)<<"="<<(improvements.*b.member)<<" ";f<<"\n";
        f<<"Traffic riskEntity="<<trafficRisk.entity<<" ttc="<<trafficRisk.ttc<<" cap="<<trafficRisk.cap<<" hornHeld="<<hornHeld<<" flightFrames="<<flightRecorder.size()<<"\n";
        f<<"IgnoredTimedStops "<<ignoredTimedStops<<"\nSceneState "<<sample.scene.state<<" lastSuccess "<<sample.scene.lastSuccess<<" retainedAgeMs "<<sample.scene.reusedAgeMs
         <<" failureStreak "<<sample.scene.failureStreak<<" failureReason "<<sample.scene.failureReason<<" membershipMask "<<sample.scene.membershipMask
         <<" poolRetries "<<sample.scene.poolRetries<<" attempts "<<sample.scene.attempts<<" failures "<<sample.scene.failures<<" recoveries "<<sample.scene.recoveries
         <<" criticalRange "<<sample.scene.requiredRange<<" firstDroppedDistance "<<sample.scene.firstDroppedDistance<<"\n";
        const char*categories[]={"vehicles","peds","objects"};
        for(unsigned i=0;i<3;i++){
            f<<"Pool "<<categories[i]<<" capacity="<<sample.scene.capacity[i]<<" nonNull="<<sample.scene.nonNull[i]
             <<" unreadableOrInvalidTransform="<<sample.scene.readFailed[i]<<" wrongType="<<sample.scene.typeRejected[i]
             <<" modelBounds="<<sample.scene.boundsValid[i]<<" fallbackBounds="<<sample.scene.boundsFallback[i]
             <<" outsideRange="<<sample.scene.rangeRejected[i]<<" outsideHeight="<<sample.scene.heightRejected[i]
             <<" accepted="<<sample.scene.accepted[i]<<" retained="<<sample.scene.kept[i]<<"\n";
        }
        f<<"RawEntityTypes";for(unsigned i=0;i<16;i++)if(sample.scene.rawTypes[i])f<<' '<<i<<'='<<sample.scene.rawTypes[i];
        f<<"\nObstacles: kind modelHash boundsKnown x y z fx fy halfWidth halfLength halfHeight\n";
        unsigned limit=std::min(sample.obstacleCount,full?MAX_OBSTACLES:32u);
        for(unsigned i=0;i<limit;i++){
            const auto&o=sample.obstacles[i];f<<o.kind<<' '<<o.modelHash<<' '<<o.boundsKnown<<' '<<o.position.x<<' '<<o.position.y<<' '<<o.position.z<<' '
                <<o.forward.x<<' '<<o.forward.y<<' '<<o.halfWidth<<' '<<o.halfLength<<' '<<o.halfHeight<<"\n";
        }
        if(full){
            std::ifstream events(dataDir/L"events.log",std::ios::binary);
            if(events){events.seekg(0,std::ios::end);auto length=events.tellg();
                if(length>0){auto offset=std::max(std::streamoff(0),std::streamoff(length)-8192);events.seekg(offset);
                    std::string tail((std::istreambuf_iterator<char>(events)),std::istreambuf_iterator<char>());
                    if(offset>0){auto nl=tail.find('\n');if(nl!=std::string::npos)tail.erase(0,nl+1);}
                    f<<"\nRecent events\n"<<tail;
                }
            }
        }
        if(desktopDir.empty())return false;
        auto file=desktopDir/(std::wstring(L"District-")+name);auto temp=file;temp+=L".tmp";std::string bytes=f.str();
        HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
        DWORD written=0;bool ok=WriteFile(h,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();CloseHandle(h);
        if(!ok){DeleteFileW(temp.c_str());return false;}return MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;
    }catch(...){return false;}
}
void diagnostic()noexcept{
    SYSTEMTIME t{};GetLocalTime(&t);wchar_t name[128];
    swprintf(name,128,L"report-%04u%02u%02u-%02u%02u%02u-%03u.txt",unsigned(t.wYear),unsigned(t.wMonth),unsigned(t.wDay),unsigned(t.wHour),unsigned(t.wMinute),unsigned(t.wSecond),unsigned(t.wMilliseconds));
    bool saved=writeDiagnostic(name,true);
    release();mode=Mode::Idle;drivingVehicle=0;stallDetector.reset();holdClock.reset();
    message=saved?L"Отчёт сохранён на рабочий стол.":L"Не удалось сохранить отчёт на рабочий стол. Автопилот отключён.";
}
void autoDiagnostic(uint64_t now)noexcept{
    static uint64_t last=0;static int previousMode=-1;static std::wstring previousMessage;
    try{if(previousMode!=int(mode)||(mode!=Mode::Hold&&previousMessage!=message)){appendEvent(message.c_str());previousMessage=message;previousMode=int(mode);if(mode==Mode::Emergency||mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::BlockedWait||mode==Mode::RadarWait)markFlight(now,utf8(message));}
    if(now-last>=1000){last=now;writeDiagnostic(L"report-auto.txt");}}catch(...){}
}
struct ReaderArgs {DWORD pid;HWND hwnd;std::filesystem::path dll;};
DWORD WINAPI readerWorker(void*arg){
    auto args=static_cast<ReaderArgs*>(arg);auto text=new std::wstring;
    try{*text=attachReader(args->pid,args->dll);}catch(...){*text=L"Ошибка запуска внешней телеметрии.";}
    if(!PostMessageW(args->hwnd,WM_APP,0,reinterpret_cast<LPARAM>(text)))delete text;
    delete args;return 0;
}
void connect(bool automatic=false){
    if(connecting)return;
    if(mode!=Mode::Idle)stop(L"Управление отключено перед подключением.");
    if(shared&&sample.sampleBeat&&GetTickCount64()-sample.sampleBeat<1000){message=L"Телеметрия подключена. Сядь за руль автомобиля.";return;}
    detach();pid=gameProcess();
    if(!pid){message=L"Запусти один клиент District 2699 и войди в игровой мир.";return;}
    autoGpsBaselinePending=automatic;
    connecting=true;EnableWindow(connectButton,FALSE);message=L"Запуск внешней телеметрии District…";
    auto args=new ReaderArgs{pid,window,appDir/L"DistrictTelemetry.exe"};
    HANDLE thread=CreateThread(nullptr,0,readerWorker,args,0,nullptr);
    if(thread)CloseHandle(thread);else{delete args;connecting=false;EnableWindow(connectButton,TRUE);message=L"Не удалось запустить подключение.";}
}
void pauseForData(const wchar_t*why){cancelHorn();
    if(mode!=Mode::RadarWait){radarResumeMode=mode;radarPausedAt=GetTickCount64();sceneGate.reset();objectClearGate.reset();mode=Mode::RadarWait;stallDetector.reset();slowdown.reset();appendEvent(why);}
    holdHandbrake();message=why;
}
void tick(){
    uint64_t now=GetTickCount64();bool fg=foreground();
    if(now-lastSafety>1500){lastSafety=now;blocked=battleye();safetyPid=gameProcess();
        if(blocked&&mode!=Mode::Idle)stop(L"Обнаружен BattlEye. Управление отключено.");
        if(pid&&safetyPid!=pid){stop(L"Процесс District завершён.");detach();pid=0;}
    }
    bool valid=telemetry();now=GetTickCount64();if(!fg||!valid)cancelHorn();
    if(finishHold.active&&finishHold.takeover)stop(L"Ручник снят. Управление передано тебе.");
    if(mode==Mode::Parked&&(!fg||!valid||sample.vehicle!=drivingVehicle))stop(L"Удержание после финиша завершено.");
    if(gpsWheelDelta){gpsSpeed=gpsWheelSpeed(gpsSpeed,gpsWheelDelta,gpsWheelStep);gpsWheelDelta=0;WritePrivateProfileStringW(L"gps",L"speed",std::to_wstring(gpsSpeed).c_str(),settingsFile.c_str());}
    gpsChannel.open(pid);bool gpsSnapshot=gpsChannel.snapshot(),gpsReady=gpsSnapshot&&gpsFresh(gpsChannel.last,pid,now);
    if(autoGpsBaselinePending&&gpsReady){
        gpsMarkerSuppressed=true;gpsSuppressedGoal=gpsChannel.last.waypoint;autoGpsBaselinePending=false;
    }else if(autoGpsBaselinePending&&gpsSnapshot&&gpsChannel.last.status==2)autoGpsBaselinePending=false;
    if(gpsSnapshot&&(gpsChannel.last.status==2||
       (gpsReady&&gpsDistance(gpsSuppressedGoal,gpsChannel.last.waypoint)>1)))gpsMarkerSuppressed=false;
    gpsStatus=!gpsEnabled?L"GPS выключен.":gpsMarkerSuppressed?L"Поставь новую метку для следующей поездки.":!gpsSnapshot?L"Нет внешних данных GPS. Перезапусти приложение и телеметрию из этой сборки.":gpsReady?L"Путь GTA прочитан из памяти, без ASI.":gpsChannel.last.status==2?L"Поставь GPS-метку на карте GTA.":gpsChannel.last.status==10?std::wstring(L"GPS: ")+gpsReadReason(gpsChannel.last.reason):L"Ожидание проверенного пути GTA.";
    tripHistory.observe(valid?sample.sampleBeat:now,valid?sample.vehicle:0,{sample.position.x,sample.position.y,sample.position.z,0},
                        gpsDriving||(mode!=Mode::Idle&&mode!=Mode::Record&&mode!=Mode::Parked));
    tripHistory.flush(now);
    if(gpsEnabled&&!gpsMarkerSuppressed&&mode==Mode::Idle&&!blocked&&valid&&fg&&sample.vehicle&&gpsReady&&keyboardHook&&mouseHook){gpsDriving=true;gpsTurnPace={};gpsBrakeModel={};speedDemand.reset();gpsGoal=gpsChannel.last.waypoint;gpsSerial=gpsChannel.last.serial;++routeVisualRevision;playbackRoute=gpsRoute(gpsChannel.last,gpsSpeed/3.6f);resumeRequested=joinStaging=resumeReverse=false;prepareLap(now);message=L"GPS: следую дороге GTA. Колесо меняет скорость, Del останавливает.";}
    if(gpsDriving&&gpsSnapshot&&gpsChannel.last.status==2){stop(L"GPS-метка снята. Управление отключено.");return;}
    if(gpsDriving&&(!fg||(valid&&sample.vehicle!=drivingVehicle))){stop(L"GPS отключён: изменился автомобиль, координаты или фокус игры.");return;}
    if(gpsDriving&&gpsSnapshot&&valid&&mode==Mode::Drive&&gpsChannel.last.beat&&now-gpsChannel.last.beat<1800&&std::hypot(sample.position.x-gpsChannel.last.waypoint.x,sample.position.y-gpsChannel.last.waypoint.y)<6){mode=Mode::Brake;deadline=now+3500;}
    if(gpsDriving&&!gpsReady&&mode!=Mode::Emergency&&mode!=Mode::Brake){if(gpsChannel.last.status==6)release();else holdHandbrake();message=L"GPS: путь устарел. Ручник до получения актуальной дороги.";gpsPreviewLine.clear();overlay.gpsPrediction({},1);gpsChannel.command(now,true,false,1,{});autoDiagnostic(now);if(now-lastUi>=200){lastUi=now;SetWindowTextW(statusLabel,message.c_str());updateApilUi();}return;}
    if(gpsDriving)gpsBrakeModel.track(now,mode==Mode::Drive&&keyState[1]&&!keyState[0]&&!keyState[4]);
    if(gpsDriving&&gpsReady&&gpsSerial!=gpsChannel.last.serial&&(mode==Mode::Drive||mode==Mode::Armed)&&(!forwardDetour.active()||gpsDistance(gpsGoal,gpsChannel.last.waypoint)>3)){if(gpsDistance(gpsGoal,gpsChannel.last.waypoint)>3)gpsTurnPace={};gpsSerial=gpsChannel.last.serial;gpsGoal=gpsChannel.last.waypoint;++routeVisualRevision;activeRoute=gpsRoute(gpsChannel.last,gpsSpeed/3.6f);playbackRoute=activeRoute;segment=0;pendingStop=SIZE_MAX;routeActions.reset();detour.clear();forwardDetour.reset();}
    if(valid){
        Point p=point();
        if(!previousValid){if(gpsDriving)gpsBrakeModel.sample(0,false);previous=p;previousValid=true;lastSpeed=sample.sampleBeat;speed=0;}
        else if(sample.sampleBeat>lastSpeed&&sample.sampleBeat-lastSpeed>=100){
            float dt=float(sample.sampleBeat-lastSpeed)/1000.0f;float ds=distance(p,previous);float current=ds/dt;
            if((!std::isfinite(current)||ds>std::max(50.0f,speed*dt*4))){emergency(L"Обнаружен скачок координат. Управление отключено.");speed=0;}
            else {
                float along=((p.x-previous.x)*sample.forward.x+(p.y-previous.y)*sample.forward.y)/dt;
                signedSpeed=signedSpeed*0.35f+along*0.65f;
                if(std::abs(signedSpeed)>0.45f)motionDirection=signedSpeed<0?-1:1;
                if(slowdown.update(now,!gpsDriving&&mode==Mode::Drive&&speed>4&&current<0.4f&&keyState[driveDirection>0?0:1])){
                    // Speed alone cannot confirm a collision. Let the sustained stall detector
                    // and the actual route corridor decide whether a recovery is needed.
                    message=L"Замедление: проверяю движение и препятствия.";
                }
                if(gpsDriving)gpsBrakeModel.sample(current,mode==Mode::Drive);
                speed=speed*0.45f+current*0.55f;
            }
            previous=p;lastSpeed=sample.sampleBeat;
        }
    }else{if(gpsDriving)gpsBrakeModel.sample(0,false);previousValid=false;speed=0;signedSpeed=0;}
    if(valid&&mode==Mode::Drive&&activeRoute.size()>1){size_t next=routePolicySegment(activeRoute,segment,point(),std::min(pendingStop,routeActions.next(activeRoute)));bool ignored=activeRoute[std::min(segment+1,activeRoute.size()-1)].ignoreObstacles,willIgnore=activeRoute[next+1].ignoreObstacles;if(!forwardDetour.active()||ignored||willIgnore){segment=next;if(willIgnore){forwardDetour.reset();detour.clear();}}}
    RoutePick selected;bool selectedReverse=false;
    if(overlay.takeSelection(selected,selectedReverse)){requestJoin(selected,selectedReverse,false,now);fg=foreground();}
    const int keys[]={VK_INSERT,VK_DELETE,VK_ESCAPE,'P',VK_RETURN,VK_END};
    for(unsigned i=0;i<6;i++){
        if(physicalPresses.take(unsigned(keys[i]),fg||overlay.selectionFocused())){
            if(i==5){toggleSelection();continue;}
            if(overlay.selecting()){if(i==1||i==2)stop(L"Выбор продолжения отменён.");continue;}
            if(i==0)toggleRecord();
            else if(i==1)togglePlayback();
            else if(i==4){/* Enter markers are captured at the physical key edge in the hook. */}
            else if(mode!=Mode::Idle)stop(L"Управление отключено перед открытием меню.");
        }
    }
    bool driving=mode==Mode::JoinWait||mode==Mode::PickBrake||mode==Mode::Drive||mode==Mode::Hold||mode==Mode::Brake||mode==Mode::Emergency||mode==Mode::RadarWait||mode==Mode::ObjectWait||mode==Mode::ObjectApproach||mode==Mode::EnterWait||mode==Mode::BlockedWait||mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::RecoverySettle;
    if(mode==Mode::Record&&(!valid||sample.vehicle!=drivingVehicle)){
        if((valid&&sample.vehicle!=drivingVehicle)||(sample.ped&&sample.health>0&&sample.vehicle==0))stop(L"Запись остановлена: автомобиль изменился.");
        else pauseForData(L"Проверка записи. Машина удерживается ручником.");
    }
    else if((mode==Mode::Armed||driving)&&mode!=Mode::Emergency){
        if(valid&&sample.vehicle!=drivingVehicle)stop(L"Автомобиль изменился. Управление отключено.");
        else if(!valid){
            if(sample.ped&&sample.health>0&&sample.vehicle==0)stop(L"Ты вышел из машины. Управление отключено.");
            else pauseForData(L"Проверка координат. Машина удерживается ручником.");
        }
    }
    if(mode==Mode::JoinWait&&fg)resumeUntil=0;
    if(driving&&!fg&&!(mode==Mode::JoinWait&&resumeUntil&&now<resumeUntil)){writeDiagnostic(L"diagnostic-before-focus-loss.txt");stop(L"Игра потеряла фокус. Автопилот отключён.");}
    if(mode==Mode::PickBrake){
        holdHandbrake();
        if(valid&&fg&&speed<.45f){
            mode=Mode::Idle;release();
            if(!overlay.beginSelection(route))message=L"Ошибка: нить пока недоступна. Включи оверлей, вернись в игру и нажми End ещё раз.";
            else message=L"Клик рядом с нитью - точка. Протяни по нити - направление. End/ПКМ - отмена.";
        }
    }
    if(mode==Mode::Record&&now-lastRecord>=100){
        lastRecord=now;auto p=point();
        stopRecorder.update(recording,p,speed,now);
        if(recording.size()==1&&distance(p,recording.front())>0.3f)recording.front().direction=p.direction;
        const float gap=distance(p,recording.back());
        const bool speedChanged=gap>=.35f&&std::abs(p.speed-recording.back().speed)>=.65f;
        if(gap>=1.8f||speedChanged||((speed>0.6f)&&p.direction!=recording.back().direction)){
            if(p.direction!=recording.back().direction)recording.back().waitMs=std::max(600u,recording.back().waitMs);
            recording.push_back(p);if(recording.size()>100000)stop(L"Слишком длинный маршрут. Новая запись отменена.");
        }
    }
    if(mode==Mode::Armed){
        if(valid&&requireNearbyStart&&!RouteCatalog::canLaunch(point(),route))rejectDistantStart(true);
        else if(!fg&&now>deadline)stop(L"Запуск отменён: окно игры не выбрано.");
        else if(fg){
            if(gpsDriving&&distance(point(),activeRoute.front())>12){holdHandbrake();message=L"GPS: начало дороги далеко. Подъедь к дороге и включи GPS снова.";}
            else if(distance(point(),activeRoute.front())>12){
                if(requireNearbyStart)requestJoin(RoutePick{0,0,0,0,0,true},false,false,now,true);
                else requestJoin(nearestRoutePoint(route,point()),resumeReverse,true,now);
            }
            else if(!sceneGate.update(now,radarEnabled()?sample.radarBeat:sample.sampleBeat,valid&&sample.vehicle==drivingVehicle&&(!radarEnabled()||processingReady(valid,true,sample.radarValid,sample.scene.state,sample.scene.failureReason,grid.complete)))){
                deadline=now+15000;message=L"Подготовка окружения. Проезд начнётся автоматически.";
            }else{requireNearbyStart=false;mode=Mode::Drive;lastMotion=now;motionOrigin=point();message=L"Автопилот включён. Del — отключить управление.";}
        }
    }
    auto waitPose=[](const Telemetry::Obstacle&o){
        auto v=[](Vec a){return ScenePoint{a.x,a.y,a.z};};
        return WaitPose{o.entity,v(o.origin),v(o.basisRight),v(o.basisForward),v(o.basisUp),{v(o.position),v(o.forward),o.halfWidth,o.halfLength,o.halfHeight}};
    };
    auto rawBox=[&](const Telemetry::Obstacle&o){return RadarBox{o.position.x,o.position.y,o.position.z,o.forward.x,o.forward.y,o.halfWidth,o.halfLength,o.halfHeight,obstacleRule(o)};};
    auto beginWaiting=[&](const Telemetry::Obstacle& object){
        const auto* marker=waitingMarkers.find(object.entity,object.modelHash,
            {object.position.x,object.position.y,object.position.z,0});
        waitCycle.begin(waitPose(object),sample.forward.x*driveDirection,sample.forward.y*driveDirection,
                        marker?marker->delayMs:5000,marker?marker->approach:WaitApproach{});
    };
    if(ignoringRouteObstacles()){waitCycle.reset();if(mode==Mode::ObjectWait||mode==Mode::ObjectApproach){mode=Mode::Drive;release();}trafficRisk={};}
    const bool freshTrafficScene=valid&&sample.radarValid&&sample.scene.state==1&&!sample.scene.failureReason&&now-sample.radarBeat<400;
    const bool freshWaitScene=!ignoringRouteObstacles()&&freshTrafficScene;
    if(waitCycle.id&&freshWaitScene&&sample.sceneSerial!=waitObservedSerial){
        waitObservedSerial=sample.sceneSerial;
        const Telemetry::Obstacle*target=nullptr;
        for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);i++)if(sample.obstacles[i].entity==waitCycle.id){target=&sample.obstacles[i];break;}
        if(waitCycle.passing&&waitCycle.past({sample.position.x,sample.position.y,sample.position.z},sample.ownHalfLength)){waitCycle.reset();lastGrid=0;}
        else if(waitCycle.passing&&target){
            auto b=rawBox(*target);b.passability=WAIT_OBJECT;
            float range=std::max(8.0f,speed*1.4f+speed*speed/8+2);
            if(waitingObjectDistance(sample.position.x,sample.position.y,sample.position.z,sample.forward.x*driveDirection,sample.forward.y*driveDirection,&b,1,sample.ownHalfWidth,sample.ownHalfLength,range)<=range){
                beginWaiting(*target);lastGrid=0;objectClearGate.reset();
                if(mode==Mode::Drive||mode==Mode::ObjectApproach){mode=Mode::ObjectWait;objectWaitRange=range;holdHandbrake();}
            }
        }else if(!waitCycle.passing){auto pose=target?waitPose(*target):WaitPose{};waitCycle.observe(now,target?&pose:nullptr);}
    }
    if(sample.sceneSerial!=lastSceneSerial||(sample.radarValid!=0)!=lastSceneValid||!lastGrid){
    std::vector<PassageObject> props;
    for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);i++){auto&o=sample.obstacles[i];if(o.kind==5&&!passableRule(obstacleRule(o)))props.push_back({{o.position.x,o.position.y,o.position.z,o.forward.x,o.forward.y,o.halfWidth,o.halfLength,o.halfHeight,0},o.modelHash});}
    if(mode==Mode::Record&&sample.radarValid&&now-sample.radarBeat<400&&sample.sampleBeat>lastLearning+80){
        lastLearning=sample.sampleBeat;
        auto learned=passageLearner.update({sample.position.x,sample.position.y,sample.position.z,sample.forward.x,sample.forward.y,speed,sample.ownHalfLength>0?sample.ownHalfLength:2.5f,sample.sampleBeat,motionDirection},props);
        for(const auto&o:learned){bool exists=false;for(const auto&v:observedPassages)if(observedMatches(v,{o.box,o.model})){exists=true;break;}
            if(!exists&&observedPassages.size()<2048){observedPassages.push_back(o);lastGrid=0;}}
    }else if(mode!=Mode::Record||!sample.radarValid||now-sample.radarBeat>=400)passageLearner.reset();
    std::vector<RadarBox> boxes(std::min(MAX_OBSTACLES,sample.obstacleCount));size_t count=boxes.size();
    for(size_t i=0;i<count;i++){auto&o=sample.obstacles[i];boxes[i]={o.position.x,o.position.y,o.position.z,o.forward.x,o.forward.y,o.halfWidth,o.halfLength,o.halfHeight,obstacleRule(o)};}
    for(size_t i=0;i<count;i++)if(sample.obstacles[i].kind==5&&boxes[i].passability==0){
        PassageObject p{boxes[i],sample.obstacles[i].modelHash};for(const auto&o:observedPassages)if(observedMatches(o,p)){boxes[i].passability=3;break;}}
    for(const auto&o:observedPassages)boxes.push_back(o.box);
    count=boxes.size();
    grid.halfWidth=sample.ownHalfWidth>0?sample.ownHalfWidth+0.15f:1.05f;
    lastGrid=now;grid.rebuild(boxes.data(),count,sample.position.x,sample.position.y,sample.position.z,now);
    sceneBoxes=std::move(boxes);lastSceneSerial=sample.sceneSerial;lastSceneValid=sample.radarValid!=0;
    }
    if(!gpsDriving&&valid&&sample.radarValid&&sample.sceneSerial!=waitingMarkers.serial&&(mode==Mode::Idle||mode==Mode::Record||mode==Mode::Drive||mode==Mode::ObjectWait||mode==Mode::ObjectApproach)){
        waitingMarkers.serial=sample.sceneSerial;bool changed=false;const auto&path=mode==Mode::Record?recording:route;
        for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);i++){const auto&o=sample.obstacles[i];if(passability(o.modelHash)==WAIT_OBJECT)changed=waitingMarkers.observe(o.entity,o.modelHash,{o.position.x,o.position.y,o.position.z,0},path)||changed;}
        if(changed&&mode!=Mode::Record)saveWaitingMarkers();
    }
    auto&boxes=sceneBoxes;size_t count=boxes.size();
    if(smartEnabled(improvements.trafficPrediction)&&freshTrafficScene&&sample.sceneSerial!=trafficSerial){trafficSerial=sample.sceneSerial;std::vector<TrafficObservation> observations;
        for(size_t i=0;i<std::min(size_t(std::min(MAX_OBSTACLES,sample.obstacleCount)),boxes.size());i++){const auto&o=sample.obstacles[i];if(o.kind==3&&o.entity!=sample.vehicle&&!passableRule(boxes[i].passability)&&std::hypot(o.position.x-sample.position.x,o.position.y-sample.position.y)<90)observations.push_back({o.entity,o.modelHash,boxes[i]});}
        trafficPredictor.observe(sample.radarBeat,observations);
    }
    if(!ignoringRouteObstacles()&&!activeRoute.empty()&&mode!=Mode::Idle&&mode!=Mode::Record&&sample.sceneSerial!=stopRuleScene&&processingReady(valid,sample.vehicle==drivingVehicle,sample.radarValid,sample.scene.state,sample.scene.failureReason,grid.complete)){
        stopRuleScene=sample.sceneSerial;
        size_t changed=suppressWaitingStops(activeRoute,segment,boxes.data(),count);ignoredTimedStops+=changed;
        if(changed){
            size_t before=pendingStop;pendingStop=nextStop(activeRoute,segment+1);
            if(pendingStop!=before){
                holdClock.reset();holdStarted=0;
                if(mode==Mode::Hold){mode=Mode::Drive;lastMotion=now;motionOrigin=point();}
                if(mode==Mode::RadarWait&&radarResumeMode==Mode::Hold)radarResumeMode=Mode::Drive;
            }
        }
    }
    if(resumeRequested&&requireNearbyStart&&mode==Mode::JoinWait&&valid&&!RouteCatalog::canLaunch(point(),route))
        rejectDistantStart(true);
    if(resumeRequested&&mode==Mode::JoinWait&&fg){
        holdHandbrake();
        if(valid&&speed<.45f&&(!radarEnabled()||(processingReady(valid,true,sample.radarValid,sample.scene.state,sample.scene.failureReason,grid.complete)&&now-sample.radarBeat<400))&&(!resumeAttemptAt||now-resumeAttemptAt>=900))startSelectionRoute(now);
    }
    radar=radarScan(sample.position.x,sample.position.y,sample.position.z,sample.forward.x,sample.forward.y,boxes.data(),count,std::clamp(25+speed*1.8f+speed*speed/8,40.0f,160.0f));
    if((driving||mode==Mode::Armed||mode==Mode::Record)&&mode!=Mode::Idle&&mode!=Mode::Emergency&&mode!=Mode::RadarWait&&(radarEnabled()&&(sample.scene.failureReason!=0||!sample.radarValid||now-sample.radarBeat>600||!grid.complete))){
        pauseForData(L"Проверка окружения. Машина удерживается ручником.");
        writeDiagnostic(L"diagnostic-radar-loss.txt");
    }
    if(mode==Mode::RadarWait){
        holdHandbrake();
        if(sceneGate.update(now,radarEnabled()?sample.radarBeat:sample.sampleBeat,valid&&sample.vehicle==drivingVehicle&&(!radarEnabled()||processingReady(valid,true,sample.radarValid,sample.scene.state,sample.scene.failureReason,grid.complete)))&&speed<0.45f){
            uint64_t paused=now-radarPausedAt;
            if(deadline)deadline+=paused;
            if(recoveryDeadline)recoveryDeadline+=paused;
            if(holdClock.started)holdClock.started+=paused;
            if(holdClock.movingSince)holdClock.movingSince+=paused;
            holdStarted=holdClock.started;lastMotion=now;motionOrigin=point();throttleSince=0;stallDetector.reset();release();
            mode=radarResumeMode;
            if(mode==Mode::RecoveryMove){mode=Mode::RecoveryBrake;driveDirection=resumeDirection;recoveryDeadline=now+4000;}
            if(mode==Mode::RecoverySettle)recoveryLastMotion=now;
            lastGrid=0;plannedAt=0;detour.clear();forwardDetour.reset();message=L"Автопилот включён. Del — отключить управление.";appendEvent(message.c_str());
        }
    }
    if((mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::RecoverySettle)&&!radarEnabled())stop(L"Манёвр отменён: проверка препятствий выключена.");
    if(mode==Mode::Drive&&activeRoute.size()>1)driveDirection=activeRoute[std::min(segment+1,activeRoute.size()-1)].direction;

    size_t enterIndex=routeActions.next(activeRoute);bool enterApproachActive=false;
    if(enterIndex<activeRoute.size()&&enterIndex<=pendingStop&&(mode==Mode::Drive||mode==Mode::ObjectApproach||mode==Mode::ObjectWait)){
        float ahead=0;Point from=point();for(size_t i=std::min(segment+1,enterIndex);i<=enterIndex;i++){ahead+=distance(from,activeRoute[i]);from=activeRoute[i];if(ahead>80)break;}
        enterApproachActive=ahead<=std::max(12.0f,speed*1.4f+speed*speed/8+2);
        if(enterApproachActive&&(mode==Mode::ObjectApproach||mode==Mode::ObjectWait))mode=Mode::Drive;
    }
    if(mode==Mode::EnterWait){holdHandbrake();if(!enterQueued&&!enterHeld&&now>=enterDeadline){release();mode=enterResumeMode;lastMotion=now;motionOrigin=point();}}
    if(!ignoringRouteObstacles()&&(mode==Mode::Drive||mode==Mode::ObjectWait||mode==Mode::ObjectApproach)){
        const float brakingRange=speed*1.4f+speed*speed/8.0f+2;
        float range=mode==Mode::Drive?std::max(14.0f,brakingRange):objectWaitRange;
        const Telemetry::Obstacle* nextWait=nullptr;
        float distanceToWait=std::numeric_limits<float>::infinity();
        for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);++i){
            const auto& object=sample.obstacles[i];
            if(mode!=Mode::Drive&&object.entity!=waitCycle.id)continue;
            if(waitCycle.passing&&object.entity==waitCycle.id)continue;
            const auto box=rawBox(object);
            if(box.passability!=WAIT_OBJECT)continue;
            float detectionRange=range;
            if(mode==Mode::Drive){
                const auto* marker=waitingMarkers.find(object.entity,object.modelHash,
                    {object.position.x,object.position.y,object.position.z,0});
                if(marker)detectionRange=std::max(detectionRange,marker->approach.outer+2);
            }
            const float gap=waitingObjectDistance(sample.position.x,sample.position.y,sample.position.z,
                sample.forward.x*driveDirection,sample.forward.y*driveDirection,&box,1,
                sample.ownHalfWidth,sample.ownHalfLength,detectionRange);
            if(gap<=detectionRange&&gap<distanceToWait){
                nextWait=&object;distanceToWait=gap;
                if(mode==Mode::Drive)objectWaitRange=detectionRange;
            }
        }
        const bool waitingObject=nextWait!=nullptr;
        if(mode==Mode::Drive&&waitingObject){
            if(waitCycle.id!=nextWait->entity)beginWaiting(*nextWait);
            range=objectWaitRange;mode=Mode::ObjectApproach;objectClearGate.reset();detour.clear();forwardDetour.reset();message=L"Подъезжаю к объекту ожидания.";
        }
        if(!enterApproachActive&&mode==Mode::ObjectApproach&&(waitCycle.motionCandidate||waitCycle.motionStarted)){mode=Mode::ObjectWait;holdHandbrake();}

        if(enterApproachActive&&mode==Mode::ObjectApproach)mode=Mode::Drive;
        if(mode==Mode::ObjectApproach){
            if(!waitingObject){
                holdHandbrake();
                mode=Mode::ObjectWait;
            }else{
                objectClearGate.reset();
                float cap=waitCycle.approach.speed(distanceToWait);
                float other=corridorObjectDistance(sample.position.x,sample.position.y,sample.position.z,sample.forward.x*driveDirection,sample.forward.y*driveDirection,boxes.data(),count,sample.ownHalfWidth,sample.ownHalfLength,range,false);
                if(cap<=0){mode=Mode::ObjectWait;holdHandbrake();message=L"Достигнута дистанция остановки. Ручник включён.";}
                else if(other<std::max(0.8f,speed*0.25f+speed*speed/8+0.25f)){holdHandbrake();message=L"Подъезд остановлен: перед машиной другое препятствие.";}
                else{
                    auto output=gearDriveOutput(activeRoute,segment,point(),sample.forward.x,sample.forward.y,speed,std::min(maximum(),cap),driveDirection,pendingStop,smartEnabled(improvements.anticipatorySpeed));segment=output.segment;
                    if(output.error>12||maximum()<=0)emergency(L"Подъезд остановлен: проверка маршрута. Ручник удерживается.");
                    else if(output.arrived){mode=pendingStop<activeRoute.size()-1?Mode::Hold:Mode::Brake;holdStarted=0;holdClock.reset();deadline=now+3500;holdHandbrake();}
                    else{
                        float target=std::min(cap,output.targetSpeed),phase=float(now%160)/160;
                        bool brake=speed>target+0.12f;
                        bool ok=outputKey(4,false);
                        ok=outputKey(driveDirection>0?0:1,!brake&&phase<std::clamp((target-speed)*0.9f,0.0f,1.0f))&&ok;
                        ok=outputKey(driveDirection>0?1:0,brake&&speed>0.2f)&&ok;
                        ok=outputKey(2,output.steering< -0.035f&&phase<std::abs(output.steering))&&ok;
                        ok=outputKey(3,output.steering>0.035f&&phase<std::abs(output.steering))&&ok;
                        if(!ok)emergency(L"Команды подъезда не приняты. Ручник удерживается.");
                        else message=L"Подъезд к объекту по настройкам секторов.";
                    }
                }
            }
        }
        if(mode==Mode::ObjectWait){
            holdHandbrake();
            bool clear=objectClearGate.update(now,sample.radarBeat,processingReady(valid,sample.vehicle==drivingVehicle,sample.radarValid,sample.scene.state,sample.scene.failureReason,grid.complete)&&!waitingObject);
            if(waitCycle.releaseReady(now,clear,!waitingObject)&&speed<0.45f){
                waitCycle.startPass();
                for(size_t i=0;i<std::min(size_t(sample.obstacleCount),boxes.size());i++)if(sample.obstacles[i].entity==waitCycle.id)boxes[i].passability=WAIT_PASS;
                grid.rebuild(boxes.data(),boxes.size(),sample.position.x,sample.position.y,sample.position.z,now);lastGrid=now;detour.clear();forwardDetour.reset();plannedAt=0;
                release();mode=Mode::Drive;lastMotion=now;motionOrigin=point();throttleSince=0;stallDetector.reset();message=L"Задержка ожидания прошла. Проезд открыт.";
            }else if(waitCycle.motionStarted)message=L"Ожидание после движения объекта: "+std::to_wstring((waitCycle.remaining(now)+999)/1000)+L" сек. Проверяю проезд.";
            else message=L"Ожидание начала движения объекта. Ручник включён.";
        }
    }
    bool yieldingTraffic=false;
    if(mode==Mode::Drive&&fg&&valid&&sample.vehicle==drivingVehicle&&improvements.trafficPrediction&&!activeRoute.empty()){
        const int direction=activeRoute[std::min(segment+1,activeRoute.size()-1)].direction;
        const NavPose pose{sample.position.x,sample.position.y,sample.position.z,
                           std::atan2(sample.forward.x*direction,sample.forward.y*direction)};
        const auto approaching=freshTrafficScene?trafficPredictor.approaching(now,pose,speed,sample.ownHalfWidth,
                                                                                sample.ownHalfLength,trafficYield.entity):0;
        const bool wasYielding=trafficYield.entity!=0;
        yieldingTraffic=trafficYield.update(now,approaching);
        if(yieldingTraffic){
            bool ok=true;
            if(speed<=.45f)holdHandbrake();
            else{
                ok=outputKey(direction>0?0:1,false);
                ok=outputKey(direction>0?1:0,true)&&ok;
                ok=outputKey(2,false)&&ok;
                ok=outputKey(3,false)&&ok;
                ok=outputKey(4,false)&&ok;
            }
            lastMotion=now;motionOrigin=point();stallDetector.reset();contactBackoff.reset();speedDemand.reset();
            traceTarget=traceSteer=0;trafficRisk={};
            if(!ok)emergency(L"Не удалось притормозить для пропуска автомобиля.");
            else message=L"Пропускаю приближающийся автомобиль.";
        }else if(wasYielding){
            release();lastMotion=now;motionOrigin=point();
            message=L"Продолжаю маршрут после пропуска автомобиля.";
        }
    }else trafficYield.reset();
    if(mode==Mode::Drive&&!yieldingTraffic){
        driveDirection=activeRoute[std::min(segment+1,activeRoute.size()-1)].direction;
        float fx=sample.forward.x*driveDirection,fy=sample.forward.y*driveDirection;
        size_t navigationStop=enterApproachActive?std::min(pendingStop,enterIndex):pendingStop;
        const float lookLimit=gpsDriving?11.5f:65.0f;
        const float braking=gpsDriving?gpsBrakeModel.planningDeceleration():4.5f;
        auto output=gearDriveOutput(activeRoute,segment,point(),sample.forward.x,sample.forward.y,speed,maximum(),driveDirection,navigationStop,false,lookLimit,braking);
        if(!forwardDetour.active())segment=output.segment;
        Avoidance avoidance{0,1e9f,2,false};bool obstacleNearby=false;
        std::vector<RadarBox> navigationBoxes=boxes;
        for(const auto&c:grid.contact){float x=(c.x+.5f)*CELL,y=(c.y+.5f)*CELL;bool ignored=false;for(const auto&b:boxes)if(passableRule(b.passability)&&centerCrosses(b,x,y,x,y)){ignored=true;break;}if(!ignored)navigationBoxes.push_back({x,y,sample.position.z+.5f,0,1,.3f,.3f,.7f,0});}
        NavigationScene scene(ignoringRouteObstacles()?nullptr:navigationBoxes.data(),ignoringRouteObstacles()?0:navigationBoxes.size(),sample.position.z,sample.ownHalfWidth,sample.ownHalfLength,sample.position.x,sample.position.y,true);
        NavPose car{sample.position.x,sample.position.y,sample.position.z,std::atan2(fx,fy)};
        if(enterApproachActive){
            detour.clear();forwardDetour.reset();auto target=activeRoute[enterIndex];auto approach=approachEnter(scene,car,target,speed);
            float dx=target.x-car.x,dy=target.y-car.y,angle=navAngle(std::atan2(dx,dy)-car.yaw);
            if(enterIndex<=segment+1||approach.distance<3)output.steering=std::clamp(angle*1.45f,-1.0f,1.0f)*driveDirection;
            else{auto corridor=scene.routeDistance(car,activeRoute,segment,enterIndex);approach.blocked=corridor.second;approach.remaining=std::max(0.0f,corridor.first-(corridor.second?.3f:.25f));approach.speedCap=std::min(4.0f,std::sqrt(4*approach.remaining));approach.ready=corridor.second&&approach.remaining<=.3f&&speed<=.35f;}
            output.arrived=false;
            output.targetSpeed=std::min(routeSpeedCap(activeRoute,segment,maximum()),approach.speedCap);if(std::abs(angle)>.6f)output.targetSpeed=std::min(output.targetSpeed,2.0f);
            bool stuck=enterStall.update(now,car,speed,approach.remaining>.3f&&output.targetSpeed>.5f);
            bool canPress=approach.ready||(stuck&&speed<=.35f);
            if((approach.blocked&&approach.remaining<=.3f)||approach.distance<=.4f||canPress){holdHandbrake();avoidance.blocked=true;}
            if(canPress){
                unsigned count=routeActions.consume(activeRoute,enterIndex);enterQueued+=count;enterDeadline=now;
                segment=std::min(enterIndex,activeRoute.size()-2);enterResumeMode=enterIndex+1==activeRoute.size()?Mode::Brake:(waitCycle.id&&!waitCycle.passing?Mode::ObjectWait:Mode::Drive);
                if(pendingStop==enterIndex&&enterIndex+1<activeRoute.size()){if(activeRoute[enterIndex+1].direction!=activeRoute[enterIndex].direction){enterResumeMode=Mode::Hold;holdClock.reset();}else pendingStop=nextStop(activeRoute,enterIndex+1);}
                mode=Mode::EnterWait;holdHandbrake();enterStall.reset();lastMotion=now;motionOrigin=point();message=approach.blocked||stuck?L"Перед препятствием: нажимаю Enter и продолжаю после проверки проезда.":L"Центр машины у точки: нажимаю Enter.";
            }
        }else if(radarEnabled()){
            enterStall.reset();size_t limit=std::min(activeRoute.size()-1,pendingStop);
            auto follow=forwardDetour.follow(car,speed,scene,smartEnabled(improvements.anticipatorySpeed));
            if(forwardDetour.active()&&follow.valid&&follow.arrived){segment=std::min(forwardDetour.routeGoal?forwardDetour.routeGoal-1:0,activeRoute.size()-2);detour.clear();forwardDetour.reset();output=gearDriveOutput(activeRoute,segment,point(),sample.forward.x,sample.forward.y,speed,maximum(),driveDirection,pendingStop,false,lookLimit,braking);segment=output.segment;}
            else if(forwardDetour.active()&&!follow.valid&&forwardDetour.replaceInvalid(now,false,follow.error,smartEnabled(improvements.stableDetour))){forwardDetour.path.clear();detour.clear();}
            else if(forwardDetour.active()&&follow.valid)forwardDetour.replaceInvalid(now,true,follow.error);
            // Try a clear, shallow connector before stopping for a new global join.
            if(!forwardDetour.active()&&output.error>2.5f&&driveDirection>0&&scene.clear(car)&&!stallDetector.since){
                auto direct=directRejoin(scene,car,activeRoute,segment,limit,speed);
                if(!direct.path.empty()){auto goal=direct.path.back();forwardDetour.set(std::move(direct.path),direct.routeGoal,goal);message=L"Плавно возвращаюсь на маршрут по свободному прямому пути.";}
            }
            const float obstacleLook=std::clamp(8.0f+speed,8.0f,28.0f);
            bool obstruction=forwardDetour.active()||!scene.routeClear(car,activeRoute,segment,limit,obstacleLook);obstacleNearby=obstruction;
            if(obstruction&&!forwardDetour.active()&&(!forwardDetour.lastTry||now-forwardDetour.lastTry>=700)){
                forwardDetour.lastTry=now;float arc=0;Point previousPoint=point();std::vector<size_t> goals;
                for(size_t i=segment+1;i<=limit&&i<=segment+100;i++){arc+=distance(previousPoint,activeRoute[i]);previousPoint=activeRoute[i];if(arc>30)break;if(arc>=10||i==limit)goals.push_back(i);}
                unsigned tried=0;for(auto it=goals.rbegin();it!=goals.rend()&&tried<3;){size_t index=*it;++tried;auto g=activeRoute[index];size_t before=index?index-1:0,after=std::min(index+1,activeRoute.size()-1);float gx=(activeRoute[after].x-activeRoute[before].x),gy=(activeRoute[after].y-activeRoute[before].y);
                    NavPose goal{g.x,g.y,car.z,std::atan2(gx,gy)};auto path=planForward(scene,car,goal,6000,34,70);if(!path.empty()){forwardDetour.set(std::move(path),index,goal);break;}
                    float oldArc=0;size_t old=index;while(it!=goals.rend()&&oldArc<5){++it;if(it!=goals.rend()){oldArc+=distance(activeRoute[old],activeRoute[*it]);old=*it;}}
                }
                if(!forwardDetour.active())++forwardDetour.failures;
            }
            if(forwardDetour.active()){
                follow=forwardDetour.follow(car,speed,scene,smartEnabled(improvements.anticipatorySpeed));detour.clear();for(const auto&p:forwardDetour.path)detour.push_back({p.x,p.y,p.z});
                if(follow.valid){output.steering=follow.steering*driveDirection;output.targetSpeed=routeSpeedAheadCap(activeRoute,segment,point(),maximum(),navigationStop);output.error=follow.error;output.arrived=false;message=L"Следую найденной траектории объезда.";}
                else{avoidance.blocked=true;output.targetSpeed=0;}
            }else if(obstruction){avoidance.blocked=true;output.targetSpeed=0;}
            else{forwardDetour.reset();detour.clear();}
        }else{detour.clear();forwardDetour.reset();enterStall.reset();}
        if(now-lastTraffic>=100){lastTraffic=now;std::vector<Point> predictionPath;const std::vector<Point>*path=&activeRoute;size_t start=segment;
            if(forwardDetour.active()){predictionPath.push_back(point());for(size_t i=forwardDetour.cursor+1;i<forwardDetour.path.size();i++){const auto&p=forwardDetour.path[i];predictionPath.push_back({p.x,p.y,p.z,0});}path=&predictionPath;start=0;}
            trafficRisk=smartEnabled(improvements.trafficPrediction)&&freshWaitScene&&radarEnabled()?trafficPredictor.risk(now,car,speed,sample.ownHalfWidth,sample.ownHalfLength,*path,start):TrafficRisk{};
        }
        if(radarEnabled()&&smartEnabled(improvements.trafficPrediction)&&trafficRisk.entity){output.targetSpeed=std::min(output.targetSpeed,trafficRisk.cap);message=L"Пропускаю автомобиль: прогноз пересечения пути.";if(trafficRisk.ttc<.8f)markFlight(now,"predicted traffic conflict");}
        if(gpsDriving){
            const float turnCap=gpsTurnPace.update(now,activeRoute,segment,point(),speed,maximum(),braking);
            output.targetSpeed=std::min(output.targetSpeed,turnCap);
            output.steering=gpsTurnPace.smoothSteering(now,output.steering);
        }
        output.targetSpeed=speedDemand.update(now,output.targetSpeed,speed,gpsDriving||smartEnabled(improvements.anticipatorySpeed));traceTarget=output.targetSpeed;traceSteer=output.steering;traceError=output.error;
        if(routeSpeedCap(activeRoute,segment,maximum())<=0)emergency(L"Неправильная скорость. Управление отключено.");
        else if(output.error>((forwardDetour.active()||forwardDetour.routeGoal)?35.0f:12.0f)){markFlight(now,"route tracking lost");if(gpsDriving){holdHandbrake();detour.clear();forwardDetour.reset();message=L"GPS: жду пересчёт дороги GTA.";}else requestJoin(nearestRoutePoint(route,point()),resumeReverse,true,now);}
        else if(output.arrived){mode=pendingStop<activeRoute.size()-1?Mode::Hold:Mode::Brake;holdStarted=0;holdClock.reset();stallDetector.reset();deadline=now+3500;release();}
        else {
            if(distance(point(),motionOrigin)>1){lastMotion=now;motionOrigin=point();if(!obstacleNearby)recoveryAttempts=0;}
            if(avoidance.blocked){
                markFlight(now,"corridor blocked");lastMotion=now;
                holdHandbrake();
                stallDetector.reset();if(!enterApproachActive){message=L"Проезд перекрыт: проверяю безопасную траекторию. Ручник включён.";
                bool closeToObstacle=!scene.clear(car)||corridorObjectDistance(car.x,car.y,car.z,fx,fy,boxes.data(),count,sample.ownHalfWidth,sample.ownHalfLength,3,false)<1;
                if(smartEnabled(improvements.autoRecovery)&&contactBackoff.update(now,closeToObstacle,speed)){size_t goalIndex=std::min(segment+4,activeRoute.size()-1);auto goal=activeRoute[goalIndex];recoveryGoal={goal.x,goal.y,goal.z};resumeDirection=driveDirection;mode=Mode::RecoveryBrake;recoveryDeadline=now+4000;contactBackoff.reset();}
            }}else contactBackoff.reset();
            if(!avoidance.blocked&&now-lastMotion>12000){
                if(gpsDriving)stop(L"GPS отключён: автомобиль не движется.");
                else blockedWait(L"Нет движения: проверяю путь, маршрут сохранён для продолжения.");
            }
            else if(mode==Mode::Drive&&!avoidance.blocked){
                float phase=float(now%160)/160.0f;
                const float brakeDuty=gpsDriving?gpsBrakeModel.brakeDuty(speed,output.targetSpeed):(speed>output.targetSpeed+.6f?1.0f:0.0f);
                bool brake=brakeDuty>0||(gpsDriving&&output.targetSpeed<=.15f);
                float throttle=std::clamp((output.targetSpeed-speed-(gpsDriving?.18f:0))*0.6f,0.0f,1.0f);
                float steer=std::abs(output.steering);
                bool gas=!brake&&phase<throttle;
                bool ok=outputKey(driveDirection>0?0:1,gas);
                ok=outputKey(driveDirection>0?1:0,phase<brakeDuty&&speed>0.4f)&&ok;
                ok=outputKey(4,brake&&speed<=0.4f)&&ok;
                ok=outputKey(2,output.steering< -0.035f&&phase<steer)&&ok;
                ok=outputKey(3,output.steering>0.035f&&phase<steer)&&ok;
                if(!ok)emergency(L"Windows не приняла команды клавиш. Проверь одинаковые права запуска.");
                if(mode==Mode::Drive&&stallDetector.update(now,sample.position.x,sample.position.y,speed,throttle,true)){
                    stallDetector.reset();throttleSince=0;release();
                    if(smartEnabled(improvements.autoRecovery)&&radarEnabled()&&sample.radarValid&&grid.complete){
                        size_t goalIndex=std::min(std::min(segment+4,activeRoute.size()-1),pendingStop);auto goal=activeRoute[goalIndex];
                        recoveryGoal={goal.x,goal.y,goal.z};resumeDirection=driveDirection;mode=Mode::RecoveryBrake;recoveryDeadline=now+4000;
                        bool ignoredContact=false;for(const auto&b:boxes)if(passableRule(b.passability)&&centerCrosses(b,sample.position.x,sample.position.y,sample.position.x+fx*3,sample.position.y+fy*3))ignoredContact=true;
                        if(!ignoredContact)grid.markContact(sample.position.x+fx*3,sample.position.y+fy*3,sample.position.z+0.7f,now);
                        message=L"Нет движения при уверенном газе: проверяю безопасный манёвр.";
                    }else if(gpsDriving)stop(L"GPS отключён: автомобиль не движется.");
                    else blockedWait(L"Нет движения: остановлено до подтверждения свободного пути.");
                }
            }
        }
    }
    if(mode!=Mode::Drive)stallDetector.reset();
    if(!smartEnabled(improvements.autoRecovery)&&(mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::RecoverySettle))blockedWait(L"Автовосстановление выключено. Машина удерживается ручником.",true);
    if(mode==Mode::RecoveryBrake){
        holdHandbrake();
        if(speed<0.4f){
            NavigationScene scene(sceneBoxes.data(),sceneBoxes.size(),sample.position.z,sample.ownHalfWidth,sample.ownHalfLength);recoveryStart={sample.position.x,sample.position.y,sample.position.z,std::atan2(sample.forward.x,sample.forward.y)};
            recovery=physicalEscape(scene,recoveryStart,resumeDirection,recoveryGoal,recoveryAttempts);
            if(!recovery.valid)blockedWait(L"Нет безопасного манёвра: жду освобождения пути и повторяю проверку.");
            else {++recoveryAttempts;release();driveDirection=recovery.gear;recoveryProgress=0;recoveryOrigin=recoveryPrevious={sample.position.x,sample.position.y,sample.position.z};recoveryDeadline=now+6500;recoveryLastMotion=now;mode=Mode::RecoveryMove;message=L"Манёвр: освобождаю место для бокового объезда.";}
        }else if(now>recoveryDeadline)emergency(L"Не удалось остановиться перед манёвром. Аварийный тормоз.");
    }
    if(mode==Mode::RecoveryMove){
        GridPoint p{sample.position.x,sample.position.y,sample.position.z};float moved=std::hypot(p.x-recoveryPrevious.x,p.y-recoveryPrevious.y);recoveryProgress=std::hypot(p.x-recoveryOrigin.x,p.y-recoveryOrigin.y);recoveryPrevious=p;if(moved>0.01f)recoveryLastMotion=now;
        NavigationScene scene(sceneBoxes.data(),sceneBoxes.size(),sample.position.z,sample.ownHalfWidth,sample.ownHalfLength);NavPose current{p.x,p.y,p.z,std::atan2(sample.forward.x,sample.forward.y)},ahead{p.x+sample.forward.x*recovery.gear*.35f,p.y+sample.forward.y*recovery.gear*.35f,p.z,current.yaw};
        if(!scene.escapeStep(recoveryStart,current,ahead)){release();mode=Mode::RecoverySettle;recoveryDeadline=now+3000;message=L"Сцена изменилась: завершаю манёвр и проверяю объезд.";}
        else if(recoveryProgress>=recovery.distance||now>recoveryDeadline){release();mode=Mode::RecoverySettle;recoveryDeadline=now+3000;}
        else if(now-recoveryLastMotion>1500)blockedWait(L"Манёвр остановился: жду свободного пути.");
        else {
            float phase=float(now%160)/160.0f;bool brake=speed>1.8f;
            bool ok=outputKey(recovery.gear>0?0:1,!brake&&phase<std::clamp((1.6f-speed)*1.2f,0.0f,1.0f));
            ok=outputKey(recovery.gear>0?1:0,brake)&&ok;ok=outputKey(4,false)&&ok;
            ok=outputKey(2,recovery.steering<0&&phase<std::abs(recovery.steering))&&ok;ok=outputKey(3,recovery.steering>0&&phase<std::abs(recovery.steering))&&ok;
            if(!ok)emergency(L"Windows не приняла команды манёвра. Управление остановлено.");
        }
    }
    if(mode==Mode::RecoverySettle){
        holdHandbrake();
        if(speed<0.4f){release();driveDirection=resumeDirection;mode=resumeRequested?Mode::JoinWait:Mode::Drive;resumeAttemptAt=0;lastMotion=now;motionOrigin=point();throttleSince=0;slowdown.reset();forwardDetour.reset();detour.clear();lastGrid=0;message=L"Место освобождено: повторная попытка локального объезда.";}
        else if(now>recoveryDeadline)emergency(L"Не удалось остановиться после манёвра. Аварийный тормоз.");
    }
    if(mode==Mode::Hold){
        holdHandbrake();
        if(pendingStop>=activeRoute.size()){release();mode=Mode::Drive;holdClock.reset();message=L"Некорректная остановка пропущена: продолжаю маршрут.";}
        else {
            bool complete=holdClock.update(now,speed,std::max(600u,activeRoute[pendingStop].waitMs));holdStarted=holdClock.started;
            wchar_t info[160];swprintf(info,160,L"Записанная остановка: осталось %.1f с",holdStarted?std::max(0.0f,(float(std::max(600u,activeRoute[pendingStop].waitMs))-float(now-holdStarted))/1000):float(std::max(600u,activeRoute[pendingStop].waitMs))/1000);message=info;
            if(complete){
                throttleSince=0;stallDetector.reset();slowdown.reset();holdClock.reset();
                size_t completed=pendingStop;pendingStop=nextStop(activeRoute,completed+1);segment=std::min(completed,activeRoute.size()-2);release();lastMotion=now;motionOrigin=point();
                if(completed==activeRoute.size()-1){mode=Mode::Brake;deadline=now+3500;}
                else {mode=Mode::Drive;message=L"Остановка повторена. Продолжаю маршрут.";}
            }
        }
    }
    if(mode==Mode::Brake&&joinStaging){holdHandbrake();if(speed<.45f){joinStaging=false;mode=Mode::JoinWait;resumeRequested=true;resumeAttemptAt=0;finishHold.reset();}}
    if(mode==Mode::Brake){
        if(!finishHold.active)finishHold.begin();
        holdHandbrake();
        message=L"Финиш. Ручник включён; нажми любую клавишу или кнопку мыши для передачи управления.";
        if(speed<0.45f&&!enterQueued&&!enterHeld){
            if(completedLaps<UINT64_MAX)++completedLaps;
            WritePrivateProfileStringW(L"statistics",L"laps",std::to_wstring(completedLaps).c_str(),settingsFile.c_str());
            if(!gpsDriving&&loopRestartReady(loopEnabled,playbackRoute,point(),speed,false)){
                prepareLap(now);message=L"Круг завершён. Проверяю окружение перед повтором.";appendEvent(message.c_str());
            }else if(loopEnabled&&!gpsDriving){
                RoutePick restart=resumeReverse?RoutePick{route.size()-2,1,0,0,0,true}:RoutePick{0,0,0,0,0,true};requestJoin(restart,resumeReverse,false,now);
            }else{
                if(gpsDriving){gpsMarkerSuppressed=true;gpsSuppressedGoal=gpsGoal;gpsDriving=false;gpsPreviewLine.clear();overlay.gpsPrediction({},1);++routeVisualRevision;}mode=Mode::Parked;waitCycle.reset();detour.clear();forwardDetour.reset();stallDetector.reset();throttleSince=0;
                message=L"Маршрут завершён. Ручник до ручного ввода.";appendEvent(message.c_str());
            }
        }
    }
    if(mode==Mode::Parked)holdHandbrake();
    if(mode==Mode::BlockedWait){
        holdHandbrake();
        if(now-lastBlockedCheck>=1000){lastBlockedCheck=now;
            GridPoint from{sample.position.x,sample.position.y,sample.position.z};
            // Never release a contact stop without fresh, complete scene data and a clear corridor.
            if((!blockedNeedsReposition||distance(point(),blockedOrigin)>1.5f)&&canResumeBlocked(grid.complete,sample.radarValid&&now-sample.radarBeat<400,routeCorridorClear(grid,from,activeRoute,segment,pendingStop,12.0f),speed)){
                release();mode=resumeRequested?Mode::JoinWait:Mode::Drive;resumeAttemptAt=0;throttleSince=0;slowdown.reset();lastMotion=now;motionOrigin=point();recoveryAttempts=0;detour.clear();forwardDetour.reset();message=L"Путь свободен: продолжаю маршрут.";
            }else if(smartEnabled(improvements.autoRecovery)&&valid&&sample.radarValid&&now-sample.radarBeat<400&&grid.complete&&speed<.4f){
                if(!resumeRequested&&activeRoute.size()>1){auto g=activeRoute[std::min(segment+4,activeRoute.size()-1)];recoveryGoal={g.x,g.y,g.z};}mode=Mode::RecoveryBrake;recoveryDeadline=now+4000;
            }
        }
    }
    bool actionMode=mode==Mode::Drive||mode==Mode::ObjectApproach||mode==Mode::ObjectWait||mode==Mode::Hold||mode==Mode::Brake||mode==Mode::Parked||mode==Mode::EnterWait;
    {bool allowed=actionMode&&valid&&fg&&sample.vehicle==drivingVehicle;int event=enterPulse(now,allowed,enterQueued,enterHeld,enterDeadline);if(event&&!enterKey(event>0))emergency(L"Не удалось выполнить записанный Enter. Ручник включён.");}
    if(mode==Mode::Emergency)holdHandbrake();
    if(mode==Mode::Idle&&!resumeRequested)release();
    // Quarantine channels from a previous ASI installation: never submit enabled frames.
    if(pid){legacyStartMarkers.publish(pid,now,false,0,0,0,0,0);legacyFinishMarkers.publish(pid,now,false,0,0,0,0,0);}
    bool nearbyVehicle=false;
    if(valid&&freshWaitScene){for(unsigned i=0;i<std::min(MAX_OBSTACLES,sample.obstacleCount);i++){const auto&o=sample.obstacles[i];if(o.kind==3&&o.entity&&o.entity!=sample.vehicle&&!passableRule(obstacleRule(o))&&std::abs(o.position.z-sample.position.z)<3&&std::hypot(o.position.x-sample.position.x,o.position.y-sample.position.y)<=10){nearbyVehicle=true;break;}}}
    bool hornActive=hornEnabled&&!yieldingTraffic&&fg&&valid&&freshWaitScene&&sample.vehicle==drivingVehicle&&(mode==Mode::Drive||mode==Mode::ObjectApproach||mode==Mode::ObjectWait||mode==Mode::Hold);
    int hornEvent=hornPulse.update(now,hornActive,nearbyVehicle);if(hornEvent&&!hornKey(hornEvent>0))emergency(L"Ошибка команды E. Управление остановлено.");
    static uint64_t lastRibbon=0;
    const bool showRibbon=gpsDriving&&gpsPreview&&valid&&fg&&mode==Mode::Drive&&traceTarget>.15f&&traceError<12;
    if(showRibbon&&now-lastRibbon>=25){
        lastRibbon=now;
        gpsPreviewLine=gpsRibbon(activeRoute,segment,point(),sample.forward.x,sample.forward.y,sample.ownHalfLength);
        overlay.gpsPrediction(gpsPreviewLine,sample.ownHalfWidth>0?sample.ownHalfWidth:1.05f);
    }else if(!showRibbon&&!gpsPreviewLine.empty()){
        gpsPreviewLine.clear();
        overlay.gpsPrediction({},1);
    }
    gpsChannel.command(now,gpsEnabled,false,sample.ownHalfWidth>0?sample.ownHalfWidth:1.05f,{});
    autoDiagnostic(now);
    if(improvements.flightRecording&&now-lastFlight>=200){lastFlight=now;FlightFrame frame;frame.now=now;frame.car={sample.position.x,sample.position.y,sample.position.z,std::atan2(sample.forward.x,sample.forward.y)};frame.speed=speed;frame.target=traceTarget;frame.steer=traceSteer;frame.error=traceError;frame.ttc=trafficRisk.ttc;frame.riskEntity=trafficRisk.entity;frame.mode=int(mode);frame.segment=segment;frame.reason=utf8(message);if(gpsDriving)frame.route="GPS";else if(selectedRoute<routeCatalog.entries.size())frame.route=routeCatalog.entries[selectedRoute].id;
        if(valid){std::vector<RadarBox> nearby=boxes;std::sort(nearby.begin(),nearby.end(),[&](const auto&a,const auto&b){return std::hypot(a.x-sample.position.x,a.y-sample.position.y)<std::hypot(b.x-sample.position.x,b.y-sample.position.y);});for(const auto&b:nearby)if(frame.objects.size()<48&&std::hypot(b.x-sample.position.x,b.y-sample.position.y)<70)frame.objects.push_back(b);}
        if(forwardDetour.active())for(size_t i=forwardDetour.cursor;i<forwardDetour.path.size()&&frame.planned.size()<48;i++)frame.planned.push_back(forwardDetour.path[i]);
        else for(size_t i=std::min(segment,activeRoute.size());i<activeRoute.size()&&frame.planned.size()<48;i++){const auto&p=activeRoute[i];frame.planned.push_back({p.x,p.y,p.z,std::atan2(p.facingX,p.facingY)});}
        flightRecorder.push(std::move(frame));
    }
    if(improvements.flightRecording&&!desktopDir.empty()&&flightRecorder.due(now))flightRecorder.save(desktopDir/L"District-flights",now);
    if(now-lastUi>=200){
        lastUi=now;SetWindowTextW(statusLabel,message.c_str());
        SetWindowTextW(sceneLabel,overlay.sceneModelsReady()?L"3D-обзор: модели загружены.":L"3D-обзор: не найдены файлы моделей.");
        SetWindowTextW(GetDlgItem(window,101),mode==Mode::Record?L"Стоп / сохранить · Ins":L"Запись / стоп · Ins");
        SetWindowTextW(GetDlgItem(window,103),mode!=Mode::Idle&&mode!=Mode::Record?L"Остановить · Del":L"Пуск / стоп · Del");
        std::wstring connection=haveSample?L"Телеметрия работает · координаты автомобиля доступны":(shared&&sample.error[0]?std::wstring(sample.error):L"Нет телеметрии · войди в District и нажми «Подключить»");
        if(haveSample&&sample.error[0])connection=sample.error;
        SetWindowTextW(connectionLabel,connection.c_str());
        wchar_t stats[260];swprintf(stats,260,L"Точек: %u  ·  Участок: %u  ·  %.0f км/ч  ·  Ход: %ls\nЗанятых ячеек: %u  ·  Упоров: %u",unsigned(mode==Mode::Record?recording.size():route.size()),unsigned(segment+1),speed*3.6f,(mode==Mode::Record?motionDirection:driveDirection)<0?L"R":L"D",unsigned(grid.occupied.size()),unsigned(grid.contact.size()));std::wstring statsText=stats;if(loopEnabled)statsText+=L"  ·  Кругов: "+std::to_wstring(completedLaps);SetWindowTextW(statsLabel,statsText.c_str());updateApilUi();
    }
}
std::wstring controlText(HWND control){wchar_t text[2048]{};GetWindowTextW(control,text,2048);return text;}
void uiCommand(const std::wstring&raw){
    try{
        std::vector<std::wstring> parts;size_t from=0;for(;;){size_t at=raw.find(L'|',from);parts.push_back(raw.substr(from,at==std::wstring::npos?at:at-from));if(at==std::wstring::npos)break;from=at+1;}
        if(parts.size()==2&&parts[0]==L"action"){
            const auto& a=parts[1];
            if(a==L"folder")ShellExecuteW(window,L"open",dataDir.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            else if(a==L"report")diagnostic();
            else if(a==L"flights"){
                auto folder=desktopDir/L"District-flights";std::error_code ec;std::filesystem::create_directories(folder,ec);
                ShellExecuteW(window,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            }
        }else if(parts.size()==3&&parts[0]==L"gpsvalue"){
            size_t used=0;float value=std::stof(parts[2],&used);if(used==parts[2].size()&&std::isfinite(value)){if(parts[1]==L"gpsSpeed")gpsSpeed=std::clamp(value,5.0f,300.0f);else if(parts[1]==L"gpsWheelStep")gpsWheelStep=std::clamp(value,1.0f,30.0f);WritePrivateProfileStringW(L"gps",parts[1]==L"gpsSpeed"?L"speed":L"step",std::to_wstring(parts[1]==L"gpsSpeed"?gpsSpeed:gpsWheelStep).c_str(),settingsFile.c_str());}
        }else if(parts.size()==7&&parts[0]==L"section"){
            if(mode!=Mode::Idle||overlay.selecting()||selectedRoute>=routeCatalog.entries.size()||parts[1]!=std::wstring(routeCatalog.entries[selectedRoute].id.begin(),routeCatalog.entries[selectedRoute].id.end())){message=L"Сначала останови проезд и выбери маршрут.";updateApilUi();return;}
            size_t a=0,b=0;float kmh=0;bool ignore=false;
            if(!parseSectionEdit(parts[2],parts[3],parts[4],parts[5],parts[6],route.size(),a,b,kmh,ignore)){message=L"Недопустимые параметры участка.";updateApilUi();return;}
            auto edited=route;if(!editRouteSection(edited,a,b,kmh,ignore)||!routeCatalog.update(selectedRoute,edited)){message=L"Не удалось сохранить участок.";updateApilUi();return;}
            route=std::move(edited);routeInputs.pace=routeCatalog.entries[selectedRoute].input.pace;applyRecordedPace();++routeVisualRevision;message=L"Параметры участка сохранены.";
        }else if((parts.size()==3||parts.size()==4)&&parts[0]==L"route"){
            if(mode!=Mode::Idle){message=L"Сначала останови запись или воспроизведение.";updateApilUi();return;}
            size_t index=SIZE_MAX;for(size_t i=0;i<routeCatalog.entries.size();i++)if(std::wstring(routeCatalog.entries[i].id.begin(),routeCatalog.entries[i].id.end())==parts[2])index=i;
            if(parts[1]==L"select"&&index!=SIZE_MAX){routeManuallySelected=true;selectSavedRoute(index);message=L"Маршрут выбран вручную.";}
            else if(parts[1]==L"delete"&&parts.size()==3&&index!=SIZE_MAX){
                if(overlay.selecting()){message=L"Сначала заверши выбор точки через End.";updateApilUi();return;}
                if(routeCatalog.erase(index)){
                    if(selectedRoute==index){selectedRoute=SIZE_MAX;route.clear();waitingMarkers.clear();routeInputs={};recording.clear();activeRoute.clear();playbackRoute.clear();observedPassages.clear();routeFile=dataDir/L"routes"/L"none";
                        if(!routeCatalog.entries.empty())selectSavedRoute(std::min(index,routeCatalog.entries.size()-1));else WritePrivateProfileStringW(L"routes",L"selected",L"",settingsFile.c_str());
                    }else if(selectedRoute!=SIZE_MAX&&index<selectedRoute)--selectedRoute;
                    ++routeVisualRevision;message=L"Маршрут удалён.";
                }else message=L"Не удалось удалить маршрут. Проверь доступ к папке данных.";
            }
            else if(parts[1]==L"rename"&&parts.size()==4&&index!=SIZE_MAX)routeCatalog.rename(index,utf8(parts[3]));
            else if(parts[1]==L"pace"&&parts.size()==4&&index!=SIZE_MAX&&(parts[3]==L"0"||parts[3]==L"1")){
                if(routeCatalog.setRecordedPace(index,parts[3]==L"1")){
                    if(index==selectedRoute){routeInputs.pace=routeCatalog.entries[index].input.pace;applyRecordedPace();}
                    message=parts[3]==L"1"?L"Скорость записи включена.":L"Скорость записи выключена.";
                }else message=L"Профиль скорости недоступен или не сохранился.";
            }
            else if(parts[1]==L"color"&&parts.size()==4&&index!=SIZE_MAX){
                uint32_t color=0;
                if(parseRouteColor(parts[3],color)&&routeCatalog.setColor(index,color)){
                    ++routeVisualRevision;
                    message=L"Цвет маршрута сохранён.";
                }else message=L"Не удалось сохранить цвет маршрута.";
            }
        }else if(parts.size()==3&&parts[0]==L"toggle"&&(parts[2]==L"0"||parts[2]==L"1")){
            bool value=parts[2]==L"1";
            if(parts[1]==L"gpsEnabled"){
                gpsEnabled=value;
                if(value)gpsMarkerSuppressed=false;
                else if(gpsDriving)stop(L"GPS-режим выключен.");
                WritePrivateProfileStringW(L"gps",L"enabled",value?L"1":L"0",settingsFile.c_str());
            }
            if(parts[1]==L"panelMap"){
                navigatorSettings.mapEnabled=value;
                WritePrivateProfileStringW(L"navigator",L"panelMap",parts[2].c_str(),settingsFile.c_str());
                overlay.configureScene(navigatorSettings);
            }
            if(parts[1]==L"gpsPreview"){gpsPreview=value;WritePrivateProfileStringW(L"gps",parts[1].c_str(),value?L"1":L"0",settingsFile.c_str());}
            if(improvements.set(parts[1],value)){
                WritePrivateProfileStringW(L"improvements",parts[1].c_str(),value?L"1":L"0",settingsFile.c_str());
                if(parts[1]==L"anticipatorySpeed")speedDemand.reset();
                if(parts[1]==L"stableDetour")forwardDetour.invalidSince=0;
                if(parts[1]==L"trafficPrediction"){trafficPredictor.reset();trafficYield.reset();trafficRisk={};trafficSerial=lastTraffic=0;}
                if(parts[1]==L"flightRecording"){flightRecorder.reset();lastFlight=0;}
                if(parts[1]==L"autoRecovery"){contactBackoff.reset();stallDetector.reset();if(!value&&(mode==Mode::RecoveryBrake||mode==Mode::RecoveryMove||mode==Mode::RecoverySettle))blockedWait(L"Автовосстановление выключено. Машина удерживается ручником.",true);}
            }
            if(parts[1]==L"horn"){hornEnabled=value;WritePrivateProfileStringW(L"playback",L"horn",value?L"1":L"0",settingsFile.c_str());if(!value)cancelHorn();}
            HWND control=parts[1]==L"radar"?radarBox:parts[1]==L"overlay"?overlayBox:parts[1]==L"scene"?sceneBox:parts[1]==L"loop"?loopBox:nullptr;
            if(control){SendMessageW(control,BM_SETCHECK,value?BST_CHECKED:BST_UNCHECKED,0);if(control==loopBox)loopEnabled=value;WritePrivateProfileStringW(control==sceneBox?L"scene":L"playback",control==sceneBox?L"enabled":parts[1].c_str(),parts[2].c_str(),settingsFile.c_str());}
        }else if(parts.size()==3&&parts[0]==L"value"){
            wchar_t*end=nullptr;double number=wcstod(parts[2].c_str(),&end);if(!end||end==parts[2].c_str()||*end||!std::isfinite(number))return;
            if(parts[1]==L"panelWidth"||parts[1]==L"panelHeight"||parts[1]==L"panelOpacity"||parts[1]==L"panelMapOpacity"){
                if(std::floor(number)!=number)return;
                if(parts[1]==L"panelWidth"&&number>=240&&number<=900)navigatorSettings.width=int(number);
                else if(parts[1]==L"panelHeight"&&number>=160&&number<=800)navigatorSettings.height=int(number);
                else if(parts[1]==L"panelOpacity"&&number>=0&&number<=100)navigatorSettings.opacity=int(number);
                else if(parts[1]==L"panelMapOpacity"&&number>=0&&number<=100)navigatorSettings.mapOpacity=int(number);else return;
                WritePrivateProfileStringW(L"navigator",parts[1].c_str(),parts[2].c_str(),settingsFile.c_str());overlay.configureScene(navigatorSettings);
            }else if(parts[1]==L"target"&&number>0&&parts[2].size()<=12){SetWindowTextW(speedBox,parts[2].c_str());WritePrivateProfileStringW(L"playback",L"speed",parts[2].c_str(),settingsFile.c_str());}
            else if((parts[1]==L"cell"&&setCellSize(float(number)))||(parts[1]==L"radius"&&setDrawRadius(float(number)))){WritePrivateProfileStringW(L"grid",parts[1].c_str(),parts[2].c_str(),settingsFile.c_str());grid.clear();detour.clear();forwardDetour.reset();lastGrid=0;}
        }else if(parts.size()==3&&parts[0]==L"rule"){
            wchar_t*end=nullptr;unsigned long model=wcstoul(parts[1].c_str(),&end,10);if(!model||!end||*end)return;unsigned rule=parts[2]==L"0"?0:parts[2]==L"1"?1:parts[2]==L"2"?2:parts[2]==L"4"?4:99;
            if(rule<=4){objectRules[uint32_t(model)]=rule;observedPassages.erase(std::remove_if(observedPassages.begin(),observedPassages.end(),[&](const auto&o){return o.model==uint32_t(model);}),observedPassages.end());saveObserved();WritePrivateProfileStringW(L"objects",std::to_wstring(model).c_str(),std::to_wstring(rule).c_str(),settingsFile.c_str());grid.clear();detour.clear();forwardDetour.reset();lastGrid=0;}
        }updateApilUi();
    }catch(...){message=L"Не удалось применить настройку. Попробуй ещё раз.";}
}
#include "apil_ui.inc"
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    if(apil::applyUpdateArgument())return 0;
    WinHandle single(CreateMutexW(nullptr,TRUE,L"Local\\Lisurgut.District2699.Controller.v1"));
    if(!single.h||GetLastError()==ERROR_ALREADY_EXISTS)return 0;
    wchar_t local[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,local)))return 1;
    dataDir=std::filesystem::path(local)/L"Lisurgut"/L"DistrictAutopilot2699";
    std::error_code ec;std::filesystem::create_directories(dataDir,ec);
    const auto runtime=dataDir/L"runtime"/apilVersionWide;
    if(ec||!unpackApilRuntime(runtime)){
        MessageBoxW(nullptr,L"Не удалось подготовить файлы APIL в LocalAppData.",L"APIL",MB_OK|MB_ICONERROR);return 1;
    }
    appDir=runtime/L"bin";
    overlay.loadSceneModels(runtime/L"assets"/L"vehicles");
    overlay.loadNavigatorMap(runtime/L"assets"/L"map"/L"navigator-atlas.bmp",appDir/L"DistrictWebP.dll");
    overlay.startMapCache(dataDir/archiveCacheFolder);
    wchar_t desktop[MAX_PATH]{};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_DESKTOPDIRECTORY|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,desktop)))desktopDir=desktop;
    settingsFile=dataDir/L"settings.ini";wchar_t cellText[64];GetPrivateProfileStringW(L"grid",L"cell",L"1.25",cellText,64,settingsFile.c_str());setCellSize(float(wcstod(cellText,nullptr)));GetPrivateProfileStringW(L"grid",L"radius",L"20",cellText,64,settingsFile.c_str());setDrawRadius(float(wcstod(cellText,nullptr)));
    {wchar_t rules[32768]{};GetPrivateProfileSectionW(L"objects",rules,32768,settingsFile.c_str());for(const wchar_t*entry=rules;*entry;entry+=wcslen(entry)+1){wchar_t*end=nullptr;unsigned long hash=wcstoul(entry,&end,10);if(hash&&end&&*end==L'='){unsigned rule=unsigned(wcstoul(end+1,nullptr,10));if(rule==1||rule==2||rule==4)objectRules[uint32_t(hash)]=rule;}}}
    tripHistory.open(dataDir/L"trips-v1.bin");
    wchar_t lapsText[32]{};
    GetPrivateProfileStringW(L"statistics",L"laps",L"0",lapsText,32,settingsFile.c_str());
    completedLaps=wcstoull(lapsText,nullptr,10);
    gpsEnabled=GetPrivateProfileIntW(L"gps",L"enabled",1,settingsFile.c_str())!=0;
    improvements.load([&](const wchar_t*key,bool fallback){return GetPrivateProfileIntW(L"improvements",key,fallback?1:0,settingsFile.c_str())!=0;});
    hornEnabled=GetPrivateProfileIntW(L"playback",L"horn",1,settingsFile.c_str())!=0;
    navigatorSettings.width=std::clamp(int(GetPrivateProfileIntW(L"navigator",L"panelWidth",360,settingsFile.c_str())),240,900);
    navigatorSettings.height=std::clamp(int(GetPrivateProfileIntW(L"navigator",L"panelHeight",480,settingsFile.c_str())),160,800);
    navigatorSettings.opacity=std::clamp(int(GetPrivateProfileIntW(L"navigator",L"panelOpacity",85,settingsFile.c_str())),0,100);
    navigatorSettings.mapEnabled=GetPrivateProfileIntW(L"navigator",L"panelMap",0,settingsFile.c_str())!=0;
    navigatorSettings.mapOpacity=std::clamp(int(GetPrivateProfileIntW(L"navigator",L"panelMapOpacity",75,settingsFile.c_str())),0,100);
    overlay.configureScene(navigatorSettings);
    gpsSpeed=std::clamp(float(GetPrivateProfileIntW(L"gps",L"speed",30,settingsFile.c_str())),5.0f,300.0f);gpsWheelStep=std::clamp(float(GetPrivateProfileIntW(L"gps",L"step",5,settingsFile.c_str())),1.0f,30.0f);gpsPreview=GetPrivateProfileIntW(L"gps",L"gpsPreview",1,settingsFile.c_str())!=0;
    routeFile=dataDir/L"route.txt";loadRoute(routeFile,route);routeCatalog.open(dataDir/L"routes");
    if(!routeCatalog.hasHistory()&&route.size()>=2){if(routeCatalog.add(route,u8"Маршрут 1")){auto old=routeFile;old+=L".observed";auto migrated=routeCatalog.file(0);migrated+=L".observed";std::filesystem::copy_file(old,migrated,std::filesystem::copy_options::skip_existing,ec);}}
    wchar_t selected[128]{};GetPrivateProfileStringW(L"routes",L"selected",L"",selected,128,settingsFile.c_str());size_t initial=0;for(size_t i=0;i<routeCatalog.entries.size();i++)if(std::wstring(routeCatalog.entries[i].id.begin(),routeCatalog.entries[i].id.end())==selected)initial=i;
    route.clear();if(routeCatalog.entries.empty())routeFile=dataDir/L"routes"/L"none";
    selectSavedRoute(initial);
    SetProcessDPIAware();WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"GTA5StandaloneAutopilot2";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=ui::background();if(!RegisterClassW(&wc))return 1;
    HWND w=CreateWindowW(wc.lpszClassName,L"APIL",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1040,790,nullptr,nullptr,instance,nullptr);
    if(!w)return 1;
    HMODULE dwm=LoadLibraryW(L"dwmapi.dll");if(dwm){using SetAttribute=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);auto set=reinterpret_cast<SetAttribute>(GetProcAddress(dwm,"DwmSetWindowAttribute"));if(set){BOOL dark=TRUE;set(w,20,&dark,sizeof(dark));}FreeLibrary(dwm);}
    ShowWindow(w,show);UpdateWindow(w);
    keyboardHook=SetWindowsHookExW(WH_KEYBOARD_LL,manualKeyboard,instance,0);
    mouseHook=SetWindowsHookExW(WH_MOUSE_LL,manualMouse,instance,0);
    MSG msg{};bool quit=false;
    while(!quit){
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT){quit=true;break;}
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(quit)break;
        const auto now=GetTickCount64();
        updater.tick(now,dataDir/L"updates");
        static uint64_t nextConnect=0;
        if(now>=nextConnect){
            nextConnect=now+4000;
            if(autoConnectEnabled&&!connecting&&mode==Mode::Idle&&!apilConnected()&&gameProcess()&&!battleye())connect(true);
        }
        if(mode==Mode::Idle&&!connecting&&!foreground()&&!activeForm&&updater.apply()){
            SendMessageW(w,WM_CLOSE,0,0);continue;
        }
        try{
            tick();selectNearbyStart();updateNavigatorNotices();overlay.draw(pid,sample,grid,mode==Mode::Record?recording:gpsDriving?activeRoute:route,detour,mode==Mode::PickBrake||overlay.selecting()||SendMessageW(overlayBox,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(sceneBox,BM_GETCHECK,0,0)==BST_CHECKED,speed,gpsDriving&&mode==Mode::Drive?traceTarget:mode==Mode::Drive?routeSpeedCap(activeRoute,segment,maximum()):maximum(),foreground()||overlay.selecting(),radarEnabled(),hudStatus(mode==Mode::Record||(mode==Mode::RadarWait&&radarResumeMode==Mode::Record),mode!=Mode::Idle&&mode!=Mode::Record,mode==Mode::Emergency||mode==Mode::RadarWait,message),objectRules,routeVisualRevision,sceneBoxes,currentRouteColor(),mode==Mode::Record?L"Запись":mode==Mode::Idle?L"Не воспроизводится":mode==Mode::Drive&&trafficYield.entity?L"Уступаю":mode==Mode::Parked?L"Маршрут завершён":mode==Mode::Emergency?L"Ошибка · ручник":mode==Mode::RadarWait?L"Пауза · обработка":mode==Mode::PickBrake||overlay.selecting()?L"Выбор точки":L"Воспроизводится",routeCatalog.entries,gpsDriving,(gpsDriving||mode==Mode::Record)?SIZE_MAX:selectedRoute);
        }catch(...){if(mode!=Mode::Idle)emergency(L"Обработка остановлена. Ручник удерживается до проверки и отмены через Del.");else message=L"Проверка данных. Сохрани отчёт на рабочий стол.";}
        if(!foreground())MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);else SwitchToThread();
    }
    if(keyboardHook)UnhookWindowsHookEx(keyboardHook);
    if(mouseHook)UnhookWindowsHookEx(mouseHook);
    return int(msg.wParam);
}
