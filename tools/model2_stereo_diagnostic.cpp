#include "../src/common/stereo_frame_ipc.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int wmain(int argc,wchar_t** argv){
    using namespace m2vr::transport;
    if(argc<2){
        std::wcerr << L"Usage: Model2VRStereoDiagnostic.exe <emulator_pid> [frames]\n";
        return 2;
    }
    const auto key=static_cast<std::uint32_t>(std::wcstoul(argv[1],nullptr,10));
    const auto count=argc>=3?std::clamp(std::wcstoul(argv[2],nullptr,10),1ul,5000ul):300ul;
    SharedFrames writer;
    if(!writer.Create(key)){
        std::wcerr << L"Shared stereo frame channel unavailable; check other writer.\n";
        return 3;
    }
    std::vector<std::uint8_t> left(kBytesPerEye),right(kBytesPerEye);
    for(std::uint64_t frame=1;frame<=count;++frame){
        for(int eye=0;eye<2;++eye){
            auto& out=eye==0?left:right;
            const int shift=eye==0?22:-22;
            for(std::uint32_t y=0;y<kHeight;++y)for(std::uint32_t x=0;x<kWidth;++x){
                const auto p=static_cast<std::size_t>(y)*kPitch+4*x;
                const bool near_square=(int(x)-512-shift)>=-145 &&
                        (int(x)-512-shift)<145 &&
                        (int(y)-512)>=-130 &&(int(y)-512)<130;
                const bool far_square=(int(x)-512-(eye==0?6:-6))>=-235 &&
                        (int(x)-512-(eye==0?6:-6))<235 &&
                        (int(y)-512)>=-210 && (int(y)-512)<210;
                out[p+0]=near_square?220:(far_square?75:22);
                out[p+1]=near_square?100:(far_square?110:30);
                out[p+2]=near_square?40:(far_square?180:40);
                out[p+3]=255;
            }
        }
        if(!writer.Publish(left.data(),right.data(),frame,FrameKind::Diagnostic))
            return 4;
        Sleep(33);
    }
    std::cout<<"Published "<<count<<" diagnostic-only stereo frames\n";
    return 0;
}
