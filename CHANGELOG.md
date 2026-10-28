# Changelog

Changes in this copy, newest first. Upstream's own history is in
[codepdbh/nfsmw-android](https://github.com/codepdbh/nfsmw-android); this copy starts from its 0.8.2.

## Unreleased

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
