// BC1-5 transcoded to ETC2/EAC, block by block, for GPUs that sample ETC2 but not BC (Mali, PowerVR):
//   BC1 -> ETC2 RGB8A1 (its 1-bit alpha is punch-through, as in BC1), BC2 and BC3 -> ETC2 RGBA8 (EAC alpha),
//   BC4 -> EAC R11, BC5 -> EAC RG11.
// Each output block is the size of its input block (8 or 16 bytes for 4x4 texels), so a texture keeps the BC
// layout and the level offsets of the native BC path. The target of each block is what bc::Bloque decodes,
// which is what these GPUs showed before. GPU-independent, tested on the PC (tools/tests/texturas_etc_test.cpp).
//
// Block layouts: Khronos Data Format Specification 1.3, sections ETC2 and EAC (the T, H and planar bit fields
// checked against etcpak's decoder, the RGB8A1 rules against etc2comp's). Blocks are big-endian 64-bit words.
// Pixel p of a block is p = x * 4 + y (column after column), the order of ETC's index bits.
//
// The encoder works on the block's palette: a BC1-3 color block has at most four colors, a BC4 or BC3 alpha block
// at most eight values, so each candidate is scored per palette entry with its pixel count instead of per pixel.
#pragma once

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <span>

#include "nfsmw_texturas_bc.h"

namespace nfsmw::etc {
using bc::Formato;

// Encoder version: part of the disk cache's key, so a change here makes every cached texture be encoded again.
inline constexpr uint32_t kVersion = 1;

inline constexpr int kModificadores[8][2] = {{2, 8},   {5, 17},  {9, 29},  {13, 42},
                                             {18, 60}, {24, 80}, {33, 106}, {47, 183}};
inline constexpr int kDistancias[8] = {3, 6, 11, 16, 23, 32, 41, 64};
inline constexpr int kTablasEac[16][8] = {
    {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12},
    {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11},  {-3, -7, -9, -11, 2, 6, 8, 10},
    {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10},  {-2, -6, -8, -10, 1, 5, 7, 9},
    {-2, -5, -8, -10, 1, 4, 7, 9},  {-2, -4, -8, -10, 1, 3, 7, 9},   {-2, -5, -7, -10, 1, 4, 6, 9},
    {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},   {-4, -6, -8, -9, 3, 5, 7, 8},
    {-3, -5, -7, -9, 2, 4, 6, 8}};

inline int Saturar(int v, int maximo = 255) { return v < 0 ? 0 : v > maximo ? maximo : v; }

inline int Expandir(int q, int bits) {
  switch (bits) {
    case 4: return (q << 4) | q;
    case 5: return (q << 3) | (q >> 2);
    case 6: return (q << 2) | (q >> 4);
    default: return (q << 1) | (q >> 6);
  }
}

// The `bits`-bit value whose expansion to 8 bits is nearest to v.
inline int Cuantizar(int v, int bits) {
  const int maximo = (1 << bits) - 1;
  v = Saturar(v);
  const int q = (v * maximo + 127) / 255;
  int mejor = q;
  int error = std::abs(Expandir(q, bits) - v);
  for (const int c : {q - 1, q + 1}) {
    if (c >= 0 && c <= maximo && std::abs(Expandir(c, bits) - v) < error) {
      mejor = c;
      error = std::abs(Expandir(c, bits) - v);
    }
  }
  return mejor;
}

// Rounded integer division, also for negative numerators.
inline int Dividir(int n, int d) { return n >= 0 ? (n + d / 2) / d : -((-n + d / 2) / d); }

inline int Distancia2(const int* a, int r, int g, int b) {
  return (a[0] - r) * (a[0] - r) + (a[1] - g) * (a[1] - g) + (a[2] - b) * (a[2] - b);
}

// ETC1 modifier of pixel index i (msb * 2 + lsb): +a, +b, -a, -b. Without the opaque bit (RGB8A1) index 2 is
// transparent and index 0 adds nothing.
inline int Modificador(int tabla, int i, bool opaco) {
  const int a = kModificadores[tabla][0];
  const int b = kModificadores[tabla][1];
  switch (i) {
    case 0: return opaco ? a : 0;
    case 1: return b;
    case 2: return -a;
    default: return -b;
  }
}

inline void EscribirBigEndian(uint64_t v, uint8_t* salida) {
  for (int i = 0; i < 8; ++i) salida[i] = uint8_t(v >> (56 - 8 * i));
}

inline uint64_t LeerBigEndian(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
  return v;
}

// --- Color: ETC2 RGB and RGB8A1 ----------------------------------------------------------------------------------

struct Colores {
  int rgb[4][3]{};             // the block's palette (a BC color block's four colors)
  uint8_t entrada[16]{};       // palette entry of each pixel p
  uint16_t transparentes = 0;  // bit p: transparent pixel (index 3 of BC1's three-color mode)
};

struct Codificado {
  uint64_t bloque = 0;
  int error = INT_MAX;  // sum of squared RGB differences over the opaque pixels
};

// The 32 index bits from each pixel's index (msb * 2 + lsb): msb of pixel p at bit 16 + p, lsb at bit p.
inline uint64_t BitsIndices(const uint8_t indice[16]) {
  uint64_t bits = 0;
  for (int p = 0; p < 16; ++p) {
    bits |= uint64_t(indice[p] >> 1) << (16 + p);
    bits |= uint64_t(indice[p] & 1) << p;
  }
  return bits;
}

// Half 0 is the left 2x4 pixels (flip 0) or the top 4x2 (flip 1).
inline int MitadDe(int p, bool flip) { return flip ? (p & 3) >> 1 : p >> 3; }

struct Mitad {
  int error = INT_MAX;
  int q[3]{};           // base color at 4 or 5 bits
  int tabla = 0;
  uint8_t indice[4]{};  // index of each palette entry
  bool vacia = false;   // no opaque pixel: any base will do
};

// Error of a half with this base (8 bits) and table; each palette entry takes its best index.
inline int ErrorTabla(const Colores& c, const int cuenta[4], const int base[3], int tabla, bool opaco,
                      uint8_t indice[4], int limite) {
  const int b = kModificadores[tabla][1];
  const bool sin_saturar = std::min({base[0], base[1], base[2]}) - b >= 0 &&
                           std::max({base[0], base[1], base[2]}) + b <= 255;
  int total = 0;
  for (int k = 0; k < 4 && total < limite; ++k) {
    if (!cuenta[k]) continue;
    int mejor = INT_MAX;
    if (sin_saturar) {
      // Without clamping, sum((base + m - c)^2) = D + 2 m S + 3 m^2, with S = sum(base - c).
      const int d0 = base[0] - c.rgb[k][0], d1 = base[1] - c.rgb[k][1], d2 = base[2] - c.rgb[k][2];
      const int d = d0 * d0 + d1 * d1 + d2 * d2;
      const int s = d0 + d1 + d2;
      for (int i = 0; i < 4; ++i) {
        if (!opaco && i == 2) continue;
        const int m = Modificador(tabla, i, opaco);
        const int e = d + 2 * m * s + 3 * m * m;
        if (e < mejor) {
          mejor = e;
          indice[k] = uint8_t(i);
        }
      }
    } else {
      for (int i = 0; i < 4; ++i) {
        if (!opaco && i == 2) continue;
        const int m = Modificador(tabla, i, opaco);
        const int e = Distancia2(c.rgb[k], Saturar(base[0] + m), Saturar(base[1] + m), Saturar(base[2] + m));
        if (e < mejor) {
          mejor = e;
          indice[k] = uint8_t(i);
        }
      }
    }
    total += mejor * cuenta[k];
  }
  return total;
}

// Best table for a base at `bits` bits; keeps it in m if it beats what m has.
inline void BuscarTabla(const Colores& c, const int cuenta[4], const int q[3], int bits, bool opaco, Mitad& m) {
  const int base[3] = {Expandir(q[0], bits), Expandir(q[1], bits), Expandir(q[2], bits)};
  for (int t = 0; t < 8; ++t) {
    uint8_t indice[4]{};
    const int e = ErrorTabla(c, cuenta, base, t, opaco, indice, m.error);
    if (e < m.error) {
      m.error = e;
      m.tabla = t;
      std::copy(q, q + 3, m.q);
      std::copy(indice, indice + 4, m.indice);
    }
  }
}

// Base and table of a half: the base from the average color, then once more from the average of
// (color - chosen modifier).
inline Mitad BuscarMitad(const Colores& c, const int cuenta[4], int bits, bool opaco) {
  Mitad m;
  int total = 0;
  int suma[3] = {};
  for (int k = 0; k < 4; ++k) {
    total += cuenta[k];
    for (int ch = 0; ch < 3; ++ch) suma[ch] += cuenta[k] * c.rgb[k][ch];
  }
  if (!total) {
    m.error = 0;
    m.vacia = true;
    return m;
  }
  int q[3];
  for (int ch = 0; ch < 3; ++ch) q[ch] = Cuantizar(Dividir(suma[ch], total), bits);
  BuscarTabla(c, cuenta, q, bits, opaco, m);
  if (m.error == 0) return m;
  int q2[3];
  for (int ch = 0; ch < 3; ++ch) {
    int s = 0;
    for (int k = 0; k < 4; ++k) {
      if (cuenta[k]) s += cuenta[k] * (c.rgb[k][ch] - Modificador(m.tabla, m.indice[k], opaco));
    }
    q2[ch] = Cuantizar(Dividir(s, total), bits);
  }
  if (!std::equal(q2, q2 + 3, m.q)) BuscarTabla(c, cuenta, q2, bits, opaco, m);
  return m;
}

// Individual (diferencial = false: two 4-bit colors) or differential layout (a 5-bit color and a 3-bit
// difference). `bit33` is the diff bit, or the opaque bit in RGB8A1.
inline uint64_t EmpaquetarEtc1(bool diferencial, bool bit33, bool flip, const Mitad& a, const Mitad& b,
                               const Colores& c) {
  uint64_t v = 0;
  if (diferencial) {
    for (int ch = 0; ch < 3; ++ch) {
      const int delta = b.q[ch] - a.q[ch];
      v |= uint64_t(a.q[ch]) << (59 - 8 * ch);
      v |= uint64_t(delta & 7) << (56 - 8 * ch);
    }
  } else {
    for (int ch = 0; ch < 3; ++ch) {
      v |= uint64_t(a.q[ch]) << (60 - 8 * ch);
      v |= uint64_t(b.q[ch]) << (56 - 8 * ch);
    }
  }
  v |= uint64_t(a.tabla) << 37;
  v |= uint64_t(b.tabla) << 34;
  v |= uint64_t(bit33) << 33;
  v |= uint64_t(flip) << 32;
  uint8_t indice[16];
  for (int p = 0; p < 16; ++p) {
    indice[p] = (c.transparentes >> p) & 1 ? 2 : (MitadDe(p, flip) ? b : a).indice[c.entrada[p]];
  }
  return v | BitsIndices(indice);
}

inline bool CabeDiferencia(const int a[3], const int b[3]) {
  for (int ch = 0; ch < 3; ++ch) {
    if (b[ch] - a[ch] < -4 || b[ch] - a[ch] > 3) return false;
  }
  return true;
}

// Individual and differential modes for one flip.
inline void ProbarEtc1(const Colores& c, bool flip, bool punch, bool opaco, Codificado& mejor) {
  int cuenta[2][4] = {};
  for (int p = 0; p < 16; ++p) {
    if (!((c.transparentes >> p) & 1)) ++cuenta[MitadDe(p, flip)][c.entrada[p]];
  }
  const bool bit33 = punch ? opaco : true;
  Mitad a = BuscarMitad(c, cuenta[0], 5, opaco);
  Mitad b = BuscarMitad(c, cuenta[1], 5, opaco);
  if (a.vacia) std::copy(b.q, b.q + 3, a.q);
  if (b.vacia) std::copy(a.q, a.q + 3, b.q);
  if (CabeDiferencia(a.q, b.q)) {
    if (a.error + b.error < mejor.error) {
      mejor.error = a.error + b.error;
      mejor.bloque = EmpaquetarEtc1(true, bit33, flip, a, b, c);
    }
    return;  // two 5-bit bases that fit: the individual mode's 4 bits would not do better
  }
  if (!punch) {  // RGB8A1 has no individual mode
    const Mitad ia = BuscarMitad(c, cuenta[0], 4, true);
    const Mitad ib = BuscarMitad(c, cuenta[1], 4, true);
    if (ia.error + ib.error < mejor.error) {
      mejor.error = ia.error + ib.error;
      mejor.bloque = EmpaquetarEtc1(false, false, flip, ia, ib, c);
    }
  }
  // Too far apart for the 3-bit difference: one half's base moves toward the other's.
  for (int lado = 0; lado < 2; ++lado) {
    const Mitad& fija = lado ? b : a;
    int q[3];
    for (int ch = 0; ch < 3; ++ch) {
      const int delta = std::clamp(b.q[ch] - a.q[ch], -4, 3);
      q[ch] = lado ? b.q[ch] - delta : a.q[ch] + delta;
    }
    Mitad movida;
    BuscarTabla(c, cuenta[lado ? 0 : 1], q, 5, opaco, movida);
    if (fija.error + movida.error < mejor.error) {
      mejor.error = fija.error + movida.error;
      mejor.bloque = lado ? EmpaquetarEtc1(true, bit33, flip, movida, b, c)
                          : EmpaquetarEtc1(true, bit33, flip, a, movida, c);
    }
  }
}

// Error of four paint colors (T and H modes); each palette entry takes its best one. Without the opaque bit,
// paint 2 is transparent.
inline int ErrorPinturas(const Colores& c, const int cuenta[4], const int pintura[4][3], bool opaco,
                         uint8_t indice[4], int limite) {
  int total = 0;
  for (int k = 0; k < 4 && total < limite; ++k) {
    if (!cuenta[k]) continue;
    int mejor = INT_MAX;
    for (int i = 0; i < 4; ++i) {
      if (!opaco && i == 2) continue;
      const int e = Distancia2(c.rgb[k], pintura[i][0], pintura[i][1], pintura[i][2]);
      if (e < mejor) {
        mejor = e;
        indice[k] = uint8_t(i);
      }
    }
    total += mejor * cuenta[k];
  }
  return total;
}

inline uint64_t IndicesPinturas(const Colores& c, const uint8_t indice[4]) {
  uint8_t por_pixel[16];
  for (int p = 0; p < 16; ++p) por_pixel[p] = (c.transparentes >> p) & 1 ? 2 : indice[c.entrada[p]];
  return BitsIndices(por_pixel);
}

// T mode: R of the differential layout overflows. R1 is split into bits 60-59 and 57-56; bits 63-61 and 58
// make R + dR leave 0-31.
inline uint64_t EmpaquetarT(const int q1[3], const int q2[3], int distancia, bool bit33, uint64_t indices) {
  const int ra = q1[0] >> 2, rb = q1[0] & 3;
  uint64_t v = ra + rb >= 4 ? (uint64_t(7) << 61) : (uint64_t(1) << 58);
  v |= uint64_t(ra) << 59 | uint64_t(rb) << 56 | uint64_t(q1[1]) << 52 | uint64_t(q1[2]) << 48;
  v |= uint64_t(q2[0]) << 44 | uint64_t(q2[1]) << 40 | uint64_t(q2[2]) << 36;
  v |= uint64_t(distancia >> 1) << 34 | uint64_t(bit33) << 33 | uint64_t(distancia & 1) << 32;
  return v | indices;
}

// H mode: G overflows and R does not. The distance's lowest bit is whether the first color (as a 12-bit
// number) is not below the second.
inline uint64_t EmpaquetarH(const int q1[3], const int q2[3], int distancia, bool bit33, uint64_t indices) {
  uint64_t v = uint64_t(q1[0]) << 59 | uint64_t(q1[1] >> 1) << 56 | uint64_t(q1[1] & 1) << 52 |
               uint64_t(q1[2] >> 3) << 51 | uint64_t(q1[2] & 7) << 47;
  v |= uint64_t(q2[0]) << 43 | uint64_t(q2[1]) << 39 | uint64_t(q2[2]) << 35;
  v |= uint64_t((distancia >> 2) & 1) << 34 | uint64_t(bit33) << 33 | uint64_t((distancia >> 1) & 1) << 32;
  // R = bit 63 and R1; dR = the top three bits of G1, as a signed number: bit 63 keeps R + dR within 0-31.
  const int dr = int((v >> 56) & 7) >= 4 ? int((v >> 56) & 7) - 8 : int((v >> 56) & 7);
  if (q1[0] + dr < 0) v |= uint64_t(1) << 63;
  // G = bits 55-53, the low bit of G1 and the top bit of B1; dG = bit 50 and bits 49-48: G + dG leaves 0-31.
  const int bajos = int((v >> 51) & 3), dg = int((v >> 48) & 3);
  v |= bajos + dg >= 4 ? (uint64_t(7) << 53) : (uint64_t(1) << 50);
  return v | indices;
}

inline int Valor12(const int q[3]) { return (q[0] << 8) | (q[1] << 4) | q[2]; }

// T and H modes: the palette entries in use, along the line of the block's colors, split in two groups.
inline void ProbarTH(const Colores& c, bool punch, bool opaco, Codificado& mejor) {
  int cuenta[4] = {};
  for (int p = 0; p < 16; ++p) {
    if (!((c.transparentes >> p) & 1)) ++cuenta[c.entrada[p]];
  }
  int orden[4];
  int n = 0;
  for (int k = 0; k < 4; ++k) {
    if (cuenta[k]) orden[n++] = k;
  }
  if (n < 2) return;
  const int eje[3] = {c.rgb[1][0] - c.rgb[0][0], c.rgb[1][1] - c.rgb[0][1], c.rgb[1][2] - c.rgb[0][2]};
  const auto proyeccion = [&](int k) {
    return (c.rgb[k][0] - c.rgb[0][0]) * eje[0] + (c.rgb[k][1] - c.rgb[0][1]) * eje[1] +
           (c.rgb[k][2] - c.rgb[0][2]) * eje[2];
  };
  std::sort(orden, orden + n, [&](int x, int y) { return proyeccion(x) < proyeccion(y); });
  const bool bit33 = punch ? opaco : true;
  for (int corte = 1; corte < n; ++corte) {
    int centro[2][3] = {};
    int peso[2] = {};
    for (int j = 0; j < n; ++j) {
      const int g = j >= corte;
      peso[g] += cuenta[orden[j]];
      for (int ch = 0; ch < 3; ++ch) centro[g][ch] += cuenta[orden[j]] * c.rgb[orden[j]][ch];
    }
    int q[2][3];
    for (int g = 0; g < 2; ++g) {
      for (int ch = 0; ch < 3; ++ch) q[g][ch] = Cuantizar(Dividir(centro[g][ch], peso[g]), 4);
    }
    for (int primero = 0; primero < 2; ++primero) {
      const int* q1 = q[primero];
      const int* q2 = q[1 - primero];
      const int e1[3] = {Expandir(q1[0], 4), Expandir(q1[1], 4), Expandir(q1[2], 4)};
      const int e2[3] = {Expandir(q2[0], 4), Expandir(q2[1], 4), Expandir(q2[2], 4)};
      // H: q1 +- d, q2 +- d; only the distances whose lowest bit matches the colors' order.
      const int bajo = Valor12(q1) >= Valor12(q2) ? 1 : 0;
      for (int di = bajo; di < 8; di += 2) {
        const int d = kDistancias[di];
        const int pintura[4][3] = {{Saturar(e1[0] + d), Saturar(e1[1] + d), Saturar(e1[2] + d)},
                                   {Saturar(e1[0] - d), Saturar(e1[1] - d), Saturar(e1[2] - d)},
                                   {Saturar(e2[0] + d), Saturar(e2[1] + d), Saturar(e2[2] + d)},
                                   {Saturar(e2[0] - d), Saturar(e2[1] - d), Saturar(e2[2] - d)}};
        uint8_t indice[4]{};
        const int e = ErrorPinturas(c, cuenta, pintura, opaco, indice, mejor.error);
        if (e < mejor.error) {
          mejor.error = e;
          mejor.bloque = EmpaquetarH(q1, q2, di, bit33, IndicesPinturas(c, indice));
        }
      }
      // T: q1 alone, q2 and q2 +- d.
      for (int di = 0; di < 8; ++di) {
        const int d = kDistancias[di];
        const int pintura[4][3] = {{e1[0], e1[1], e1[2]},
                                   {Saturar(e2[0] + d), Saturar(e2[1] + d), Saturar(e2[2] + d)},
                                   {e2[0], e2[1], e2[2]},
                                   {Saturar(e2[0] - d), Saturar(e2[1] - d), Saturar(e2[2] - d)}};
        uint8_t indice[4]{};
        const int e = ErrorPinturas(c, cuenta, pintura, opaco, indice, mejor.error);
        if (e < mejor.error) {
          mejor.error = e;
          mejor.bloque = EmpaquetarT(q1, q2, di, bit33, IndicesPinturas(c, indice));
        }
      }
    }
  }
}

// Planar mode: a plane per channel through the block, O at pixel (0,0), H at (4,0), V at (0,4):
// c(x, y) = (x (H - O) + y (V - O) + 4 O + 2) >> 2, with O, H, V at 6, 7, 6 bits for R, G, B.
inline int ValorPlanar(int o, int h, int v, int x, int y) { return Saturar((x * (h - o) + y * (v - o) + 4 * o + 2) >> 2); }

inline uint64_t EmpaquetarPlanar(const int o[3], const int h[3], const int v[3]) {
  uint64_t b = uint64_t(o[0]) << 57 | uint64_t(o[1] >> 6) << 56 | uint64_t(o[1] & 63) << 49 |
               uint64_t(o[2] >> 5) << 48 | uint64_t((o[2] >> 3) & 3) << 43 | uint64_t(o[2] & 7) << 39;
  b |= uint64_t(h[0] >> 1) << 34 | uint64_t(1) << 33 | uint64_t(h[0] & 1) << 32;
  b |= uint64_t(h[1]) << 25 | uint64_t(h[2]) << 19 | uint64_t(v[0]) << 13 | uint64_t(v[1]) << 6 | uint64_t(v[2]);
  // R and G of the differential layout must stay within 0-31 (bits 63 and 55), B must leave it (bits 47-45, 42).
  const auto con_signo = [](int x) { return x >= 4 ? x - 8 : x; };
  if (int((b >> 59) & 15) + con_signo(int((b >> 56) & 7)) < 0) b |= uint64_t(1) << 63;
  if (int((b >> 51) & 15) + con_signo(int((b >> 48) & 7)) < 0) b |= uint64_t(1) << 55;
  const int bajos = int((b >> 43) & 3), db = int((b >> 40) & 3);
  b |= bajos + db >= 4 ? (uint64_t(7) << 45) : (uint64_t(1) << 42);
  return b;
}

inline void ProbarPlanar(const Colores& c, Codificado& mejor) {
  int o[3], h[3], v[3];
  int total = 0;
  for (int ch = 0; ch < 3; ++ch) {
    // Least squares with x' = 2x - 3 and y' = 2y - 3 (-3, -1, 1, 3): sum(x'^2) = 80 over the block.
    int s = 0, sx = 0, sy = 0;
    for (int p = 0; p < 16; ++p) {
      const int valor = c.rgb[c.entrada[p]][ch];
      s += valor;
      sx += (2 * (p >> 2) - 3) * valor;
      sy += (2 * (p & 3) - 3) * valor;
    }
    // Per pixel step: sx / 40; O = mean - 1.5 steps in x and in y; H and V four steps further.
    const double paso_x = sx / 40.0, paso_y = sy / 40.0;
    const double origen = s / 16.0 - 1.5 * paso_x - 1.5 * paso_y;
    const int bits = ch == 1 ? 7 : 6;
    const int qo = Cuantizar(int(origen + 0.5 - (origen < 0)), bits);
    const int qh = Cuantizar(int(origen + 4 * paso_x + 0.5 - (origen + 4 * paso_x < 0)), bits);
    const int qv = Cuantizar(int(origen + 4 * paso_y + 0.5 - (origen + 4 * paso_y < 0)), bits);
    // The plane's channel is independent of the others: its neighbors at 1 step are tried.
    const int maximo = (1 << bits) - 1;
    int mejor_error = INT_MAX;
    for (int a = std::max(qo - 1, 0); a <= std::min(qo + 1, maximo); ++a) {
      for (int b = std::max(qh - 1, 0); b <= std::min(qh + 1, maximo); ++b) {
        for (int d = std::max(qv - 1, 0); d <= std::min(qv + 1, maximo); ++d) {
          const int eo = Expandir(a, bits), eh = Expandir(b, bits), ev = Expandir(d, bits);
          int e = 0;
          for (int p = 0; p < 16 && e < mejor_error; ++p) {
            const int diferencia = ValorPlanar(eo, eh, ev, p >> 2, p & 3) - c.rgb[c.entrada[p]][ch];
            e += diferencia * diferencia;
          }
          if (e < mejor_error) {
            mejor_error = e;
            o[ch] = a;
            h[ch] = b;
            v[ch] = d;
          }
        }
      }
    }
    total += mejor_error;
    if (total >= mejor.error) return;
  }
  mejor.error = total;
  mejor.bloque = EmpaquetarPlanar(o, h, v);
}

// One ETC2 RGB block (punch = false; individual mode allowed) or RGB8A1 block (punch = true).
inline Codificado CodificarColor(const Colores& c, bool punch) {
  Codificado mejor;
  const bool opaco = c.transparentes == 0;
  for (const bool flip : {false, true}) {
    ProbarEtc1(c, flip, punch, opaco, mejor);
    if (mejor.error == 0) return mejor;
  }
  ProbarTH(c, punch, opaco, mejor);
  if (opaco && mejor.error) ProbarPlanar(c, mejor);
  return mejor;
}

// --- EAC: R11 and the alpha of RGBA8 -----------------------------------------------------------------------------

// The 16 values of a block (alpha 0-255, or R11 0-2047), counted per distinct value.
struct Valores {
  int valor[16]{};  // by pixel p
  int distinto[16]{};
  int cuenta[16]{};
  int n = 0;
};

inline void Contar(Valores& v) {
  v.n = 0;
  for (int p = 0; p < 16; ++p) {
    int j = 0;
    while (j < v.n && v.distinto[j] != v.valor[p]) ++j;
    if (j == v.n) {
      v.distinto[v.n] = v.valor[p];
      v.cuenta[v.n++] = 0;
    }
    ++v.cuenta[j];
  }
}

// The 8 values of base, multiplier and table: 8-bit alpha, or R11 (base * 8 + 4, multiplier * 8, 0 = 1).
inline void PaletaEac(int base, int multiplicador, int tabla, bool once, int paleta[8]) {
  for (int i = 0; i < 8; ++i) {
    paleta[i] = once ? Saturar(base * 8 + 4 + kTablasEac[tabla][i] * (multiplicador ? multiplicador * 8 : 1), 2047)
                     : Saturar(base + kTablasEac[tabla][i] * multiplicador);
  }
}

inline int ErrorEac(const Valores& v, const int paleta[8], int limite) {
  int total = 0;
  for (int j = 0; j < v.n && total < limite; ++j) {
    int mejor = INT_MAX;
    for (int i = 0; i < 8; ++i) mejor = std::min(mejor, (paleta[i] - v.distinto[j]) * (paleta[i] - v.distinto[j]));
    total += mejor * v.cuenta[j];
  }
  return total;
}

struct Eac {
  uint64_t bloque = 0;
  int error = INT_MAX;  // sum of squared differences, in 8 or 11 bits
};

inline uint64_t EmpaquetarEac(const Valores& v, int base, int multiplicador, int tabla, bool once) {
  int paleta[8];
  PaletaEac(base, multiplicador, tabla, once, paleta);
  uint64_t b = uint64_t(base) << 56 | uint64_t(multiplicador) << 52 | uint64_t(tabla) << 48;
  for (int p = 0; p < 16; ++p) {
    int mejor = 0;
    for (int i = 1; i < 8; ++i) {
      if (std::abs(paleta[i] - v.valor[p]) < std::abs(paleta[mejor] - v.valor[p])) mejor = i;
    }
    b |= uint64_t(mejor) << (45 - 3 * p);
  }
  return b;
}

// One EAC block. Each table gets the base and multiplier that span the block's range; the three best are then
// tried with their neighbors. The multiplier is never 0 (in 8-bit alpha it is not needed: table 13 has a 0).
inline Eac CodificarEac(Valores v, bool once) {
  Contar(v);
  const int minimo = *std::min_element(v.valor, v.valor + 16);
  const int maximo = *std::max_element(v.valor, v.valor + 16);
  struct Candidato {
    int error = INT_MAX, base = 0, multiplicador = 1, tabla = 0;
  };
  Candidato mejores[2];
  const auto probar = [&](int base, int multiplicador, int tabla, Candidato* lista, int tam) {
    int paleta[8];
    PaletaEac(base, multiplicador, tabla, once, paleta);
    const int e = ErrorEac(v, paleta, lista[tam - 1].error);
    if (e >= lista[tam - 1].error) return;
    int i = tam - 1;
    while (i > 0 && lista[i - 1].error > e) {
      lista[i] = lista[i - 1];
      --i;
    }
    lista[i] = {e, base, multiplicador, tabla};
  };
  const int escala = once ? 8 : 1;
  for (int t = 0; t < 16; ++t) {
    const int bajo = kTablasEac[t][3], alto = kTablasEac[t][7];
    const int multiplicador = std::clamp(Dividir(maximo - minimo, (alto - bajo) * escala), 1, 15);
    const int centro = (minimo + maximo) / 2 - (once ? 4 : 0) - (bajo + alto) * multiplicador * escala / 2;
    probar(Saturar(Dividir(centro, escala)), multiplicador, t, mejores, 2);
  }
  Candidato mejor = mejores[0];
  for (const Candidato& c : mejores) {
    if (c.error == INT_MAX || mejor.error == 0) break;
    for (int m = std::max(c.multiplicador - 1, 1); m <= std::min(c.multiplicador + 1, 15); ++m) {
      for (int b = std::max(c.base - 1, 0); b <= std::min(c.base + 1, 255); ++b) probar(b, m, c.tabla, &mejor, 1);
    }
  }
  return {EmpaquetarEac(v, mejor.base, mejor.multiplicador, mejor.tabla, once), mejor.error};
}

// --- From BC -----------------------------------------------------------------------------------------------------

// The palette and pixels of a BC color block (bytes as the GPU reads them), with bc::Bloque's arithmetic.
// Only BC1 has the three-color mode (first endpoint not above the second), whose index 3 is transparent.
inline Colores ColoresBc(const uint8_t* color, bool bc1) {
  Colores c;
  const uint16_t a = bc::Leer16(color), b = bc::Leer16(color + 2);
  for (int e = 0; e < 2; ++e) {
    const uint32_t v = e ? b : a;
    const uint32_t r = v >> 11, g = (v >> 5) & 63, z = v & 31;
    c.rgb[e][0] = int((r << 3) | (r >> 2));
    c.rgb[e][1] = int((g << 2) | (g >> 4));
    c.rgb[e][2] = int((z << 3) | (z >> 2));
  }
  const bool tres = bc1 && a <= b;
  for (int ch = 0; ch < 3; ++ch) {
    if (tres) {
      c.rgb[2][ch] = (c.rgb[0][ch] + c.rgb[1][ch]) / 2;
    } else {
      c.rgb[2][ch] = (2 * c.rgb[0][ch] + c.rgb[1][ch]) / 3;
      c.rgb[3][ch] = (c.rgb[0][ch] + 2 * c.rgb[1][ch]) / 3;
    }
  }
  for (int i = 0; i < 16; ++i) {
    const int indice = (color[4 + i / 4] >> (2 * (i % 4))) & 3;
    const int p = (i % 4) * 4 + i / 4;  // BC is row after row: i = y * 4 + x
    c.entrada[p] = uint8_t(indice);
    if (tres && indice == 3) c.transparentes |= uint16_t(1u << p);
  }
  return c;
}

// The 16 values of a BC4-style block (BC4, BC5's channels, BC3's alpha), in 8 or 11 bits.
inline Valores ValoresBc(const uint8_t* bloque, bool once) {
  Valores v;
  uint8_t salida[16];
  bc::Alfa(bloque, salida, 1);
  for (int i = 0; i < 16; ++i) {
    v.valor[(i % 4) * 4 + i / 4] = once ? (salida[i] * 2047 + 127) / 255 : salida[i];
  }
  return v;
}

// One block. Returns the encoder's error (RGB plus alpha, or R11 per channel) for the tests.
inline int64_t TranscodificarBloque(Formato formato, const uint8_t* bc, uint8_t* etc) {
  switch (formato) {
    case Formato::BC1: {
      const Codificado c = CodificarColor(ColoresBc(bc, true), true);
      EscribirBigEndian(c.bloque, etc);
      return c.error;
    }
    case Formato::BC2:
    case Formato::BC3: {
      Valores alfa;
      if (formato == Formato::BC2) {
        for (int i = 0; i < 16; ++i) alfa.valor[(i % 4) * 4 + i / 4] = ((bc[i / 2] >> (4 * (i % 2))) & 15) * 17;
      } else {
        alfa = ValoresBc(bc, false);
      }
      // Read both before writing: the output may be the input.
      const Codificado c = CodificarColor(ColoresBc(bc + 8, false), false);
      const Eac a = CodificarEac(alfa, false);
      EscribirBigEndian(a.bloque, etc);
      EscribirBigEndian(c.bloque, etc + 8);
      return int64_t(c.error) + a.error;
    }
    case Formato::BC4: {
      const Eac r = CodificarEac(ValoresBc(bc, true), true);
      EscribirBigEndian(r.bloque, etc);
      return r.error;
    }
    case Formato::BC5: {
      const Eac r = CodificarEac(ValoresBc(bc, true), true);
      const Eac g = CodificarEac(ValoresBc(bc + 8, true), true);
      EscribirBigEndian(r.bloque, etc);
      EscribirBigEndian(g.bloque, etc + 8);
      return int64_t(r.error) + g.error;
    }
  }
  return 0;
}

// Every block of a texture's data, in any layout of whole blocks (all levels and layers). In place if
// entrada and salida are the same.
inline bool Transcodificar(Formato formato, std::span<const uint8_t> entrada, std::span<uint8_t> salida) {
  const size_t bytes = bc::BytesBloque(formato);
  if (entrada.size() != salida.size() || entrada.size() % bytes) return false;
  for (size_t i = 0; i < entrada.size(); i += bytes) TranscodificarBloque(formato, entrada.data() + i, salida.data() + i);
  return true;
}

// --- Decoding (tests and the encoder's runtime check) ------------------------------------------------------------

// An ETC2 RGB or RGB8A1 block to 16 RGBA texels row after row (y * 4 + x), like bc::Bloque.
inline void DecodificarColor(const uint8_t* bloque, bool punch, uint8_t* salida) {
  const uint64_t v = LeerBigEndian(bloque);
  const bool bit33 = (v >> 33) & 1;
  const bool diferencial = punch || bit33;
  const bool opaco = !punch || bit33;
  int pintura[4][3];
  bool etc1 = true;
  int base[2][3] = {};
  const auto con_signo = [](int x) { return x >= 4 ? x - 8 : x; };
  const uint32_t indices = uint32_t(v);
  const auto indice = [&](int p) { return int(((indices >> (16 + p)) & 1) << 1 | ((indices >> p) & 1)); };
  if (diferencial) {
    int q[3], d[3];
    for (int ch = 0; ch < 3; ++ch) {
      q[ch] = int((v >> (59 - 8 * ch)) & 31);
      d[ch] = con_signo(int((v >> (56 - 8 * ch)) & 7));
    }
    if (q[0] + d[0] < 0 || q[0] + d[0] > 31) {  // T
      const int c1[3] = {int(((v >> 59) & 3) << 2 | ((v >> 56) & 3)), int((v >> 52) & 15), int((v >> 48) & 15)};
      const int c2[3] = {int((v >> 44) & 15), int((v >> 40) & 15), int((v >> 36) & 15)};
      const int dist = kDistancias[((v >> 34) & 3) << 1 | ((v >> 32) & 1)];
      for (int ch = 0; ch < 3; ++ch) {
        pintura[0][ch] = Expandir(c1[ch], 4);
        pintura[1][ch] = Saturar(Expandir(c2[ch], 4) + dist);
        pintura[2][ch] = Expandir(c2[ch], 4);
        pintura[3][ch] = Saturar(Expandir(c2[ch], 4) - dist);
      }
      etc1 = false;
    } else if (q[1] + d[1] < 0 || q[1] + d[1] > 31) {  // H
      const int c1[3] = {int((v >> 59) & 15), int(((v >> 56) & 7) << 1 | ((v >> 52) & 1)),
                         int(((v >> 51) & 1) << 3 | ((v >> 47) & 7))};
      const int c2[3] = {int((v >> 43) & 15), int((v >> 39) & 15), int((v >> 35) & 15)};
      const int di = int(((v >> 34) & 1) << 2 | ((v >> 32) & 1) << 1) | (Valor12(c1) >= Valor12(c2) ? 1 : 0);
      const int dist = kDistancias[di];
      for (int ch = 0; ch < 3; ++ch) {
        pintura[0][ch] = Saturar(Expandir(c1[ch], 4) + dist);
        pintura[1][ch] = Saturar(Expandir(c1[ch], 4) - dist);
        pintura[2][ch] = Saturar(Expandir(c2[ch], 4) + dist);
        pintura[3][ch] = Saturar(Expandir(c2[ch], 4) - dist);
      }
      etc1 = false;
    } else if (q[2] + d[2] < 0 || q[2] + d[2] > 31) {  // planar: always opaque
      const int o[3] = {Expandir(int((v >> 57) & 63), 6), Expandir(int(((v >> 56) & 1) << 6 | ((v >> 49) & 63)), 7),
                        Expandir(int(((v >> 48) & 1) << 5 | ((v >> 43) & 3) << 3 | ((v >> 39) & 7)), 6)};
      const int h[3] = {Expandir(int(((v >> 34) & 31) << 1 | ((v >> 32) & 1)), 6), Expandir(int((v >> 25) & 127), 7),
                        Expandir(int((v >> 19) & 63), 6)};
      const int w[3] = {Expandir(int((v >> 13) & 63), 6), Expandir(int((v >> 6) & 127), 7),
                        Expandir(int(v & 63), 6)};
      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          uint8_t* t = salida + (y * 4 + x) * 4;
          for (int ch = 0; ch < 3; ++ch) t[ch] = uint8_t(ValorPlanar(o[ch], h[ch], w[ch], x, y));
          t[3] = 255;
        }
      }
      return;
    } else {
      for (int ch = 0; ch < 3; ++ch) {
        base[0][ch] = Expandir(q[ch], 5);
        base[1][ch] = Expandir(q[ch] + d[ch], 5);
      }
    }
  } else {
    for (int ch = 0; ch < 3; ++ch) {
      base[0][ch] = Expandir(int((v >> (60 - 8 * ch)) & 15), 4);
      base[1][ch] = Expandir(int((v >> (56 - 8 * ch)) & 15), 4);
    }
  }
  const bool flip = (v >> 32) & 1;
  const int tabla[2] = {int((v >> 37) & 7), int((v >> 34) & 7)};
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      const int p = x * 4 + y;
      const int i = indice(p);
      uint8_t* t = salida + (y * 4 + x) * 4;
      if (!opaco && i == 2) {
        t[0] = t[1] = t[2] = t[3] = 0;
        continue;
      }
      for (int ch = 0; ch < 3; ++ch) {
        if (etc1) {
          const int h = MitadDe(p, flip);
          t[ch] = uint8_t(Saturar(base[h][ch] + Modificador(tabla[h], i, opaco)));
        } else {
          t[ch] = uint8_t(pintura[i][ch]);
        }
      }
      t[3] = 255;
    }
  }
}

// An EAC block to 16 values row after row: alpha 0-255, or R11 0-2047.
inline void DecodificarEac(const uint8_t* bloque, bool once, int salida[16]) {
  const uint64_t v = LeerBigEndian(bloque);
  int paleta[8];
  PaletaEac(int(v >> 56), int((v >> 52) & 15), int((v >> 48) & 15), once, paleta);
  for (int p = 0; p < 16; ++p) salida[(p & 3) * 4 + (p >> 2)] = paleta[(v >> (45 - 3 * p)) & 7];
}

inline uint8_t R11a8(int v) { return uint8_t((v * 255 + 1023) / 2047); }

// A transcoded block to what bc::Bloque gives for the BC one: RGBA8, R8 for BC4, RG8 for BC5.
inline void Decodificar(Formato formato, const uint8_t* etc, uint8_t* salida) {
  int valores[16];
  switch (formato) {
    case Formato::BC1:
      DecodificarColor(etc, true, salida);
      return;
    case Formato::BC2:
    case Formato::BC3:
      DecodificarColor(etc + 8, false, salida);
      DecodificarEac(etc, false, valores);
      for (int i = 0; i < 16; ++i) salida[i * 4 + 3] = uint8_t(valores[i]);
      return;
    case Formato::BC4:
      DecodificarEac(etc, true, valores);
      for (int i = 0; i < 16; ++i) salida[i] = R11a8(valores[i]);
      return;
    case Formato::BC5:
      DecodificarEac(etc, true, valores);
      for (int i = 0; i < 16; ++i) salida[i * 2] = R11a8(valores[i]);
      DecodificarEac(etc + 8, true, valores);
      for (int i = 0; i < 16; ++i) salida[i * 2 + 1] = R11a8(valores[i]);
      return;
  }
}
}  // namespace nfsmw::etc
