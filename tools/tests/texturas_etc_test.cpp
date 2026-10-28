// clang++ -std=c++20 -O2 tools/tests/texturas_etc_test.cpp -o texturas_etc_test
// BC1-5 -> ETC2/EAC (app/src/nfsmw_texturas_etc.h):
//   - the decoder gives back exactly the error the encoder reports, for every block (the bit packing of every
//     mode matches the decoder; the decoder follows the Khronos layouts);
//   - every ETC2 mode is used, and BC1's transparent texels stay transparent and its opaque ones opaque;
//   - quality against the BC texels on synthetic textures (PSNR), next to the 16-bit fallback it replaces;
//   - speed per block.
#include "../../app/src/nfsmw_texturas_etc.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

using nfsmw::bc::Formato;
namespace etc = nfsmw::etc;

static void Check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

// The error the encoder optimizes, recomputed from the decoded ETC block and the decoded BC block.
static int64_t ErrorDecodificado(Formato f, const uint8_t* bc, const uint8_t* etc_bloque) {
  uint8_t esperado[64]{}, obtenido[64]{};
  nfsmw::bc::Bloque(f, bc, esperado);
  int64_t error = 0;
  if (f == Formato::BC4 || f == Formato::BC5) {
    const int canales = f == Formato::BC4 ? 1 : 2;
    for (int c = 0; c < canales; ++c) {
      int valores[16];
      etc::DecodificarEac(etc_bloque + 8 * c, true, valores);
      for (int i = 0; i < 16; ++i) {
        const int objetivo = (esperado[i * canales + c] * 2047 + 127) / 255;
        error += int64_t(valores[i] - objetivo) * (valores[i] - objetivo);
      }
    }
    return error;
  }
  etc::Decodificar(f, etc_bloque, obtenido);
  for (int i = 0; i < 16; ++i) {
    const uint8_t* e = esperado + i * 4;
    const uint8_t* o = obtenido + i * 4;
    if (f == Formato::BC1) {
      Check((e[3] == 0) == (o[3] == 0), "BC1 transparency kept texel by texel");
      if (e[3] == 0) {
        Check(o[0] == 0 && o[1] == 0 && o[2] == 0, "transparent texels are transparent black, as in BC1");
        continue;
      }
      Check(o[3] == 255, "BC1 opaque texels stay opaque");
    }
    for (int c = 0; c < 3; ++c) error += (e[c] - o[c]) * (e[c] - o[c]);
    if (f != Formato::BC1) error += (e[3] - o[3]) * (e[3] - o[3]);
  }
  return error;
}

// The mode of an ETC2 RGB block, for the coverage check: 0 individual, 1 differential, 2 T, 3 H, 4 planar.
static int Modo(const uint8_t* bloque, bool punch) {
  const uint64_t v = etc::LeerBigEndian(bloque);
  if (!punch && !((v >> 33) & 1)) return 0;
  const auto con_signo = [](int x) { return x >= 4 ? x - 8 : x; };
  const int r = int((v >> 59) & 31) + con_signo(int((v >> 56) & 7));
  const int g = int((v >> 51) & 31) + con_signo(int((v >> 48) & 7));
  const int b = int((v >> 43) & 31) + con_signo(int((v >> 40) & 7));
  if (r < 0 || r > 31) return 2;
  if (g < 0 || g > 31) return 3;
  if (b < 0 || b > 31) return 4;
  return 1;
}

// --- A small BC encoder for the test images ---------------------------------------------------------------------
static uint16_t A565(const int* c) {
  return uint16_t(((c[0] * 31 + 127) / 255) << 11 | ((c[1] * 63 + 127) / 255) << 5 | ((c[2] * 31 + 127) / 255));
}

// BC1 (or the color half of BC2/3) from 16 RGBA texels row after row: endpoints at the bounding box's corners
// along the main channel's direction, then the nearest of the four colors. With `transparente`, texels with
// alpha < 128 use the three-color mode's index 3.
static void CodificarBc1(const uint8_t* rgba, bool transparente, uint8_t* bloque) {
  int minimo[3] = {255, 255, 255}, maximo[3] = {0, 0, 0};
  bool hay_transparentes = false;
  for (int i = 0; i < 16; ++i) {
    if (transparente && rgba[i * 4 + 3] < 128) {
      hay_transparentes = true;
      continue;
    }
    for (int c = 0; c < 3; ++c) {
      minimo[c] = std::min(minimo[c], int(rgba[i * 4 + c]));
      maximo[c] = std::max(maximo[c], int(rgba[i * 4 + c]));
    }
  }
  if (minimo[0] > maximo[0]) std::fill(minimo, minimo + 3, 0), std::fill(maximo, maximo + 3, 0);
  uint16_t a = A565(maximo), b = A565(minimo);
  const bool tres = transparente && hay_transparentes;
  if (tres ? a > b : a <= b) std::swap(a, b);
  if (!tres && a == b) {
    if (b > 0) --b; else ++a;
  }
  bloque[0] = uint8_t(a);
  bloque[1] = uint8_t(a >> 8);
  bloque[2] = uint8_t(b);
  bloque[3] = uint8_t(b >> 8);
  std::memset(bloque + 4, 0, 4);
  const etc::Colores paleta = etc::ColoresBc(bloque, transparente);
  for (int i = 0; i < 16; ++i) {
    int mejor = 0;
    if (tres && rgba[i * 4 + 3] < 128) {
      mejor = 3;
    } else {
      int menor = INT_MAX;
      for (int k = 0; k < (tres ? 3 : 4); ++k) {
        int d = 0;
        for (int c = 0; c < 3; ++c) d += (paleta.rgb[k][c] - rgba[i * 4 + c]) * (paleta.rgb[k][c] - rgba[i * 4 + c]);
        if (d < menor) menor = d, mejor = k;
      }
    }
    bloque[4 + i / 4] |= uint8_t(mejor << (2 * (i % 4)));
  }
}

// BC4 from 16 values: endpoints at the extremes, eight-value mode.
static void CodificarBc4(const uint8_t* valores, int paso, uint8_t* bloque) {
  int minimo = 255, maximo = 0;
  for (int i = 0; i < 16; ++i) minimo = std::min(minimo, int(valores[i * paso])), maximo = std::max(maximo, int(valores[i * paso]));
  if (minimo == maximo) maximo = std::min(maximo + 1, 255), minimo = maximo - 1;
  bloque[0] = uint8_t(maximo);
  bloque[1] = uint8_t(minimo);
  uint8_t paleta[16];
  nfsmw::bc::Alfa(bloque, paleta, 1);  // only to get the 8-entry table: indices are set below
  int tabla[8] = {maximo, minimo};
  for (int i = 1; i <= 6; ++i) tabla[i + 1] = ((7 - i) * maximo + i * minimo) / 7;
  uint64_t indices = 0;
  for (int i = 0; i < 16; ++i) {
    int mejor = 0;
    for (int k = 1; k < 8; ++k) {
      if (std::abs(tabla[k] - valores[i * paso]) < std::abs(tabla[mejor] - valores[i * paso])) mejor = k;
    }
    indices |= uint64_t(mejor) << (3 * i);
  }
  for (int i = 0; i < 6; ++i) bloque[2 + i] = uint8_t(indices >> (8 * i));
}

// --- Test images -------------------------------------------------------------------------------------------------
struct Imagen {
  int ancho, alto;
  std::vector<uint8_t> rgba;
};

static Imagen Sintetica(int tipo, std::mt19937& azar) {
  Imagen im{256, 256, std::vector<uint8_t>(256 * 256 * 4)};
  std::uniform_int_distribution<int> u(0, 255);
  // Value noise: a coarse grid of random colors, bilinearly interpolated (roughly like a photo texture).
  std::vector<int> rejilla(17 * 17 * 4);
  for (int& v : rejilla) v = u(azar);
  for (int y = 0; y < 256; ++y) {
    for (int x = 0; x < 256; ++x) {
      uint8_t* t = im.rgba.data() + (y * 256 + x) * 4;
      switch (tipo) {
        case 0:  // smooth gradients in three directions (sky, lighting)
          t[0] = uint8_t(x);
          t[1] = uint8_t((x + y) / 2);
          t[2] = uint8_t(255 - y);
          t[3] = uint8_t(y);
          break;
        case 1: {  // value noise with fine detail on top
          const int gx = x / 16, gy = y / 16, fx = x % 16, fy = y % 16;
          for (int c = 0; c < 4; ++c) {
            const auto g = [&](int i, int j) { return rejilla[((gy + j) * 17 + gx + i) * 4 + c]; };
            const int v = (g(0, 0) * (16 - fx) * (16 - fy) + g(1, 0) * fx * (16 - fy) + g(0, 1) * (16 - fx) * fy +
                           g(1, 1) * fx * fy) / 256;
            t[c] = uint8_t(std::clamp(v + u(azar) / 16 - 8, 0, 255));
          }
          break;
        }
        case 2: {  // hard edges between two saturated colors (signs, decals), cut-out alpha
          const bool dentro = ((x / 7) ^ (y / 5)) & 1;
          t[0] = dentro ? 230 : 20;
          t[1] = dentro ? 40 : 200;
          t[2] = dentro ? 30 : 60;
          t[3] = (x * x + y * y) % 97 < 30 ? 0 : 255;
          break;
        }
        default: {  // noise: the worst case
          for (int c = 0; c < 4; ++c) t[c] = uint8_t(u(azar));
        }
      }
    }
  }
  return im;
}

static void Bloque4x4(const Imagen& im, int bx, int by, uint8_t* rgba) {
  for (int y = 0; y < 4; ++y) {
    std::memcpy(rgba + y * 16, im.rgba.data() + ((by * 4 + y) * im.ancho + bx * 4) * 4, 16);
  }
}

// BC data of an image in one format (blocks row after row).
static std::vector<uint8_t> ABc(const Imagen& im, Formato f) {
  const int bytes = nfsmw::bc::BytesBloque(f);
  std::vector<uint8_t> datos;
  for (int by = 0; by < im.alto / 4; ++by) {
    for (int bx = 0; bx < im.ancho / 4; ++bx) {
      uint8_t rgba[64], bloque[16]{};
      Bloque4x4(im, bx, by, rgba);
      switch (f) {
        case Formato::BC1: CodificarBc1(rgba, true, bloque); break;
        case Formato::BC2:
          for (int i = 0; i < 16; ++i) bloque[i / 2] |= uint8_t((rgba[i * 4 + 3] / 17) << (4 * (i % 2)));
          CodificarBc1(rgba, false, bloque + 8);
          break;
        case Formato::BC3: CodificarBc4(rgba + 3, 4, bloque); CodificarBc1(rgba, false, bloque + 8); break;
        case Formato::BC4: CodificarBc4(rgba, 4, bloque); break;
        case Formato::BC5: CodificarBc4(rgba, 4, bloque); CodificarBc4(rgba + 1, 4, bloque + 8); break;
      }
      datos.insert(datos.end(), bloque, bloque + bytes);
    }
  }
  return datos;
}

// PSNR of the decoded ETC data against the decoded BC data, over the channels the format has.
static double Psnr(Formato f, const std::vector<uint8_t>& bc, const std::vector<uint8_t>& etc_datos, bool a16 = false) {
  const int bytes = nfsmw::bc::BytesBloque(f);
  const int canales = nfsmw::bc::Canales(f);
  double suma = 0;
  int64_t n = 0;
  for (size_t i = 0; i < bc.size(); i += bytes) {
    uint8_t esperado[64]{}, obtenido[64]{};
    nfsmw::bc::Bloque(f, bc.data() + i, esperado);
    if (a16) {
      // The 16-bit fallback (EmpaquetarA16) of BC1: 5:6:5, or 5:5:5:1 with alpha.
      std::memcpy(obtenido, esperado, 64);
      for (int t = 0; t < 16; ++t) {
        uint8_t* p = obtenido + t * 4;
        const int r = (p[0] * 31 + 127) / 255, g = (p[1] * 63 + 127) / 255, b = (p[2] * 31 + 127) / 255;
        p[0] = uint8_t((r << 3) | (r >> 2));
        p[1] = uint8_t((g << 2) | (g >> 4));
        p[2] = uint8_t((b << 3) | (b >> 2));
      }
    } else {
      etc::Decodificar(f, etc_datos.data() + i, obtenido);
    }
    for (int t = 0; t < 16; ++t) {
      if (f == Formato::BC1 && esperado[t * 4 + 3] == 0) continue;
      for (int c = 0; c < std::min(canales, f == Formato::BC1 ? 3 : 4); ++c) {
        const double d = double(esperado[t * canales + c]) - obtenido[t * canales + c];
        suma += d * d;
        ++n;
      }
    }
  }
  const double mse = suma / double(n);
  return mse == 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

int main() {
  std::mt19937 azar(12345);
  const Formato formatos[] = {Formato::BC1, Formato::BC2, Formato::BC3, Formato::BC4, Formato::BC5};
  const char* nombres[] = {"", "BC1", "BC2", "BC3", "BC4", "BC5"};

  // 1. Random blocks, including endpoints in both orders (BC1's two modes): the decoder agrees with the
  //    encoder's error on every block, and every mode of the color encoder shows up.
  int modos[2][5] = {};
  for (const Formato f : formatos) {
    const int bytes = nfsmw::bc::BytesBloque(f);
    for (int n = 0; n < 40000; ++n) {
      uint8_t bc[16], salida[16];
      for (int i = 0; i < bytes; ++i) bc[i] = uint8_t(azar());
      if (n % 4 == 1) {  // near endpoints: few, close colors (most of a real texture)
        bc[bytes - 6] = bc[bytes - 8] ^ uint8_t(azar() & 3);
        bc[bytes - 5] = bc[bytes - 7];
      }
      if (n % 4 == 2) {  // one endpoint unused: three colors or fewer
        for (int i = 0; i < 4; ++i) bc[bytes - 4 + i] &= 0xAA;
      }
      const int64_t error = etc::TranscodificarBloque(f, bc, salida);
      Check(error == ErrorDecodificado(f, bc, salida), "the decoded block has exactly the encoder's error");
      if (f == Formato::BC1 || f == Formato::BC3) ++modos[f == Formato::BC1][Modo(salida + (f == Formato::BC3 ? 8 : 0), f == Formato::BC1)];
    }
  }
  std::printf("Modes over random blocks (individual, differential, T, H, planar): RGB %d %d %d %d %d; RGB8A1 %d %d %d %d %d\n",
              modos[0][0], modos[0][1], modos[0][2], modos[0][3], modos[0][4], modos[1][0], modos[1][1], modos[1][2],
              modos[1][3], modos[1][4]);
  for (int m = 0; m < 5; ++m) Check(modos[0][m] > 0, "every ETC2 RGB mode is used");
  Check(modos[1][0] == 0, "RGB8A1 never uses the individual mode (it does not exist there)");
  for (int m = 1; m < 5; ++m) Check(modos[1][m] > 0, "every RGB8A1 mode is used");

  // 2. Exact cases: a flat color, fully transparent BC1, alpha 0 and 255, R11 at its ends.
  {
    uint8_t bc1[8] = {0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};  // three-color mode, every texel index 3
    uint8_t salida[16], rgba[64];
    etc::TranscodificarBloque(Formato::BC1, bc1, salida);
    etc::Decodificar(Formato::BC1, salida, rgba);
    for (int i = 0; i < 16; ++i) Check(rgba[i * 4 + 3] == 0, "a fully transparent BC1 block stays transparent");
    uint8_t bc3[16] = {255, 255, 0, 0, 0, 0, 0, 0, 0x1F, 0xF8, 0x1F, 0xF8, 0, 0, 0, 0};
    etc::TranscodificarBloque(Formato::BC3, bc3, salida);
    etc::Decodificar(Formato::BC3, salida, rgba);
    for (int i = 0; i < 16; ++i) Check(rgba[i * 4 + 3] == 255, "alpha 255 exact");
    bc3[0] = bc3[1] = 0;
    etc::TranscodificarBloque(Formato::BC3, bc3, salida);
    etc::Decodificar(Formato::BC3, salida, rgba);
    for (int i = 0; i < 16; ++i) Check(rgba[i * 4 + 3] == 0, "alpha 0 exact");
    uint8_t bc4[8] = {255, 0, 0, 0, 0, 0, 0, 0};  // index 0 everywhere: 255
    for (const int valor : {0, 255}) {
      bc4[0] = uint8_t(valor);
      etc::TranscodificarBloque(Formato::BC4, bc4, salida);
      int r11[16];
      etc::DecodificarEac(salida, true, r11);
      for (int i = 0; i < 16; ++i) Check(r11[i] == (valor ? 2047 : 0), "R11 reaches 0 and 2047 exactly");
    }
  }

  // 3. In place, and size checks.
  {
    std::vector<uint8_t> datos(16 * 7);
    for (auto& b : datos) b = uint8_t(azar());
    std::vector<uint8_t> copia = datos, aparte(datos.size());
    Check(etc::Transcodificar(Formato::BC3, copia, aparte), "transcode");
    Check(etc::Transcodificar(Formato::BC3, copia, copia), "transcode in place");
    Check(copia == aparte, "in place gives the same blocks");
    Check(!etc::Transcodificar(Formato::BC3, std::span(datos).first(20), std::span(aparte).first(20)),
          "reject partial blocks");
  }

  // 4. Quality and speed on synthetic textures.
  const char* tipos[] = {"gradients", "value noise", "hard edges", "random noise"};
  const double minimos[4] = {40.0, 33.0, 32.0, 20.0};
  for (int tipo = 0; tipo < 4; ++tipo) {
    const Imagen im = Sintetica(tipo, azar);
    for (const Formato f : formatos) {
      const std::vector<uint8_t> bc = ABc(im, f);
      std::vector<uint8_t> salida(bc.size());
      const auto inicio = std::chrono::steady_clock::now();
      etc::Transcodificar(f, bc, salida);
      const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - inicio).count();
      const double psnr = Psnr(f, bc, salida);
      std::printf("%-13s %s: %5.1f dB against the BC texels", tipos[tipo], nombres[int(f)], psnr);
      if (f == Formato::BC1) std::printf(" (16-bit fallback %5.1f dB)", Psnr(f, bc, salida, true));
      std::printf(", %.2f us per block\n", us / double(bc.size() / nfsmw::bc::BytesBloque(f)));
      Check(psnr >= minimos[tipo], "PSNR above this texture type's floor");
    }
  }
  std::printf("ETC2/EAC transcoding: OK\n");
  return 0;
}
