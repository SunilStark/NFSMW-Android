cmake_minimum_required(VERSION 3.22.1)
project(nfsmw_android LANGUAGES C CXX)

# Small launcher probe: it must also load on ARMv8.0 phones without loading the game.
add_library(nfsmw_diagnostics SHARED android_diagnostico.cpp)
target_include_directories(nfsmw_diagnostics PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../sdk/thirdparty/vulkan-headers/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../sdk/include")
target_compile_options(nfsmw_diagnostics PRIVATE -march=armv8-a)
target_link_libraries(nfsmw_diagnostics PRIVATE rex_android_gpu_driver dl)
target_link_options(nfsmw_diagnostics PRIVATE "-Wl,-z,max-page-size=16384")

# SDL's Android Activity loads libSDL3.so and then libmain.so. Keep both native
# libraries and the ReXGlue app in this Gradle externalNativeBuild graph.
get_filename_component(NFSMW_REXSDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../sdk" ABSOLUTE)
set(REXSDK_DIR "${NFSMW_REXSDK_DIR}" CACHE PATH "ReXGlue SDK source tree")
set(REXGLUE_USE_VULKAN ON CACHE BOOL "Build ReXGlue's Vulkan backend" FORCE)
set(REXGLUE_USE_D3D12 OFF CACHE BOOL "Android uses Vulkan" FORCE)
set(REXGLUE_ENABLE_TRACY OFF CACHE BOOL "Disable Tracy on Android" FORCE)
set(REXGLUE_VERSION_OVERRIDE "0.10.0-dev.android" CACHE STRING "ReXGlue SDK version for Android")
set(REXGLUE_RECOMP_DEBUG_INFO "none" CACHE STRING "Omit debug tables for the large generated guest code")
set(SDL_SHARED ON CACHE BOOL "SDL shared library loaded by SDLActivity" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Use SDL's shared Android library" FORCE)

# Parallel jobs on a PC with little RAM (-Pnfsmw.nativeJobs / -Pnfsmw.ltoJobs from Gradle). 0: Ninja's own
# choice and every logical core for ThinLTO. Set before the targets are created: CMAKE_JOB_POOL_* only seeds new
# targets. Links get a pool of one, so the ThinLTO link never runs beside other compiles.
set(NFSMW_TRABAJOS 0 CACHE STRING "Compilaciones a la vez (0: las que elija Ninja)")
set(NFSMW_TRABAJOS_LTO 0 CACHE STRING "Hilos de ThinLTO al enlazar (0: todos los nucleos logicos)")
if(NFSMW_TRABAJOS GREATER 0)
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS nfsmw_compilar=${NFSMW_TRABAJOS} nfsmw_enlazar=1)
    set(CMAKE_JOB_POOL_COMPILE nfsmw_compilar)
    set(CMAKE_JOB_POOL_LINK nfsmw_enlazar)
endif()

set(NFSMW_EDITION_APP "app" CACHE STRING "Prepared edition source tree")
if(NOT NFSMW_EDITION_APP MATCHES "^(app|app_[a-z][a-z0-9_]*)$")
    message(FATAL_ERROR "Invalid edition source tree")
endif()
set(NFSMW_EDITION_LIBRARY "main" CACHE STRING "Edition shared library name")
if(NOT NFSMW_EDITION_LIBRARY MATCHES "^(main|main_[a-z][a-z0-9_]*)$")
    message(FATAL_ERROR "Invalid edition library name")
endif()
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../../../${NFSMW_EDITION_APP}" "nfsmw-app")
set_target_properties(nfsmw PROPERTIES OUTPUT_NAME "${NFSMW_EDITION_LIBRARY}")
target_sources(nfsmw PRIVATE "${CMAKE_CURRENT_LIST_DIR}/android_touch.cpp"
                             "${CMAKE_CURRENT_LIST_DIR}/android_rendimiento.cpp"
                             "${CMAKE_CURRENT_LIST_DIR}/android_ritmo.cpp"
                             "${CMAKE_CURRENT_LIST_DIR}/android_vsync.cpp")

if(TARGET SDL3-shared)
    # SDLActivity calls System.loadLibrary("SDL3") in every build variant.
    # CMake otherwise gives RelWithDebInfo libraries an "rd" suffix, which
    # SDLActivity cannot resolve from Java.
    set_target_properties(SDL3-shared PROPERTIES
        OUTPUT_NAME "SDL3"
        DEBUG_POSTFIX ""
        RELEASE_POSTFIX ""
        RELWITHDEBINFO_POSTFIX ""
        MINSIZEREL_POSTFIX ""
    )
endif()

# Package the namespace hooks adrenotools opens from nativeLibraryDir.
add_dependencies(nfsmw main_hook file_redirect_hook gsl_alloc_hook hook_impl)

# The game never needs to access the extracted disc through a SAF URI. Java
# imports it into app-private POSIX storage before GameActivity starts.
target_link_options(nfsmw PRIVATE "-Wl,-z,max-page-size=16384")

# Both libmain and rexruntime contain FFmpeg. Its C symbols are hidden, but
# assembly DSP symbols are not covered by C_VISIBILITY_PRESET. Bind those too:
# otherwise libmain initializes its FFT tables and calls rexruntime's NEON FFT,
# whose larger tables are still empty during the startup movies.
foreach(objetivo nfsmw rexruntime)
    target_link_options(${objetivo} PRIVATE
        "LINKER:--exclude-libs,liblibavcodec.a:liblibavutil.a")
endforeach()

# ThinLTO over the translated game and the app, as the Switch build does with GCC's LTO. The generated code
# already calls __imp__sub_X directly (tools/llamadas_directas.py), so clang can inline game functions across
# the 265 files. Release only: in Debug it would just slow the link down.
option(NFSMW_ANDROID_LTO "ThinLTO en el codigo del juego y de la app (Android, Release)" ON)
if(NFSMW_ANDROID_LTO AND CMAKE_BUILD_TYPE STREQUAL "Release")
    foreach(objetivo nfsmw_recomp nfsmw)
        if(TARGET ${objetivo})
            target_compile_options(${objetivo} PRIVATE -flto=thin)
        endif()
    endforeach()
    if(NFSMW_TRABAJOS_LTO GREATER 0)
        set(NFSMW_NUCLEOS ${NFSMW_TRABAJOS_LTO})
    else()
        cmake_host_system_information(RESULT NFSMW_NUCLEOS QUERY NUMBER_OF_LOGICAL_CORES)
    endif()
    target_link_options(nfsmw PRIVATE -flto=thin "-Wl,--thinlto-jobs=${NFSMW_NUCLEOS}"
                        "-Wl,--thinlto-cache-dir=${CMAKE_BINARY_DIR}/thinlto-cache")
    # Hot code together, as orden_funciones.ld does on the Switch; lld has no section ordering file for GCC,
    # but it can keep the text of each function in its own section and drop the unused ones.
    target_link_options(nfsmw PRIVATE -Wl,--gc-sections -Wl,--icf=safe)
endif()

# A build for one SoC. exynos1380 (Galaxy Tab S10 Lite): its Cortex-A78 and Cortex-A55 are both ARMv8.2-A with
# half-precision arithmetic, dot product and RCpc, so the code may use them on either core; it is scheduled for
# the A78, where the game threads run (android_rendimiento.cpp). Only the game and the app: the SDK's libraries
# and the launcher's probe stay generic. Empty: generic ARMv8-A, for any phone.
set(NFSMW_ANDROID_CPU "" CACHE STRING "SoC de destino: vacio (cualquier ARMv8-A) o exynos1380")
if(NFSMW_ANDROID_CPU STREQUAL "exynos1380")
    set(NFSMW_CPU_OPCIONES -march=armv8.2-a+fp16+dotprod+rcpc -mtune=cortex-a78)
elseif(NFSMW_ANDROID_CPU)
    message(FATAL_ERROR "NFSMW_ANDROID_CPU tiene que ser vacio o exynos1380 (es '${NFSMW_ANDROID_CPU}')")
endif()

# Profile-guided optimization with clang's own profiles, in two steps (the GCC path in app/CMakeLists.txt is the
# Switch's).
#   generar: instrumented build; android_pgo.cpp writes the counters to pgo/nfsmw.profraw in the game folder
#            every minute (Android kills the app instead of letting it exit, so the usual dump at exit never
#            runs). After a session: adb pull it, then llvm-profdata merge -o out/pgo/nfsmw.profdata.
#   usar:    rebuilds with that profile.
set(NFSMW_ANDROID_PGO "" CACHE STRING "Optimizacion por perfil: vacio, generar o usar")
get_filename_component(NFSMW_PGO_PERFIL "${CMAKE_CURRENT_LIST_DIR}/../../../../../out/pgo/nfsmw.profdata" ABSOLUTE)
if(NFSMW_ANDROID_PGO STREQUAL "generar")
    list(APPEND NFSMW_CPU_OPCIONES -fprofile-generate)
    target_link_options(nfsmw PRIVATE -fprofile-generate)
    target_sources(nfsmw PRIVATE "${CMAKE_CURRENT_LIST_DIR}/android_pgo.cpp")
elseif(NFSMW_ANDROID_PGO STREQUAL "usar")
    if(NOT EXISTS "${NFSMW_PGO_PERFIL}")
        message(FATAL_ERROR "Falta el perfil ${NFSMW_PGO_PERFIL} (llvm-profdata merge del .profraw de la tableta)")
    endif()
    # Profiled code that changed since the session is optimized without its profile: warn, do not fail.
    list(APPEND NFSMW_CPU_OPCIONES "-fprofile-use=${NFSMW_PGO_PERFIL}" -Wno-profile-instr-unprofiled
         -Wno-profile-instr-out-of-date -Wno-backend-plugin)
elseif(NFSMW_ANDROID_PGO)
    message(FATAL_ERROR "NFSMW_ANDROID_PGO tiene que ser vacio, generar o usar (es '${NFSMW_ANDROID_PGO}')")
endif()
if(NFSMW_CPU_OPCIONES)
    foreach(objetivo nfsmw_recomp nfsmw)
        if(TARGET ${objetivo})
            target_compile_options(${objetivo} PRIVATE ${NFSMW_CPU_OPCIONES})
        endif()
    endforeach()
    message(STATUS "nfsmw: CPU '${NFSMW_ANDROID_CPU}', PGO '${NFSMW_ANDROID_PGO}': ${NFSMW_CPU_OPCIONES}")
endif()
