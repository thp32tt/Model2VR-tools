#include "../src/ffb/ini_edit_transaction.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace m2vr::ffbui;
#define CHECK(c) do{if(!(c)){std::cerr<<"Ini transaction failed at line "<<__LINE__<<": "<<#c<<"\n";return 1;}}while(0)

int main() {
    const auto folder=fs::temp_directory_path()/
        (L"m2vr-ffb-static-test-"+std::to_wstring(GetCurrentProcessId())+
         L"-"+std::to_wstring(GetTickCount64()));
    fs::create_directories(folder);
    const auto path=folder/L"FFBPlugin.ini";
    const auto write=[&](const char* contents){
        std::ofstream out(path,std::ios::binary|std::ios::trunc);
        out<<contents;out.flush();
        return out.good();
    };
    CHECK(write("[Settings]\nGameId=99\nStartButton=7\n[Other]\nKey=keep\n"));
    const std::vector<IniEdit> change={
        {L"Settings",L"StartButton",std::wstring(L"7"),L"8"}};
    CHECK(SaveModel2Ini(path,change)==IniSaveStatus::WrongGame);
    CHECK(ReadIniSetting(path,L"Settings",L"StartButton")==L"7");

    CHECK(write("[Settings]\nGameId=25\nStartButton=7\n"
                "InputDeviceWheelSteeringAxis=2\n[Other]\nKey=keep\n"));
    CHECK(!ReadIniSetting(path,L"Settings",L"Coin2Button").has_value());
    CHECK(SaveModel2Ini(path,{})==IniSaveStatus::NoChanges);
    std::vector<IniEdit> edits={
        {L"Settings",L"StartButton",std::wstring(L"7"),L"8"},
        {L"Settings",L"ShiftUpButton",std::nullopt,L"4"}};
    CHECK(SaveModel2Ini(path,edits)==IniSaveStatus::Saved);
    CHECK(ReadIniSetting(path,L"Settings",L"StartButton")==L"8");
    CHECK(ReadIniSetting(path,L"Settings",L"ShiftUpButton")==L"4");
    CHECK(ReadIniSetting(path,L"Settings",L"InputDeviceWheelSteeringAxis")==L"2");
    CHECK(ReadIniSetting(path,L"Other",L"Key")==L"keep");
    CHECK(!ReadIniSetting(path,L"Settings",L"Coin2Button").has_value());

    std::size_t backups=0;
    for(const auto& entry:fs::directory_iterator(folder))
        if(entry.path().extension()==L".bak")++backups;
    CHECK(backups==1); // no backup for no-op, one for real changes

    CHECK(SaveModel2Ini(path,edits)==IniSaveStatus::Conflict);
    CHECK(ReadIniSetting(path,L"Settings",L"StartButton")==L"8");
    const std::vector<IniEdit> duplicate={
        {L"Settings",L"StartButton",std::wstring(L"8"),L"9"}};
    CHECK(SaveModel2Ini(path,duplicate)==IniSaveStatus::Saved);
    CHECK(ReadIniSetting(path,L"Settings",L"StartButton")==L"9");

    const std::vector<IniEdit> invalid={
        {L"Settings",L"StartButton",std::wstring(L"9"),L""}};
    CHECK(SaveModel2Ini(path,invalid)==IniSaveStatus::IoError);
    CHECK(ReadIniSetting(path,L"Settings",L"StartButton")==L"9");

    const auto other=folder/L"not-existing.ini";
    CHECK(SaveModel2Ini(other,change)==IniSaveStatus::IoError);
    fs::remove_all(folder);
    return 0;
}
