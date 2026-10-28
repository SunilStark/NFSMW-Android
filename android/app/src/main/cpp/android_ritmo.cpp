// ADPF and thermal step-down on Android.
//
// ADPF (Android Dynamic Performance Framework, API 33+): the game tells Android, frame by frame, how long its work
// took against the frame budget, and Android sets the CPU clocks for that instead of guessing from the load. Without
// it the governor sees threads that wait for each other every frame, judges them half idle and lowers the clocks:
// then a frame does not fit. The session holds the game's hot threads: the preparer and the executor (Main XThread),
// which join it from the frame handoff (nfsmw_espera_fotograma.cpp), and the ring thread, which reports each frame
// from its Swap (nfsmw_nativo_sistema.cpp). A frame's work is the CPU time of the busiest of them
// (android_termico.h), against 1/limit of nfsmw_limite_fps.
//
// Thermals (API 31+): a thread reads the thermal headroom forecast for 10 s ahead every 2 s and, before the tablet
// throttles, sets a floor under the 30 FPS guard's shadow steps (one shadow map, then no cast shadows); when it has
// cooled down for a minute, it raises them back one at a time. The player's own settings are not touched.
//
// Both are loaded from libandroid.so with dlsym: the app also runs on Android 8, where they do not exist. Where the
// system has no session or no headroom, the log says so and nothing changes.
#include <dlfcn.h>
#include <pthread.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#include "android_termico.h"

REXCVAR_DEFINE_BOOL(nfsmw_android_adpf, true, "NFSMW",
                    "Android 13+: el juego dice al sistema cuanto trabajo lleva cada fotograma frente a su presupuesto "
                    "(ADPF), y el sistema sube los relojes antes de que un fotograma no quepa")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(nfsmw_android_termico, true, "NFSMW",
                    "Android 12+: antes de que el sistema baje los relojes por calor, recortar las sombras (un mapa, "
                    "luego sin sombras proyectadas) y devolverlas al enfriarse")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(nfsmw_android_termico_nivel1_pct, 85, "NFSMW",
                     "Margen termico (100 = el sistema recorta fuerte) desde el que se pasa a un mapa de sombras")
    .range(50, 100)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(nfsmw_android_termico_nivel2_pct, 95, "NFSMW",
                     "Margen termico desde el que se quitan las sombras proyectadas")
    .range(50, 120)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(nfsmw_android_termico_volver_pct, 70, "NFSMW",
                     "Margen termico por debajo del cual, durante nfsmw_android_termico_volver_s, se devuelve un nivel")
    .range(20, 100)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(nfsmw_android_termico_volver_s, 60, "NFSMW",
                     "Segundos de margen antes de devolver cada nivel de sombras")
    .range(10, 600)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

extern "C" void nfsmw_guardia30_minimo_termico(int escalon);  // nfsmw_recorte_sombras.cpp

namespace {

struct APerformanceHintManager;
struct APerformanceHintSession;
struct AThermalManager;

// libandroid.so's functions, or null where this Android does not have them.
struct Funciones {
  APerformanceHintManager* (*obtener)() = nullptr;
  APerformanceHintSession* (*crear)(APerformanceHintManager*, const int32_t*, size_t, int64_t) = nullptr;
  int (*informar)(APerformanceHintSession*, int64_t) = nullptr;
  void (*cerrar)(APerformanceHintSession*) = nullptr;
  int (*poner_hilos)(APerformanceHintSession*, const int32_t*, size_t) = nullptr;  // API 34
  int64_t (*ritmo_preferido)(APerformanceHintManager*) = nullptr;
  AThermalManager* (*termico)() = nullptr;
  int (*estado)(AThermalManager*) = nullptr;
  float (*margen)(AThermalManager*, int) = nullptr;  // API 31
};

template <typename F>
void Cargar(void* biblioteca, const char* nombre, F& destino) {
  destino = reinterpret_cast<F>(dlsym(biblioteca, nombre));
}

Funciones CargarFunciones() {
  Funciones f;
  void* biblioteca = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  if (!biblioteca) {
    return f;
  }
  Cargar(biblioteca, "APerformanceHint_getManager", f.obtener);
  Cargar(biblioteca, "APerformanceHint_createSession", f.crear);
  Cargar(biblioteca, "APerformanceHint_reportActualWorkDuration", f.informar);
  Cargar(biblioteca, "APerformanceHint_closeSession", f.cerrar);
  Cargar(biblioteca, "APerformanceHint_setThreads", f.poner_hilos);
  Cargar(biblioteca, "APerformanceHint_getPreferredUpdateRateNanos", f.ritmo_preferido);
  Cargar(biblioteca, "AThermal_acquireManager", f.termico);
  Cargar(biblioteca, "AThermal_getCurrentThermalStatus", f.estado);
  Cargar(biblioteca, "AThermal_getThermalHeadroom", f.margen);
  return f;  // libandroid.so stays loaded: the process already has it
}

int64_t AhoraNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

int64_t TiempoCpu(clockid_t reloj) {
  timespec t{};
  return clock_gettime(reloj, &t) == 0 ? int64_t(t.tv_sec) * 1'000'000'000 + t.tv_nsec : 0;
}

struct Hilo {
  pid_t tid;
  clockid_t reloj;
  std::string papel;
};

// Threads that joined (any thread), and whether the list changed since the ring last read it.
std::mutex g_mutex;
std::vector<Hilo> g_hilos;
std::atomic<bool> g_hilos_cambiados{false};

// The ring thread's state (only it touches this).
struct Ritmo {
  bool iniciado = false;
  bool activo = false;
  Funciones f;
  APerformanceHintManager* gestor = nullptr;
  APerformanceHintSession* sesion = nullptr;
  int64_t objetivo_ns = 0;
  std::vector<Hilo> hilos;
  std::vector<int64_t> antes, ahora;
  // For the periodic line.
  int64_t desde_ns = 0;
  uint64_t fotogramas = 0, informes = 0, fallos = 0;
  int64_t suma_trabajo = 0, max_trabajo = 0, ns_informando = 0, por_encima = 0;
};
Ritmo g_ritmo;

void HiloTermico(Funciones f) {
  pthread_setname_np(pthread_self(), "NFSMW termico");
  setpriority(PRIO_PROCESS, gettid(), 10);
  AThermalManager* gestor = f.termico();
  if (!gestor) {
    REXLOG_INFO("[termico] el sistema no da su gestor termico: sin recorte por calor");
    return;
  }
  nfsmw::termico::Umbrales u;
  u.nivel1 = float(REXCVAR_GET(nfsmw_android_termico_nivel1_pct)) / 100.0f;
  u.nivel2 = float(REXCVAR_GET(nfsmw_android_termico_nivel2_pct)) / 100.0f;
  u.volver = float(REXCVAR_GET(nfsmw_android_termico_volver_pct)) / 100.0f;
  u.ns_volver = int64_t(REXCVAR_GET(nfsmw_android_termico_volver_s)) * 1'000'000'000;
  nfsmw::termico::Nivel nivel(u);
  REXLOG_INFO("[termico] recorte por calor: un mapa de sombras desde un margen de {:.2f}, sin sombras desde {:.2f}, "
              "se devuelve un nivel tras {} s por debajo de {:.2f}{}",
              u.nivel1, u.nivel2, u.ns_volver / 1'000'000'000, u.volver,
              f.margen ? "" : " (este Android no da el margen: solo el estado termico)");
  int anterior = 0;
  int64_t en_nivel_ns[3] = {};
  int64_t ultima = AhoraNs(), ultimo_informe = ultima;
  float max_margen = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const float margen = f.margen ? f.margen(gestor, 10) : NAN;
    const int estado = f.estado ? f.estado(gestor) : nfsmw::termico::kDesconocido;
    const int64_t ahora = AhoraNs();
    en_nivel_ns[anterior] += ahora - ultima;
    ultima = ahora;
    if (std::isfinite(margen)) {
      max_margen = std::max(max_margen, margen);
    }
    const int n = nivel.Actualizar(margen, estado, ahora);
    if (n != anterior) {
      nfsmw_guardia30_minimo_termico(n);
      REXLOG_INFO("[termico] margen {:.2f} a 10 s, estado {}: nivel {} -> {} ({})", margen, estado, anterior, n,
                  n == 0 ? "sombras como siempre" : n == 1 ? "un mapa de sombras" : "sin sombras proyectadas");
      anterior = n;
    }
    if (ahora - ultimo_informe >= 60'000'000'000) {
      REXLOG_INFO("[termico] ultimo minuto: margen max {:.2f}, ahora {:.2f}, estado {}, nivel {}; en total {} s en "
                  "nivel 0, {} s en 1, {} s en 2",
                  max_margen, margen, estado, n, en_nivel_ns[0] / 1'000'000'000, en_nivel_ns[1] / 1'000'000'000,
                  en_nivel_ns[2] / 1'000'000'000);
      ultimo_informe = ahora;
      max_margen = 0;
    }
  }
}

void Iniciar(Ritmo& r) {
  r.iniciado = true;
  r.f = CargarFunciones();
  if (REXCVAR_GET(nfsmw_android_termico) && r.f.termico && (r.f.margen || r.f.estado)) {
    std::thread([f = r.f] { HiloTermico(f); }).detach();
  } else {
    REXLOG_INFO("[termico] recorte por calor {}", REXCVAR_GET(nfsmw_android_termico)
                                                      ? "no disponible (Android 11 o anterior)"
                                                      : "apagado (nfsmw_android_termico)");
  }
  if (!REXCVAR_GET(nfsmw_android_adpf)) {
    REXLOG_INFO("[ritmo] ADPF apagado (nfsmw_android_adpf)");
    return;
  }
  if (!r.f.obtener || !r.f.crear || !r.f.informar || !r.f.cerrar || !(r.gestor = r.f.obtener())) {
    REXLOG_INFO("[ritmo] ADPF no disponible (Android 12 o anterior)");
    return;
  }
  const int fps = std::atoi(rex::cvar::GetFlagByName("nfsmw_limite_fps").c_str());
  r.objetivo_ns = nfsmw::termico::Presupuesto(fps);
  REXLOG_INFO("[ritmo] ADPF: objetivo {:.2f} ms por fotograma ({} FPS); el sistema prefiere un informe cada {} us{}",
              double(r.objetivo_ns) / 1e6, fps > 0 ? fps : 60,
              r.f.ritmo_preferido ? r.f.ritmo_preferido(r.gestor) / 1000 : -1,
              r.f.poner_hilos ? "" : " (Android 13: la sesion se rehace al llegar un hilo)");
  r.activo = true;
  r.desde_ns = AhoraNs();
}

// The session with the threads that joined: setThreads on Android 14+, a new session on 13.
void ActualizarSesion(Ritmo& r) {
  {
    std::lock_guard<std::mutex> cerrojo(g_mutex);
    r.hilos = g_hilos;
  }
  std::vector<int32_t> tids;
  std::string nombres;
  for (const Hilo& h : r.hilos) {
    tids.push_back(int32_t(h.tid));
    nombres += (nombres.empty() ? "" : ", ") + h.papel + " " + std::to_string(h.tid);
  }
  r.antes.assign(r.hilos.size(), 0);
  r.ahora.assign(r.hilos.size(), 0);
  if (tids.empty()) {
    return;
  }
  if (r.sesion && r.f.poner_hilos && r.f.poner_hilos(r.sesion, tids.data(), tids.size()) == 0) {
    REXLOG_INFO("[ritmo] ADPF: hilos de la sesion: {}", nombres);
    return;
  }
  if (r.sesion) {
    r.f.cerrar(r.sesion);
  }
  r.sesion = r.f.crear(r.gestor, tids.data(), tids.size(), r.objetivo_ns);
  if (!r.sesion) {
    REXLOG_INFO("[ritmo] ADPF: el sistema no da una sesion para {}: sin ADPF", nombres);
    r.activo = false;
    return;
  }
  REXLOG_INFO("[ritmo] ADPF: sesion con {}", nombres);
}

}  // namespace

// A hot thread joins the ADPF session (from its own thread). Once per thread is enough; repeats are ignored.
extern "C" void nfsmw_android_ritmo_hilo(const char* papel) {
  Hilo h{gettid(), 0, papel ? papel : "?"};
  if (pthread_getcpuclockid(pthread_self(), &h.reloj) != 0) {
    return;
  }
  std::lock_guard<std::mutex> cerrojo(g_mutex);
  for (const Hilo& otro : g_hilos) {
    if (otro.tid == h.tid) {
      return;
    }
  }
  g_hilos.push_back(h);
  g_hilos_cambiados.store(true, std::memory_order_release);
}

// One game frame, from the ring thread at its Swap.
extern "C" void nfsmw_android_ritmo_fotograma() {
  Ritmo& r = g_ritmo;
  if (!r.iniciado) {
    nfsmw_android_ritmo_hilo("anillo");
    Iniciar(r);
  }
  if (!r.activo) {
    return;
  }
  if (g_hilos_cambiados.exchange(false, std::memory_order_acquire)) {
    ActualizarSesion(r);
    if (!r.activo) {
      return;
    }
  }
  for (size_t i = 0; i < r.hilos.size(); ++i) {
    r.ahora[i] = TiempoCpu(r.hilos[i].reloj);
  }
  const int64_t trabajo = nfsmw::termico::TrabajoFotograma(r.antes, r.ahora);
  r.antes.swap(r.ahora);
  if (trabajo <= 0 || !r.sesion) {
    return;
  }
  const int64_t antes_informe = AhoraNs();
  const int resultado = r.f.informar(r.sesion, trabajo);
  const int64_t ahora = AhoraNs();
  r.ns_informando += ahora - antes_informe;
  ++r.fotogramas;
  r.informes += resultado == 0;
  r.fallos += resultado != 0;
  r.suma_trabajo += trabajo;
  r.max_trabajo = std::max(r.max_trabajo, trabajo);
  r.por_encima += trabajo > r.objetivo_ns;
  if (ahora - r.desde_ns >= 30'000'000'000 && r.fotogramas) {
    REXLOG_INFO("[ritmo] ADPF, ultimos 30 s: {} fotogramas, trabajo del hilo mas cargado {:.2f} ms de media y {:.2f} "
                "de maximo (objetivo {:.2f}; {} por encima); {} informes, {} rechazados, {:.0f} us cada uno",
                r.fotogramas, double(r.suma_trabajo) / double(r.fotogramas) / 1e6, double(r.max_trabajo) / 1e6,
                double(r.objetivo_ns) / 1e6, r.por_encima, r.informes, r.fallos,
                double(r.ns_informando) / double(r.fotogramas) / 1e3);
    r.desde_ns = ahora;
    r.fotogramas = r.informes = r.fallos = 0;
    r.suma_trabajo = r.max_trabajo = r.ns_informando = r.por_encima = 0;
  }
}
