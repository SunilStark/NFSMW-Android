# Changelog

Changes in this copy, newest first. Upstream's own history is in
[codepdbh/nfsmw-android](https://github.com/codepdbh/nfsmw-android); this copy starts from its 0.8.2.

## Unreleased

- **ADPF and thermal step-down** (Android 13+ and 12+). The game's three hot threads report each frame's work to
  Android against the frame budget, so it can set the clocks for it. Before the tablet throttles, the shadows step
  down (one map, then no cast shadows) and come back once it has cooled. Launcher: **Control térmico**, **Ayuda al
  sistema (ADPF)**, **Núcleos del juego**. Not yet run on the tablet.
- **Half precision fixed for shadows and highlights.** The 16-bit color math (`nfsmw_nativo_media_precision`, off by
  default) also relaxed the shadow map, which holds depth: shadow acne. Its samples, and pow/exp/log with what feeds
  them, now stay at 32 bits. Launcher: **Precisión del color**. Tested on compiled shaders off-device; not yet on
  the tablet.
- **ETC2 textures on GPUs without BC** (Mali, PowerVR; Vulkan). The game's BC1-5 textures stay compressed as ETC2
  RGB8A1, ETC2 RGBA8 and EAC R11/RG11, the size of BC, instead of being decoded to 4 bytes a texel (2 for BC1). They
  are transcoded in the background on the cores the game does not use and kept in a disk cache: the first time a
  texture is seen it is still decoded, then it comes from the cache. Launcher: **Texturas en GPUs sin BC**. Checked
  off-device: every block decodes to the encoder's error, Mesa's ETC2 decoder agrees on every texel, 38-50 dB PSNR
  against the BC texels on synthetic textures (30-33 on pure noise). Not yet run on the tablet.
- **Repository cleanup.** New README and changelog for this copy; one English build guide
  ([docs/building-android.md](docs/building-android.md)). Removed upstream's Switch build files (`pgo/`, `mesa/`,
  `tools/switch/`, `tools/build.ps1`, edition PGO tools), its release notes, planning and tester docs, and the Spanish
  README.
- **Local builds.** `build_android.ps1` and `build_android.sh` pass their arguments to Gradle;
  `-Pnfsmw.applicationIdSuffix=.dev` builds `com.nfsmw.android.dev` beside the published app. The GitHub workflow is
  manual only, since it cannot build without the generated code. [CLAUDE.md](CLAUDE.md) is the runbook.
- **Keyboard.** The launcher's TECLADO section turns on the SDK's keyboard driver and remaps its 24 buttons; the
  touch controls hide while the keyboard plays. The game screen is no longer recreated when a controller or keyboard
  connects. Not yet tested on the tablet.
- **Fast cores.** The game's threads are kept off the Cortex-A55s on the Exynos 1380, whose clock is close to the
  A78s', by MIDR part number or CPU capacity. Tested on known SoC layouts; not yet on the tablet.

## 0.8.2 (upstream)

Edition-specific Android builds, strict XEX-to-module selection and edition-bound shader caches. Earlier upstream
releases: saved-game manager and graphics presets (0.8.1), OpenGL ES backend and experimental LAN (0.8.0), custom
Vulkan drivers and four-descriptor-set GPUs (0.5.4), Vulkan 1.1 without `shaderInt64` (0.3.7).
