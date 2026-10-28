// clang++ -std=c++20 -O2 -pthread -DFMT_HEADER_ONLY -Itools/tests/rex_minimo -Isdk/thirdparty/fmt/include \
//     -Isdk/thirdparty/xxHash tools/tests/texturas_cache_etc_test.cpp -o texturas_cache_etc_test
// (sdk/thirdparty from tools/fetch_thirdparty.py, for fmt and xxHash.)
// The disk cache of ETC2 textures (app/src/nfsmw_texturas_cache_etc.cpp): what the threads transcode is what
// Transcodificar gives, it survives a restart, damaged files are deleted, older encoders' folders and cut-short
// writes are removed, the size limit drops the oldest files and a full queue drops the job.
#include "../../app/src/nfsmw_texturas_cache_etc.cpp"

#include <cstdio>
#include <random>

namespace fs = std::filesystem;
using nfsmw::bc::Formato;
using nfsmw::etc::CacheEtc;

static void Check(bool ok, const char* message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

static bool Esperar(const CacheEtc& cache, uint64_t clave) {
  for (int i = 0; i < 500; ++i) {
    if (cache.Contiene(clave)) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

int main() {
  const fs::path raiz = fs::temp_directory_path() / ("nfsmw_cache_etc_test_" + std::to_string(::getpid()));
  fs::remove_all(raiz);
  const fs::path carpeta = raiz / ("texturas_etc2_v" + std::to_string(nfsmw::etc::kVersion));
  fs::create_directories(raiz / "texturas_etc2_v0");
  std::ofstream(raiz / "texturas_etc2_v0" / "0000000000000001.etc") << "old encoder";
  fs::create_directories(carpeta);
  std::ofstream(carpeta / "0000000000000002.etc.tmp") << "cut short";

  std::mt19937 azar(7);
  const auto aleatorio = [&](size_t bytes) {
    std::vector<uint8_t> v(bytes);
    for (auto& b : v) b = uint8_t(azar());
    return v;
  };
  const Formato formatos[] = {Formato::BC1, Formato::BC3, Formato::BC5};
  std::vector<std::vector<uint8_t>> esperados;
  {
    CacheEtc cache;
    Check(cache.Iniciar(raiz, 64 << 20, 2, 16 << 20), "start");
    Check(!fs::exists(raiz / "texturas_etc2_v0"), "an older encoder's folder is deleted");
    Check(!fs::exists(carpeta / "0000000000000002.etc.tmp"), "a cut-short write is deleted");
    for (uint64_t i = 0; i < 3; ++i) {
      std::vector<uint8_t> bc = aleatorio(16 * 1000);
      std::vector<uint8_t> etc(bc.size());
      nfsmw::etc::Transcodificar(formatos[i], bc, etc);
      esperados.push_back(etc);
      Check(cache.Encargar(100 + i, formatos[i], bc), "a job is queued");
    }
    for (uint64_t i = 0; i < 3; ++i) Check(Esperar(cache, 100 + i), "the threads transcode and save it");
    Check(!cache.Encargar(100, Formato::BC1, aleatorio(16)), "a cached key is not queued again");
    std::vector<uint8_t> leido;
    for (uint64_t i = 0; i < 3; ++i) {
      Check(cache.Leer(100 + i, esperados[i].size(), leido) && leido == esperados[i],
            "what is read is what Transcodificar gives");
    }
    Check(!cache.Leer(100, 8, leido) && cache.Contiene(100), "another size is another texture: missing, file kept");
    Check(cache.Guardar(200, esperados[0]) && Esperar(cache, 200), "transcoded blocks are saved");
    Check(cache.Leer(200, esperados[0].size(), leido) && leido == esperados[0], "and read back");
    Check(!cache.Encargar(300, Formato::BC1, aleatorio(17 << 20)), "a full queue drops the job");
    const CacheEtc::Estadisticas e = cache.Estado();
    Check(e.codificadas == 3 && e.bloques == 4000 && e.descartadas == 1 && e.texturas == 4, "statistics");
    Check(e.medidas == 3 && e.psnr_peor > 20, "the first textures' quality is measured");
    std::printf("cache: %llu textures, %llu KB, %.2f us per block, PSNR of random blocks %.1f dB (worst %.1f)\n",
                (unsigned long long)e.texturas, (unsigned long long)(e.bytes >> 10),
                e.ns_codificar / 1000.0 / double(e.bloques), e.psnr_medio, e.psnr_peor);
  }
  // A damaged file is deleted when read.
  {
    std::fstream f(carpeta / "0000000000000065.etc", std::ios::in | std::ios::out | std::ios::binary);
    f.seekp(100);
    f.put(char(0x5A));
  }
  {
    CacheEtc cache;
    Check(cache.Iniciar(raiz, 64 << 20, 1, 16 << 20), "restart");
    Check(cache.Contiene(100) && cache.Contiene(101) && cache.Contiene(102) && cache.Contiene(200),
          "the index is rebuilt from the folder");
    std::vector<uint8_t> leido;
    Check(!cache.Leer(101, esperados[1].size(), leido), "a damaged file is not used");
    Check(!cache.Contiene(101) && !fs::exists(carpeta / "0000000000000065.etc"), "and it is deleted");
    Check(cache.Leer(100, esperados[0].size(), leido) && leido == esperados[0], "the others still read");
  }
  // Over the limit, the oldest files go first.
  {
    const auto ahora = fs::file_time_type::clock::now();
    fs::last_write_time(carpeta / "0000000000000064.etc", ahora - std::chrono::hours(3));  // 100
    fs::last_write_time(carpeta / "0000000000000066.etc", ahora - std::chrono::hours(2));  // 102
    fs::last_write_time(carpeta / "00000000000000c8.etc", ahora - std::chrono::hours(1));  // 200
    CacheEtc cache;
    const uint64_t uno = esperados[0].size() + 32;
    Check(cache.Iniciar(raiz, uno * 2, 1, 16 << 20), "restart with a smaller limit");
    Check(!cache.Contiene(100) && !cache.Contiene(102) && cache.Contiene(200), "the oldest go, down to 90 %");
  }
  fs::remove_all(raiz);
  std::printf("ETC2 texture cache: OK\n");
  return 0;
}
