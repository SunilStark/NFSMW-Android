#pragma once
// nfsmw - the size the game draws at on screens that are not 16:9, for nfsmw_render_targets.cpp's big mode (mode
// 4). Pure, tested on the PC (tools/tests/tamano_pantalla_test.cpp).
//
// Wider screens (a 20:9 phone) keep the height and draw wider, with the camera widened to match (Hor+,
// nfsmw_panoramico.cpp). Narrower ones (a 16:10 tablet) keep the height and draw narrower; there the game's own
// projection already does the right thing: its horizontal angle depends only on the field of view and its vertical
// angle grows with height / width, so a 16:10 target shows the same sides and more above and below (Vert+), with
// square pixels to within 0.3 % at the game's field of view. Two internal sizes are 16:10 whatever the proportion:
// 1056x660, exactly half of the Galaxy Tab S10 Lite's 2112x1320 (a 2x scale to the screen), and 1280x800.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace nfsmw::pantalla {

constexpr double kProporcion169 = 16.0 / 9.0;
constexpr double kMargen = 0.02;  // closer to 16:9 than this: 16:9, not worth a mode
constexpr double kMinima = 4.0 / 3.0;
constexpr double kMaxima = 2.4;
constexpr uint32_t kModoSalida1080p = 3;  // the game's output table: 0 -> 640x480, 2 -> 1280x720, 3 -> 1920x1080

struct Tamano {
  uint32_t ancho;        // set width and visible width, a multiple of 32 like the game's pitches
  uint32_t alto;         // set height: the visible one rounded up to 32, like its own 720 -> 736
  uint32_t visible;      // visible height
  uint32_t modo_salida;  // for the game's output mode global; 0 = the game's own (720p)
  bool otra_proporcion;  // not 16:9
};

// The 16:10 internal sizes of nfsmw_resolucion_interna.
inline bool Es1610(std::string_view resolucion) { return resolucion == "1056x660" || resolucion == "1280x800"; }

inline Tamano Calcular(double proporcion, std::string_view resolucion) {
  if (resolucion == "1056x660") {
    return {1056, 672, 660, 0, true};
  }
  if (resolucion == "1280x800") {
    return {1280, 800, 800, 0, true};
  }
  if (!(proporcion > 0.0) || std::fabs(proporcion - kProporcion169) <= kMargen) {
    return {1920, 1088, 1080, kModoSalida1080p, false};  // the 1080p mode
  }
  const uint32_t visible = resolucion == "1920x1080" ? 1080u : resolucion == "1024x576" ? 576u : 720u;
  const double p = std::clamp(proporcion, kMinima, kMaxima);
  const uint32_t ancho = uint32_t(std::lround(double(visible) * p / 32.0)) * 32u;
  return {ancho, (visible + 31u) & ~31u, visible, visible == 1080 ? kModoSalida1080p : 0u, true};
}

// How much wider than the 16:9 of the same height (2400x1080: 1.25; 1152x720: 0.9).
inline double Factor(const Tamano& t) { return double(t.ancho) * 9.0 / (double(t.visible) * 16.0); }

// Whether a front buffer is shown at its own proportion instead of 16:9: when it is clearly wider (as before), or
// when it is exactly the non-16:9 size the game draws at (a narrower one too). Other sizes keep 16:9, as on the
// Xbox 360.
inline bool ProporcionPropia(uint32_t ancho, uint32_t alto, const Tamano& t) {
  if (uint64_t(ancho) * 9 > uint64_t(alto) * 16 + alto) {
    return true;
  }
  return t.otra_proporcion && ancho == t.ancho && alto == t.visible;
}

}  // namespace nfsmw::pantalla
