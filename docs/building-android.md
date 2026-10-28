# Building the Android app

Everything is built on a Windows PC that has your own copy of the game, then installed on the tablet with `adb`.
GitHub Actions cannot do it: the build needs the C++ the code generator writes from your `default.xex`, which is never
in the repository. The commands are for PowerShell unless a step says Git Bash.

## What you need, once

- **Android SDK** (Android Studio, or the command-line tools) with `platform-tools`, `platforms;android-35`,
  `build-tools;35.0.0`, `ndk;28.2.13676358` and `cmake;3.30.5` (`sdkmanager "<package>"`; the versions
  `android/app/build.gradle` asks for). `ANDROID_HOME` set, or the SDK at `%LOCALAPPDATA%\Android\Sdk`.
- **JDK 17 or later**. Android Studio's `jbr` works; `build_android.ps1` finds it.
- **Python 3.10 or later**, and **Git** with Git Bash (`tools/codegen.sh` is a bash script).
- For the code generator, which runs on the PC: **LLVM clang 20 or later**, **CMake 3.25 or later** and a single
  **Ninja**.
- About **16 GB of RAM**. The generated code is about 144 MB of C++, and the ThinLTO link of the game is heavy. If it
  runs out of memory, lower `--thinlto-jobs` in `android/app/src/main/cpp/CMakeLists.txt`.
- **Your own Need for Speed: Most Wanted for Xbox 360, PAL Spain edition.** `app/` is built for that executable;
  other editions need the steps in [ANDROID_EDITIONS.md](ANDROID_EDITIONS.md).

Three traps of CMake on Windows that cost a lot of time:

- Run CMake from PowerShell with the Windows CMake, never from an MSYS or devkitPro shell.
- Windows PowerShell 5.1 stops `& cmake ... *> log` at the first line written to stderr, even a warning. Run long
  builds with `Start-Process` and send the output to a file.
- Keep one Ninja only: an older one (Visual Studio's) throws away a newer one's build records and rebuilds everything.

## From a fresh clone

1. **Third-party sources.** `sdk/thirdparty` only holds the files this port changed; the rest comes from the ReXGlue
   release it is based on (v0.10.0) and its submodules:

   ```powershell
   python tools/fetch_thirdparty.py --cache-dir out/evolved2/dependency-cache
   ```

2. **The game's executable** goes to `assets/game_root/default.xex` (git ignores `assets/`). If the game is already on
   the tablet, where the app reads it:

   ```powershell
   New-Item -ItemType Directory -Force assets/game_root | Out-Null
   adb pull /sdcard/nsfmw-androidevolved/default.xex assets/game_root/default.xex
   ```

   From an ISO instead: `python tools/fase1_extraer.py <iso> -o assets/game_root`. Then check the edition:
   `(Get-FileHash assets/game_root/default.xex).Hash` must be
   `AAD15FC218D034DE3131F746E0B763DD8049A40DD676EFC87667FCAF4FC72694` (PAL Spain).

3. **The code generator** (`rexglue`), built for the PC:

   ```powershell
   cmake -S sdk -B out/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
   cmake --build out/host --target rexglue
   Get-ChildItem sdk/out,out/host -Recurse -Filter rexglue.exe   # the SDK sets its own output folder
   ```

4. **Translate the game to C++**, from Git Bash at the repository root:

   ```bash
   REXGLUE=<path to rexglue.exe> PYTHON=py tools/codegen.sh app
   ```

   It writes `app/generated/default` and `app/src/copias_literales` (both ignored by git), and its errors go to
   `app/codegen.log`. Run it again only when the code generator, `app/*.toml` or the hooks change, or after merging
   the original project's changes.

5. **The APK:**

   ```powershell
   .\build_android.ps1 '-Pnfsmw.applicationIdSuffix=.dev'
   ```

   Keep the quotes: PowerShell splits `-P...` at its dots. The suffix builds the package `com.nfsmw.android.dev`,
   named **NFS Most Wanted (dev)**, which installs beside the published app instead of replacing it. Both read the
   same game folder; each keeps its own saves and settings. Without the suffix the build replaces the published
   package, which has another signature: Android asks to uninstall that one first, which deletes its saves (export
   them first from the launcher's **Partidas guardadas**).

   The APK is `android\app\build\outputs\apk\release\app-release.apk`: Release (`-O3`, ThinLTO), ARM64 only, signed
   with this PC's debug key. Keep that key (`%USERPROFILE%\.android\debug.keystore`): every later build must have the
   same one to install over the last without uninstalling.

6. **Install:** `adb install -r android\app\build\outputs\apk\release\app-release.apk`. Open **NFS Most Wanted
   (dev)** and allow **All files access**. The game folder is `Internal storage/nsfmw-androidevolved/` with
   `default.xex`, `NFS/` and `Movies/`. The launcher builds the shader library there from your disc if it is missing
   or out of date.

## Checking the APK

```powershell
& "$env:ANDROID_HOME\build-tools\35.0.0\apksigner.bat" verify --print-certs android\app\build\outputs\apk\release\app-release.apk
& "$env:ANDROID_HOME\build-tools\35.0.0\aapt.exe" dump badging android\app\build\outputs\apk\release\app-release.apk | Select-String 'package|native-code|sdkVersion'
```

The native libraries must keep 16 KB ELF load alignment (`-Wl,-z,max-page-size=16384`) for devices with 16 KB pages.

## Tests that need neither the game nor a device

```powershell
javac -encoding UTF-8 -d out/save-tests android/app/src/main/java/com/nfsmw/android/SaveArchive.java android/app/src/main/java/com/nfsmw/android/GraphicsPresets.java tools/tests/SaveArchiveTest.java tools/tests/GraphicsPresetsTest.java
java -cp out/save-tests com.nfsmw.android.SaveArchiveTest
java -cp out/save-tests com.nfsmw.android.GraphicsPresetsTest

# the launcher's keys against the SDK's keyboard driver (KeyEvent's constants come from the SDK's android.jar)
javac -encoding UTF-8 -cp $env:ANDROID_HOME/platforms/android-35/android.jar -d out/keyboard-tests android/app/src/main/java/com/nfsmw/android/KeyboardBindings.java tools/tests/KeyboardBindingsTest.java
java -cp out/keyboard-tests com.nfsmw.android.KeyboardBindingsTest

# the fast-core choice on known SoC layouts
clang++ -std=c++17 tools/tests/android_nucleos_test.cpp -o out/android_nucleos_test.exe; out/android_nucleos_test.exe

# BC to ETC2/EAC: every block decodes to the encoder's own error, every mode is used, quality and speed
clang++ -std=c++20 -O2 tools/tests/texturas_etc_test.cpp -o out/texturas_etc_test.exe; out/texturas_etc_test.exe

# its disk cache (needs sdk/thirdparty from step 1, for fmt and xxHash)
clang++ -std=c++20 -O2 -DFMT_HEADER_ONLY -Itools/tests/rex_minimo -Isdk/thirdparty/fmt/include -Isdk/thirdparty/xxHash tools/tests/texturas_cache_etc_test.cpp -o out/texturas_cache_etc_test.exe; out/texturas_cache_etc_test.exe
```

`tools/tests/texturas_etc_vulkan_test.cpp` runs on the tablet: it checks that its GPU decodes the transcoded blocks
as the encoder's decoder does. Its first lines say how to build it with the NDK and run it over `adb`.

`tools/tests/` also holds checks of the GPU driver probe, BC texture conversion, SPIR-V for Vulkan 1.1, GLES shaders,
audio and memory; several run only on the device or with the SDK built, as their first lines say where they do.
