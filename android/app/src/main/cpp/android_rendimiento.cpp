// Keeps the game off the phone's little cores.
//
// On the Switch the game gets three whole cores at a fixed clock. A phone mixes big and little cores, and the
// scheduler judges a thread by its recent load: the ring thread and the game thread wait for each other every
// frame, look half idle, and end up on a little core at a low clock. That is a lost frame each time.
//
// At startup, before the game creates its threads (they inherit the mask), every thread of the process is
// limited to its fast cores: those whose top clock is at least 70 % of the fastest one, and which are not
// little cores. A little core is one of ARM's in-order designs (Cortex-A53, A55, A510, A520, and Qualcomm's
// Kryo Silver cores built on them), known by the part number in its MIDR; where the kernel shows no MIDR, a
// core whose cpu_capacity is under half the largest. The clock alone misses them where they run nearly as
// fast as the big cores: the Exynos 1380 of the Galaxy Tab S10 Lite has its Cortex-A55s at 2.0 GHz beside
// Cortex-A78s at 2.4 GHz (83 %), so every core passed and the game threads still landed on the A55s.
// SoCs without a slow cluster (Snapdragon 8 Elite: 4.32 and 3.53 GHz) keep every core.
//
// Nor when fewer than four cores are fast (2+6 of the Snapdragon 680/695 and Helio G, 2+4 of the Snapdragon
// 650): the game's ring, main, audio and shader threads and the driver's would share two cores, worse than letting
// the scheduler use the little ones (from dvaustria741-a11y, victorgbd/NFSMW-Recompiled-Mobile d9c90ea).
#include <sched.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "android_nucleos.h"

REXCVAR_DEFINE_BOOL(nfsmw_android_nucleos_grandes, true, "NFSMW",
                    "Android: el juego solo usa los nucleos rapidos (reloj maximo de al menos el 70 % del mas "
                    "rapido, y no un nucleo pequeno: Cortex-A53/A55/A510/A520); false = los que decida el sistema")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

std::string RutaCpu(int cpu, const char* fichero) {
  return "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/" + fichero;
}

// The core's top clock, or 0 when the kernel does not give it (an offline core, or no cpufreq for it).
long RelojMaximoKhz(int cpu) {
  long valor = 0;
  std::ifstream f(RutaCpu(cpu, "cpufreq/cpuinfo_max_freq"));
  return (f >> valor && valor > 0) ? valor : 0;
}

// The core's capacity as the scheduler weighs it (0-1024: its clock times the work it does per cycle), or 0.
long Capacidad(int cpu) {
  long valor = 0;
  std::ifstream f(RutaCpu(cpu, "cpu_capacity"));
  return (f >> valor && valor > 0) ? valor : 0;
}

// The core's MIDR (implementer and part number), or 0 when the kernel does not show it: from sysfs, else
// from the core's block of /proc/cpuinfo.
unsigned long long Midr(int cpu) {
  std::string texto;
  if (std::ifstream f(RutaCpu(cpu, "regs/identification/midr_el1")); f >> texto) {
    const unsigned long long valor = std::strtoull(texto.c_str(), nullptr, 16);
    if (valor != 0) {
      return valor;
    }
  }
  std::ifstream info("/proc/cpuinfo");
  std::string linea;
  int actual = -1;
  unsigned long implementador = 0, parte = 0;
  bool hay_implementador = false, hay_parte = false;
  while (std::getline(info, linea)) {
    const size_t dos_puntos = linea.find(':');
    if (dos_puntos == std::string::npos || dos_puntos == 0) {
      continue;
    }
    const size_t fin_clave = linea.find_last_not_of(" \t", dos_puntos - 1);
    const std::string clave = fin_clave == std::string::npos ? std::string() : linea.substr(0, fin_clave + 1);
    const char* valor = linea.c_str() + dos_puntos + 1;
    if (clave == "processor") {
      if (actual == cpu) {
        break;
      }
      actual = int(std::strtol(valor, nullptr, 10));
      hay_implementador = hay_parte = false;
    } else if (actual == cpu && clave == "CPU implementer") {
      implementador = std::strtoul(valor, nullptr, 0);
      hay_implementador = true;
    } else if (actual == cpu && clave == "CPU part") {
      parte = std::strtoul(valor, nullptr, 0);
      hay_parte = true;
    }
  }
  if (actual != cpu || !hay_implementador || !hay_parte) {
    return 0;
  }
  return (static_cast<unsigned long long>(implementador & 0xff) << 24) | ((parte & 0xfff) << 4);
}

}  // namespace

extern "C" void nfsmw_android_nucleos_grandes_aplicar() {
  if (!REXCVAR_GET(nfsmw_android_nucleos_grandes)) {
    return;
  }
  const int total = int(sysconf(_SC_NPROCESSORS_CONF));
  if (total <= 1 || total > CPU_SETSIZE) {
    return;
  }
  namespace nucleos = nfsmw::android_nucleos;
  std::vector<nucleos::Nucleo> leidos(static_cast<size_t>(total));
  long reloj_maximo = 0;
  for (int cpu = 0; cpu < total; ++cpu) {
    leidos[size_t(cpu)] = {RelojMaximoKhz(cpu), Capacidad(cpu), Midr(cpu)};
    reloj_maximo = std::max(reloj_maximo, leidos[size_t(cpu)].reloj_khz);
  }
  const nucleos::Eleccion eleccion = nucleos::ElegirRapidos(leidos);
  if (eleccion.sin_datos) {
    REXLOG_INFO("[android] no se pueden leer los relojes ni el tipo de los nucleos: afinidad sin cambios");
    return;
  }
  cpu_set_t mascara;
  CPU_ZERO(&mascara);
  std::string lista, descripcion;
  for (int cpu = 0; cpu < total; ++cpu) {
    const nucleos::Nucleo& n = leidos[size_t(cpu)];
    char texto[96];
    std::snprintf(texto, sizeof(texto), "%s%d:%ldMHz/cap%ld/%03llx%s", descripcion.empty() ? "" : " ", cpu,
                  n.reloj_khz / 1000, n.capacidad, (n.midr >> 4) & 0xfff,
                  eleccion.pequenos[size_t(cpu)] ? "(pequeno)" : "");
    descripcion += texto;
    if (eleccion.rapidos[size_t(cpu)]) {
      CPU_SET(cpu, &mascara);
      lista += (lista.empty() ? "" : ",") + std::to_string(cpu);
    }
  }
  REXLOG_INFO("[android] nucleos (cpu:reloj/capacidad/parte): {}", descripcion);
  if (eleccion.elegidos == total) {
    REXLOG_INFO("[android] {} nucleos sin grupo lento: afinidad sin cambios", total);
    return;
  }
  if (eleccion.elegidos < 4) {
    REXLOG_INFO("[android] solo {} nucleos rapidos de {} ({}): afinidad sin cambios", eleccion.elegidos, total,
                lista);
    return;
  }
  // Every thread that already exists (the SDL thread, the UI thread, the logger); the ones created later
  // inherit the mask of their creator.
  int hilos = 0;
  std::error_code ec;
  for (const auto& tarea : std::filesystem::directory_iterator("/proc/self/task", ec)) {
    const pid_t tid = pid_t(std::strtol(tarea.path().filename().c_str(), nullptr, 10));
    if (tid > 0 && sched_setaffinity(tid, sizeof(mascara), &mascara) == 0) {
      ++hilos;
    }
  }
  REXLOG_INFO("[android] juego en los nucleos {} de {} (reloj >= 70 % de {} MHz, sin nucleos pequenos); {} hilos "
              "movidos",
              lista, total, reloj_maximo / 1000, hilos);
}
