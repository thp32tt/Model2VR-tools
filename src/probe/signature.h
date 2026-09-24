#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace m2vr {

bool VerifySignature(
    const std::uint8_t* target,
    const std::vector<std::uint8_t>& signature,
    std::string& error);

}  // namespace m2vr
