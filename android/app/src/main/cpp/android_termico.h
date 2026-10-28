// The decisions of android_ritmo.cpp, apart from Android so they can be tested on the PC
// (tools/tests/android_termico_test.cpp): the thermal step-down level and a frame's work for ADPF.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nfsmw::termico {

// AThermal_getCurrentThermalStatus (android/thermal.h).
enum Estado : int {
  kDesconocido = -1,
  kNinguno = 0,
  kLigero = 1,
  kModerado = 2,  // the system starts throttling
  kSevero = 3,
  kCritico = 4,
};

/*
 * The thermal step-down level, 0 to 2, from the thermal headroom forecast a few seconds ahead
 * (AThermal_getThermalHeadroom: 1.0 is where the system throttles hard, NaN when it does not know) and the
 * thermal status. Level 1 is one shadow map, level 2 no cast shadows: the 30 FPS guard's own steps, which do
 * not flicker (nfsmw_recorte_sombras.cpp).
 *
 * It steps down at once (two readings in a row over the threshold, so one spike does not count) because by then
 * the clocks are about to drop; it steps back up one level at a time and only after `ns_volver` below
 * `volver`, because going back and forth every few seconds looks worse than staying down.
 */
struct Umbrales {
  float nivel1 = 0.85f;
  float nivel2 = 0.95f;
  float volver = 0.70f;
  int lecturas_bajar = 2;
  int64_t ns_volver = 60'000'000'000;
};

class Nivel {
 public:
  explicit Nivel(Umbrales u = {}) : u_(u) {}

  // One reading. Returns the level.
  int Actualizar(float margen, int estado, int64_t ahora_ns) {
    const bool hay_margen = std::isfinite(margen);
    int deseado = estado >= kSevero ? 2 : estado >= kModerado ? 1 : 0;
    if (hay_margen) {
      deseado = std::max(deseado, margen >= u_.nivel2 ? 2 : margen >= u_.nivel1 ? 1 : 0);
    }
    if (deseado > nivel_) {
      volver_desde_ = -1;
      if (++lecturas_altas_ >= u_.lecturas_bajar) {
        nivel_ = deseado;
        lecturas_altas_ = 0;
      }
      return nivel_;
    }
    lecturas_altas_ = 0;
    // Room again: a known margin under `volver` (or, without one, a status of light or none).
    const bool frio = (hay_margen ? margen < u_.volver : true) && estado < kModerado;
    if (nivel_ == 0 || !frio) {
      volver_desde_ = -1;
      return nivel_;
    }
    if (volver_desde_ < 0) {
      volver_desde_ = ahora_ns;
    } else if (ahora_ns - volver_desde_ >= u_.ns_volver) {
      --nivel_;
      volver_desde_ = ahora_ns;  // the next level up waits as long again
    }
    return nivel_;
  }

  int nivel() const { return nivel_; }

 private:
  Umbrales u_;
  int nivel_ = 0;
  int lecturas_altas_ = 0;
  int64_t volver_desde_ = -1;
};

/*
 * A frame's work for ADPF (APerformanceHint_reportActualWorkDuration): the CPU time of the busiest of the
 * game's threads since the last frame. The game is a pipeline (preparer, executor, ring) and the frame rate is
 * set by the slowest stage; CPU time leaves out the waits, so it is the work that has to fit in the frame and
 * that the clocks decide. Thread times of 0 or below are threads that are gone or unreadable. 0 = nothing to
 * report.
 */
inline int64_t TrabajoFotograma(const std::vector<int64_t>& antes, const std::vector<int64_t>& ahora) {
  int64_t maximo = 0;
  for (size_t i = 0; i < std::min(antes.size(), ahora.size()); ++i) {
    if (antes[i] > 0 && ahora[i] > antes[i]) {
      maximo = std::max(maximo, ahora[i] - antes[i]);
    }
  }
  return maximo;
}

// The frame budget for the FPS limit (nfsmw_limite_fps), in ns.
inline int64_t Presupuesto(int fps) {
  return 1'000'000'000LL / (fps > 0 ? fps : 60);
}

}  // namespace nfsmw::termico
