// Profile dump of the instrumented build (NFSMW_ANDROID_PGO=generar, src/main/cpp/CMakeLists.txt).
//
// clang's profile runtime writes its counters when the process exits, and an Android app never exits: the system
// kills it. So a thread writes them every minute to pgo/nfsmw.profraw in the game folder (the app has All files
// access there, and adb can pull it). Each write is the whole session so far: the last one is the one to keep.
// The counters are updated without locks while the game runs, so a write may catch a few of them mid-update;
// llvm-profdata takes that, and a profile only needs the proportions.
#include <sys/stat.h>

#include <chrono>
#include <string>
#include <thread>

#include <rex/cvar.h>
#include <rex/logging.h>

extern "C" {
int __llvm_profile_write_file(void);
void __llvm_profile_set_filename(const char* nombre);
}

namespace {

void HiloPgo() {
  // The first minute: the cvars and the log are up by then, and loading is not what the profile is for.
  std::this_thread::sleep_for(std::chrono::seconds(60));
  std::string raiz = rex::cvar::GetFlagByName("game_data_root");
  if (raiz.empty()) {
    raiz = "/sdcard/nsfmw-androidevolved";  // GameActivity's default folder
  }
  const std::string carpeta = raiz + "/pgo";
  mkdir(carpeta.c_str(), 0775);
  static std::string fichero;  // the runtime keeps the pointer
  fichero = carpeta + "/nfsmw.profraw";
  __llvm_profile_set_filename(fichero.c_str());
  for (int minuto = 1;; ++minuto) {
    const int resultado = __llvm_profile_write_file();
    REXLOG_INFO("[pgo] perfil escrito en {} ({} min de juego): {}", fichero, minuto,
                resultado == 0 ? "bien" : "ERROR");
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
}

struct Arranque {
  Arranque() { std::thread(HiloPgo).detach(); }
} g_arranque;

}  // namespace
