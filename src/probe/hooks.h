#pragma once

#include "config.h"

#include <Windows.h>

namespace m2vr {

bool InstallProbeHooks(HMODULE executable, const ProbeConfig& config);
void RemoveProbeHooks();

}  // namespace m2vr
