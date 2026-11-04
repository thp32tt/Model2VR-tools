#pragma once
// Bounded original-frame visibility evidence for the explicitly MONO
// fallback. The grid contains only aggregates: NO texture/ROM/game pixel
// bytes, frame hash, identifying image features or caller addresses in logs.
// May indicate a dark/flat game backbuffer, never 6DoF world proof.
#include <cstddef>
#include <cstdint>

namespace m2vr::daytona {
struct OriginalMonoContentEvidence {
    std::uint32_t sampled=0;
    std::uint32_t nonblack=0;
    std::uint32_t varied=0;
};
inline OriginalMonoContentEvidence InspectOriginalMonoRGBA(
    const std::uint8_t* rgba,std::size_t bytes,std::uint32_t width,
    std::uint32_t height,std::uint32_t pitch) noexcept {
    OriginalMonoContentEvidence result{};
    if(!rgba||width==0||height==0||pitch<width*4ull ||
       bytes<static_cast<std::size_t>(pitch)*height)return result;
    // Sample content, not the possibly letterboxed perimeter alone.
    constexpr std::uint32_t columns=16u,rows=9u;
    std::uint8_t reference[3]{};
    for(std::uint32_t y=0;y<rows;++y){
        const auto sy=static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(y)*2u+1u)*height/(rows*2u));
        for(std::uint32_t x=0;x<columns;++x){
            const auto sx=static_cast<std::uint32_t>(
                (static_cast<std::uint64_t>(x)*2u+1u)*width/(columns*2u));
            const auto offset=static_cast<std::size_t>(sy)*pitch+
                              static_cast<std::size_t>(sx)*4u;
            const auto* sample=rgba+offset;
            if(result.sampled==0u){
                reference[0]=sample[0];reference[1]=sample[1];
                reference[2]=sample[2];
            }
            ++result.sampled;
            if(sample[0]>8u||sample[1]>8u||sample[2]>8u)++result.nonblack;
            if((sample[0]>reference[0]?sample[0]-reference[0]:reference[0]-sample[0])>8u ||
               (sample[1]>reference[1]?sample[1]-reference[1]:reference[1]-sample[1])>8u ||
               (sample[2]>reference[2]?sample[2]-reference[2]:reference[2]-sample[2])>8u)
                ++result.varied;
        }
    }
    return result;
}
} // namespace m2vr::daytona
