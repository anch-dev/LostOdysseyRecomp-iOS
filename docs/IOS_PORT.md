# iOS port (work in progress)

Goal: run LostOdysseyRecomp natively on iPhone/iPad (arm64, Metal through plume), installable unsigned
through LiveContainer or a normal sideload. No game data or generated game code is ever committed.

## Status
- [x] iOS platform in CMake (`LO_TARGET_PLATFORM=ios`, `LO_TARGET_APPLE`), iOS deployment target 16.0
- [ ] Third-party libraries build for iPhoneOS/arm64 (SDL3, plume, SPIRV-Cross, FFmpeg, XenonUtils) - CI: `ios-arm64.yml`
- [ ] Runtime compiles for iOS (needs generated PPC headers, i.e. a private `default.xex`)
- [ ] Guest address space on iOS (4 GiB reservation, 16 KiB pages)
- [ ] Touch controls, Files-app import of discs, shader pack without DXC
- [ ] Unsigned IPA packaging

## Design notes
- iOS reuses the macOS (Mach/POSIX/Metal) code paths: `LO_PLATFORM_MACOS` is 1 on both, `LO_PLATFORM_IOS` marks iOS.
- No DXC on device. Metal consumes the portable SPIR-V shader pack (`vulkan` contract) and translates to MSL via SPIRV-Cross.
- The game-derived code is generated from the user's own `default.xex` and must never be published; builds that include it
  are produced only in a private location.
