# Working on this repository (local session, Windows PC with the tablet attached)

This is a private copy of [codepdbh/nfsmw-android](https://github.com/codepdbh/nfsmw-android): Need for Speed: Most
Wanted (Xbox 360), statically recompiled to C++ with ReXGlue and built as an ARM64 Android APK. It is tuned here for
one device, the owner's **Samsung Galaxy Tab S10 Lite**, played with a **Bluetooth controller** and a **keyboard**.
The goal is a steady **60 FPS first, then resolution**.

Remotes: `origin` is this repository (`SunilStark/NFSMW-Android`); add the original as `upstream`
(`git remote add upstream https://github.com/codepdbh/nfsmw-android`) and merge `upstream/main` to take its changes.
This copy removed upstream's Switch build files (`pgo/`, `mesa/`, `tools/switch/`, `tools/build.ps1`), its release
notes and planning docs, and replaced `README.md` and `CHANGELOG.md`. A merge that touches those files stops with
modify/delete or content conflicts: keep this copy's side (`git rm` the deleted ones), and move anything useful from
upstream's release notes into `CHANGELOG.md` under an "Upstream" line.

## Rules

- **Nothing from the game is ever committed or pushed**: not `default.xex`, the disc's files, `app/generated/`,
  `app/src/copias_literales/`, `nfsmw_shaders*.nfsp`, nor any APK or `.so` built from them. `.gitignore` covers them;
  check `git status` before every commit. The repository is private, but it is still a copy on GitHub.
- GitHub Actions cannot build this (the build needs the generated code): `.github/workflows/android.yml` is manual
  only. **Builds and tests happen on this PC and on the tablet.**
- Follow the code's own style: identifiers, cvars and log text in Spanish, comments in English. Commit subjects start
  with the area (`android: ...`), the body says why and what was checked, including whether it ran on the tablet.
- Say plainly what was measured on the tablet and what was not. Never present a build that compiled as one that works.

## The tablet

From Samsung's specifications and the owner's screenshots (2112x1320). **Confirm on the first session** with the
commands below and correct this section:

- Exynos 1380: 4 Cortex-A55 at 2.0 GHz + 4 Cortex-A78 at 2.4 GHz; Mali-G68 MP5 (no BC textures, 4 descriptor sets).
- 6 or 8 GB RAM; 10.9" 2112x1320 (16:10), 90 Hz.

```powershell
New-Item -ItemType Directory -Force out | Out-Null
adb devices -l
adb shell getprop ro.product.model; adb shell getprop ro.soc.model; adb shell getprop ro.build.version.release
adb shell "cat /sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_max_freq; cat /sys/devices/system/cpu/cpu*/cpu_capacity"
adb shell dumpsys display | Select-String -Pattern 'supportedModes|refreshRate'
adb shell cmd gpu vkjson > out/tablet-vk.json   # descriptor indexing, maxBoundDescriptorSets, BC/ETC2/ASTC
adb shell cat /proc/meminfo | Select-Object -First 2
```

The Vulkan renderer needs the descriptor-indexing features (README, Requirements). Without them the launcher's
**API gráfica → OpenGL ES** is the path.

## One-time setup

Install, and check each with `--version` or its path:

- Android SDK (Android Studio, or the command-line tools) with `platform-tools`, `platforms;android-35`,
  `build-tools;35.0.0`, `ndk;28.2.13676358` and `cmake;3.30.5` (`sdkmanager "<package>"`; the version Gradle asks
  for in `android/app/build.gradle`). `ANDROID_HOME` set, or the SDK at `%LOCALAPPDATA%\Android\Sdk`.
- JDK 17 or later (Android Studio's `jbr` works; `build_android.ps1` finds it).
- Python 3.10+, Git with Git Bash (for `tools/codegen.sh`), and for the code generator on the PC: LLVM clang 20 or
  later, CMake 3.25+ and **one** Ninja (see the three CMake traps in `docs/building-android.md`: run CMake from
  PowerShell, and long builds with `Start-Process` and a log file).
- About 16 GB of RAM: the generated code is about 144 MB of C++, and the ThinLTO link is heavy. If the link runs out
  of memory, lower `--thinlto-jobs` in `android/app/src/main/cpp/CMakeLists.txt`.

## Build, from a fresh clone

The same steps, with more detail, are in `docs/building-android.md`; keep both in step when one changes.

1. **Third-party sources** (the SDK's `sdk/thirdparty` only holds the files this port changed):
   `python tools/fetch_thirdparty.py --cache-dir out/evolved2/dependency-cache`
2. **The game's executable** goes to `assets/game_root/default.xex` (git ignores `assets/`). The owner's copy is
   probably on the tablet already, where the published app reads it:
   `adb pull /sdcard/nsfmw-androidevolved/default.xex assets/game_root/default.xex`.
   Ask the owner otherwise (an extracted folder, or an ISO: `python tools/fase1_extraer.py <iso> -o assets/game_root`).
   Check the edition: `(Get-FileHash assets/game_root/default.xex).Hash` must be
   `AAD15FC218D034DE3131F746E0B763DD8049A40DD676EFC87667FCAF4FC72694` (PAL Spain), the edition `app/` is built for.
   For another edition, stop and read `docs/ANDROID_EDITIONS.md` with the owner before going on.
3. **The code generator** for the PC:
   `cmake -S sdk -B out/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
   then `cmake --build out/host --target rexglue`. The executable lands under `sdk/out/` (the SDK sets its output
   folder): `Get-ChildItem sdk/out,out/host -Recurse -Filter rexglue.exe`.
4. **Translate the game to C++**, from Git Bash at the repository root:
   `REXGLUE=<path to rexglue.exe> PYTHON=py tools/codegen.sh app`. It writes `app/generated/default` and
   `app/src/copias_literales`; errors go to `app/codegen.log`. Run it again only when the code generator, the
   manifests or the hooks change, or after merging `upstream`.
5. **The APK**, from PowerShell: `.\build_android.ps1 '-Pnfsmw.applicationIdSuffix=.dev'` (the quotes matter:
   PowerShell splits `-P...` at its dots). The suffix gives the build its own package (`com.nfsmw.android.dev`) and
   launcher name, so it installs **beside** the published app instead of replacing it; both read the same game
   folder. Without the suffix it replaces the published package, which has another signature: Android then asks to
   uninstall it, and that deletes its saves (export them first from **Partidas guardadas**).
   Output: `android\app\build\outputs\apk\release\app-release.apk`.
6. **Install and run**: `adb install -r android\app\build\outputs\apk\release\app-release.apk`, open
   **NFS Most Wanted (dev)**, allow **All files access**, and play. The first run of a new package may rebuild the
   shader library on the tablet. Ignore the launcher's update offers on this build: they point at the published app.

## Checking a build on the tablet

- Log: `adb logcat -s NFSMW-rex NFSMW` (the game's own log goes to logcat under `NFSMW-rex`; the launcher's
  **Enviar crash o log** makes a ZIP with the native logs too).
- Fast cores: at start the log lists each core (`[android] nucleos (cpu:reloj/capacidad/parte): ...`) and should then
  say `juego en los nucleos 4,5,6,7` on the Exynos 1380.
- Keyboard: launcher **TECLADO → Jugar con teclado**, then **Teclas** to remap (for example W to RT and S to LT).
  Connect the Bluetooth controller and the keyboard **in the middle of a race**: the game must carry on.
- Frame rate, on the same race and route each time, with the same settings and the tablet at the same temperature:

```powershell
adb shell dumpsys SurfaceFlinger --timestats -enable -clear   # then play 2 minutes
adb shell dumpsys SurfaceFlinger --timestats -dump > out/timestats.txt
adb shell dumpsys thermalservice | Select-String -Context 0,4 'Current temperatures'
```

Record the average FPS, the 95th/99th percentile frame times, the temperature after 15 minutes and the battery drain
per hour, before and after each change.

Asset-free tests (no game needed): the commands at the end of `docs/building-android.md`
(`SaveArchiveTest`, `GraphicsPresetsTest`, `KeyboardBindingsTest`, `tools/tests/android_nucleos_test.cpp`).

## Starting settings for the tablet

Launcher options: API gráfica **Vulkan** (if the GPU has descriptor indexing), Resolución interna **1024x576** first
(the code measured that 640x360 does not lower the scene's resolution), Límite de FPS **60** (the launcher then picks
the display's 60 Hz mode), Sombras **cada 2 fotogramas**, Reflejos del coche **Medios**, Antialiasing and Filtro off,
Estabilidad gráfica **Automática**. On the tablet: the app's battery use **Unrestricted**, Game Booster on.

## Done in this copy

- `android: keep the game off little cores that clock near the big ones`: the fast-core mask
  (`android/app/src/main/cpp/android_rendimiento.cpp`, choice in `android_nucleos.h`) left every core on the Exynos
  1380; it now excludes the A55s by MIDR or capacity. Tested on known layouts, not yet on the tablet.
- `android: keyboard play and key binds in the launcher; no restart on hot-plug`: `GameActivity`'s configChanges
  as SDL's own, the launcher's TECLADO section (`KeyboardBindings.java`), touch controls hidden while the keyboard
  plays. Compiled and tested off-device, not yet on the tablet.
- The `.dev` package suffix, `build_android.ps1` passing its arguments to Gradle, and the manual-only workflow.
- Cleanup: this copy's own `README.md`, `CHANGELOG.md` and `docs/building-android.md`; upstream's Switch build files
  and release/planning docs removed. The Switch background docs (`docs/platform-notes.md`, `mesa.md`, `editions.md`,
  `performance-history.md`, `audio-and-video.md`) stay because comments in `sdk/` and `app/` point at them.

## Next, in order (each measured on the tablet before and after)

1. **Confirm the two changes above on the tablet**: the core log line, keyboard play and remapping, controller and
   keyboard connected mid-race.
2. **Textures for Mali**: with no BC support, BC1-5 are decoded on the CPU to 16 or 32 bits a texel
   (`app/src/nfsmw_nativo_dibujos.cpp`, the `bc_cpu_` path around `formato_host`). Transcode BC1 to ETC2 RGB8 and
   BC3 to ASTC 4x4 or ETC2 EAC RGBA8 (whichever the tablet's `vkjson` lists) instead, and cache the result on disk.
3. **Half precision**: try `nfsmw_nativo_media_precision = true` (off by default; RelaxedPrecision in the pixel
   shaders), and look for banding or glitches in a full race.
4. **ADPF and thermals**: an `APerformanceHint` session (API 33+) for the game, ring and render threads with the
   frame budget, and `AThermal_getThermalHeadroom` to step down (resolution, shadows) before the tablet throttles;
   this can replace the hard pinning of `android_rendimiento.cpp` if it measures better.
5. **16:10**: the tablet is 2112x1320. `nfsmw_proporcion_pantalla` (`app/src/nfsmw_render_targets.cpp`,
   `nfsmw_panoramico.cpp`) only widens past 16:9 (Hor+); add the narrower case (Vert+) and internal sizes such as
   1056x660 (exactly half the screen) and 1280x800, to replace the launcher's stretch or bars.
6. **45 FPS** for the 90 Hz display (half its rate) where 60 does not hold: `nfsmw_limite_fps` allows 30/60/90/120.
7. **Game data in the app's own folder** (`getExternalFilesDir`): no shared-storage overhead while the track
   streams, and no All files access. `MainActivity.GAME_FOLDER_NAME` and `GameActivity` hold the path.
8. **A build for this SoC**: `-mcpu=cortex-a78` for the game code (both core types are ARMv8.2: LSE atomics, FP16),
   then PGO from a 15-minute session on the tablet with clang's own profiles (`-fprofile-generate`, then
   `-fprofile-use`); the PGO code in `app/CMakeLists.txt` and `app/src/nfsmw_pgo.cpp` is the Switch's GCC path and
   does not apply, and its profiles were removed from this copy.
9. **Renderer on Mali**: load/store operations (clear or don't-care) and transient depth attachments; confirm with
   Arm Performance Studio (Streamline) counters over adb.
