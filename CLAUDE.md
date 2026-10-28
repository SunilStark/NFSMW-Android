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

`tools/android/comprobar_tableta.ps1` runs the checks of step 1 below; these are the pieces it uses.

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
- `tools/android/comprobar_tableta.ps1`, the checks of step 1 (not run yet).
- `android: keep BC textures compressed as ETC2 on GPUs without BC` (step 2 below, Vulkan only): BC1 to ETC2
  RGB8A1, BC2/3 to ETC2 RGBA8, BC4/5 to EAC R11/RG11 (`app/src/nfsmw_texturas_etc.h`), transcoded in the background
  on the little cores into a disk cache (`app/src/nfsmw_texturas_cache_etc.cpp`, under the game's cache folder,
  `texturas_etc2_v1/`); a texture is decoded as before the first time, ETC2 from then on. cvars
  `nfsmw_nativo_bc_a_etc2` (launcher: **Texturas en GPUs sin BC**), `nfsmw_nativo_etc2_cache_mb`,
  `nfsmw_nativo_etc2_hilos`; the SDK now enables `textureCompressionETC2`. Checked off-device (encoder, cache, and
  Mesa's ETC2 decoder against ours on every texel); not yet on the tablet.
- `android: half precision keeps the shadow map and pow at 32 bits` (step 3 below): `nfsmw_nativo_media_precision`
  relaxed every result computed from a texture, the shadow map too, which the game samples as an ordinary texture
  and compares with the pixel's depth (at 16 bits, steps of 0.0005 near 1.0: shadow acne). Now its samples, found by
  tfetch2DSombraMin's marker, stay at 32 bits, and so do pow/exp/log with what feeds them; a shader whose shadow
  samples cannot be found is left alone. Launcher: **Precisión del color**. Tested on glslang-compiled shaders
  shaped like the library's (`tools/tests/spirv_precision_test.cpp`; spirv-val passes); not yet on the tablet.
- `android: ADPF session and thermal step-down` (step 4 below): `android/app/src/main/cpp/android_ritmo.cpp`. The
  preparer, executor and ring threads in an `APerformanceHint` session (API 33+); each frame, at the ring's Swap, the
  busiest one's CPU time is reported against 1/`nfsmw_limite_fps`. A thread reads `AThermal_getThermalHeadroom` (10 s
  ahead) every 2 s and sets a floor under the 30 FPS guard's shadow steps (0.85: one map, 0.95: no cast shadows;
  back one level per minute under 0.70; policy in `android_termico.h`). Loaded with dlsym (minSdk 26). Launcher:
  **Control térmico**, **Ayuda al sistema (ADPF)**, **Núcleos del juego**. Policy tested off-device; not on the tablet.
- `android: 16:10 internal sizes and the narrower screen proportion` (step 5 below): `nfsmw_proporcion_pantalla`
  below 16:9 draws narrower at the same height, and `nfsmw_resolucion_interna` gains 1056x660 and 1280x800, both
  16:10 whatever the proportion (sizes in `app/src/nfsmw_tamano_pantalla.h`). The camera is left alone when
  narrower: the game's projection already shows more above and below (Vert+); the presenters show that front
  buffer at its own proportion. Launcher: the two sizes and **Proporción de imagen → La de la pantalla**. Sizes
  tested off-device; not on the tablet.

## Next, in order (each measured on the tablet before and after)

1. **Confirm the two changes above on the tablet**: the core log line, keyboard play and remapping, controller and
   keyboard connected mid-race. Build and install the `.dev` APK, then run
   `powershell -ExecutionPolicy Bypass -File tools\android\comprobar_tableta.ps1` with the owner at the tablet: it
   records the device's data (correct "The tablet" above from it), checks the core and argument lines in the log and
   that the game screen is not recreated on hot-plug, and asks the owner what only a person can see. Its summary,
   `out/tablet/paso1.txt`, goes into the commit or into "Done in this copy".
2. **Confirm the ETC2 textures on the tablet** (done in code, see above):
   - The GPU decodes our blocks as our decoder does: build and run `tools/tests/texturas_etc_vulkan_test.cpp` over
     adb (its first lines). It must print PASS for the four formats; if not, set **Texturas en GPUs sin BC →
     Descomprimidas** and report which format differs.
   - The log at start: `[compatibilidad] BC1: ETC2 RGB8A1 si esta en la cache del disco...` for each BC format (or
     why not), and `ETC2: cache de texturas en ... N texturas`. During play, every 512 BC textures, `[compatibilidad]
     ETC2: ... desde la cache, ... convertidas al momento ..., ... decodificadas` (on a first session almost all
     decoded; on the second, mostly from the cache) and, after the first textures, their PSNR.
   - Look in a full race (second session, cache warm) for wrong colors, black or transparent patches, or blocky
     gradients on the road, buildings, signs and the car. Then measure against **Descomprimidas** on the same route:
     FPS, the 95th/99th percentile frame times, temperature, and the texture cache line (`[nativo] C3 diag cache:
     ... MB`). Watch that `convertidas al momento` stays near 0 (it is ring time) and how fast the cache fills.
   - The OpenGL ES backend still decodes BC (its own path in `app/src/nfsmw_nativo_dibujos_gles.cpp`); if the tablet
     ends up on GLES, port the same lookup there.
3. **Try half precision on the tablet** (the pass is fixed, see "Done"): launcher **Precisión del color → 16
   bits**, both backends. It changes the shaders, so the first race after switching compiles pipelines: play one
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
   - Three 20-minute sessions on the same route, from a cool tablet (launcher options): all on; **Ayuda al sistema
     (ADPF) → Desactivada**; and **Núcleos del juego → Los que decida Android** with ADPF on. For each: FPS, the
     95th/99th percentile frame times, temperature (`dumpsys thermalservice`), battery drain per hour, and the
     seconds per thermal level. Keep the pinning only if it still wins with ADPF on; if Android's choice with ADPF
     is as good, it is the better default (it adapts to other SoCs).
   - The thresholds are cvars (`nfsmw_android_termico_nivel1_pct` 85, `_nivel2_pct` 95, `_volver_pct` 70,
     `_volver_s` 60): if the tablet throttles before level 1 kicks in, lower `nivel1`.
5. **Check 16:10 on the tablet** (done in code, see "Done"): launcher **Resolución interna → 1056x660** (or
   1280x800, or **Proporción de imagen → La de la pantalla** with 1024x576/1280x720: 928x576 and 1152x720).
   - The log: `[resolucion] modo 4 preparado a 1056x672 (area visible 1056x660): el panoramico`, `salida del juego
     al arrancar: 1056x660, impuesta`, `[panoramico] vista 1: destino 1056x672, mas estrecho que 16:9: ... se deja
     como esta` (no `k` line: the camera is not corrected).
   - On screen: the image fills the 16:10 screen with no bars and no 3D stretch (round wheels, the speedometer's
     circle may be stretched: the HUD is laid out for the screen size, a prototype as in the wide mode); more road
     and sky visible than at 16:9; shadows that match the objects (the shadow fit uses the same projection);
     bloom, motion blur, the road reflection and the rear-view mirror in place; menus and the map usable.
   - Measure 1056x660 against 1024x576 and 1280x720 at 16:9 on the same route (FPS, 95th/99th percentile, GPU
     temperature): 1056x660 is 18 % more pixels than 1024x576 and a clean 2x scale to the screen.
6. **45 FPS** for the 90 Hz display (half its rate) where 60 does not hold: `nfsmw_limite_fps` allows 30/60/90/120.
7. **Game data in the app's own folder** (`getExternalFilesDir`): no shared-storage overhead while the track
   streams, and no All files access. `MainActivity.GAME_FOLDER_NAME` and `GameActivity` hold the path.
8. **A build for this SoC**: `-mcpu=cortex-a78` for the game code (both core types are ARMv8.2: LSE atomics, FP16),
   then PGO from a 15-minute session on the tablet with clang's own profiles (`-fprofile-generate`, then
   `-fprofile-use`); the PGO code in `app/CMakeLists.txt` and `app/src/nfsmw_pgo.cpp` is the Switch's GCC path and
   does not apply, and its profiles were removed from this copy.
9. **Renderer on Mali**: load/store operations (clear or don't-care) and transient depth attachments; confirm with
   Arm Performance Studio (Streamline) counters over adb.
