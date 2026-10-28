#pragma once
// nfsmw - native renderer: half precision in the pixel shaders (nfsmw_nativo_media_precision).
//
// Mali and Adreno do twice the arithmetic at 16 bits and need half the registers, but the library's SPIR-V asks for
// 32 bits everywhere. This marks with RelaxedPrecision the color math: the results of sampling a texture and every
// float operation that comes from them. Vulkan drivers then compute those at 16 bits, and SPIRV-Cross writes them as
// mediump for OpenGL ES. Whatever ends up as a texture coordinate, a depth reference or gl_FragDepth stays at 32 bits,
// and so does everything that does not come from a texture (positions, fog, constants).
//
// Two kinds of texture data stay at 32 bits too, because 16 bits would show:
//   - The shadow map. The game samples it as an ordinary texture (it holds depth) and compares it with the pixel's
//     depth in the shader: at 16 bits the depth near 1.0 has steps of 0.0005, shadow acne all over the world. Its
//     samples are the ones under tfetch2DSombraMin's marker branch (NFSMW_MARCA_SOMBRA_MINIMO), which the library
//     puts on every shadow map fetch.
//   - pow, exp and log, with what feeds them: specular highlights are pow(x, n), and n multiplies the error of x.

#include <cstdint>
#include <vector>

namespace nfsmw::nativo {

struct InformePrecision {
  uint32_t marcados = 0;          // results decorated RelaxedPrecision
  uint32_t muestras_sombra = 0;   // shadow map samples kept at 32 bits
  uint32_t potencias = 0;         // pow, exp and log kept at 32 bits
  bool marca_sombra = false;      // the module has tfetch2DSombraMin's marker
  bool sin_resolver = false;      // ...but not all its shadow samples were found: the module was left as it was
};

// Marks the pixel shader `spirv` in place. Returns how many results were marked (0: unchanged, also for SPIR-V it
// does not understand).
uint32_t RelajarPrecision(std::vector<uint32_t>& spirv, InformePrecision* informe = nullptr);

}  // namespace nfsmw::nativo
