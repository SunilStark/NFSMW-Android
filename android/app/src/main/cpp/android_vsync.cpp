// The panel's vsync as the game's vblank (Android).
//
// The game paces itself by counting vblank interrupts (BucleVblank, app/src/nfsmw_nativo_sistema.cpp), which came
// from a timer at nfsmw_limite_fps. That timer and the panel are two clocks: they beat against each other, and every
// few seconds a frame stays on screen one refresh longer or shorter (the micro-stutter described for the Switch in
// nfsmw_ajustes_graficos.cpp). At 45 FPS on a 90 Hz panel that is the difference between every frame shown for
// exactly two refreshes and a visible judder.
//
// Here a thread with a looper receives Choreographer's callback on every refresh of the panel, and BucleVblank fires
// the game's interrupt every N of them: 45 FPS on 90 Hz is every 2nd refresh, 60 on 60 every one, 30 on 60 every 2nd.
// One clock, the panel's. Where the panel's rate is not a whole multiple of the limit (this tablet switches between
// 60 and 90 Hz by itself: 90 over a limit of 60 is 1.5), BucleVblank uses its timer for as long as that lasts, so the
// game always gets its full rate.
//
// The panel's period comes from Android (AChoreographer_registerRefreshRateCallback, API 30), which says when it
// changes. Without it, it is the median gap between callbacks over the last 32 (on the tablet the shortest gap was
// not usable: callbacks that ran late came in pairs a few ms apart, read as 240-400 Hz). A callback that came late
// counts as the refreshes it covered.
//
// Loaded from libandroid.so with dlsym (AChoreographer_postFrameCallback64 is API 29, the app's minimum is 26). Where
// it is missing, or while no callback comes (the app in the background), BucleVblank keeps its timer.
#include <dlfcn.h>
#include <pthread.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_BOOL(nfsmw_android_vblank_pantalla, true, "NFSMW",
                    "Android: el vblank del juego sigue al refresco real de la pantalla (Choreographer), una "
                    "interrupcion cada N refrescos, en vez de un temporizador propio que bate contra el panel. "
                    "false = el temporizador de siempre")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace {

struct ALooper;
struct AChoreographer;
using RetroLlamada64 = void (*)(int64_t, void*);
using RetroLlamada = void (*)(long, void*);
using RetroRefresco = void (*)(int64_t, void*);

struct Funciones {
  void (*registrar_refresco)(AChoreographer*, RetroRefresco, void*) = nullptr;  // API 30
  ALooper* (*preparar)(int) = nullptr;
  int (*sondear)(int, int*, int*, void**) = nullptr;
  AChoreographer* (*instancia)() = nullptr;
  void (*pedir64)(AChoreographer*, RetroLlamada64, void*) = nullptr;  // API 29
  void (*pedir)(AChoreographer*, RetroLlamada, void*) = nullptr;      // API 24, deprecated
};

template <typename F>
void Cargar(void* biblioteca, const char* nombre, F& destino) {
  destino = reinterpret_cast<F>(dlsym(biblioteca, nombre));
}

constexpr size_t kVentana = 32;

std::mutex g_mutex;
std::condition_variable g_cv;
uint64_t g_cuenta = 0;        // panel refreshes seen
int64_t g_ultimo_ns = 0;      // the last callback's frame time (CLOCK_MONOTONIC)
int64_t g_periodo_ns = 0;     // the panel's period: Android's, or the median gap of the window
int64_t g_periodo_sistema_ns = 0;  // from the refresh rate callback (0 until it says)
std::array<int64_t, kVentana> g_huecos{};
size_t g_hueco = 0;

// A gap between two refreshes: 4 ms (250 Hz) to 50 ms (20 Hz). Anything else is a late or bunched callback.
bool HuecoCreible(int64_t hueco) {
  return hueco >= 4'000'000 && hueco <= 50'000'000;
}

int64_t Mediana() {
  std::array<int64_t, kVentana> copia{};
  size_t n = 0;
  for (int64_t h : g_huecos) {
    if (HuecoCreible(h)) copia[n++] = h;
  }
  if (n == 0) return 0;
  std::nth_element(copia.begin(), copia.begin() + n / 2, copia.begin() + n);
  return copia[n / 2];
}

void AlCambiarRefresco(int64_t periodo_ns, void*) {
  {
    std::lock_guard<std::mutex> cerrojo(g_mutex);
    g_periodo_sistema_ns = periodo_ns;
    g_periodo_ns = periodo_ns;
  }
  REXLOG_INFO("[vblank] el sistema dice: panel a {:.1f} Hz", periodo_ns > 0 ? 1e9 / double(periodo_ns) : 0.0);
}

Funciones g_f;
AChoreographer* g_coreografo = nullptr;  // only the looper thread uses it

void Pedir();

void AlRefrescar(int64_t tiempo_ns) {
  {
    std::lock_guard<std::mutex> cerrojo(g_mutex);
    if (g_ultimo_ns > 0 && tiempo_ns > g_ultimo_ns) {
      const int64_t hueco = tiempo_ns - g_ultimo_ns;
      g_huecos[g_hueco++ % kVentana] = hueco;
      if (g_periodo_sistema_ns <= 0) {
        g_periodo_ns = Mediana();
      }
      // A late callback covers the refreshes it missed.
      const int64_t periodo = g_periodo_ns > 0 ? g_periodo_ns : hueco;
      g_cuenta += uint64_t(std::max<int64_t>(1, (hueco + periodo / 2) / periodo));
    } else {
      ++g_cuenta;
    }
    g_ultimo_ns = tiempo_ns;
  }
  g_cv.notify_all();
  Pedir();
}

void AlRefrescar64(int64_t tiempo_ns, void*) { AlRefrescar(tiempo_ns); }
void AlRefrescarViejo(long tiempo_ns, void*) { AlRefrescar(int64_t(tiempo_ns)); }

void Pedir() {
  if (g_f.pedir64) {
    g_f.pedir64(g_coreografo, AlRefrescar64, nullptr);
  } else {
    g_f.pedir(g_coreografo, AlRefrescarViejo, nullptr);
  }
}

}  // namespace

// Starts the looper thread once. False where Choreographer is missing or the cvar is off: then the timer.
extern "C" bool nfsmw_android_vsync_iniciar() {
  static const bool listo = [] {
    if (!REXCVAR_GET(nfsmw_android_vblank_pantalla)) {
      REXLOG_INFO("[vblank] temporizador propio (nfsmw_android_vblank_pantalla = false)");
      return false;
    }
    void* biblioteca = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (biblioteca) {
      Cargar(biblioteca, "ALooper_prepare", g_f.preparar);
      Cargar(biblioteca, "ALooper_pollOnce", g_f.sondear);
      Cargar(biblioteca, "AChoreographer_getInstance", g_f.instancia);
      Cargar(biblioteca, "AChoreographer_postFrameCallback64", g_f.pedir64);
      Cargar(biblioteca, "AChoreographer_postFrameCallback", g_f.pedir);
      Cargar(biblioteca, "AChoreographer_registerRefreshRateCallback", g_f.registrar_refresco);
    }
    if (!g_f.preparar || !g_f.sondear || !g_f.instancia || (!g_f.pedir64 && !g_f.pedir)) {
      REXLOG_WARN("[vblank] sin Choreographer en este Android: temporizador propio");
      return false;
    }
    std::mutex m;
    std::condition_variable cv;
    int estado = 0;  // 0 starting, 1 running, -1 failed
    std::thread([&] {
      pthread_setname_np(pthread_self(), "nfsmw-vsync");
      g_f.preparar(0);
      g_coreografo = g_f.instancia();
      {
        // Notified under the lock: the starting function returns, and m and cv go, as soon as it sees estado.
        std::lock_guard<std::mutex> cerrojo(m);
        estado = g_coreografo ? 1 : -1;
        cv.notify_all();
      }
      if (!g_coreografo) {
        return;
      }
      if (g_f.registrar_refresco) {
        g_f.registrar_refresco(g_coreografo, AlCambiarRefresco, nullptr);
      }
      Pedir();
      for (;;) {
        g_f.sondear(-1, nullptr, nullptr, nullptr);
      }
    }).detach();
    std::unique_lock<std::mutex> cerrojo(m);
    cv.wait(cerrojo, [&] { return estado != 0; });
    if (estado < 0) {
      REXLOG_WARN("[vblank] Choreographer no dio instancia: temporizador propio");
      return false;
    }
    REXLOG_INFO("[vblank] al ritmo del panel (Choreographer{}; periodo {})", g_f.pedir64 ? "" : ", API antigua",
                g_f.registrar_refresco ? "del sistema" : "medido");
    return true;
  }();
  return listo;
}

// Waits up to espera_ms for refreshes after *visto. Returns how many came (0: none in that time), updates *visto and
// the panel's period in *periodo_ns (0 until two callbacks came).
extern "C" int nfsmw_android_vsync_esperar(uint64_t* visto, int64_t* periodo_ns, int espera_ms) {
  std::unique_lock<std::mutex> cerrojo(g_mutex);
  g_cv.wait_for(cerrojo, std::chrono::milliseconds(espera_ms), [&] { return g_cuenta != *visto; });
  const uint64_t nuevos = g_cuenta - *visto;
  *visto = g_cuenta;
  *periodo_ns = g_periodo_ns;
  return int(std::min<uint64_t>(nuevos, 1000));
}
