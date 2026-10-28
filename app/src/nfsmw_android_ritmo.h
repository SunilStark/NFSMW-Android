#pragma once
// nfsmw - ADPF on Android (android/app/src/main/cpp/android_ritmo.cpp): the game's hot threads join the
// performance hint session, and the ring thread reports each frame's work. Nothing on other platforms.

#include <cstdint>

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID
extern "C" void nfsmw_android_ritmo_hilo(const char* papel);  // from the joining thread itself
extern "C" void nfsmw_android_ritmo_fotograma(uint64_t gpu_total_ns);  // once per frame, from the ring thread
// The panel's vsync as the game's vblank (android/app/src/main/cpp/android_vsync.cpp).
extern "C" bool nfsmw_android_vsync_iniciar();
extern "C" int nfsmw_android_vsync_esperar(uint64_t* visto, int64_t* periodo_ns, int espera_ms);
#endif
