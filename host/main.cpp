#include "xr_app.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc,char** argv){
    try {
        const int frames=argc>1?std::clamp(std::stoi(argv[1]),1,10000):600;
        m2vr::host::XrApp app;
        app.initialize();
        app.run(frames);
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"Model2VR OpenXR HMD test: "<<e.what()<<"\n";
        return 1;
    }
}
