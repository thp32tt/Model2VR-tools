# Model2VR public architecture

Target separation:

- `vr-core`: OpenXR frame lifecycle, pose ownership and stereo transport.
- `model2-adapter`: emulator-specific render classification and camera/projection integration.
- `ffb-core`: normalized force-feedback event model and wheel backend.
- `tools`: generic binary/i960 utilities that contain no proprietary data.

Core rule: simulation, input and FFB execute once per emulated frame. Stereo rendering may duplicate only the render submission needed for the second eye.

Private reverse-analysis addresses and raw evidence are intentionally not part of this repository.