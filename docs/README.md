# Documentation

If a word is new to you, look it up in the [glossary](glossary.md).

## How the Android app fits together

1. **The game's code is translated.** `default.xex`, written for the Xbox 360's PowerPC processor, is translated to
   C++ by [ReXGlue](glossary.md#rexglue)'s code generator on the PC, then compiled for ARM64 into `libmain.so`.
   See [toolchain.md](toolchain.md).
2. **The game gets a fake Xbox 360 around it.** ReXGlue (`sdk/`) gives it what it would find on the console: files,
   memory, threads, audio and controllers, on top of SDL3 and Android.
3. **The port draws the frame itself.** The game writes its GPU commands as it always did; the native renderer
   (`app/src/nfsmw_nativo_*`) reads them and draws the same frame with Vulkan, or OpenGL ES.
   See [native-renderer.md](native-renderer.md).
4. **The shaders are translated on the tablet.** The launcher builds the shader library from your own disc in a
   hidden WebView, with the translator compiled to WebAssembly. See [shaders.md](shaders.md).
5. **Audio and cutscenes** are decoded by the port. See [android-audio.md](android-audio.md) and
   [audio-and-video.md](audio-and-video.md).

## What to read

| You want to... | Read |
|---|---|
| Build and install the app | [building-android.md](building-android.md) |
| Make it faster on the tablet | [measuring.md](measuring.md), then [performance-history.md](performance-history.md), [toolchain.md](toolchain.md), [native-renderer.md](native-renderer.md) |
| Build for another edition of the game | [ANDROID_EDITIONS.md](ANDROID_EDITIONS.md), with the background in [editions.md](editions.md) |

## All documents

| Document | What it explains |
|---|---|
| [building-android.md](building-android.md) | The PC tools, the build from a fresh clone, installing, and the tests that need no device |
| [ANDROID_EDITIONS.md](ANDROID_EDITIONS.md) | Android builds of editions other than PAL Spain (in Spanish) |
| [android-audio.md](android-audio.md) | Audio on Android (in Spanish) |
| [android-xclipse-diagnostic.md](android-xclipse-diagnostic.md) | How a reflection bug on Samsung's Xclipse GPU was found (in Spanish) |
| [glossary.md](glossary.md) | Every technical word, in plain words |
| [native-renderer.md](native-renderer.md) | How the port draws the game |
| [shaders.md](shaders.md) | How the game's shaders are translated, and what had to be fixed |
| [toolchain.md](toolchain.md) | How the game's code is translated and compiled, and the options that make it faster |
| [measuring.md](measuring.md) | How to measure performance without being misled |

### Background from the Switch port

This app began as [nfsmw-nx](https://github.com/StevensND/nfsmw-nx), the Switch port, and shares its code. These
pages describe that port: comments in `sdk/` and `app/` point at them, so they stay. Their Switch build steps,
profiles and driver patches are not in this repository.

| Document | What it explains |
|---|---|
| [platform-notes.md](platform-notes.md) | The Switch system's surprises; much of the SDK's Switch code refers to it |
| [mesa.md](mesa.md) | The Switch's Vulkan driver (NVK) and that port's changes to it |
| [audio-and-video.md](audio-and-video.md) | The game's audio and cutscenes, as first done on the Switch |
| [editions.md](editions.md) | How the editions and languages were supported on the Switch |
| [performance-history.md](performance-history.md) | How the Switch frame rate went from a few FPS to about 30, step by step |
