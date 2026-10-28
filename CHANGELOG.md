# Changelog

Changes in this copy, newest first. Upstream's own history is in
[codepdbh/nfsmw-android](https://github.com/codepdbh/nfsmw-android); this copy starts from its 0.8.2.

## Unreleased

- **For measuring:** fragments per pixel shader on Mali (precise occlusion queries per draw), `nfsmw_args.txt` in
  the game folder for extra `--cvar=value` arguments, and `tools/android/extraer_spirv.py` to pull the shader
  library's SPIR-V for Arm's offline compiler. Run on the tablet.

- **Faster on Mali (step 9).** Clears are done by the next pass with `loadOp = CLEAR` instead of writing the whole
  image to memory and loading it back (race clears 1.2-3.3 → 0.16 ms of GPU per frame). **Screen scaling → Display
  hardware** (built, not yet measured): the game's surface stays at the internal resolution's width and the display
  hardware scales it to the panel, instead of the GPU drawing every frame again at 2112x1320.
- **Car reflections → Off** (the new default): no reflection drawn per frame, the rear-view mirror's face included
  (6.2 ms in a race on the tablet); one face is redrawn every 30 frames so the car's reflection still follows the
  surroundings. The road reflection is skipped again when nothing reads it (its address was wrong at 1280x720).
- **45 FPS** and **steadier pacing**: the game's frame clock follows the panel's real refresh when the panel's rate
  is a whole multiple of the limit (45 on 90 Hz, 60 on 60 Hz).
- **Movies play** with English text (the game asked for English movie files the Spanish disc does not have).
- **Touch pad HIDE button**, and **Back twice** asks to leave the game and returns to the launcher.
- **First build of this copy** (2026-10-28), on a Windows PC with 7.8 GB of RAM: rexglue, the code generator (265
  files, 139 MB of C++), and the `.dev` APK (31 MB, `libmain.so` 40 MB) in 47 minutes; it runs on the tablet. Fixes
  on the way: the host build on Windows (Visual Studio Build Tools, `-march=x86-64-v2`,
  `socklen_t`), the ETC2 cache test on Windows, and `-Pnfsmw.nativeJobs` / `-Pnfsmw.ltoJobs` to cap the build's
  parallelism on a PC with little RAM. Crash reports go to this copy's owner.
- **English.** The launcher (options, dialogs, saved-game manager, touch-control editor and hints, diagnostics,
  shader builder messages) is in English, and the game's text and menus default to English (the PAL disc carries
  it); speech and movies stay in the disc's language. Edition names in English.
- **16:10.** Internal resolutions 1056x660 (exactly half of the tablet's 2112x1320) and 1280x800, and **Aspect ratio →
  The screen's** for any screen: the game draws at the screen's proportion with no bars and no stretched 3D. Narrower
  than 16:9 it shows more above and below (the game's own projection); wider, more at the sides (as before). The HUD is
  still stretched (prototype). Not yet run on the tablet.
- **ADPF and thermal step-down** (Android 13+ and 12+). The game's three hot threads report each frame's work to
  Android against the frame budget, so it can set the clocks for it. Before the tablet throttles, the shadows step
  down (one map, then no cast shadows) and come back once it has cooled. Launcher: **Thermal control**, **System
  performance hints (ADPF)**, **Game cores**. Not yet run on the tablet.
- **Half precision fixed for shadows and highlights.** The 16-bit color math (`nfsmw_nativo_media_precision`, off by
  default) also relaxed the shadow map, which holds depth: shadow acne. Its samples, and pow/exp/log with what feeds
  them, now stay at 32 bits. Launcher: **Color precision**. Tested on compiled shaders off-device; not yet on
  the tablet.
- **ETC2 textures on GPUs without BC** (Mali, PowerVR; Vulkan). The game's BC1-5 textures stay compressed as ETC2
  RGB8A1, ETC2 RGBA8 and EAC R11/RG11, the size of BC, instead of being decoded to 4 bytes a texel (2 for BC1). They
  are transcoded in the background on the cores the game does not use and kept in a disk cache: the first time a
  texture is seen it is still decoded, then it comes from the cache. Launcher: **Textures on GPUs without BC**. Checked
  off-device: every block decodes to the encoder's error, Mesa's ETC2 decoder agrees on every texel, 38-50 dB PSNR
  against the BC texels on synthetic textures (30-33 on pure noise). Not yet run on the tablet.
- **Repository cleanup.** New README and changelog for this copy; one English build guide
  ([docs/building-android.md](docs/building-android.md)). Removed upstream's Switch build files (`pgo/`, `mesa/`,
  `tools/switch/`, `tools/build.ps1`, edition PGO tools), its release notes, planning and tester docs, and the Spanish
  README.
- **Local builds.** `build_android.ps1` and `build_android.sh` pass their arguments to Gradle;
  `-Pnfsmw.applicationIdSuffix=.dev` builds `com.nfsmw.android.dev` beside the published app. The GitHub workflow is
  manual only, since it cannot build without the generated code. [CLAUDE.md](CLAUDE.md) is the runbook.
- **Keyboard.** The launcher's KEYBOARD section turns on the SDK's keyboard driver and remaps its 24 buttons; the
  touch controls hide while the keyboard plays. The game screen is no longer recreated when a controller or keyboard
  connects. Not yet tested on the tablet.
- **Fast cores.** The game's threads are kept off the Cortex-A55s on the Exynos 1380, whose clock is close to the
  A78s', by MIDR part number or CPU capacity. Tested on known SoC layouts; not yet on the tablet.

## 0.8.2 (upstream)

Edition-specific Android builds, strict XEX-to-module selection and edition-bound shader caches. Earlier upstream
releases: saved-game manager and graphics presets (0.8.1), OpenGL ES backend and experimental LAN (0.8.0), custom
Vulkan drivers and four-descriptor-set GPUs (0.5.4), Vulkan 1.1 without `shaderInt64` (0.3.7).
