# Third-party notices

This app is built on the work of other projects. Their licenses apply to their code and to the parts of this
repository derived from it.

## Projects this port is derived from

| Project | Used for | License |
|---|---|---|
| [nfsmw-android](https://github.com/codepdbh/nfsmw-android) by codepdbh | The Android port this repository is a copy of (0.8.2, commit `71b4c80`): `android/`, the SDK's Android layer, the native renderer's Android work and the launcher | GPL-3.0 |
| [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) by StevensND | The Switch port that Android port grew from: the native renderer, the hooks and tools in `app/` and `tools/`, the SDK's Horizon layer, `shaders/` and `docs/` | GPL-3.0, with the BSD-3-Clause and MIT parts below |
| [NFSMW Recompiled](https://github.com/madelrandel-blip/NFSMW-Recompiled) by madelrandel-blip | The recompilation project this port started from: `app/` (hooks, configuration, codegen setup) and tools | GPL-3.0 |
| [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) by Tom Clay, with portions from [Xenia](https://xenia.jp) (Ben Vanik and the Xenia contributors) | `sdk/`: code generator, Xbox 360 kernel and runtime, GPU command processing | BSD-3-Clause (`sdk/LICENSE`) |
| [XenosRecomp](https://github.com/hedge-dev/XenosRecomp) by hedge-dev and contributors | `shaders/XenosRecomp`: Xenos microcode to HLSL translator | MIT (`shaders/LICENSE.md`) |
| [mesa-switch](https://github.com/danfromtico/mesa-switch) by danfromtico, on top of [Mesa](https://mesa3d.org) | The Vulkan driver (NVK) and shader compiler (NAK) for Horizon, linked into the Switch NRO; its patches are not in this copy (`docs/mesa.md` describes them) | MIT (Mesa's licenses per file) |
| [NFSMW-Recompiled-Mobile](https://github.com/victorgbd/NFSMW-Recompiled-Mobile) by victorgbd and contributors (a fork of this repository) | Android 0.8.0: the OpenGL ES backend of the native renderer (`app/src/*_gles*`, `sdk/src/ui/gles/`), the LAN multiplayer (`xam_net.cpp`, `xsocket.cpp`, `ServidorEa.java` and friends, protocol of [nfsmw-server-lan](https://github.com/victorgbd/nfsmw-server-lan)), the CRT memcpy overlap fix by kayo-b and the 2+N core affinity idea by dvaustria741-a11y | GPL-3.0 |

The Horizon (Switch) layer added to `sdk/` is under the SDK's BSD-3-Clause license, and the changes in `shaders/`
under that project's license (MIT), so they can be reused by other ports. The rest of this
repository is GPL-3.0 (`LICENSE`).

## Libraries

Libraries used by the SDK, the NRO and the code generator, fetched from the submodules of ReXGlue SDK v0.10.0
(`tools/fetch_thirdparty.py`):

| Library | License |
|---|---|
| [FFmpeg](https://ffmpeg.org) (libavcodec, libavutil: the WMV3 decoder for the cutscenes), from `wmarti/FFmpeg` at `0604b464c7cb` | LGPL-2.1-or-later (configured without GPL parts) |
| [libmspack](https://www.cabextract.org.uk/libmspack/) by Stuart Caie (LZX decompression) | LGPL-2.1 |
| [glslang](https://github.com/KhronosGroup/glslang) | BSD-3-Clause and others (see its LICENSE.txt) |
| [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) | Apache-2.0 |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) at `2c32b6bf86f3`, its GLSL backend vendored in `sdk/thirdparty/spirv-cross` (SPIR-V to GLSL ES for the OpenGL ES backend) | Apache-2.0 |
| [SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers), [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | MIT, Apache-2.0 |
| [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | MIT |
| [fmt](https://github.com/fmtlib/fmt), [spdlog](https://github.com/gabime/spdlog) | MIT |
| [xxHash](https://github.com/Cyan4973/xxHash) | BSD-2-Clause |
| [SIMDe](https://github.com/simd-everywhere/simde) | MIT |
| [toml++](https://github.com/marzer/tomlplusplus) | MIT |
| [Dear ImGui](https://github.com/ocornut/imgui) | MIT |
| [o1heap](https://github.com/pavel-kirienko/o1heap) | MIT |
| [utfcpp](https://github.com/nemtrif/utfcpp) | BSL-1.0 |
| [CLI11](https://github.com/CLIUtils/CLI11) | BSD-3-Clause |
| AES-128 by LuoPeng (`thirdparty/aes_128`) | MIT |
| [libnx](https://github.com/switchbrew/libnx) and the devkitA64 runtime (newlib, libstdc++) | ISC; newlib and GCC runtime licenses |

### FFmpeg and libmspack (LGPL)

This port modifies these files of the SDK's third-party tree; the modified copies are in `sdk/thirdparty`:

- `FFmpeg/libavcodec/fft_template.c` and `FFmpeg/libavcodec/mdct_template.c`
- `FFmpeg/config.h`, plus `FFmpeg/config_switch_aarch64.h` and `FFmpeg/config_nfsmw_wmv3.h` (new): the configuration
  for AArch64 Horizon and for a build limited to the WMV3 decoder
- `ffmpeg-overlay/codec_list.c`: registers the WMV3 decoder
- `CMakeLists.txt`: builds the libraries for the Switch

The rest of FFmpeg and libmspack is the unmodified upstream source at the commits pinned by ReXGlue SDK v0.10.0.
Everything needed to rebuild and relink the app with a modified version of either library is in this repository and
in [docs/building-android.md](docs/building-android.md).

## Installer page

The [installer page](https://github.com/StevensND/nfsmw-nx-installer) runs WebAssembly builds of the shader translator
and packer from `shaders/`, libmspack (LZX), and [DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)
v2025.1 (University of Illinois/NCSA Open Source License, with LLVM's license terms). Its own notices are in that
repository.

## Trademarks

The saved-game ZIP workflow and reversible graphics presets introduced in 0.8.1 are inspired by `Partidas.java`
and the “Mis ajustes” feature in [victorgbd/NFSMW-Recompiled-Mobile](https://github.com/victorgbd/NFSMW-Recompiled-Mobile)
at `dd76acb032ccbd7b0494985242489cd9cd90a939` (GPL-3.0). Our archive implementation adds a strict allowlist,
SHA-256 verification and recoverable directory swaps; it does not copy the fork's launcher or package identity.

Need for Speed and Need for Speed: Most Wanted are trademarks of Electronic Arts Inc. Nintendo Switch is a trademark of
Nintendo. Xbox 360 is a trademark of Microsoft. This project is not affiliated with or endorsed by any of them.
