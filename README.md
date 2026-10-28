# NFSMW Android, tuned for the Galaxy Tab S10 Lite

Need for Speed: Most Wanted (2005, Xbox 360) as a native ARM64 Android app, set up for one device: a **Samsung Galaxy
Tab S10 Lite**, played with a **Bluetooth controller** or a **keyboard**. The goal is a steady **60 FPS first, then
resolution**.

This is a private working copy of [codepdbh/nfsmw-android](https://github.com/codepdbh/nfsmw-android) (0.8.2), which
brought the [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) recompilation to Android on the
[ReXGlue](https://github.com/rexglue/rexglue-sdk) SDK. It is not an emulator: the game's PowerPC code is translated to
C++ on the PC and compiled for ARM64, and a native renderer draws its frames with Vulkan or OpenGL ES. No game files
are included, and none are ever committed.

## Status

| Change | State |
|---|---|
| Game threads kept off the little cores on the Exynos 1380 | Tested on known SoC layouts |
| Keyboard play and key remapping in the launcher; no game-screen restart on hot-plug | Tested off-device |
| BC textures kept compressed as ETC2/EAC on Mali, with a disk cache (Vulkan) | Tested off-device |
| Half precision that keeps shadow maps and pow at 32 bits | Tested on compiled shaders off-device |
| ADPF performance hints and thermal step-down of shadows | Policy tested off-device |
| 16:10 internal sizes (1056x660, 1280x800) and the screen's own proportion | Sizes tested off-device |
| Launcher in English; game text in English by default | Java tests pass |
| `.dev` package that installs beside the published app | Done |

None of it has been built as an APK or run on the tablet yet. [CLAUDE.md](CLAUDE.md) has the status at handover and
the plan, in order: build and confirm the above on the tablet, then 45 FPS on the 90 Hz display, game data in the
app's own folder, a Cortex-A78 build with PGO, and Mali render pass tuning.

## Requirements

- Android 8 or later, ARM64.
- A GPU with Vulkan 1.1 or later **and** the descriptor-indexing features (Vulkan 1.2, or `VK_EXT_descriptor_indexing`).
  Without them, use the launcher's **Graphics API → OpenGL ES**.
- Your own **Need for Speed: Most Wanted for Xbox 360, PAL Spain edition**, extracted: `default.xex`, `NFS/` and
  `Movies/`. Other editions need their own build ([docs/ANDROID_EDITIONS.md](docs/ANDROID_EDITIONS.md)).

## Building

Only on a PC with your copy of the game: the build needs the C++ generated from your `default.xex`, so GitHub Actions
cannot build it (the workflow is manual only). In short:

```powershell
python tools/fetch_thirdparty.py --cache-dir out/evolved2/dependency-cache
# default.xex -> assets/game_root/, build rexglue, then from Git Bash: tools/codegen.sh app
.\build_android.ps1 '-Pnfsmw.applicationIdSuffix=.dev'
adb install -r android\app\build\outputs\apk\release\app-release.apk
```

The full steps, the PC tools they need and the tests that run without the game or a device are in
[docs/building-android.md](docs/building-android.md). The `.dev` build is `com.nfsmw.android.dev`, named **NFS Most
Wanted (dev)**: it installs beside the published app and reads the same game folder.

## On the tablet

1. Allow **All files access** when the launcher asks.
2. Copy `default.xex`, `NFS/` and `Movies/` into `Internal storage/nsfmw-androidevolved/`, or pick the extracted folder
   with **Choose game folder**.
3. Tap **Play**. The first run builds the shader library (`nfsmw_shaders.nfsp`) from your disc, on the tablet.

Starting settings and how to measure frame rate, temperature and battery are in [CLAUDE.md](CLAUDE.md). The launcher's
update offers point at upstream's releases; ignore them on a `.dev` build.

### Controls

- **Bluetooth or USB controller**: player 1; the touch controls hide.
- **Keyboard**: **KEYBOARD → Play with keyboard** makes it player 1's controller (A/D steer, E accelerates, Q brakes
  by default). **Keys** remaps each button; a key given to one button leaves the others.
- **Touch**: analog or tilt steering and every button, movable and resizable in the editor (gear button).

### Logs

`adb logcat -s NFSMW-rex NFSMW`, or `tools/android_logcat.sh`. **Send crash or log** in the launcher makes a ZIP with
the native logs and the device report.

## Layout

| Folder | What it has |
|---|---|
| `android/` | The launcher (Java), SDL3 integration, the native glue (`app/src/main/cpp`) and the Gradle build |
| `app/` | The game's port: hooks, native renderer, audio, video; `app/generated/` is written by the code generator and ignored |
| `sdk/` | ReXGlue, the runtime that stands in for the Xbox 360 |
| `shaders/` | The shader translator, compiled to WebAssembly for the launcher |
| `tools/` | Code generation, fix-ups, checks and tests ([tools/README.md](tools/README.md)) |
| `docs/` | How it works and how to build it ([docs/README.md](docs/README.md)) |

## Credits and licence

The port is the work of codepdbh (Android), StevensND (nfsmw-nx) and the ReXGlue authors; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Licensed under the GPL-3.0 ([LICENSE](LICENSE)). A fan project with no
affiliation with Electronic Arts; "Need for Speed" is a trademark of Electronic Arts Inc.
