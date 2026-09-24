#include "logger.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>

namespace m2vr {
namespace {

std::mutex g_log_mutex;
std::FILE* g_log = nullptr;
std::atomic<std::uint64_t> g_sequence{0};

}  // namespace

bool OpenLog(const std::filesystem::path& path) {
    std::scoped_lock lock(g_log_mutex);
    if (g_log) {
        return true;
    }

    if (_wfopen_s(&g_log, path.c_str(), L"ab") != 0 || !g_log) {
        g_log = nullptr;
        return false;
    }

    std::fseek(g_log, 0, SEEK_END);
    if (std::ftell(g_log) == 0) {
        std::fputs("seq,qpc,thread,event,a0,a1,a2,a3\r\n", g_log);
        std::fflush(g_log);
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

void LogEvent(
    std::string_view event,
    std::uint64_t a0,
    std::uint64_t a1,
    std::uint64_t a2,
    std::uint64_t a3) {
    LARGE_INTEGER qpc{};
    QueryPerformanceCounter(&qpc);
    const auto seq = g_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto tid = GetCurrentThreadId();

    std::scoped_lock lock(g_log_mutex);
    if (!g_log) {
        return;
    }

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

    if ((seq & 0x3ffu) == 0) {
        std::fflush(g_log);
    }
}

}  // namespace m2vr
