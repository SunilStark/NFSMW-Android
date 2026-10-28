// clang++ -std=c++17 tools/tests/tamano_pantalla_test.cpp -o tamano_pantalla_test
// The size the game draws at on screens that are not 16:9 (app/src/nfsmw_tamano_pantalla.h), and which front
// buffers are shown at their own proportion.
#include "../../app/src/nfsmw_tamano_pantalla.h"

#include <cstdio>
#include <cstdlib>

using namespace nfsmw::pantalla;

static void Check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

static bool Es(const Tamano& t, uint32_t ancho, uint32_t alto, uint32_t visible, uint32_t salida, bool otra) {
  return t.ancho == ancho && t.alto == alto && t.visible == visible && t.modo_salida == salida &&
         t.otra_proporcion == otra;
}

int main() {
  // 16:9 and close to it: the 1080p mode, as before.
  Check(Es(Calcular(0.0, "1280x720"), 1920, 1088, 1080, 3, false), "no proportion: the 1080p mode");
  Check(Es(Calcular(16.0 / 9.0, "1280x720"), 1920, 1088, 1080, 3, false), "16:9");
  Check(Es(Calcular(1.79, "1024x576"), 1920, 1088, 1080, 3, false), "within 0.02 of 16:9");
  // Wider (as before): same height, wider, a multiple of 32.
  Check(Es(Calcular(2400.0 / 1080.0, "1920x1080"), 2400, 1088, 1080, 3, true), "20:9 at 1080");
  Check(Es(Calcular(2400.0 / 1080.0, "1280x720"), 1600, 736, 720, 0, true), "20:9 at 720");
  Check(Es(Calcular(3.0, "1280x720"), 1728, 736, 720, 0, true), "at most 2.4");
  // Narrower: the Galaxy Tab S10 Lite (2112x1320 = 1.6).
  const double tableta = 2112.0 / 1320.0;
  Check(Es(Calcular(tableta, "1280x720"), 1152, 736, 720, 0, true), "16:10 at 720: 1152x720");
  Check(Es(Calcular(tableta, "1024x576"), 928, 576, 576, 0, true), "16:10 at 576: 928x576 (1.61)");
  Check(Es(Calcular(tableta, "1920x1080"), 1728, 1088, 1080, 3, true), "16:10 at 1080: 1728x1080");
  Check(Es(Calcular(1.0, "1280x720"), 960, 736, 720, 0, true), "at least 4:3");
  // The 16:10 internal sizes, whatever the proportion.
  Check(Es(Calcular(0.0, "1056x660"), 1056, 672, 660, 0, true), "1056x660: half the tablet's screen");
  Check(Es(Calcular(2.2, "1280x800"), 1280, 800, 800, 0, true), "1280x800");
  Check(Es1610("1056x660") && Es1610("1280x800") && !Es1610("1280x720"), "16:10 sizes");
  for (const char* r : {"1056x660", "1280x800"}) {
    const Tamano t = Calcular(0.0, r);
    Check(t.ancho % 32 == 0 && t.alto % 32 == 0 && t.alto >= t.visible, "multiples of 32, the set covers it");
    Check(std::fabs(double(t.ancho) / double(t.visible) - 1.6) < 1e-9, "exactly 16:10");
  }
  // The factor against 16:9 at the same height: above 1 widens the camera (Hor+), below 1 the game's own Vert+.
  Check(std::fabs(Factor(Calcular(2400.0 / 1080.0, "1920x1080")) - 1.25) < 1e-9, "20:9: 1.25");
  Check(std::fabs(Factor(Calcular(tableta, "1280x720")) - 0.9) < 1e-9, "16:10: 0.9");
  // Front buffers shown at their own proportion.
  const Tamano t1610 = Calcular(tableta, "1280x720");
  const Tamano t169 = Calcular(0.0, "1280x720");
  Check(ProporcionPropia(1152, 720, t1610), "the 16:10 front buffer at 16:10");
  Check(!ProporcionPropia(1280, 720, t1610), "a 16:9 one at 16:9");
  Check(!ProporcionPropia(640, 480, t1610) && !ProporcionPropia(640, 480, t169),
        "another narrow one (640x480) still at 16:9, as on the Xbox 360");
  Check(!ProporcionPropia(1152, 720, t169), "without the 16:10 mode, a narrow one stays 16:9");
  Check(ProporcionPropia(2400, 1080, t169), "a wider one always at its own (as before)");
  std::printf("Screen proportions: OK\n");
  return 0;
}
