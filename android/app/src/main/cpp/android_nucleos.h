// The choice of the fast cores (android_rendimiento.cpp), apart from reading them, so that
// tools/tests/android_nucleos_test.cpp can try it on the layouts of known SoCs.
#pragma once

#include <cstddef>
#include <vector>

namespace nfsmw::android_nucleos {

// What the kernel shows of a core; 0 where it shows nothing (an offline core, or an old kernel).
struct Nucleo {
  long reloj_khz = 0;           // cpufreq/cpuinfo_max_freq
  long capacidad = 0;           // cpu_capacity, 0-1024
  unsigned long long midr = 0;  // regs/identification/midr_el1, or /proc/cpuinfo's implementer and part
};

struct Eleccion {
  // nothing tells the cores apart: the affinity stays as it is
  bool sin_datos = true;
  std::vector<bool> pequenos;
  std::vector<bool> rapidos;
  int elegidos = 0;
};

// ARM's in-order little cores, and Qualcomm's Kryo Silver cores built on them.
inline bool NucleoPequeno(unsigned long long midr) {
  const unsigned implementador = unsigned(midr >> 24) & 0xff;
  const unsigned parte = unsigned(midr >> 4) & 0xfff;
  if (implementador == 0x41) {
    // Cortex-A53, A35, A55, A510, A520
    return parte == 0xd03 || parte == 0xd04 || parte == 0xd05 || parte == 0xd46 || parte == 0xd80;
  }
  if (implementador == 0x51) {
    // Kryo 2xx Silver (A53), Kryo 385 Silver and the later Kryo Silver cores (A55)
    return parte == 0x801 || parte == 0x803 || parte == 0x805;
  }
  return false;
}

// A core is fast when its top clock is at least 70 % of the fastest one and it is not a little core: by its
// part number where every core shows one, else where its capacity is under half the largest. A core with no
// clock, offline at startup, is not fast.
inline Eleccion ElegirRapidos(const std::vector<Nucleo>& nucleos) {
  Eleccion eleccion;
  const size_t total = nucleos.size();
  long reloj_maximo = 0, capacidad_maxima = 0;
  bool todas_capacidades = total > 0, todos_midr = total > 0;
  for (const Nucleo& n : nucleos) {
    reloj_maximo = reloj_maximo > n.reloj_khz ? reloj_maximo : n.reloj_khz;
    capacidad_maxima = capacidad_maxima > n.capacidad ? capacidad_maxima : n.capacidad;
    todas_capacidades &= n.capacidad > 0;
    todos_midr &= n.midr != 0;
  }
  const bool hay_relojes = reloj_maximo > 0;
  const bool sabe_pequenos = todos_midr || todas_capacidades;
  eleccion.sin_datos = !hay_relojes && !sabe_pequenos;
  eleccion.pequenos.assign(total, false);
  eleccion.rapidos.assign(total, false);
  if (eleccion.sin_datos) {
    return eleccion;
  }
  for (size_t cpu = 0; cpu < total; ++cpu) {
    const Nucleo& n = nucleos[cpu];
    if (sabe_pequenos) {
      eleccion.pequenos[cpu] = todos_midr ? NucleoPequeno(n.midr) : n.capacidad * 2 < capacidad_maxima;
    }
    eleccion.rapidos[cpu] = (!hay_relojes || n.reloj_khz * 10 >= reloj_maximo * 7) && !eleccion.pequenos[cpu];
    eleccion.elegidos += eleccion.rapidos[cpu] ? 1 : 0;
  }
  return eleccion;
}

}  // namespace nfsmw::android_nucleos
