#include "../src/probe/logger.h"
#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#define CHECK(x) do { if(!(x)) { \
    std::fprintf(stderr,"original x86 log creation line %d: %s\n", \
                 __LINE__,#x); return 1; } } while(0)

int main(){
    // Only invented, local scratch paths and test rows. Never use a real
    // original emulator process, ROM, PRIVATE CSV or headset source.
    wchar_t temp[MAX_PATH]{};
    CHECK(GetTempPathW(MAX_PATH,temp)>0);
    wchar_t path[MAX_PATH]{};
    CHECK(GetTempFileNameW(temp,L"m2v",0,path)!=0);
    const std::filesystem::path test_path(path);
    const auto cleanup=[&](){m2vr::CloseLog();
                              std::error_code ec;
                              std::filesystem::remove(test_path,ec);};
    // GetTempFileName creates a real existing file. A second process must
    // NEVER append to it, truncate it, or silently accept it as fresh.
    {
        std::ofstream existing(test_path,std::ios::binary|std::ios::trunc);
        existing<<"untouched prior emulator PRIVATE process";
    }
    CHECK(!m2vr::OpenLog(test_path));
    {
        std::ifstream existing(test_path,std::ios::binary);
        const std::string saved{std::istreambuf_iterator<char>(existing),
                                std::istreambuf_iterator<char>()};
        CHECK(saved=="untouched prior emulator PRIVATE process");
    }
    // Even a zero-length file left by a crashed original x86 probe is
    // already owned by another session. Never silently fill it in later.
    {
        std::ofstream empty(test_path,std::ios::binary|std::ios::trunc);
    }
    CHECK(!m2vr::OpenLog(test_path));
    CHECK(std::filesystem::file_size(test_path)==0u);
    std::filesystem::remove(test_path);
    CHECK(m2vr::OpenLog(test_path));
    m2vr::LogEvent("synthetic_one_process",42u);
    m2vr::FlushLog();
    m2vr::CloseLog();
    CHECK(!m2vr::OpenLog(test_path)); // Simulate same PID/time filename reuse.
    {
        std::ifstream stream(test_path,std::ios::binary);
        const std::string data{std::istreambuf_iterator<char>(stream),
                               std::istreambuf_iterator<char>()};
        CHECK(data.rfind("seq,qpc,thread,event,a0,a1,a2,a3\r\n",0)==0);
        CHECK(data.find("synthetic_one_process")!=std::string::npos);
        CHECK(data.find("synthetic_one_process")==
              data.rfind("synthetic_one_process"));
        CHECK(data.find("seq,qpc,thread,event,a0,a1,a2,a3\r\n",1)
              ==std::string::npos);
    }
    cleanup();
    return 0;
}
