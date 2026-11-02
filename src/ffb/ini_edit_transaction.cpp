#include "ini_edit_transaction.hpp"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace m2vr::ffbui {
namespace {
constexpr wchar_t kAbsent[]=L"{m2vr-no-such-ini-key-6D4728}";
// Backups from multiple consecutive saves must not share a millisecond name.
std::atomic<std::uint64_t> g_backup_sequence{0};
struct Handle {
    HANDLE value=nullptr;
    explicit Handle(HANDLE handle):value(handle){}
    ~Handle(){if(value)CloseHandle(value);}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
    explicit operator bool()const{return value!=nullptr;}
};
struct Temp {
    std::filesystem::path path;
    ~Temp(){if(!path.empty())DeleteFileW(path.c_str());}
};
bool IsModel2(const std::filesystem::path& ini) {
    return GetPrivateProfileIntW(L"Settings",L"GameId",-1,ini.c_str())==25;
}
bool SameFileMetadata(const WIN32_FILE_ATTRIBUTE_DATA& a,
                      const WIN32_FILE_ATTRIBUTE_DATA& b) {
    return CompareFileTime(&a.ftLastWriteTime,&b.ftLastWriteTime)==0 &&
           a.nFileSizeHigh==b.nFileSizeHigh &&
           a.nFileSizeLow==b.nFileSizeLow;
}
std::wstring MutexKey(const std::filesystem::path& absolute) {
    // Lock across both Model2VR windows, without globally intercepting input.
    std::uint64_t hash=14695981039346656037ull;
    const auto value=absolute.wstring();
    for(wchar_t ch:value) {
        wchar_t lower=ch;
        if(ch>=L'A'&&ch<=L'Z')lower=ch+32;
        hash^=static_cast<std::uint32_t>(lower);
        hash*=1099511628211ull;
    }
    return L"Local\\Model2VRIni_"+std::to_wstring(hash);
}
class HeldMutex {
    Handle mutex_;
    bool locked_=false;
public:
    explicit HeldMutex(const std::filesystem::path& path)
      :mutex_(CreateMutexW(nullptr,FALSE,MutexKey(path).c_str())) {
        if(mutex_){
            const auto result=WaitForSingleObject(mutex_.value,3000);
            locked_=result==WAIT_OBJECT_0; // abandon means fail closed
        }
    }
    ~HeldMutex(){if(locked_)ReleaseMutex(mutex_.value);}
    explicit operator bool()const{return locked_;}
};
} // namespace

std::optional<std::wstring> ReadIniSetting(const std::filesystem::path& ini,
                                           const wchar_t* section,
                                           const wchar_t* key) {
    std::array<wchar_t,1024> output{};
    const auto read=GetPrivateProfileStringW(
        section,key,kAbsent,output.data(),
        static_cast<DWORD>(output.size()),ini.c_str());
    if(read>=output.size()-1)return std::nullopt; // truncate => fail closed
    if(std::wstring_view(output.data())==kAbsent)return std::nullopt;
    return std::wstring(output.data(),read);
}
IniSaveStatus SaveModel2Ini(const std::filesystem::path& ini,
                            std::span<const IniEdit> edits) {
    std::error_code error;
    const auto absolute=std::filesystem::absolute(ini,error);
    if(error || !std::filesystem::is_regular_file(absolute,error) || error)
        return IniSaveStatus::IoError;
    HeldMutex mutex(absolute);
    if(!mutex)return IniSaveStatus::IoError;
    if(!IsModel2(absolute))return IniSaveStatus::WrongGame;
    std::vector<const IniEdit*> changed;
    changed.reserve(edits.size());
    for(const auto& edit:edits){
        if(edit.section.empty() || edit.key.empty() || edit.after.empty())
            return IniSaveStatus::IoError;
        const auto current=ReadIniSetting(absolute,edit.section.c_str(),edit.key.c_str());
        if(current!=edit.before)return IniSaveStatus::Conflict;
        if(current && *current==edit.after)continue;
        changed.push_back(&edit);
    }
    if(changed.empty())return IniSaveStatus::NoChanges;

    WIN32_FILE_ATTRIBUTE_DATA initial{};
    if(!GetFileAttributesExW(absolute.c_str(),GetFileExInfoStandard,&initial))
        return IniSaveStatus::IoError;
    auto folder=absolute.parent_path();
    std::array<wchar_t,MAX_PATH> tempName{};
    if(!GetTempFileNameW(folder.c_str(),L"M2V",0,tempName.data()))
        return IniSaveStatus::IoError;
    Temp temp{tempName.data()};
    if(!CopyFileW(absolute.c_str(),temp.path.c_str(),FALSE))
        return IniSaveStatus::IoError;
    // All writes are performed on the private temporary file only.
    for(const auto* e:changed){
        if(!WritePrivateProfileStringW(e->section.c_str(),e->key.c_str(),
                                       e->after.c_str(),temp.path.c_str()))
            return IniSaveStatus::IoError;
        const auto read=ReadIniSetting(temp.path,e->section.c_str(),e->key.c_str());
        if(!read||*read!=e->after)return IniSaveStatus::IoError;
    }
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,temp.path.c_str());
    // Recheck before replacement; a concurrent official GUI save is not lost.
    WIN32_FILE_ATTRIBUTE_DATA latest{};
    if(!GetFileAttributesExW(absolute.c_str(),GetFileExInfoStandard,&latest))
        return IniSaveStatus::IoError;
    if(!SameFileMetadata(initial,latest))return IniSaveStatus::Conflict;
    for(const auto& e:edits)
        if(ReadIniSetting(absolute,e.section.c_str(),e.key.c_str())!=e.before)
            return IniSaveStatus::Conflict;

    const auto backup=std::filesystem::path(
        absolute.wstring()+L".model2vr-"+std::to_wstring(GetCurrentProcessId())+
        L"-"+std::to_wstring(GetTickCount64())+L"-"+
        std::to_wstring(g_backup_sequence.fetch_add(
            1u,std::memory_order_relaxed)+1u)+L".bak");
    if(!CopyFileW(absolute.c_str(),backup.c_str(),TRUE))
        return IniSaveStatus::IoError;
    if(!MoveFileExW(temp.path.c_str(),absolute.c_str(),
                    MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        return IniSaveStatus::IoError; // source remains intact + backup exists
    temp.path.clear();
    return IniSaveStatus::Saved;
}
} // namespace m2vr::ffbui
