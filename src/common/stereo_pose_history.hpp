#pragma once
// Source OpenXR predicted-view provenance: a small bounded tag->value ring.
// Does not perform rendering, sample tracking or claim game camera accuracy.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace m2vr::transport {
template<class ViewPair,std::size_t Slots=32>
class StereoPoseHistory final {
    static_assert(Slots>=2);
    struct Entry {std::uint32_t sequence=0;ViewPair value{};};
public:
    // Sequence zero is reserved for uncalibrated/legacy GameStereo frames.
    // Reject truncation rather than colliding with an earlier low-32 tag.
    bool Record(std::uint64_t full_sequence,const ViewPair& pair) noexcept {
        if(!full_sequence ||
           full_sequence>std::numeric_limits<std::uint32_t>::max())
            return false;
        const auto sequence=static_cast<std::uint32_t>(full_sequence);
        if(sequence<=highest_sequence_)return false;
        auto& slot=entries_[sequence%Slots];
        slot.value=pair;
        slot.sequence=sequence; // publish the tag only after the pair
        highest_sequence_=sequence;
        return true;
    }
    const ViewPair* Find(std::uint32_t sequence)const noexcept {
        if(!sequence)return nullptr;
        const auto& slot=entries_[sequence%Slots];
        return slot.sequence==sequence ? &slot.value : nullptr;
    }
    void Clear() noexcept {
        for(auto& slot:entries_)slot.sequence=0;
    }
private:
    std::array<Entry,Slots> entries_{};
    std::uint32_t highest_sequence_=0; // Clear revokes but never rewinds
};
} // namespace m2vr::transport
