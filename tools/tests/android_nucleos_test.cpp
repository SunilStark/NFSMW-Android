// The fast cores android_rendimiento.cpp keeps the game on, for the layouts of known SoCs. Runs on the PC:
//   c++ -std=c++17 tools/tests/android_nucleos_test.cpp -o out/android_nucleos_test && out/android_nucleos_test
#include "../../android/app/src/main/cpp/android_nucleos.h"

#include <cstdio>
#include <string>
#include <vector>

using nfsmw::android_nucleos::ElegirRapidos;
using nfsmw::android_nucleos::Nucleo;

namespace {

int fallos = 0;

// The chosen cores as "0,1,...", or "-" when nothing tells them apart.
std::string Elegidos(const std::vector<Nucleo>& nucleos) {
  const auto eleccion = ElegirRapidos(nucleos);
  if (eleccion.sin_datos) {
    return "-";
  }
  std::string lista;
  for (size_t cpu = 0; cpu < nucleos.size(); ++cpu) {
    if (eleccion.rapidos[cpu]) {
      lista += (lista.empty() ? "" : ",") + std::to_string(cpu);
    }
  }
  return lista;
}

void Comprobar(const char* caso, const std::vector<Nucleo>& nucleos, const std::string& esperado) {
  const std::string obtenido = Elegidos(nucleos);
  if (obtenido != esperado) {
    std::printf("FAIL %s: %s, se esperaba %s\n", caso, obtenido.c_str(), esperado.c_str());
    ++fallos;
  } else {
    std::printf("ok   %s: %s\n", caso, obtenido.c_str());
  }
}

std::vector<Nucleo> Repetir(const Nucleo& n, int veces) {
  return std::vector<Nucleo>(size_t(veces), n);
}

std::vector<Nucleo> Unir(std::vector<std::vector<Nucleo>> grupos) {
  std::vector<Nucleo> todos;
  for (const auto& grupo : grupos) {
    todos.insert(todos.end(), grupo.begin(), grupo.end());
  }
  return todos;
}

}  // namespace

int main() {
  // Exynos 1380 (Galaxy Tab S10 Lite, Galaxy A54): 4 Cortex-A55 at 2.0 GHz and 4 Cortex-A78 at 2.4 GHz. The
  // clock alone (83 %) took every core.
  const Nucleo a55{2000000, 380, 0x410fd050}, a78{2400000, 1024, 0x410fd410};
  Comprobar("Exynos 1380", Unir({Repetir(a55, 4), Repetir(a78, 4)}), "4,5,6,7");
  Comprobar("Exynos 1380, sin MIDR (por la capacidad)",
            Unir({Repetir({2000000, 380, 0}, 4), Repetir({2400000, 1024, 0}, 4)}), "4,5,6,7");
  Comprobar("Exynos 1380, kernel sin MIDR ni capacidad (solo el reloj, como antes)",
            Unir({Repetir({2000000, 0, 0}, 4), Repetir({2400000, 0, 0}, 4)}), "0,1,2,3,4,5,6,7");
  // Exynos 1480 (Galaxy A55): 4 Cortex-A55 at 2.0 GHz and 4 Cortex-A78 at 2.75 GHz (73 %).
  Comprobar("Exynos 1480",
            Unir({Repetir({2000000, 340, 0x410fd050}, 4), Repetir({2750000, 1024, 0x410fd410}, 4)}), "4,5,6,7");
  // Snapdragon 8 Gen 2: 3 Cortex-A510, 2 A715 and 2 A710 at 2.8 GHz, an X3 at 3.2 GHz.
  Comprobar("Snapdragon 8 Gen 2",
            Unir({Repetir({2016000, 410, 0x410fd461}, 3), Repetir({2803200, 820, 0x410fd4d0}, 2),
                  Repetir({2803200, 790, 0x410fd471}, 2), Repetir({3187200, 1024, 0x411fd4e0}, 1)}),
            "3,4,5,6,7");
  // Snapdragon 8 Elite: 6 Oryon at 3.53 GHz and 2 at 4.32 GHz, no little cores: every core.
  Comprobar("Snapdragon 8 Elite",
            Unir({Repetir({3532800, 870, 0x510f0020}, 6), Repetir({4320000, 1024, 0x510f0010}, 2)}),
            "0,1,2,3,4,5,6,7");
  // Snapdragon 695: 6 Kryo 660 Silver (A55) at 1.8 GHz and 2 Gold at 2.2 GHz: two fast cores, which
  // android_rendimiento.cpp leaves unpinned.
  Comprobar("Snapdragon 695",
            Unir({Repetir({1804800, 430, 0x51df8050}, 6), Repetir({2208000, 1024, 0x51df8040}, 2)}), "6,7");
  // Dimensity 9300: 4 Cortex-A720 at 2.0 GHz and 4 X4 at 2.85 and 3.25 GHz, no little cores: the clock.
  Comprobar("Dimensity 9300",
            Unir({Repetir({2000000, 560, 0x410fd810}, 4), Repetir({2850000, 970, 0x410fd820}, 3),
                  Repetir({3250000, 1024, 0x410fd820}, 1)}),
            "4,5,6,7");
  // A core offline at startup has no clock: not fast.
  Comprobar("nucleo apagado", Unir({Repetir(a55, 4), Repetir(a78, 3), Repetir({0, 1024, 0x410fd410}, 1)}),
            "4,5,6");
  Comprobar("sin datos", Repetir({0, 0, 0}, 8), "-");
  if (fallos) {
    std::printf("%d FAIL\n", fallos);
    return 1;
  }
  std::puts("PASS");
  return 0;
}
