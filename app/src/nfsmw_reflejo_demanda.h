// nfsmw - road reflection on demand. The decision is made in nfsmw_recortes_carrera.cpp.
//
// The reflection (views 4 and 5, a 640x360 or 512x288 pass) was drawn in every race frame: ~210 draws on
// average. In one run 42 % of those draws were for copies nobody read (stretches with no water in view).
// The native renderer records here when it is resolved and when it is read, and the game only draws it
// if it was read recently.
//
// And only if the water is really visible. At the Heritage & Omega start the reflection was read in
// 100 % of frames (175-338 draws per copy) without the sea showing on screen (PC captures at 100, 110
// and 120 s): the water is submitted because it falls inside the frustum, but the terrain covers it
// completely. The ring measures every draw that samples the reflection with an occlusion query and
// records whether it left any sample.
#pragma once

#include <cstdint>

namespace nfsmw::reflejo_demanda {

// Guest physical address where the game resolves the reflection. 0x07C5A000 in every run measured on the PC and
// the console; but EDRAM addresses move with the internal resolution, and on the tablet at 1280x720 it was
// 0x07647000: the guard saw no copy there and the reflection was drawn in every frame, read or not (5 ms of GPU
// per frame on Mali). So the first copy of the reflection's size (640x360, or 512x288 at the small modes) takes
// the address over while kDireccion has never been resolved. nfsmw_recortes_carrera.cpp still checks it in
// every run before skipping any frame.
constexpr uint32_t kDireccion = 0x07C5A000;
uint32_t Direccion();

// From the native renderer (ring thread).
void AnotarLectura();  // a draw samples the reflection texture
// A copy (resolve) to base of ancho x alto; it counts as the reflection if base is Direccion(), and may set it.
void AnotarCopiaDestino(uint32_t base, uint32_t ancho, uint32_t alto);
void AnotarSwap();     // end of a renderer frame

// nfsmw_reflejo_visibilidad. All from the ring thread.
// Whether to measure: the setting is on and the guard has not tripped. Asked once per frame.
bool MedirVisibilidad();
// Whether the witnesses are already verified (visibility decides). Until then the witness is measured
// more often.
bool VisibilidadComprobada();
// A draw that samples the reflection left samples on screen. medido = false: it could not be measured
// (no room for the query, a game query already open, the deferred sky...) and is treated as visible,
// the safe choice.
void AnotarVisible(bool medido);
// A draw that samples the reflection left no samples: the terrain or something else covered it entirely.
void AnotarOculto();
// The witness: the final race composite, which covers the whole screen and must leave samples. If it
// ever gives 0, the queries cannot be trusted and the decision falls back to reads.
void AnotarTestigo(bool con_muestras);

}  // namespace nfsmw::reflejo_demanda
