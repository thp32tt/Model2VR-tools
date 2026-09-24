#include "signature.h"

#include <Windows.h>

#include <cstring>

namespace m2vr {

bool VerifySignature(
    const std::uint8_t* target,
    const std::vector<std::uint8_t>& signature,
    std::string& error) {
    if (!target || signature.empty()) {
        error = "empty target/signature";
        return false;
    }

    __try {
        if (std::memcmp(target, signature.data(), signature.size()) != 0) {
            error = "signature mismatch";
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        error = "signature read fault";
        return false;
    }

    return true;
}

}  // namespace m2vr
