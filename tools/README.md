# Tools

This folder has the scripts that build the app: they get the missing libraries, extract the game, translate it to
C++, fix up the generated code and check the result. Look here when a step of
[docs/building-android.md](../docs/building-android.md) names a script and you want to know what it does. The names
are in Spanish; each one is translated below.

## Main steps

In the order of [docs/building-android.md](../docs/building-android.md) (the APK itself is built by
`build_android.ps1` at the root):

| File | What it does |
|---|---|
| `fetch_thirdparty.py` | Fetches the third-party sources of the SDK into `sdk/thirdparty`. The repository only keeps the files this port changed. |
| `fase1_extraer.py` ("phase 1: extract") | Extracts the Xbox 360 ISO into `assets/game_root` and prints the [XEX](../docs/glossary.md#xex) information. |
| `codegen.sh` | Runs the code generator (`rexglue codegen`, which translates the game to C++) on an edition tree (`app/`, or the `app_<edition>` copy of another [edition](../docs/glossary.md#edition)), then `llamadas_directas.py` and `copia_literal.py`. |

## After every code generation

The build depends on these two tools. `codegen.sh` runs them for you, right after the code generator. If you run the
code generator another way, run them afterwards, in this order.

| File | What it does |
|---|---|
| `llamadas_directas.py` ("direct calls") | Turns calls between recompiled functions into direct calls, except for calls to functions with a [hook](../docs/glossary.md#hook). [LTO](../docs/glossary.md#lto) needs this to optimise across them (see [docs/toolchain.md](../docs/toolchain.md#direct-calls)). |
| `copia_literal.py` ("literal copy") | Writes the literal copies of five game functions. The [guards](../docs/glossary.md#guard) of their [native versions](../docs/glossary.md#native-replacement) compare against these copies. |

## Android

| File | What it does |
|---|---|
| `android_logcat.sh` | Shows the app's log on the PC (`adb logcat` with the app's tags). |
| `android/comprobar_tableta.ps1` ("check tablet") | Step 1 of [CLAUDE.md](../CLAUDE.md) on the tablet: device data, the fast-core and argument lines in the log, no game-screen restart on hot-plug, and the owner's answers; summary in `out/tablet/paso1.txt`. |
| `biblioteca_shaders.mjs`, `biblioteca_shaders_gles.mjs` ("shader library") | Build `nfsmw_shaders.nfsp` (Vulkan) and `nfsmw_shaders_gles.nfsp` on the PC with Node, as the launcher does on the tablet. |
| `gles_efectos_presentador.cpp` | Converts the presenter's effect shaders to GLSL ES for the OpenGL ES backend; run it again only when those shaders change. |

## Checks and analysis

| File | What it does |
|---|---|
| `huecos.py`, `huecos_excluir.txt` ("gaps", "gaps to exclude") | Finds code the code generator's analysis left without a function and writes it to `app/huecos.toml`. `huecos_excluir.txt` lists the gaps to leave out. |
| `lee_antes.py` ("read before") | Lists, per recompiled function, the non-volatile registers (the ones a function must keep unchanged for its caller) it reads before writing them. See [docs/toolchain.md](../docs/toolchain.md#registers-as-c-locals) for why this matters. |
| `calientes.py` ("hot ones") | Resolves the samples of the console profiler (`rex_perfil.log`) to functions, hottest first. Its output is the input for `app/orden_funciones.ld`, the list of functions placed first in the program ([function ordering](../docs/glossary.md#function-ordering)). |
| `comprobar_clave_textura.py` ("check texture key") | Checks in the built program (an ELF, such as `libmain.so`) that the texture key is written before it is hashed. It catches an LTO/PGO aliasing pitfall (see [docs/toolchain.md](../docs/toolchain.md#an-lto-and-pgo-pitfall-strict-aliasing-in-xxhash)). |

## Folders

| Folder | What it has |
|---|---|
| `editions/` | Support for the other editions: address matching, edition tree creation and checks ([docs/editions.md](../docs/editions.md)); `android/editions.py` drives them for Android builds ([docs/ANDROID_EDITIONS.md](../docs/ANDROID_EDITIONS.md)). |
| `tests/` | Checks of the launcher's Java (saves, presets, keys, editions, drivers), the fast-core choice, and the GPU, texture (BC, ETC2 and its cache), shader and audio code; [docs/building-android.md](../docs/building-android.md) lists the ones that need no device. `rex_minimo/` stands in for the SDK's logging and threads in tests built without the SDK. |
| `icono/` | `icono_adaptativo.py`: the launcher's adaptive icon from the artwork. |
