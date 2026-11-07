#include "../host/original_source_process_lease.hpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(v) do {if(!(v)){std::fprintf(stderr,"original source lease %d: %s\n",__LINE__,#v);std::exit(1);}}while(0)
int main(){
    m2vr::host::OriginalSourceProcessLease lease;
    CHECK(!lease.Alive());
    CHECK(!lease.Attach(0u));
    // A genuine Windows process is started SUSPENDED, so it cannot
    // terminate before this test binds the original process HANDLE.
    wchar_t command[]=L"cmd.exe /C exit 0";
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION child{};
    CHECK(CreateProcessW(nullptr,command,nullptr,nullptr,FALSE,
        CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,
        &startup,&child)!=FALSE);
    CHECK(lease.Attach(child.dwProcessId));
    CHECK(lease.Pid()==child.dwProcessId);
    CHECK(lease.CreationTime100ns()!=0u);
    CHECK(lease.Attach(child.dwProcessId));
    CHECK(lease.Alive());
    CHECK(!lease.Attach(child.dwProcessId+1u)); // wrong original process
    CHECK(ResumeThread(child.hThread)!=static_cast<DWORD>(-1));
    CHECK(WaitForSingleObject(child.hProcess,20000u)==WAIT_OBJECT_0);
    CHECK(!lease.Alive());
    CHECK(lease.Terminal());
    CHECK(!lease.Attach(child.dwProcessId)); // irreversible PID-reuse guard
    CloseHandle(child.hThread);CloseHandle(child.hProcess);
    return 0;
}
