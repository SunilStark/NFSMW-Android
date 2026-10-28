// clang++ -std=c++20 -O1 tools/tests/spirv_precision_test.cpp -o spirv_precision_test
// nfsmw_nativo_media_precision (app/src/nfsmw_spirv_precision.cpp) on pixel shaders shaped like the library's
// world shaders (spirv_precision_casos.hlsl, compiled with glslang into spirv_precision_casos.h): the diffuse color
// math goes to 16 bits; the shadow map's samples (under tfetch2DSombraMin's marker) and pow/exp/log with what feeds
// them stay at 32 bits, in the four shapes the marker test can take; a marker whose samples cannot be found leaves
// the shader untouched. (The relaxed modules also pass spirv-val; checked when the fixture was generated.)
#include "../../app/src/nfsmw_spirv_precision.cpp"
#include "spirv_precision_casos.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <span>
#include <string>

using nfsmw::nativo::InformePrecision;
using nfsmw::nativo::RelajarPrecision;

static void Check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

struct Modulo {
  std::map<uint32_t, std::string> nombre;     // OpName
  std::map<uint32_t, std::string> textura;    // result id of a sample -> the texture variable's name
  std::map<uint32_t, uint32_t> extinst;       // result id -> GLSL.std.450 instruction
  std::set<uint32_t> relajados;
  size_t primer_tipo = 0, ultima_decoracion = 0;
};

static Modulo Analizar(const std::vector<uint32_t>& s) {
  Modulo m;
  std::map<uint32_t, std::string> cargado, combinado;
  for (size_t pos = 5; pos < s.size();) {
    const uint32_t n = s[pos] >> 16, op = s[pos] & 0xFFFF;
    Check(n && pos + n <= s.size(), "the module still parses");
    const uint32_t* w = s.data() + pos;
    if (op == 5) m.nombre[w[1]] = reinterpret_cast<const char*>(w + 2);
    if (op == 61 && m.nombre.count(w[3])) cargado[w[2]] = m.nombre[w[3]];  // OpLoad of a named variable
    if (op == 86 && cargado.count(w[3])) combinado[w[2]] = cargado[w[3]];  // OpSampledImage
    if (op >= 87 && op <= 97 && combinado.count(w[3])) m.textura[w[2]] = combinado[w[3]];
    if (op == 12) m.extinst[w[2]] = w[4];
    if (op == 71 && w[2] == 0) m.relajados.insert(w[1]), m.ultima_decoracion = pos;
    if (!m.primer_tipo && op >= 19 && op <= 39) m.primer_tipo = pos;
    pos += n;
  }
  return m;
}

static void Variante(const char* nombre, std::span<const uint32_t> palabras, uint32_t potencias_esperadas) {
  std::vector<uint32_t> s(palabras.begin(), palabras.end());
  InformePrecision info;
  const uint32_t marcados = RelajarPrecision(s, &info);
  const Modulo m = Analizar(s);
  Check(marcados > 0 && marcados == m.relajados.size(), "results marked, one decoration each");
  Check(m.ultima_decoracion < m.primer_tipo, "decorations go before the first type");
  Check(info.marca_sombra, "the shadow marker is found");
  uint32_t sombra = 0, difusas = 0, normales = 0;
  for (const auto& [id, tex] : m.textura) {
    const bool relajada = m.relajados.count(id) != 0;
    if (tex == "sombraA" || tex == "sombraB") {
      Check(!relajada, "shadow map samples stay at 32 bits");
      ++sombra;
    } else if (tex == "diffuse") {
      Check(relajada, "the diffuse color goes to 16 bits");
      ++difusas;
    } else if (tex == "normales") {
      Check(!relajada, "the normal map feeds the specular pow: 32 bits");
      ++normales;
    }
  }
  Check(sombra == 4 && difusas == 1 && normales == 1, "every sample of the fixture is seen");
  Check(info.muestras_sombra == 4, "the four shadow samples (two fetches, the minimum of two maps each)");
  uint32_t potencias = 0;
  for (const auto& [id, instruccion] : m.extinst) {
    if (instruccion >= 26 && instruccion <= 30) {
      Check(!m.relajados.count(id), "pow, exp and log stay at 32 bits");
      ++potencias;
    }
  }
  Check(potencias == potencias_esperadas && info.potencias == potencias_esperadas, "every pow/exp/log is seen");
  std::printf("%s: %u results at 16 bits; %u shadow samples and %u pow/exp/log at 32 bits\n", nombre, marcados,
              info.muestras_sombra, info.potencias);
}

// The id of the boolean the marker comparison gives, and the instruction words of the branches on it.
static std::vector<size_t> RamasMarca(const std::vector<uint32_t>& s, uint32_t& condicion, uint32_t& otra) {
  uint32_t marca = 0;
  condicion = otra = 0;
  std::vector<size_t> ramas;
  for (size_t pos = 5; pos < s.size(); pos += s[pos] >> 16) {
    const uint32_t op = s[pos] & 0xFFFF;
    const uint32_t* w = s.data() + pos;
    if (op == 43 && (s[pos] >> 16) == 4 && w[3] == 0x5E3B1A84u) marca = w[2];
    if (op == 52 && (w[4] == marca || w[5] == marca)) condicion = w[2];
    else if (op == 52 && !otra) otra = w[2];  // another boolean specialization constant
    if (op == 250 && w[1] == condicion) ramas.push_back(pos);
  }
  return ramas;
}

int main() {
  Variante("A [branch] and pow", kCasoA, 1);
  Variante("B [flatten]", kCasoB, 1);
  Variante("C marker test as !=", kCasoC, 1);
  Variante("D exp2(log2(x) * n)", kCasoD, 2);

  // The marker without a branch on it (the shadow fetches cannot be told apart): the shader is left as it was.
  {
    std::vector<uint32_t> s(std::begin(kCasoA), std::end(kCasoA));
    uint32_t condicion = 0, otra = 0;
    const std::vector<size_t> ramas = RamasMarca(s, condicion, otra);
    Check(!ramas.empty() && condicion && otra, "the fixture has its marker branches");
    for (size_t pos : ramas) s[pos + 1] = otra;
    const std::vector<uint32_t> antes = s;
    InformePrecision info;
    Check(RelajarPrecision(s, &info) == 0 && s == antes && info.marca_sombra && info.sin_resolver,
          "an unresolved marker: unchanged, and reported");
  }
  // Without the marker (a library from before tfetch2DSombraMin) nothing tells the shadow map apart: its samples
  // would be relaxed like any texture. ShadersNativos::Cargar does not relax those shaders.
  {
    std::vector<uint32_t> s(std::begin(kCasoA), std::end(kCasoA));
    for (size_t pos = 5; pos < s.size(); pos += s[pos] >> 16) {
      if ((s[pos] & 0xFFFF) == 43 && (s[pos] >> 16) == 4 && s[pos + 3] == 0x5E3B1A84u) s[pos + 3] = 0x12345678u;
    }
    InformePrecision info;
    Check(RelajarPrecision(s, &info) > 0 && !info.marca_sombra && info.muestras_sombra == 0, "no marker");
    const Modulo m = Analizar(s);
    for (const auto& [id, tex] : m.textura) {
      if (tex == "sombraA") Check(m.relajados.count(id) != 0, "without the marker the shadow map is not recognized");
    }
  }
  // Not SPIR-V: untouched.
  {
    std::vector<uint32_t> basura = {1, 2, 3, 4, 5, 6};
    Check(RelajarPrecision(basura) == 0 && basura == std::vector<uint32_t>({1, 2, 3, 4, 5, 6}), "not SPIR-V");
  }
  std::printf("Half precision: OK\n");
  return 0;
}
