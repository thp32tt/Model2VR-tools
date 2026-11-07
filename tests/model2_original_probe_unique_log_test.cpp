#include "../src/probe/logger.h"
#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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
    // A genuine original emulator can trigger D3D9 callbacks on multiple
    // threads. CSV sequence/order must still be a single total order.
    constexpr int kThreads=8;
    constexpr int kRowsPerThread=256;
    std::vector<std::thread> writers;
    for(int thread=0;thread<kThreads;++thread){
        writers.emplace_back([thread](){
            for(int row=0;row<kRowsPerThread;++row){
                m2vr::LogEvent("synthetic_concurrent_draw",
                    static_cast<std::uint64_t>(thread),
                    static_cast<std::uint64_t>(row));
                if((row&3)==0)std::this_thread::yield();
            }
        });
    }
    for(auto& writer:writers)writer.join();
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
        std::istringstream rows(data);
        std::string line;
        CHECK(static_cast<bool>(std::getline(rows,line)));
        CHECK(line=="seq,qpc,thread,event,a0,a1,a2,a3\r");
        std::uint64_t previous_seq=0;
        long long previous_qpc=0;
        int concurrent_rows=0;
        int total_rows=0;
        while(std::getline(rows,line)){
            CHECK(!line.empty());
            const auto first=line.find(',');
            CHECK(first!=std::string::npos);
            const auto second=line.find(',',first+1);
            CHECK(second!=std::string::npos);
            const auto seq=std::stoull(line.substr(0,first));
            const auto qpc=std::stoll(line.substr(first+1,
                                                  second-first-1));
            CHECK(seq==previous_seq+1);
            CHECK(qpc>=previous_qpc);
            previous_seq=seq;
            previous_qpc=qpc;
            ++total_rows;
            if(line.find(",synthetic_concurrent_draw,")!=std::string::npos)
                ++concurrent_rows;
        }
        CHECK(concurrent_rows==kThreads*kRowsPerThread);
        CHECK(total_rows==1+kThreads*kRowsPerThread);
    }
    cleanup();
    return 0;
}
