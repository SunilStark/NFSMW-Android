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

## Status at handover (2026-10-28, first local session)

**Runs on the tablet; the work is now GPU performance (step 9).** The first local session (the owner's Windows PC)
built everything from this copy, installed `com.nfsmw.android.dev` (no published app on the tablet; the game folder,
from the owner's ISO, is in `/sdcard/nsfmw-androidevolved/`) and played it with the owner. Confirmed on the tablet:
the fast cores (`juego en los nucleos 4,5,6,7`), Vulkan on the Mali-G68 with ETC2 (61 dB on the first textures),
the ADPF session (reports 25-35 us, 0 rejected) and the thermal reader, movies with sound, HIDE, Back twice.

The owner asked to change the order: steps 6 and 9 came before 2-5 and step 1's script. **Not done yet: "Next"
step 1's `comprobar_tableta.ps1`, and the measured comparisons of steps 2-5.** Measured so far, race GPU time per
frame from the renderer's own C2 report (`[nativo] C2: GPU por Swap`): 46.5 ms in the first race; 26.7-29.8 ms after
the reflection fixes and 9a (not the same route, so no clean before/after for the total). Breakdown and the plan for
what is left are under "Next", step 9. The tablet reaches thermal level 1 within a few minutes of play.

Measuring on this PC: record the whole log while the owner plays
(`cmd /c "adb logcat -v time -s NFSMW-rex:V NFSMW:V >> out\tablet\<name>.log"` in a Terminal panel tab; Git Bash in
the background worked once and later exited at once, and a PowerShell background job was cut off after a minute),
then read the C2/C6 reports from it.

On this PC, the asset-free tests pass: the three Java tests in `docs/building-android.md`, and `android_nucleos`,
`texturas_etc`, `tamano_pantalla`, `android_termico`, `spirv_precision`, `texturas_cache_etc` (after a Windows fix).

Earlier, everything after upstream 0.8.2 (`git log --oneline 5507002^..`) was written in a cloud session that had
neither the NDK, nor the game, nor the tablet. The plan's steps 1 to 5 and the English launcher are done in code;
steps 6 to 9 are not started.

What was checked, off-device only:

- The asset-free tests at the end of `docs/building-android.md` pass on Linux: the four Java tests, and
  `android_nucleos`, `android_termico`, `tamano_pantalla`, `texturas_etc` (also under ASan), `texturas_cache_etc`
  (also under TSan) and `spirv_precision` (shaders from glslang; spirv-val passes). `build.js` passes `node --check`.
- Every changed C++ file passes `clang++ -std=c++23 -fsyntax-only` on Linux, with stub headers for the window
  system and the generated shader headers. `android/app/src/main/cpp/android_ritmo.cpp` was checked the same way
  with a stub `android/*.h`.
- The Java sources parse against `android-all` (only SDL, androidx, `R` and `BuildConfig` were missing).

Never compiled before the first local session; all of them now compile with NDK 28.2 (ADPF and thermal symbols,
ETC2 cache and the new log lines are in `libmain.so`), but none has run yet:

- The `REX_PLATFORM_ANDROID` / `__ANDROID__` branches: the ADPF calls in `app/src/nfsmw_espera_fotograma.cpp` and
  `nfsmw_nativo_sistema.cpp`, the thread pinning in `app/src/nfsmw_texturas_cache_etc.cpp`, and
  `android_ritmo.cpp` against the NDK's real headers (API 33/34 symbols are loaded with dlsym, minSdk 26).
- The Java against SDL and the Android SDK (Gradle, R8), and `android/app/src/main/cpp/CMakeLists.txt` and
  `app/CMakeLists.txt` with the new sources.
- `tools/tests/texturas_etc_vulkan_test.cpp`, built with the NDK for the tablet.

### First local session, in order

1. One-time setup and the build below, from a fresh clone. Fix what does not compile; the files above are the
   likely places. Run the asset-free tests on the PC too.
2. Install the `.dev` APK and do "Next" step 1 with the owner (`tools/android/comprobar_tableta.ps1`); correct
   "The tablet" from its output.
3. Then "Next" steps 2 to 5, each measured against its switch turned off. These are on by default in a fresh install:
   **Textures on GPUs without BC → ETC2**, **Thermal control → Automatic**, **System performance hints (ADPF) → On**,
   **Game cores → Fast cores only**, **Text language → English**. Off by default: **Color precision** (32 bits), the
   16:10 sizes (**Internal resolution** stays 1280x720) and **Aspect ratio** (16:9). If the first run crashes or draws
   wrongly, turn the default-on switches off one at a time to find the change, read the log, and fix that change rather
   than leaving it off.
4. Record the results in "Done in this copy" and `CHANGELOG.md`, and say in each commit what ran on the tablet.

### Open points

- Answered 2026-10-28: the tablet has 6 GB and Android 16 / One UI 8.5 (see "The tablet"). The published app is
  **not** installed: the game came from the owner's ISO (`Desktop\Need for Speed - Most Wanted (Spain)\...iso`,
  PAL Spain hash confirmed) and was copied to the tablet. Crash reports now go to the owner (`Diagnostics.java`).
- The owner keeps a list of everything installed for this project outside the repository
  (`Downloads\Claude Projects\NFSMW-cleanup-list.md`): add to it whatever a session downloads or installs.
- Still in Spanish: log text (by rule), and the cvar descriptions in the in-game settings overlay. Speech and movies
  are Spanish on the PAL Spain disc and cannot change.
- The OpenGL ES backend still decodes BC textures (no ETC2 there), and the HUD is stretched at 16:10, as it is at
  wide proportions.

## Rules

- **Nothing from the game is ever committed or pushed**: not `default.xex`, the disc's files, `app/generated/`,
  `app/src/copias_literales/`, `nfsmw_shaders*.nfsp`, nor any APK or `.so` built from them. `.gitignore` covers them;
  check `git status` before every commit. The repository is private, but it is still a copy on GitHub.
- GitHub Actions cannot build this (the build needs the generated code): `.github/workflows/android.yml` is manual
  only. **Builds and tests happen on this PC and on the tablet.**
- Follow the code's own style: identifiers, cvars and log text in Spanish, comments in English. The launcher's UI text
  is English in this copy (the owner's language); upstream's is Spanish, so merges conflict there: keep English. Commit
  subjects start with the area (`android: ...`), the body says why and what was checked, including whether it ran on the
  tablet.
- Say plainly what was measured on the tablet and what was not. Never present a build that compiled as one that works.

## The tablet

From Samsung's specifications and the owner's screenshots (2112x1320). **Confirm on the first session** with the
commands below and correct this section:

- Exynos 1380: 4 Cortex-A55 at 2.0 GHz + 4 Cortex-A78 at 2.4 GHz; Mali-G68 MP5 (no BC textures, 4 descriptor sets).
- 10.9" 2112x1320 (16:10), 90 Hz.
- Read over adb on 2026-10-28: SM-X400 (`gts10litewifieea`), `ro.soc.model` s5e8835 (Exynos 1380), **6 GB**
  (MemTotal 5.84 GB), **Android 16 (API 36), One UI 8.5**, build X400XXS8CZI2. The rest is confirmed in step 1.

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
**Graphics API → OpenGL ES** is the path.

## One-time setup

Install, and check each with `--version` or its path:

- Android SDK (Android Studio, or the command-line tools) with `platform-tools`, `platforms;android-35`,
  `build-tools;35.0.0`, `ndk;28.2.13676358` and `cmake;3.30.5` (`sdkmanager "<package>"`; the version Gradle asks
  for in `android/app/build.gradle`). `ANDROID_HOME` set, or the SDK at `%LOCALAPPDATA%\Android\Sdk`.
- JDK 17 or later (Android Studio's `jbr` works; `build_android.ps1` finds it).
- Python 3.10+, Git with Git Bash (for `tools/codegen.sh`), and for the code generator on the PC: LLVM clang 20 or
  later, CMake 3.25+ and **one** Ninja (see the three CMake traps in `docs/building-android.md`: run CMake from
  PowerShell, and long builds with `Start-Process` and a log file). The SDK's `cmake;3.30.5` brings a Ninja: use it.
- Visual Studio 2022 Build Tools, C++ workload (MSVC + Windows SDK): clang on Windows needs them. Load `vcvars64.bat`
  in the shell, then put LLVM's `bin` and the SDK's CMake first on `PATH`.
- About 16 GB of RAM: the generated code is about 144 MB of C++, and the ThinLTO link is heavy. With less, cap the
  build with `-Pnfsmw.nativeJobs=3 -Pnfsmw.ltoJobs=2` (see `docs/building-android.md`, step 5); the owner's PC has
  7.8 GB and builds with those in 47 minutes.
- Gradle cannot open its loopback socket from an agent's sandboxed shell, even with the sandbox off: run it from the
  Terminal panel (a script with `Start-Process gradlew.bat -WorkingDirectory android ...` and a log file).

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
   `cmake -S sdk -B out/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2`
   (the flags are the SDK presets'; without them the SSSE3 code in `sdk/src/core/memory.cpp` does not compile), then `cmake --build out/host --target rexglue`. The executable lands under `sdk/out/` (the SDK sets its output
   folder): `Get-ChildItem sdk/out,out/host -Recurse -Filter rexglue.exe`.
4. **Translate the game to C++**, from Git Bash at the repository root:
   `REXGLUE=<path to rexglue.exe> PYTHON=py tools/codegen.sh app`. It writes `app/generated/default` and
   `app/src/copias_literales`; errors go to `app/codegen.log`. Run it again only when the code generator, the
   manifests or the hooks change, or after merging `upstream`.
5. **The APK**, from PowerShell: `.\build_android.ps1 '-Pnfsmw.applicationIdSuffix=.dev'` (the quotes matter:
   PowerShell splits `-P...` at its dots). The suffix gives the build its own package (`com.nfsmw.android.dev`) and
   launcher name, so it installs **beside** the published app instead of replacing it; both read the same game
   folder. Without the suffix it replaces the published package, which has another signature: Android then asks to
   uninstall it, and that deletes its saves (export them first from **Saved games**).
   Output: `android\app\build\outputs\apk\release\app-release.apk`.
6. **Install and run**: `adb install -r android\app\build\outputs\apk\release\app-release.apk`, open
   **NFS Most Wanted (dev)**, allow **All files access**, and play. The first run of a new package may rebuild the
   shader library on the tablet. Ignore the launcher's update offers on this build: they point at the published app.

## Checking a build on the tablet

`tools/android/comprobar_tableta.ps1` runs the checks of step 1 below; these are the pieces it uses.

- Extra arguments without a launcher option: put `--cvar=value` lines in `nfsmw_args.txt` in the game folder
  (`adb push` from Git Bash needs `MSYS_NO_PATHCONV=1`, or the `/sdcard/...` path is rewritten); delete it after.
- Log: `adb logcat -s NFSMW-rex NFSMW` (the game's own log goes to logcat under `NFSMW-rex`; the launcher's
  **Send crash or log** makes a ZIP with the native logs too).
- Fast cores: at start the log lists each core (`[android] nucleos (cpu:reloj/capacidad/parte): ...`) and should then
  say `juego en los nucleos 4,5,6,7` on the Exynos 1380.
- Keyboard: launcher **KEYBOARD → Play with keyboard**, then **Keys** to remap (for example W to RT and S to LT).
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

Launcher options: Graphics API **Vulkan** (if the GPU has descriptor indexing), Internal resolution **1024x576**
first (the code measured that 640x360 does not lower the scene's resolution), FPS limit **60** (the launcher then
picks the display's 60 Hz mode), Shadows **Every 2 frames**, Car reflections **Medium**, Antialiasing and Image
filter off, Graphics stability **Automatic**, Text language **English** (the default). On the tablet: the app's
battery use **Unrestricted**, Game Booster on.

## Done in this copy

- `android: keep the game off little cores that clock near the big ones`: the fast-core mask
  (`android/app/src/main/cpp/android_rendimiento.cpp`, choice in `android_nucleos.h`) left every core on the Exynos
  1380; it now excludes the A55s by MIDR or capacity. Tested on known layouts, not yet on the tablet.
- `android: keyboard play and key binds in the launcher; no restart on hot-plug`: `GameActivity`'s configChanges
  as SDL's own, the launcher's KEYBOARD section (`KeyboardBindings.java`), touch controls hidden while the keyboard
  plays. Compiled and tested off-device, not yet on the tablet.
- The `.dev` package suffix, `build_android.ps1` passing its arguments to Gradle, and the manual-only workflow.
- Cleanup: this copy's own `README.md`, `CHANGELOG.md` and `docs/building-android.md`; upstream's Switch build files
  and release/planning docs removed. The Switch background docs (`docs/platform-notes.md`, `mesa.md`, `editions.md`,
  `performance-history.md`, `audio-and-video.md`) stay because comments in `sdk/` and `app/` point at them.
- `tools/android/comprobar_tableta.ps1`, the checks of step 1 (not run yet).
- `android: keep BC textures compressed as ETC2 on GPUs without BC` (step 2 below, Vulkan only): BC1 to ETC2
  RGB8A1, BC2/3 to ETC2 RGBA8, BC4/5 to EAC R11/RG11 (`app/src/nfsmw_texturas_etc.h`), transcoded in the background
  on the little cores into a disk cache (`app/src/nfsmw_texturas_cache_etc.cpp`, under the game's cache folder,
  `texturas_etc2_v1/`); a texture is decoded as before the first time, ETC2 from then on. cvars
  `nfsmw_nativo_bc_a_etc2` (launcher: **Textures on GPUs without BC**), `nfsmw_nativo_etc2_cache_mb`,
  `nfsmw_nativo_etc2_hilos`; the SDK now enables `textureCompressionETC2`. Checked off-device (encoder, cache, and
  Mesa's ETC2 decoder against ours on every texel); not yet on the tablet.
- `android: half precision keeps the shadow map and pow at 32 bits` (step 3 below): `nfsmw_nativo_media_precision`
  relaxed every result computed from a texture, the shadow map too, which the game samples as an ordinary texture
  and compares with the pixel's depth (at 16 bits, steps of 0.0005 near 1.0: shadow acne). Now its samples, found by
  tfetch2DSombraMin's marker, stay at 32 bits, and so do pow/exp/log with what feeds them; a shader whose shadow
  samples cannot be found is left alone. Launcher: **Color precision**. Tested on glslang-compiled shaders
  shaped like the library's (`tools/tests/spirv_precision_test.cpp`; spirv-val passes); not yet on the tablet.
- `android: ADPF session and thermal step-down` (step 4 below): `android/app/src/main/cpp/android_ritmo.cpp`. The
  preparer, executor and ring threads in an `APerformanceHint` session (API 33+); each frame, at the ring's Swap, the
  busiest one's CPU time is reported against 1/`nfsmw_limite_fps`. A thread reads `AThermal_getThermalHeadroom` (10 s
  ahead) every 2 s and sets a floor under the 30 FPS guard's shadow steps (0.85: one map, 0.95: no cast shadows;
  back one level per minute under 0.70; policy in `android_termico.h`). Loaded with dlsym (minSdk 26). Launcher:
  **Thermal control**, **System performance hints (ADPF)**, **Game cores**. Policy tested off-device; not on the tablet.
- `android: 16:10 internal sizes and the narrower screen proportion` (step 5 below): `nfsmw_proporcion_pantalla`
  below 16:9 draws narrower at the same height, and `nfsmw_resolucion_interna` gains 1056x660 and 1280x800, both
  16:10 whatever the proportion (sizes in `app/src/nfsmw_tamano_pantalla.h`). The camera is left alone when
  narrower: the game's projection already shows more above and below (Vert+); the presenters show that front
  buffer at its own proportion. Launcher: the two sizes and **Aspect ratio → The screen's**. Sizes
  tested off-device; not on the tablet.
- `android: launcher in English; game text in English by default`: every launcher string, edition names and the
  shader builder's messages; `nfsmw_idioma` defaults to 0 (English). cvar values, preference keys and log text are
  unchanged, so saved settings carry over (a saved language choice too: set it to English once on an existing
  install). Java tests pass; not built.

- First local session (2026-10-28), each commit says what ran on the tablet:
  - `tools: build the code generator on Windows with clang`, `android: close a damaged ETC2 cache file before
    deleting it`, `android: first build of this copy; cap the native build's parallelism` (`-Pnfsmw.nativeJobs`,
    `-Pnfsmw.ltoJobs`), `android: crash reports go to this copy's owner`.
  - `android: movies in the disc's language when the text is English`: with English text no movie played (the game
    asked for `*_ENGLISH_pal.wmv`); a hook on the movie path builder `sub_82282E98` asks for the edition's
    language. Confirmed by the owner (speech and music).
  - `android: HIDE button on the touch pad`, `android: Back twice asks to leave the game and returns to the
    launcher` (`RestartActivity` in its own process). Both confirmed by the owner.
  - `android: the panel's vsync as the game's vblank, and 45 FPS (step 6)`: `android_vsync.cpp`. Pacing confirmed in
    the log; at the 45 limit races run 42-44.6 FPS and the owner finds them smooth (`out/tablet/sesion9.log`).
  - `android: reflections nothing reads, and a still reflection that follows the car`: Car reflections → Off (the
    default) also unpins the mirror face (6.2 ms); one face redrawn every 30 frames; the road reflection's EDRAM
    address is learned (0x07647000 at 1280x720 on the tablet, not 0x07C5A000).
  - `android: clears done by the next pass with loadOp = CLEAR (step 9a)`: clears 1.2-3.3 → 0.16 ms per frame, no
    DIFERENCIA, nothing wrong on screen.
  - `android: display hardware scales the image; brightness, color filter and shadow options (step 9c)` and
    `android: smooth shadow edges on the cars only`: confirmed by the owner (fills the screen, brighter, no banding,
    smooth shadows). Race at 1024x576 / 45 FPS limit: 41-44 FPS, GPU 22-25 ms per frame.
  - `android: fragments per pixel shader on Mali, and extra arguments from the game folder`: Mali has no
    pipeline statistics, so `--nfsmw_nativo_estadisticas_por_dibujo_s=N` now counts each draw's fragments with
    precise occlusion queries (draws inside the game's own occlusion queries are skipped). `nfsmw_args.txt` in the
    game folder: each `--cvar=value` line is added to the game's arguments (for measuring without a launcher
    option). `tools/android/extraer_spirv.py` writes the shader library's SPIR-V (`ps_NNN.spv`) for malioc. Race
    on the tablet (`out/tablet/sesion7.log`): scene 12.7-14.4 ms of GPU, 3.0-3.7 M fragments and ~770 draws per
    frame; PS n108 37-40 % of them (~200 draws), n29 12-13 % (one draw), n57 11-14 % (~220 draws), n131 7-10 %,
    n24 5-6 % (one draw), n90 4-6 %, n101 4-5 %. Road reflection 1.2-1.7 ms, 1.2 M fragments (PS n63 ~53 %, n19
    ~40 %, one draw).

## Next, in order (each measured on the tablet before and after)

The owner reordered: 6 and 9 are under way, 1-5 still to measure. Step 9 status (race GPU ms per frame after 9a:
scene 7.9-9.1, copies 4.3, gaps between jobs 3.9-4.1, shadows 3.5-4.4, blur 3.4-3.5, post 2.9-3.4, road reflection
2.8-3.0, small targets 1.9-2.0; 27-30 total, so ~11-13 ms to cut for 60 FPS):
- 9a done (clears in passes). 9c in progress: the GPU is never starved (each job takes ~61 ms of wall time, two
  frames queued; the ring waits 0.04 ms per Swap), so the "gap" is GPU work between jobs, mostly the presenter
  drawing the image again at 2112x1320. Screen scaling → Display hardware puts the surface at 1280x800 and lets the
  display hardware scale; measure the gap with it on and off. Then: the ~4 pass resumes per frame after copies
  (each reloads color and depth), and 9b (store depth only when read).
- Later: 9c done; 9b measured at ~0.3-0.5 ms and skipped by the owner. The scene pass (12.7-14.4 ms) is the cost
  left; its fragments per pixel shader are measured (see "Done"). Next: Arm's Mali Offline Compiler (`malioc`,
  Arm Performance Studio; its installer failed with 1603 on this PC, retry with a `/l*v` log) on
  `out/tablet/spirv/ps_*.spv` for cycles per fragment, times the fragment counts, then work on the costliest
  shaders. The road reflection (**Wet road reflection → Off**) would save ~1.2-1.7 ms.
- malioc done (Arm Performance Studio 2026.5 installed; its MSI needs `EULA=1`, which the owner accepted in its own
  window): `out/tablet/malioc/ps_NNN.txt`. Fragments times the longest-path cycles, over the Mali-G68's 5 cores at
  949 MHz, comes to only ~2 ms of the scene's 12.7-14.4 ms, so the shaders' arithmetic is not where the scene's time
  goes. Most are varying-bound (PS n108: A 0.38, V 1.38, T 0.25 cycles), so 16-bit varyings would save part of that
  ~2 ms at most. The rest (~11 ms) is vertices, tiling, memory traffic or per-draw cost: measure with Streamline's
  hardware counters next. The tablet has no Perfetto GPU counters (`perfetto --query`: only `android.gpu.memory`),
  and it is not rooted (`ro.debuggable=0`), so Streamline needs a debuggable `.dev` build.
- Streamline done (`-Pnfsmw.debuggable=true`, only with the `.dev` suffix; a 1-minute rebuild). Capture: all 150
  Mali-G68 counters for 150 s of a race (`out/tablet/streamline/`; `streamline_me.py --headless` with a
  `configuration.xml` listing every counter in `streamline/gator/daemon/events-Mali-G68_hw.xml`). Beware: the script
  force-stops the app when it starts and again when the capture ends. **The GPU clock in the race never went above
  552 MHz of its 949 MHz** (frequency table 221-949 MHz; inferred from GPU-active cycles per 1 ms bucket: mostly
  351-455 MHz), with the tablet cool (thermal level 0 for the first 90 s). So the 22 ms of GPU per frame are ~10-11
  ms of work at full clock. Not known yet: whether the Interactive governor (`/sys/kernel/gpu/gpu_governor`) sees
  low load or something caps it (Samsung's `com.samsung.android.game.gos` is installed; the governor's limits are
  not readable without root). Next: sample `/sys/kernel/gpu/gpu_clock` and `gpu_busy` during a race; if the load is
  high and the clock low, it is a cap; if the load is low, report the GPU time to ADPF
  (`AWorkDuration_setActualGpuDurationNanos`, API 35) and look for the CPU-GPU waits.
- Sampled in a race (`out/tablet/reloj_gpu1.txt`, 250 samples, `gpu_clock`/`gpu_busy`/`gpu_tmu` every ~0.4 s): 552
  MHz 39 %, 455 MHz 48 %, 351 MHz 12 % of the time; the driver's load 85 % on average at every one of them (71-93 %).
  Never above 552 MHz in either race, even at 90-93 % load; GPU 52-54 C, thermal status 0 then 1. So the clock does
  not follow the load: either a cap at 552 MHz (Samsung's game service or power profile) or the governor's step-up
  threshold is above the ~93 % the game reaches. Next: the owner's Samsung performance setting, then sample again.
- The owner chose **steady 45 FPS and a cooler tablet** over chasing 60 (Gaming Hub is disabled on the tablet; its
  service `com.samsung.android.game.gos` still runs). `android: the GPU's time in the ADPF report`: Android 15+'s
  `APerformanceHint_reportActualWorkDuration2` with the renderer's GPU time per frame (cvar
  `nfsmw_android_adpf_gpu`, on). Race at the 45 limit (`out/tablet/sesion9.log`, `reloj_gpu2.txt`): reports
  accepted (0 rejected, 47-74 us each), GPU 19.2-20.1 ms per frame against 22.22 (80-258 of ~1300 frames over it
  per 30 s), 42-44.6 FPS; the clock barely moved (mean 478 → 495 MHz, 552 MHz 39 → 48 % of samples, 650 MHz 2 %),
  and the previous race was at the 60 limit, so this is no clean A/B. GPU per frame: scene 9.5-11.3 ms, copies 3.0,
  gaps between jobs 2.8-3.4, blur 1.9, scene without depth 1.7-1.8, shadows 1.4-1.6, small targets 1.1. For a
  steady 45, the copies (3 ms) and the gaps are the next work.
- Copies: the lazy scene-depth copy (`nfsmw_nativo_profundidad_perezosa`) never dropped one on the tablet (12,253
  deferred, 0 dropped: the depth target is always rewritten before the composite asks). Dropping it at the rewrite
  (`nfsmw_nativo_profundidad_perezosa_tirar`) tripped the guard in the first race: a real draw sampled 08C90000
  after the drop. So it is **off** by default; the reader is not identified yet. The other repeated 1280x720 copy
  (09758000, its second resolve per frame) was already measured upstream as no saving with the lazy composite.
- Step 8 done in code and run (`android: a build for the Exynos 1380, and clang PGO`), but **not yet measured
  against the build before it**: the first race on it (`out/tablet/sesion12_todo.log`) ran 36-40 FPS with 21-24.6
  ms of GPU per frame, against 42-44.6 FPS and 19-20 ms in sesion9, with the busiest CPU thread the same or a
  little lower (7.2-9.1 ms against 8.7-9.5). A CPU-side build does not change the GPU's work, so the tablet's state
  (after an hour of builds and a 15-minute instrumented session) is the likely cause; the GPU clock was not
  sampled.
- **A/B done** (same source, same race, both runs from a cool tablet: GPU 33-35 C at the start; 2 minutes each with
  `gpu_clock` sampled; `out/tablet/ab_*`): the GPU clock was the same (mean 510 vs 513 MHz), so it is a fair
  comparison. Tuned (exynos1380 + PGO) against generic: FPS 40.8 vs 40.2 (mean of four 30 s windows), busiest CPU
  thread 8.8 vs 10.1 ms (~13 % less), frames over 25 ms 34 % vs 46 %, over 36 ms 1.3 % vs 0.7 % (few frames
  either way), GPU per frame about the same. One run each, so some noise; CPU time and pacing point the same way.
  **The tuned build is the one on the tablet** (`out/apk/dev-a78-pgo.apk`; the generic one is
  `out/apk/dev-generico.apk`). The frame rate is still set by the GPU.
- For everyday play the owner's `nfsmw_args.txt` turns off three measurement-only features that are on by default:
  `--nfsmw_nativo_diag_reutilizar=false` (texture fingerprints, counted only), `--nfsmw_nativo_informe_s=120` (the
  big report every 2 minutes instead of 20 s: measure with longer runs, or set it back in the file) and
  `--nfsmw_nativo_nvk_medir=0` (timing inside NVK, the Switch's driver). `nfsmw_streaming_diag` stays on: it also
  rescues the streamer's prediction. Not measured; expected effect small.
- Audio after minimizing (`android: no crackle after minimizing the app`): the rescue freed packets the paused
  voice still held, and their late ends left the game mixing one packet ahead of the voice, crackly until a
  restart, sometimes. Fixed; four minimize/restore cycles on the tablet: ring back to 1,875 ends = 1,875
  submissions per 10 s, 2 packets on nearly every pass; no crackle heard by the owner.

1. **Confirm the two changes above on the tablet**: the core log line, keyboard play and remapping, controller and
   keyboard connected mid-race. Build and install the `.dev` APK, then run
   `powershell -ExecutionPolicy Bypass -File tools\android\comprobar_tableta.ps1` with the owner at the tablet: it
   records the device's data (correct "The tablet" above from it), checks the core and argument lines in the log and
   that the game screen is not recreated on hot-plug, and asks the owner what only a person can see. Its summary,
   `out/tablet/paso1.txt`, goes into the commit or into "Done in this copy".
2. **Confirm the ETC2 textures on the tablet** (done in code, see above):
   - The GPU decodes our blocks as our decoder does: build and run `tools/tests/texturas_etc_vulkan_test.cpp` over
     adb (its first lines). It must print PASS for the four formats; if not, set **Textures on GPUs without BC →
     Decompressed** and report which format differs.
   - The log at start: `[compatibilidad] BC1: ETC2 RGB8A1 si esta en la cache del disco...` for each BC format (or
     why not), and `ETC2: cache de texturas en ... N texturas`. During play, every 512 BC textures, `[compatibilidad]
     ETC2: ... desde la cache, ... convertidas al momento ..., ... decodificadas` (on a first session almost all
     decoded; on the second, mostly from the cache) and, after the first textures, their PSNR.
   - Look in a full race (second session, cache warm) for wrong colors, black or transparent patches, or blocky
     gradients on the road, buildings, signs and the car. Then measure against **Decompressed** on the same route:
     FPS, the 95th/99th percentile frame times, temperature, and the texture cache line (`[nativo] C3 diag cache:
     ... MB`). Watch that `convertidas al momento` stays near 0 (it is ring time) and how fast the cache fills.
   - The OpenGL ES backend still decodes BC (its own path in `app/src/nfsmw_nativo_dibujos_gles.cpp`); if the tablet
     ends up on GLES, port the same lookup there.
3. **Try half precision on the tablet** (the pass is fixed, see "Done"): launcher **Color precision → 16 bits**, both
   backends. It changes the shaders, so the first race after switching compiles pipelines: play one
   race before measuring.
   - The log: `[nativo] C5a: media precision en N pixel shaders (M resultados a 16 bits). A 32 bits: S muestras del
     mapa de sombras en K shaders, P pow/exp/log...`. K must be above 0 (the library's world shaders sample the
     shadow map); `sin tocar` counts shaders left at 32 bits because their shadow samples could not be told apart.
     If that number is large, the library's SPIR-V has a shape `tools/tests/spirv_precision_test.cpp` lacks: dump
     one such shader and add it to the fixture.
   - Look in a full race, day and night routes, for: shadow acne or stripes on roads and walls, blocky or
     shimmering shadow edges, banding in car paint highlights and reflections, in the sky and in fog, wrong bloom
     or motion blur, and the HUD. Compare screenshots at the same spot against **32 bits**.
   - Measure both on the same route: FPS, 95th/99th percentile frame times, temperature after 15 minutes. Keep 16
     bits only if it is faster and nothing above shows.
4. **Measure ADPF and the thermal step-down on the tablet** (done in code, see "Done"):
   - The log at start: `[ritmo] ADPF: objetivo 16.67 ms...`, then `[ritmo] ADPF: sesion con anillo ..., preparador
     ..., ejecutor ...` (the three threads; on Android 13 the session is rebuilt as each joins) and `[termico] recorte
     por calor: ...`. "no disponible" or "el sistema no da una sesion" means this Android/One UI does not offer it:
     note it in "The tablet" and stop here. `adb shell dumpsys performance_hint` should list the session.
   - Every 30 s `[ritmo] ADPF, ultimos 30 s: ...`: the busiest thread's CPU time against the budget, how many frames
     went over it, and what each report costs (keep it under ~50 us; it runs on the ring thread). Every minute
     `[termico] ultimo minuto: margen max ...`, and a line on each level change.
   - Three 20-minute sessions on the same route, from a cool tablet (launcher options): all on; **System performance
     hints (ADPF) → Off**; and **Game cores → Let Android decide** with ADPF on. For each: FPS, the
     95th/99th percentile frame times, temperature (`dumpsys thermalservice`), battery drain per hour, and the
     seconds per thermal level. Keep the pinning only if it still wins with ADPF on; if Android's choice with ADPF
     is as good, it is the better default (it adapts to other SoCs).
   - The thresholds are cvars (`nfsmw_android_termico_nivel1_pct` 85, `_nivel2_pct` 95, `_volver_pct` 70,
     `_volver_s` 60): if the tablet throttles before level 1 kicks in, lower `nivel1`.
5. **Check 16:10 on the tablet** (done in code, see "Done"): launcher **Internal resolution → 1056x660** (or
   1280x800, or **Aspect ratio → The screen's** with 1024x576/1280x720: 928x576 and 1152x720).
   - The log: `[resolucion] modo 4 preparado a 1056x672 (area visible 1056x660): el panoramico`, `salida del juego
     al arrancar: 1056x660, impuesta`, `[panoramico] vista 1: destino 1056x672, mas estrecho que 16:9: ... se deja
     como esta` (no `k` line: the camera is not corrected).
   - On screen: the image fills the 16:10 screen with no bars and no 3D stretch (round wheels, the speedometer's
     circle may be stretched: the HUD is laid out for the screen size, a prototype as in the wide mode); more road
     and sky visible than at 16:9; shadows that match the objects (the shadow fit uses the same projection);
     bloom, motion blur, the road reflection and the rear-view mirror in place; menus and the map usable.
   - Measure 1056x660 against 1024x576 and 1280x720 at 16:9 on the same route (FPS, 95th/99th percentile, GPU
     temperature): 1056x660 is 18 % more pixels than 1024x576 and a clean 2x scale to the screen.
6. **45 FPS**: done (see "Done in this copy").
7. **Game data in the app's own folder** (`getExternalFilesDir`): no shared-storage overhead while the track
   streams, and no All files access. `MainActivity.GAME_FOLDER_NAME` and `GameActivity` hold the path.
8. **A build for this SoC**: done (`-Pnfsmw.cpu=exynos1380`, `-Pnfsmw.pgo=generar|usar`; recipe in
   `docs/building-android.md`). A/B on the tablet: ~13 % less CPU time and steadier pacing, same FPS (step 9
   notes above). Redo the profile session after large changes to the game code.
9. **Renderer on Mali**: load/store operations (clear or don't-care) and transient depth attachments; confirm with
   Arm Performance Studio (Streamline) counters over adb.
