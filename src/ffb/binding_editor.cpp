#include "binding_contract.hpp"
#include "binding_editor.hpp"
#include <Windows.h>
#include <array>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <string>

namespace m2vr::ffbui {
namespace {
constexpr int kSave=800, kReload=801, kAdvanced=802, kExit=803;
constexpr int kInputId=1000;
HWND g_editor=nullptr, g_status=nullptr;
std::array<HWND,kBindings.size()> g_edits{};
std::filesystem::path g_ini, g_game_dir;

void Status(const wchar_t* text) {
    if(g_status)SetWindowTextW(g_status,text);
}
void Read() {
    if(!std::filesystem::is_regular_file(g_ini)) {
        Status(L"FFBPlugin.ini missing. Install Generic DD v2 first.");
        return;
    }
    for(std::size_t i=0;i<kBindings.size();++i){
        const auto& b=kBindings[i];
        const std::wstring key(b.key);
        const int value=GetPrivateProfileIntW(L"Settings",key.c_str(),b.initial,g_ini.c_str());
        SetWindowTextW(g_edits[i],std::to_wstring(value).c_str());
    }
    Status(L"Numeric SDL axis/button indices. Save requires emulator restart.");
}
bool Parse(const wchar_t* text,int& value) {
    if(!text||!*text)return false;
    wchar_t* end=nullptr;
    const long raw=wcstol(text,&end,10);
    if(end==text)return false;
    while(*end && iswspace(*end))++end;
    if(*end)return false;
    value=static_cast<int>(raw);
    return true;
}
void Save() {
    if(!std::filesystem::is_regular_file(g_ini)) {
        Status(L"Missing FFBPlugin.ini: nothing saved.");return;
    }
    std::array<int,kBindings.size()> proposed{};
    for(std::size_t i=0;i<kBindings.size();++i){
        wchar_t buffer[32]{};
        GetWindowTextW(g_edits[i],buffer,32);
        if(!Parse(buffer,proposed[i]) || !Valid(kBindings[i],proposed[i])){
            Status(L"Invalid binding index: ALL changes rejected.");
            SetFocus(g_edits[i]);return;
        }
    }
    // Never mutate the game's real wheel input while it is active.
    // Back up every previous INI before any change; any failure is visible.
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t suffix[64]{};
    swprintf_s(suffix,L".bindings-%04hu%02hu%02hu-%02hu%02hu%02hu.bak",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    std::filesystem::path backup=g_ini;
    backup+=suffix;
    if(!CopyFileW(g_ini.c_str(),backup.c_str(),TRUE)){
        Status(L"Cannot back up FFBPlugin.ini. No changes written.");
        return;
    }
    for(std::size_t i=0;i<kBindings.size();++i){
        const std::wstring key(kBindings[i].key);
        const auto new_value=std::to_wstring(proposed[i]);
        if(!WritePrivateProfileStringW(
            L"Settings",key.c_str(),new_value.c_str(),g_ini.c_str())){
            // Retain backup for manual recovery; partial save is not claimed.
            Status(L"INI write failed. Restore .bindings-*.bak if needed.");
            return;
        }
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,g_ini.c_str());
    Status(L"Bindings saved. Restart Model 2 for consistent game input.");
}
void LaunchOfficialGui(){
    const auto gui=g_game_dir/L"FFBPluginGUI.exe";
    if(!std::filesystem::is_regular_file(gui)){
        Status(L"FFBPluginGUI.exe missing beside emulator.");return;
    }
    std::wstring command=L"\""+gui.wstring()+L"\"";
    STARTUPINFOW si{};si.cb=sizeof(si);
    PROCESS_INFORMATION p{};
    if(CreateProcessW(gui.c_str(),command.data(),nullptr,nullptr,FALSE,0,
                      nullptr,g_game_dir.c_str(),&si,&p)){
        CloseHandle(p.hThread);CloseHandle(p.hProcess);
        Status(L"Opened official GUI for GUID, keyboard & advanced controls.");
    }else{
        Status(L"Could not launch FFBPluginGUI.");
    }
}
LRESULT CALLBACK EditorProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: {
        const auto font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        auto add=[&](const wchar_t* kind,const wchar_t* text,DWORD style,
                     int x,int y,int width,int height,int id) {
            auto child=CreateWindowExW(0,kind,text,
                WS_VISIBLE|WS_CHILD|style,x,y,width,height,hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                GetModuleHandleW(nullptr),nullptr);
            SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
            return child;
        };
        add(L"STATIC",L"Model 2 wheel/joystick mapping (Generic DD v2)",
            0,14,10,725,22,0);
        add(L"STATIC",L"0-based SDL indices. Device selection and keyboard names: Official GUI.",
            0,14,32,725,20,0);
        constexpr int rows=12;
        for(std::size_t i=0;i<kBindings.size();++i){
            const int column=static_cast<int>(i)/rows;
            const int row=static_cast<int>(i)%rows;
            const int x=14+column*382;
            const int y=66+row*30;
            const auto& b=kBindings[i];
            const std::wstring label(b.label);
            add(L"STATIC",label.c_str(),0,x,y+3,228,24,0);
            g_edits[i]=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL|ES_NUMBER,
                        x+236,y,72,24,kInputId+static_cast<int>(i));
            const std::wstring range=std::to_wstring(b.min)+L".."+
                                      std::to_wstring(b.max);
            add(L"STATIC",range.c_str(),0,x+312,y+3,64,24,0);
        }
        add(L"BUTTON",L"Save all",BS_PUSHBUTTON,15,444,132,31,kSave);
        add(L"BUTTON",L"Reload",BS_PUSHBUTTON,158,444,100,31,kReload);
        add(L"BUTTON",L"Official GUI (advanced)",BS_PUSHBUTTON,270,444,199,31,kAdvanced);
        add(L"BUTTON",L"Close",BS_PUSHBUTTON,483,444,101,31,kExit);
        g_status=add(L"STATIC",L"",0,15,485,720,38,0);
        Read();return 0;
    }
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case kSave: Save();return 0;
        case kReload: Read();return 0;
        case kAdvanced: LaunchOfficialGui();return 0;
        case kExit: DestroyWindow(hwnd);return 0;
        default:break;
        }break;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:g_editor=nullptr;g_status=nullptr;return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
} // namespace

void ShowBindingEditor(HWND owner,const std::filesystem::path& ini_path,
                       const std::filesystem::path& game_dir){
    if(g_editor&&IsWindow(g_editor)){
        ShowWindow(g_editor,SW_SHOWNORMAL);SetForegroundWindow(g_editor);
        return;
    }
    g_ini=ini_path;g_game_dir=game_dir;
    WNDCLASSW wc{};
    wc.lpfnWndProc=EditorProc;
    wc.hInstance=GetModuleHandleW(nullptr);
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    wc.lpszClassName=L"Model2VR_FFB_BindingEditor_v1";
    RegisterClassW(&wc);
    g_editor=CreateWindowExW(0,wc.lpszClassName,
        L"Model2VR F11 / Wheel & Button Bindings",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,795,570,owner,nullptr,
        GetModuleHandleW(nullptr),nullptr);
    if(g_editor){
        ShowWindow(g_editor,SW_SHOWNORMAL);
        SetForegroundWindow(g_editor);
    }
}
} // namespace m2vr::ffbui
