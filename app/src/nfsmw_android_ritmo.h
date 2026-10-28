#pragma once
// nfsmw - ADPF on Android (android/app/src/main/cpp/android_ritmo.cpp): the game's hot threads join the
// performance hint session, and the ring thread reports each frame's work. Nothing on other platforms.

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID
extern "C" void nfsmw_android_ritmo_hilo(const char* papel);  // from the joining thread itself
extern "C" void nfsmw_android_ritmo_fotograma();             // once per game frame, from the ring thread
#endif
