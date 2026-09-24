# Source layout

Planned modules:

src/vr-core/        OpenXR/runtime-independent stereo core
src/model2-adapter/ Model 2 emulator integration
src/ffb-core/       normalized arcade FFB events and wheel output
src/common/         shared logging/configuration/utilities

Implementation code should consume stable interfaces or signatures supplied by a private build-time mapping step; raw proprietary evidence stays out of this repository.