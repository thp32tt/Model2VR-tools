// Model 2 VR F11 settings companion. No wheel-effect backend is created here.
// The already-installed Generic DD FFB Arcade Plugin remains sole FFB owner.
// Native desktop control surface; XR-rendered UI is a separate milestone.
#include <Windows.h>
#include <filesystem>
#include <array>
#include <string>
#include <cwchar>
#include <cwctype>

namespace {
constexpr int kHotkey=1, kSave=900, kReload=901, kBindings=902, kClose=903;
constexpr int kFirstEdit=100;
struct Option {
    const wchar_t* section;
    const wchar_t* key;
    const wchar_t* label;
    int initial, min, max;
    HWND edit=nullptr;
};
std::array<Option,14> options{{
    {L"Settings",L"Model2ModernFFB",L"Modern FFB enabled (0/1)",1,0,1},
    {L"Settings",L"InputDeviceWheelEnable",L"Plugin wheel input (0/1)",1,0,1},
    {L"Model2Modern",L"Enabled",L"Generic DD active (0/1)",1,0,1},
    {L"Model2Modern",L"ConstantGain",L"Cornering/constant gain",100,0,200},
    {L"Model2Modern",L"SpringGain",L"Spring gain",45,0,200},
    {L"Model2Modern",L"FrictionGain",L"Friction gain",35,0,200},
    {L"Model2Modern",L"PeriodicGain",L"Periodic gain",30,0,200},
    {L"Model2Modern",L"RumbleGain",L"Rumble gain",20,0,200},
    {L"Model2Modern",L"BuildRate",L"Force build rate",18,0,100},
    {L"Model2Modern",L"ReversalReleaseRate",L"Torque reversal release",40,0,100},
    {L"Model2Modern",L"InvertConstant",L"Invert constant (0/1)",0,0,1},
    {L"Model2Modern",L"InvertSpring",L"Invert spring (0/1)",0,0,1},
    {L"Model2Modern",L"FocusSafety",L"Release FFB on focus loss",1,0,1},
    {L"Model2Modern",L"CommandLeaseMs",L"Torque lease (ms)",180,40,1000},
}};
HWND window=nullptr, status_label=nullptr;
bool hotkey_registered=false;
std::filesystem::path game_dir, ini_path;
DWORD emulator_pid=0;
HANDLE emulator_process=nullptr;

void Status(const wchar_t* message){
    if(status_label)SetWindowTextW(status_label,message);
}
void LoadValues(){
    for(auto& o:options){
        const auto value=GetPrivateProfileIntW(o.section,o.key,o.initial,ini_path.c_str());
        SetWindowTextW(o.edit,std::to_wstring(value).c_str());
    }
    Status(L"Loaded. Save writes FFBPlugin.ini (relaunch emulator to apply force changes).");
}
bool ReadEdit(const Option& o,int& result){
    wchar_t value[48]{};
    if(!GetWindowTextW(o.edit,value,48))return false;
    wchar_t* end=nullptr;
    const long parsed=wcstol(value,&end,10);
    while(end && *end && iswspace(*end))++end;
    if(!end||end==value||*end!=L'\0'||parsed<o.min||parsed>o.max)return false;
    result=static_cast<int>(parsed);return true;
}
void SaveValues(){
    if(!std::filesystem::exists(ini_path)){
        Status(L"Missing FFBPlugin.ini: install Generic DD v2 next to emulator.");
        return;
    }
    std::array<int,options.size()> next{};
    for(std::size_t i=0;i<options.size();++i){
        if(!ReadEdit(options[i],next[i])){
            Status(L"Invalid or out-of-range setting. No settings were written.");
            SetFocus(options[i].edit);return;
        }
    }
    // These INI writes are the *only* mutation. Never acquire a second haptic
    // device or send test torques: plugin owns all wheel effects and watchdog.
    for(std::size_t i=0;i<options.size();++i){
        if(!WritePrivateProfileStringW(options[i].section,options[i].key,
                                      std::to_wstring(next[i]).c_str(),ini_path.c_str())){
            Status(L"Write failed. Check FFBPlugin.ini permissions and reload.");
            return;
        }
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,ini_path.c_str());
    Status(L"Saved. Restart the emulator to apply FFB gains and wheel input mode.");
}
void LaunchBindingEditor(){
    const auto gui=game_dir / L"FFBPluginGUI.exe";
    if(!std::filesystem::exists(gui)){
        Status(L"Missing FFBPluginGUI.exe in emulator directory.");
        return;
    }
    STARTUPINFOW si{};
    si.cb=sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring command=L"\""+gui.wstring()+L"\"";
    if(CreateProcessW(gui.c_str(),command.data(),nullptr,nullptr,FALSE,0,
                      nullptr,game_dir.c_str(),&si,&pi)){
        CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
        Status(L"FFBPluginGUI opened. Bind steering, pedals, gears, Start and buttons there.");
    }else{
        Status(L"Unable to launch the Generic DD binding editor.");
    }
}
bool EmulatorForeground(){
    if(!emulator_pid)return true; // explicit standalone control-panel mode
    const HWND foreground=GetForegroundWindow();
    DWORD pid=0;
    if(foreground)GetWindowThreadProcessId(foreground,&pid);
    return pid==emulator_pid;
}
// Register F11 only while the emulator or its companion menu has focus.
// A process-wide/global F11 grab in other apps would be an OS input bug.
void UpdateHotkey(){
    const bool panel_focused=IsWindowVisible(window) &&
        GetForegroundWindow()==window;
    const bool allowed=EmulatorForeground() || panel_focused;
    if(allowed && !hotkey_registered)
        hotkey_registered=RegisterHotKey(nullptr,kHotkey,MOD_NOREPEAT,VK_F11)!=FALSE;
    else if(!allowed && hotkey_registered){
        UnregisterHotKey(nullptr,kHotkey);
        hotkey_registered=false;
    }
}
void Toggle(){
    if(IsWindowVisible(window)){
        ShowWindow(window,SW_HIDE);
    }else if(EmulatorForeground()){
        LoadValues();
        ShowWindow(window,SW_SHOWNORMAL);
        SetForegroundWindow(window);
    }
}
LRESULT CALLBACK WindowProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE: {
        HFONT font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto add=[&](const wchar_t* type,const wchar_t* label,DWORD style,
                     int x,int y,int width,int height,int id)->HWND{
            HWND child=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style,
                x,y,width,height,h,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr),nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
            return child;
        };
        add(L"STATIC",L"Model2VR / Generic DD v2 FFB",0,18,13,480,25,0);
        add(L"STATIC",L"F11: show/hide | Wheel effects stay owned by FFB Arcade Plugin",
            0,18,38,530,22,0);
        for(std::size_t i=0;i<options.size();++i){
            const int y=70+static_cast<int>(i)*30;
            const auto& o=options[i];
            add(L"STATIC",o.label,0,18,y+3,315,24,0);
            options[i].edit=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|ES_NUMBER,
                                340,y,100,24,kFirstEdit+static_cast<int>(i));
            add(L"STATIC",(std::to_wstring(o.min)+L".."+std::to_wstring(o.max)).c_str(),
                0,453,y+3,90,24,0);
        }
        add(L"BUTTON",L"Save FFB settings",BS_PUSHBUTTON,18,504,162,32,kSave);
        add(L"BUTTON",L"Wheel / key bindings...",BS_PUSHBUTTON,190,504,183,32,kBindings);
        add(L"BUTTON",L"Reload",BS_PUSHBUTTON,383,504,78,32,kReload);
        add(L"BUTTON",L"Hide",BS_PUSHBUTTON,470,504,72,32,kClose);
        status_label=add(L"STATIC",L"",0,18,548,550,44,0);
        LoadValues();return 0;
    }
    case WM_COMMAND:
        switch(LOWORD(w)){
        case kSave:SaveValues();return 0;
        case kReload:LoadValues();return 0;
        case kBindings:LaunchBindingEditor();return 0;
        case kClose:ShowWindow(h,SW_HIDE);return 0;
        default:break;
        }break;
    case WM_CLOSE:ShowWindow(h,SW_HIDE);return 0;
    case WM_TIMER:
        if(emulator_process && WaitForSingleObject(emulator_process,0)==WAIT_OBJECT_0){
            DestroyWindow(h);return 0;
        }
        UpdateHotkey();
        return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    const int argc=__argc;
    wchar_t** argv=__wargv;
    if(argc>1){
        wchar_t* end=nullptr;
        const auto value=wcstoul(argv[1],&end,10);
        if(end==argv[1]||*end||value>0xffffffffu)return 2;
        emulator_pid=static_cast<DWORD>(value);
        emulator_process=OpenProcess(SYNCHRONIZE,FALSE,emulator_pid);
        if(!emulator_process)return 3;
    }
    if(argc>2)game_dir=std::filesystem::absolute(argv[2]);
    else game_dir=std::filesystem::current_path();
    ini_path=game_dir/L"FFBPlugin.ini";

    WNDCLASSW wc{};
    wc.lpfnWndProc=WindowProc;
    wc.hInstance=instance;
    wc.lpszClassName=L"Model2VR_F11_Config_v1";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc))return 4;
    window=CreateWindowExW(0,wc.lpszClassName,L"Model2VR / F11 FFB & Controls",
            WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            CW_USEDEFAULT,CW_USEDEFAULT,570,655,nullptr,nullptr,instance,nullptr);
    if(!window)return 5;
    // Never capture F11 globally while other applications are foreground.
    // The panel can be opened manually if another program reserves F11.
    if(!emulator_pid)ShowWindow(window,SW_SHOWNORMAL);
    SetTimer(window,1,125,nullptr);
    UpdateHotkey();
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){
        if(msg.message==WM_HOTKEY&&msg.wParam==kHotkey){
            Toggle();continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if(hotkey_registered)UnregisterHotKey(nullptr,kHotkey);
    if(emulator_process)CloseHandle(emulator_process);
    return 0;
}
