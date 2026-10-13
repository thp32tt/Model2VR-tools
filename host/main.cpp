#include "xr_app.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv){
    try {
        const int frames=argc>1?std::clamp(std::stoi(argv[1]),1,10000):600;
        m2vr::host::XrApp app;
        if(argc>2){
            const auto pid=std::stoul(argv[2]);
            if(pid==0 || pid>0xffffffffUL)throw std::runtime_error("Invalid emulator PID");
            app.set_source_pid(static_cast<std::uint32_t>(pid));
            std::cout<<"Stereo channel key PID: "<<pid<<"\n";
        }else{
            std::cout<<"No stereo source supplied: debug colors only\n";
        }
        app.initialize();
        app.run(frames);
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"Model2VR OpenXR HMD test: "<<e.what()<<"\n";
        return 1;
    }
}
