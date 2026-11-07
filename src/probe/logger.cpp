#include "logger.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <mutex>

namespace m2vr {
namespace {

std::mutex g_log_mutex;
std::FILE* g_log = nullptr;
std::atomic<std::uint64_t> g_sequence{0};
ULONGLONG g_last_flush_tick=0;

}  // namespace

bool OpenLog(const std::filesystem::path& path) {
    std::scoped_lock lock(g_log_mutex);
    if (g_log) {
        return true;
    }

    // Every original x86 emulator process must own a BRAND NEW log.
    // Opening in append mode silently merged runs if Windows reused a PID
    // and the old second-resolution filename was still on disk. A CSV with
    // concatenated headers or two process lifetimes cannot be used as an
    // independent Daytona PRIVATE source session.
    //
    // CREATE_NEW is atomic across processes and refuses existing files,
    // including same-name reopens. Let the probe fail closed rather than
    // corrupt a user-owned original game capture. FILE_SHARE_READ allows
    // an observer to inspect an actively written log without writing it.
    const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,
        nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    const int fd=_open_osfhandle(reinterpret_cast<intptr_t>(file),
                                _O_WRONLY|_O_BINARY);
    if(fd==-1){
        CloseHandle(file); // Ownership transfers only on successful import.
        return false;
    }
    // MSVC's UCRT exposes _fdopen(int,const char*), not _fdopen_s.
    // On failure _close still owns and disposes the imported OS handle.
    std::FILE* opened=_fdopen(fd,"wb");
    if(!opened){
        _close(fd);
        return false;
    }
    g_log=opened;
    g_last_flush_tick=GetTickCount64();
    std::fputs("seq,qpc,thread,event,a0,a1,a2,a3\r\n",g_log);
    if(std::fflush(g_log)!=0){
        std::fclose(g_log);
        g_log=nullptr;
        return false;
    }
    return true;
}

void CloseLog() {
    std::scoped_lock lock(g_log_mutex);
    if (g_log) {
        std::fflush(g_log);
        std::fclose(g_log);
        g_log = nullptr;
    }
}

void FlushLog() {
    std::scoped_lock lock(g_log_mutex);
    if (g_log) {
        std::fflush(g_log);
        g_last_flush_tick=GetTickCount64();
    }
}

void LogEvent(
    std::string_view event,
    std::uint64_t a0,
    std::uint64_t a1,
    std::uint64_t a2,
    std::uint64_t a3) {
    std::scoped_lock lock(g_log_mutex);
    if (!g_log) {
        return;
    }

    // Original D3D9 can emit source events from different threads. Capture
    // both the CSV sequence and QPC while holding the writer lock: stamping
    // before the lock let a later sequence reach disk first, making an
    // authentic original physical Draw/Present audit look reordered.
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    const auto seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto tid = GetCurrentThreadId();

    std::fprintf(
        g_log,
        "%llu,%lld,%lu,%.*s,%llu,%llu,%llu,%llu\r\n",
        static_cast<unsigned long long>(seq),
        static_cast<long long>(qpc.QuadPart),
        static_cast<unsigned long>(tid),
        static_cast<int>(event.size()),
        event.data(),
        static_cast<unsigned long long>(a0),
        static_cast<unsigned long long>(a1),
        static_cast<unsigned long long>(a2),
        static_cast<unsigned long long>(a3));

    // 2D Daytona sessions are short; waiting for 1,024 events loses
    // valuable last-frame evidence on an emulator crash/forced exit.
    // Flush at most once per 64 rows or about once per second while
    // events arrive, without syncing the filesystem on every Draw.
    const ULONGLONG tick=GetTickCount64();
    if ((seq & 63u) == 0u || tick-g_last_flush_tick>=1000u) {
        std::fflush(g_log);
        g_last_flush_tick=tick;
    }
}

}  // namespace m2vr
