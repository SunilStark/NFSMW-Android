// clang++ -std=c++17 tools/tests/android_termico_test.cpp -o android_termico_test
// The decisions of android/app/src/main/cpp/android_ritmo.cpp (android_termico.h): the thermal step-down level
// from the headroom and status readings, and a frame's work for ADPF.
#include "../../android/app/src/main/cpp/android_termico.h"

#include <cstdio>
#include <cstdlib>

using namespace nfsmw::termico;

static void Check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

constexpr int64_t kS = 1'000'000'000;

int main() {
  // Readings every 2 s, as the thermal thread takes them.
  {
    Nivel n;
    int64_t t = 0;
    const auto leer = [&](float margen, int estado = kNinguno) { t += 2 * kS; return n.Actualizar(margen, estado, t); };
    Check(leer(0.5f) == 0 && leer(0.84f) == 0, "cool: level 0");
    Check(leer(0.9f) == 0, "one reading over 0.85 is not enough (a spike)");
    Check(leer(0.5f) == 0 && leer(0.9f) == 0, "two that are not in a row are not either");
    Check(leer(0.9f) == 1, "two in a row: one shadow map");
    Check(leer(0.97f) == 1 && leer(0.97f) == 2, "two over 0.95: no cast shadows");
    Check(leer(1.2f) == 2, "it does not go past 2");
    // Cooling down: a minute under 0.70 for each level back.
    int pasos = 0;
    while (leer(0.6f) == 2) ++pasos;
    Check(pasos == 30, "level 2 -> 1 after 60 s under 0.70 (30 readings: the first starts the count)");
    for (int i = 0; i < 10; ++i) leer(0.6f);
    Check(leer(0.75f) == 1, "between 0.70 and 0.85: it holds");
    pasos = 0;
    while (leer(0.6f) == 1) ++pasos;
    Check(pasos == 30, "and the count starts again: another full minute for level 0");
    Check(n.nivel() == 0, "back to the player's shadows");
  }
  // Without the headroom (NaN: Android 11, or too soon): the thermal status alone.
  {
    Nivel n;
    int64_t t = 0;
    const auto leer = [&](int estado) { t += 2 * kS; return n.Actualizar(NAN, estado, t); };
    Check(leer(kLigero) == 0 && leer(kLigero) == 0, "light status: nothing");
    Check(leer(kModerado) == 0 && leer(kModerado) == 1, "moderate (throttling starts): one map");
    Check(leer(kSevero) == 1 && leer(kSevero) == 2, "severe: no cast shadows");
    Check(leer(kModerado) == 2, "moderate again: still too warm to give them back");
    int pasos = 0;
    while (leer(kNinguno) == 2) ++pasos;
    Check(pasos == 30, "status none for a minute: one level back");
  }
  // The status raises the level even when the headroom looks fine, and custom thresholds apply.
  {
    Umbrales u;
    u.nivel1 = 0.6f;
    u.nivel2 = 0.8f;
    u.volver = 0.5f;
    u.ns_volver = 10 * kS;
    Nivel n(u);
    int64_t t = 0;
    const auto leer = [&](float margen, int estado = kNinguno) { t += 2 * kS; return n.Actualizar(margen, estado, t); };
    Check(leer(0.3f, kSevero) == 0 && leer(0.3f, kSevero) == 2, "severe status with a low headroom: level 2");
    Check(leer(0.65f) == 2, "custom thresholds: 0.65 is level 1, but stepping back needs < 0.5");
    int pasos = 0;
    while (leer(0.4f) == 2) ++pasos;
    Check(pasos == 5, "custom time to step back: 10 s");
  }
  // A frame's work: the busiest thread's CPU time since the last frame.
  Check(TrabajoFotograma({100, 200, 300}, {5100, 9200, 4300}) == 9000, "the busiest thread");
  Check(TrabajoFotograma({0, 200}, {5000, 1200}) == 1000, "a thread with no earlier reading does not count");
  Check(TrabajoFotograma({100, 200}, {100, 0}) == 0, "an idle thread and an unreadable one: nothing to report");
  Check(TrabajoFotograma({100}, {600, 900}) == 500, "lists of different sizes (a thread just joined)");
  Check(Presupuesto(60) == 16666666 && Presupuesto(90) == 11111111 && Presupuesto(30) == 33333333,
        "frame budgets");
  Check(Presupuesto(0) == 16666666, "an unreadable limit is 60 FPS");
  std::printf("Thermal step-down and ADPF work: OK\n");
  return 0;
}
