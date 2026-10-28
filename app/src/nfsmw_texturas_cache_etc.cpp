// Disk cache of BC textures transcoded to ETC2/EAC: see nfsmw_texturas_cache_etc.h.
#include "nfsmw_texturas_cache_etc.h"

#include "nfsmw_texturas_etc.h"

#include <rex/logging.h>
#include <rex/thread.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

#if defined(__ANDROID__)
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

// As in nfsmw_nativo_dibujos.cpp: xxHash reads with memcpy (see the comment there).
#undef XXH_FORCE_MEMORY_ACCESS
#define XXH_FORCE_MEMORY_ACCESS 0
#define XXH_INLINE_ALL
#include <xxhash.h>

namespace nfsmw::etc {
namespace {

constexpr uint32_t kMagia = 0x3245464E;  // "NFE2"

struct Cabecera {
  uint32_t magia = kMagia;
  uint32_t version = kVersion;
  uint64_t clave = 0;
  uint64_t bytes = 0;  // of the blocks, after the header
  uint64_t huella = 0;  // XXH3 of the blocks
};
static_assert(sizeof(Cabecera) == 32);

int64_t Ahora() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::filesystem::file_time_type::clock::now().time_since_epoch())
      .count();
}

bool ClaveDeNombre(const std::filesystem::path& ruta, uint64_t& clave) {
  const std::string nombre = ruta.stem().string();
  if (nombre.size() != 16 || ruta.extension() != ".etc") {
    return false;
  }
  char* fin = nullptr;
  clave = std::strtoull(nombre.c_str(), &fin, 16);
  return fin && *fin == 0;
}

#if defined(__ANDROID__)
// To the cores the game is not on. android_rendimiento.cpp keeps the game on the fast cores, so these are the
// little ones, otherwise idle. Checked before every job, because that pinning moves every thread of the
// process, these included. If the process is not pinned, nothing changes (low priority only).
void ALosNucleosLibres(cpu_set_t& puesta) {
  cpu_set_t actual;
  CPU_ZERO(&actual);
  if (sched_getaffinity(0, sizeof(actual), &actual) != 0 || CPU_EQUAL(&actual, &puesta)) {
    return;
  }
  const long total = sysconf(_SC_NPROCESSORS_CONF);
  cpu_set_t libres;
  CPU_ZERO(&libres);
  for (long cpu = 0; cpu < total && cpu < CPU_SETSIZE; ++cpu) {
    if (!CPU_ISSET(cpu, &actual)) {
      CPU_SET(cpu, &libres);
    }
  }
  if (CPU_COUNT(&libres) && sched_setaffinity(0, sizeof(libres), &libres) == 0) {
    puesta = libres;
  }
}
#endif

}  // namespace

std::filesystem::path CacheEtc::Ruta(uint64_t clave) const {
  char nombre[24];
  std::snprintf(nombre, sizeof(nombre), "%016llx.etc", static_cast<unsigned long long>(clave));
  return carpeta_ / nombre;
}

bool CacheEtc::Iniciar(const std::filesystem::path& raiz, uint64_t limite_bytes, uint32_t hilos,
                       uint64_t cola_bytes) {
  Parar();
  carpeta_ = raiz / ("texturas_etc2_v" + std::to_string(kVersion));
  std::error_code error;
  std::filesystem::create_directories(carpeta_, error);
  if (error || !std::filesystem::is_directory(carpeta_, error)) {
    REXLOG_WARN("[compatibilidad] ETC2: no se puede usar la carpeta {}: sin cache de texturas", carpeta_.string());
    return false;
  }
  // The folders of earlier encoders hold blocks this one would not write.
  for (const auto& entrada : std::filesystem::directory_iterator(raiz, error)) {
    const std::string nombre = entrada.path().filename().string();
    std::error_code sin_importancia;
    if (nombre.rfind("texturas_etc2_v", 0) == 0 && entrada.path() != carpeta_ &&
        entrada.is_directory(sin_importancia)) {
      std::filesystem::remove_all(entrada.path(), sin_importancia);
    }
  }
  std::lock_guard<std::mutex> cerrojo(mutex_);
  indice_.clear();
  bytes_ = 0;
  for (const auto& entrada : std::filesystem::directory_iterator(carpeta_, error)) {
    std::error_code e;
    uint64_t clave = 0;
    if (entrada.path().extension() == ".tmp") {  // a write cut short
      std::filesystem::remove(entrada.path(), e);
      continue;
    }
    if (!ClaveDeNombre(entrada.path(), clave)) {
      continue;
    }
    const uint64_t bytes = entrada.file_size(e);
    if (e || bytes <= sizeof(Cabecera)) {
      std::filesystem::remove(entrada.path(), e);
      continue;
    }
    const auto escrito = entrada.last_write_time(e);
    indice_[clave] = {bytes, e ? 0 : std::chrono::duration_cast<std::chrono::seconds>(escrito.time_since_epoch()).count()};
    bytes_ += bytes;
  }
  limite_bytes_ = limite_bytes;
  cola_limite_ = cola_bytes;
  RecortarSinCerrojo();
  parar_ = false;
  activa_ = true;
  for (uint32_t i = 0; i < hilos; ++i) {
    hilos_.emplace_back([this, i] { Hilo(i); });
  }
  REXLOG_INFO("[compatibilidad] ETC2: cache de texturas en {}: {} texturas, {} MB de {} MB; {} hilos de conversion",
              carpeta_.string(), indice_.size(), bytes_ >> 20, limite_bytes_ >> 20, hilos);
  return true;
}

void CacheEtc::Parar() {
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    parar_ = true;
    cola_.clear();  // what is still queued is offered again next session
    en_cola_.clear();
    bytes_cola_ = 0;
  }
  aviso_.notify_all();
  for (std::thread& hilo : hilos_) {
    hilo.join();
  }
  hilos_.clear();
  activa_ = false;
}

bool CacheEtc::Contiene(uint64_t clave) const {
  std::lock_guard<std::mutex> cerrojo(mutex_);
  return indice_.count(clave) != 0;
}

void CacheEtc::Olvidar(uint64_t clave) {
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    const auto it = indice_.find(clave);
    if (it == indice_.end()) {
      return;
    }
    bytes_ -= it->second.bytes;
    indice_.erase(it);
  }
  std::error_code e;
  std::filesystem::remove(Ruta(clave), e);
  ++borradas_;
}

bool CacheEtc::Leer(uint64_t clave, size_t bytes, std::vector<uint8_t>& datos) {
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    const auto it = indice_.find(clave);
    // Another size is another texture's file (or a damaged one, which its own lookup deletes).
    if (it == indice_.end() || it->second.bytes != bytes + sizeof(Cabecera)) {
      return false;
    }
  }
  std::ifstream fichero(Ruta(clave), std::ios::binary);
  Cabecera cabecera;
  datos.resize(bytes);
  if (!fichero.read(reinterpret_cast<char*>(&cabecera), sizeof(cabecera)) || cabecera.magia != kMagia ||
      cabecera.version != kVersion || cabecera.clave != clave || cabecera.bytes != bytes ||
      !fichero.read(reinterpret_cast<char*>(datos.data()), std::streamsize(bytes)) ||
      XXH3_64bits(datos.data(), bytes) != cabecera.huella) {
    REXLOG_WARN("[compatibilidad] ETC2: {} estaba danado: se borra", Ruta(clave).filename().string());
    // Closed first: Windows does not delete a file that is still open (the PC tests run there).
    fichero.close();
    Olvidar(clave);
    return false;
  }
  return true;
}

bool CacheEtc::EncolarTrabajo(uint64_t clave, bc::Formato formato, bool transcodificar,
                              std::span<const uint8_t> datos) {
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    if (!activa_ || parar_ || indice_.count(clave) || en_cola_.count(clave)) {
      return false;
    }
    if (bytes_cola_ + datos.size() > cola_limite_) {
      ++descartadas_;
      return false;
    }
    bytes_cola_ += datos.size();
    en_cola_.insert(clave);
    cola_.push_back({clave, formato, transcodificar, std::vector<uint8_t>(datos.begin(), datos.end())});
  }
  aviso_.notify_one();
  return true;
}

bool CacheEtc::Encargar(uint64_t clave, bc::Formato formato, std::span<const uint8_t> bc) {
  return EncolarTrabajo(clave, formato, true, bc);
}

bool CacheEtc::Guardar(uint64_t clave, std::span<const uint8_t> etc) {
  return EncolarTrabajo(clave, bc::Formato::BC1, false, etc);
}

void CacheEtc::Hilo(uint32_t indice) {
  rex::thread::set_current_thread_name("NFSMW ETC2 " + std::to_string(indice));
#if defined(__ANDROID__)
  setpriority(PRIO_PROCESS, gettid(), 10);  // below the game's threads where they share a core
  cpu_set_t puesta;
  CPU_ZERO(&puesta);
#endif
  for (;;) {
    Trabajo trabajo;
    {
      std::unique_lock<std::mutex> cerrojo(mutex_);
      aviso_.wait(cerrojo, [this] { return parar_ || !cola_.empty(); });
      if (parar_) {
        return;
      }
      trabajo = std::move(cola_.front());
      cola_.pop_front();
    }
#if defined(__ANDROID__)
    ALosNucleosLibres(puesta);
#endif
    const size_t bytes = trabajo.datos.size();
    if (trabajo.transcodificar) {
      std::vector<uint8_t> etc(bytes);
      const auto inicio = std::chrono::steady_clock::now();
      const bool ok = Transcodificar(trabajo.formato, trabajo.datos, etc);
      ns_codificar_ += uint64_t(
          std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - inicio).count());
      if (ok) {
        ++codificadas_;
        bloques_ += bytes / bc::BytesBloque(trabajo.formato);
        Medir(trabajo, etc);
        trabajo.datos.swap(etc);
        Escribir(trabajo);
      }
    } else {
      Escribir(trabajo);
    }
    std::lock_guard<std::mutex> cerrojo(mutex_);
    en_cola_.erase(trabajo.clave);
    bytes_cola_ -= std::min<uint64_t>(bytes_cola_, bytes);
  }
}

void CacheEtc::Escribir(const Trabajo& trabajo) {
  const std::filesystem::path ruta = Ruta(trabajo.clave);
  std::filesystem::path temporal = ruta;
  temporal += ".tmp";
  Cabecera cabecera;
  cabecera.clave = trabajo.clave;
  cabecera.bytes = trabajo.datos.size();
  cabecera.huella = XXH3_64bits(trabajo.datos.data(), trabajo.datos.size());
  std::error_code error;
  {
    std::ofstream fichero(temporal, std::ios::binary | std::ios::trunc);
    if (!fichero || !fichero.write(reinterpret_cast<const char*>(&cabecera), sizeof(cabecera)) ||
        !fichero.write(reinterpret_cast<const char*>(trabajo.datos.data()), std::streamsize(trabajo.datos.size()))) {
      fichero.close();
      std::filesystem::remove(temporal, error);
      return;
    }
  }
  std::filesystem::rename(temporal, ruta, error);
  if (error) {
    std::filesystem::remove(temporal, error);
    return;
  }
  std::lock_guard<std::mutex> cerrojo(mutex_);
  Entrada& entrada = indice_[trabajo.clave];
  bytes_ += sizeof(Cabecera) + trabajo.datos.size() - entrada.bytes;
  entrada = {sizeof(Cabecera) + trabajo.datos.size(), Ahora()};
  RecortarSinCerrojo();
}

// Over the limit: the oldest files until 90 % of it is left.
void CacheEtc::RecortarSinCerrojo() {
  if (bytes_ <= limite_bytes_) {
    return;
  }
  std::vector<std::pair<int64_t, uint64_t>> por_tiempo;
  por_tiempo.reserve(indice_.size());
  for (const auto& [clave, entrada] : indice_) {
    por_tiempo.emplace_back(entrada.tiempo, clave);
  }
  std::sort(por_tiempo.begin(), por_tiempo.end());
  const uint64_t objetivo = limite_bytes_ / 10 * 9;
  for (const auto& [tiempo, clave] : por_tiempo) {
    if (bytes_ <= objetivo) {
      break;
    }
    bytes_ -= indice_[clave].bytes;
    indice_.erase(clave);
    std::error_code e;
    std::filesystem::remove(Ruta(clave), e);
    ++borradas_;
  }
}

// The first kTexturasMedidas textures transcoded: PSNR of their ETC texels against their BC texels, over the
// channels the format has (BC1: RGB of the opaque texels; transparency is kept exactly).
void CacheEtc::Medir(const Trabajo& bc, const std::vector<uint8_t>& etc) {
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    if (medidas_ >= kTexturasMedidas) {
      return;
    }
  }
  const uint32_t bytes = bc::BytesBloque(bc.formato);
  const uint32_t canales = bc::Canales(bc.formato);
  const uint32_t medidos = bc.formato == bc::Formato::BC1 ? 3 : canales;
  double suma = 0;
  uint64_t n = 0;
  for (size_t i = 0; i + bytes <= bc.datos.size(); i += bytes) {
    uint8_t esperado[64]{}, obtenido[64]{};
    bc::Bloque(bc.formato, bc.datos.data() + i, esperado);
    Decodificar(bc.formato, etc.data() + i, obtenido);
    for (uint32_t t = 0; t < 16; ++t) {
      if (bc.formato == bc::Formato::BC1 && esperado[t * 4 + 3] == 0) {
        continue;
      }
      for (uint32_t c = 0; c < medidos; ++c) {
        const double d = double(esperado[t * canales + c]) - obtenido[t * canales + c];
        suma += d * d;
        ++n;
      }
    }
  }
  const double psnr = !n || suma == 0 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 * double(n) / suma);
  std::lock_guard<std::mutex> cerrojo(mutex_);
  if (medidas_ >= kTexturasMedidas) {
    return;
  }
  suma_psnr_ += psnr;
  peor_psnr_ = medidas_ ? std::min(peor_psnr_, psnr) : psnr;
  ++medidas_;
  if (medidas_ == 8 || medidas_ == kTexturasMedidas) {
    REXLOG_INFO("[compatibilidad] ETC2: las {} primeras texturas convertidas, contra sus texels BC: {:.1f} dB de "
                "media, la peor {:.1f} dB",
                medidas_, suma_psnr_ / medidas_, peor_psnr_);
  }
}

CacheEtc::Estadisticas CacheEtc::Estado() const {
  Estadisticas e;
  {
    std::lock_guard<std::mutex> cerrojo(mutex_);
    e.texturas = indice_.size();
    e.bytes = bytes_;
    e.medidas = medidas_;
    e.psnr_medio = medidas_ ? suma_psnr_ / medidas_ : 0;
    e.psnr_peor = peor_psnr_;
  }
  e.codificadas = codificadas_;
  e.bloques = bloques_;
  e.ns_codificar = ns_codificar_;
  e.descartadas = descartadas_;
  e.borradas = borradas_;
  return e;
}

}  // namespace nfsmw::etc
