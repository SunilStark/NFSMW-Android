// Disk cache of textures transcoded from BC to ETC2/EAC (nfsmw_texturas_etc.h), and the threads that fill it.
//
// Why a cache: transcoding costs microseconds per 4x4 block (a 1024x1024 texture with its mips is 87,000
// blocks), far too much for the ring thread, which uploads textures while the game streams the city. So the
// first time a texture is seen it is decoded on the CPU as before and handed to these threads, which transcode
// it on the cores the game does not use and save it. The next time (the same zone later, or the next session)
// the renderer creates the texture as ETC2 and uploads it from here.
//
// One file per texture, <folder>/<key in hex>.etc: a 32-byte header (magic, version, key, bytes, XXH3 of the
// blocks) and the blocks in the BC layout. The folder carries the encoder's version (etc::kVersion): a new
// encoder starts an empty folder and the old ones are deleted. Over the size limit, the oldest files go first.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "nfsmw_texturas_bc.h"

namespace nfsmw::etc {

class CacheEtc {
 public:
  struct Estadisticas {
    uint64_t texturas = 0;      // in the cache
    uint64_t bytes = 0;
    uint64_t codificadas = 0;   // transcoded by the threads this session
    uint64_t bloques = 0;       // and their blocks
    uint64_t ns_codificar = 0;  // thread time spent transcoding them
    uint64_t descartadas = 0;   // jobs dropped: queue full
    uint64_t borradas = 0;      // files deleted: over the limit or damaged
    double psnr_medio = 0;      // the first kTexturasMedidas transcoded, against their BC texels
    double psnr_peor = 0;
    uint32_t medidas = 0;
  };

  static constexpr uint32_t kTexturasMedidas = 64;

  ~CacheEtc() { Parar(); }

  // `raiz` is the game's cache folder. False if the folder cannot be used: then nothing is cached.
  bool Iniciar(const std::filesystem::path& raiz, uint64_t limite_bytes, uint32_t hilos, uint64_t cola_bytes);
  void Parar();
  bool Activa() const { return activa_; }
  const std::filesystem::path& Carpeta() const { return carpeta_; }

  bool Contiene(uint64_t clave) const;
  // The cached blocks of `clave`, exactly `bytes` of them. False if missing or damaged (a damaged file is
  // deleted). Called from the ring thread: one file read.
  bool Leer(uint64_t clave, size_t bytes, std::vector<uint8_t>& datos);
  // BC data (host layout, all levels and layers) to transcode and save in the background; copied only if it is
  // taken. False if it is already cached or queued, or the queue is full (it is offered again the next time
  // the texture is seen).
  bool Encargar(uint64_t clave, bc::Formato formato, std::span<const uint8_t> bc);
  // Blocks already transcoded, to save in the background.
  bool Guardar(uint64_t clave, std::span<const uint8_t> etc);
  Estadisticas Estado() const;

 private:
  struct Trabajo {
    uint64_t clave = 0;
    bc::Formato formato = bc::Formato::BC1;
    bool transcodificar = false;  // false: the data is already ETC
    std::vector<uint8_t> datos;
  };
  struct Entrada {
    uint64_t bytes = 0;
    int64_t tiempo = 0;  // last write, for the eviction order
  };

  bool EncolarTrabajo(uint64_t clave, bc::Formato formato, bool transcodificar, std::span<const uint8_t> datos);
  void Hilo(uint32_t indice);
  void Escribir(const Trabajo& trabajo);
  void Medir(const Trabajo& bc, const std::vector<uint8_t>& etc);
  void RecortarSinCerrojo();
  std::filesystem::path Ruta(uint64_t clave) const;
  void Olvidar(uint64_t clave);

  std::filesystem::path carpeta_;
  uint64_t limite_bytes_ = 0;
  uint64_t cola_limite_ = 0;
  bool activa_ = false;

  mutable std::mutex mutex_;
  std::condition_variable aviso_;
  std::unordered_map<uint64_t, Entrada> indice_;
  std::unordered_set<uint64_t> en_cola_;
  std::deque<Trabajo> cola_;
  uint64_t bytes_ = 0;
  uint64_t bytes_cola_ = 0;
  bool parar_ = false;
  std::vector<std::thread> hilos_;

  std::atomic<uint64_t> codificadas_{0}, bloques_{0}, ns_codificar_{0}, descartadas_{0}, borradas_{0};
  // Quality of the first transcoded textures (Medir), under mutex_.
  double suma_psnr_ = 0, peor_psnr_ = 0;
  uint32_t medidas_ = 0;
};

}  // namespace nfsmw::etc
