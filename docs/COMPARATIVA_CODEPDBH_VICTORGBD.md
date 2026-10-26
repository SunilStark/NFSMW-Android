# Comparación técnica Evolved 2

Auditoría iniciada el 26 de octubre de 2026. Base propia: `026f7d07b57dcb1969349b220b945c8e24ca6a0a`
(0.6.0). Referencia: `dd76acb032ccbd7b0494985242489cd9cd90a939`. No se compara contra una release supuesta.
La copia local coincidía con el árbol oficial; se recuperó su historial sin reemplazar los archivos y se creó
`evolved-2/audit-20261026`. La referencia está en un checkout separado.

## Procedencia comprobada

`6df1501974d0db84893f5a3ca465b616414f6961` es un commit de nuestro historial. El commit `d737bc7` del fork
incorpora la instantánea en `nativo/`: los **88 archivos comunes de `app/` tienen blobs Git idénticos**.
Esto documenta una relación de código y las modificaciones posteriores; no es una conclusión de infracción.
La GPL permite reutilización con sus condiciones y atribución.

El inventario reproducible `python tools/evolved2_audit.py --reference <checkout-del-fork>` obtuvo 2.238 rutas
equivalentes con contenido idéntico, 77 modificadas, 170 sin ruta equivalente y 20 nuevas en el motor del fork.
Normaliza CRLF solamente en texto UTF-8; mantiene los binarios intactos. No equivale a comparación semántica:
los paquetes Java distintos, por ejemplo, requieren revisión manual. El JSON detallado queda en `out/evolved2/`.

## Matriz de selección

| Función | Propio | Referencia | Procedencia | Estado propio y diferencia | Decisión / prioridad | Compatibilidad, riesgo y licencia | Implementación |
|---|---|---|---|---|---|---|---|
| Vulkan 1.1 sin Int64/BDA | `app/src/nfsmw_spirv_vulkan11.h`, shaders | `nativo/app/` | Nuestro `6df1501` | Ya existe; no atribuirlo como novedad del fork | YA EXISTE / conservar | Requiere las funciones de descriptor indexing; GPL-3.0, SDK BSD-3-Clause | Preservada |
| Cuatro conjuntos de descriptores | `nfsmw_nativo_dibujos.cpp`, `nfsmw_spirv_descriptores.h` | Equivalentes `nativo/app/src/` | Fork `3d9358e`, adaptado en nuestro `a7e6e4c` | Ya integrado en 0.5.4 | YA EXISTE / conservar | No añade funciones ausentes del driver; GPL-3.0 | Preservada |
| Publicación ARM release/acquire | `nfsmw_espera_fotograma.cpp`, `nfsmw_ordenes_publicadas.h` | Equivalentes del fork | `3d9358e` / nuestro `a7e6e4c` | Ya integrado; revisar diferencias, no duplicar hooks | YA EXISTE / alta | Las direcciones son de PAL España; GPL-3.0 | Preservada |
| BC en CPU y Xclipse BC4/5 | `nfsmw_texturas_bc.h`, `nfsmw_nativo_dibujos.cpp` | Equivalentes del fork | Arreglos propios más `3d9358e` | Ya existe la ruta de conversión | YA EXISTE / conservar | Comparar alfa, cubos y mipmaps antes de reducir calidad; GPL-3.0 | Preservada |
| Drivers externos | `GpuDrivers.java`, cargador SDK | Launcher del fork, adrenotools | Nuestro `a7e6e4c`; bylaws | Importación, prueba aislada y retorno al sistema ya existen | YA EXISTE / conservar | Turnip/PanVK dependen del hardware; BSD-2-Clause de las librerías | Preservada |
| Solapamientos del CRT recompilado | `sdk/src/kernel/crt/memory.cpp` | `nativo/sdk/src/kernel/crt/memory.cpp` | `44481d1dc09e237ca944d16e9de8bb762d0b5264`, kayo-b | Propio usa `std::memcpy`; fork conserva la copia adelantada del juego | ADAPTAR / crítica | Limitarse a `rexcrt_memcpy`; copiar hacia atrás con memmove no siempre reproduce el juego. SDK BSD-3-Clause, atribución del cambio | Seleccionada, pendiente de integrar y probar |
| Audio XMA, reinicio de decoder | `sdk/src/audio/xma_context.cpp` | Equivalente del fork | Base propia | Las secciones revisadas de bloqueo/reinicio coinciden | YA EXISTE / alta | No cambiar SDL/volumen/búfer sin medición; SDK BSD-3-Clause y FFmpeg LGPL | Conservar; auditoría restante pendiente |
| Conversión SPIR-V a GLSL ES | No existe | `nfsmw_gles_shaders.cpp/.h`, herramienta GLES | `4554a99` | Nuevo: recursos por registro; no acepta directamente nuestra biblioteca Vulkan con descriptor arrays | ADAPTAR / alta | SPIRV-Cross `2c32b6bf86f3c4a5539aa1f0bacbd59fe61759cf` (Apache-2.0), adaptación GPL-3.0 | Pendiente |
| Presentador EGL/GLES | No existe | `nativo/sdk/src/ui/gles/`, `include/rex/ui/gles/` | `658b842`, ampliaciones `7d8aed4` | Nuevo: contextos compartidos, superficies, sincronización y efectos | ADAPTAR / alta | Mantener ciclo de vida SDL y Vulkan. Revisar avisos por archivo y SDK BSD-3-Clause; proyecto GPL-3.0 | Pendiente |
| Destinos y dibujos GLES | Vulkan en `nfsmw_nativo_destinos.cpp`, `nfsmw_nativo_dibujos.cpp` | Archivos `_gles.cpp/.h`, helpers comunes | `7d8aed4` | Implementa interfaces paralelas, no requiere sustituir Vulkan | ADAPTAR / alta | Límites reales de unidades, UBO y formatos; conserva recursos propios por API; GPL-3.0 | Pendiente |
| Caché binaria GLES | No existe | `nfsmw_nativo_dibujos_gles.cpp`, `CacheProgramas` | `7d8aed4` | Archivo por driver y versión | ADAPTAR / media | Revisar límites, validación y escritura atómica; compilar no prueba reutilización; GPL-3.0 | Pendiente |
| CAS/FSR y presentación | SDK tiene shaders Vulkan | `gles_presenter.cpp`, `_efectos.h` | SDK + fork `7d8aed4` | Nueva ruta GLES; Vulkan no debe perder efectos existentes | ADAPTAR / media | Conservar licencias AMD y regeneración de los shaders, medir costo | Pendiente |
| Media precisión | Sin `nfsmw_spirv_precision` | `nfsmw_spirv_precision.cpp/.h` | `7d8aed4` | Marca operaciones de color y protege rutas críticas | INVESTIGAR / media | No habilitar por defecto sin validar SPIR-V y escenas; GPL-3.0 | Pendiente |
| Panorámico, reflejos y culling | Optimizaciones propias en `app/src/` | `nfsmw_panoramico.cpp`, renderer | `4554a99`, `7d8aed4` | Algunas opciones existen, otras difieren | INVESTIGAR / media | No trasladar cambios visuales mezclados sin escena comparable; GPL-3.0 | Pendiente |
| ISO y VFS | Herramienta de extracción y dispositivo de disco SDK | `IsoXex`, `BuscadorIsos`, dispositivo de disco | Historial del fork por revisar | Android no ofrece lectura directa verificada | INVESTIGAR / media | SAF/seek, tamaños y permisos; no sustituir carpetas ni duplicar datos; GPL/SDK BSD | Pendiente |
| Partidas | Guardado runtime, sin gestor ZIP | `Partidas.java` | Historial del fork por revisar | Nueva exportación e importación | ADAPTAR / media | Respaldo previo y transacción; nunca importar datos fuera de partidas; GPL-3.0 | Pendiente |
| Multiedición | `tools/editions/`, comprobación PAL España | `tools/ediciones`, `apk_ediciones.py` | Base propia más adaptaciones del fork | Herramientas existentes no implican APK multiedición | INVESTIGAR / posterior | Aislar símbolos, hooks y cachés; no aceptar XEX desconocidos | Pendiente; PAL España se conserva |
| Cliente LAN | Sockets/XAM del SDK | `xam_net`, `xsocket`, `xlivebase_app`, `xam_user` | Historial de red del fork | Cambios funcionales del protocolo y sus estados | ADAPTAR / alta experimental | Desactivado por defecto, sin servicios oficiales ni cambios al modo individual; SDK BSD-3-Clause | Pendiente |
| Servidor y descubrimiento LAN | No existe | `ServidorEa`, `ServidorLanService`, `BuscadorServidores`, `ComprobacionGamertag` | Servidor Java cita `nfsmw-server-lan` `9ebeb11` | Nuevo servidor compatible local y descubrimiento | ADAPTAR / experimental | Revisar licencia del servidor separado, sockets/servicio y Android 8; no afirmar carreras de dos jugadores sin probar | Pendiente |
| Menús, identidad y controles del fork | Launcher y controles propios | `SetupActivity`, `TouchControllerView` | Proyectos distintos | No se necesita su identidad visual para integrar el motor | DESCARTAR / preservar | No cambiar `com.nfsmw.android`, iconos ni rutas; mantener créditos | Excluidos |
| PGO Switch | `pgo/`, GCC `.gcda` | Base compartida | nfsmw-nx | No son perfiles LLVM ARM64 Android | INVESTIGAR / posterior | No reutilizar perfiles incompatibles ni prometer FPS | Pendiente |

Los estados se actualizan con cada implementación y prueba. Las cifras de otros teléfonos publicadas por el fork
son informes de ese proyecto, no pruebas realizadas en nuestro APK.

## Estado en 0.8.0 (26 de octubre de 2026)

Integrado por fusión a tres bandas con base `e43bf99` del fork (sus parches equivalentes a los nuestros no se
duplican) y referencia `dd76acb`. 17 conflictos resueltos a mano: se conservan nuestras variantes de NFS Carbon,
los rectángulos y los ganchos de la cola con barreras; del fork entran los abanicos como listas y el iOS queda
inactivo en Android.

| Función | Estado |
|---|---|
| memcpy con solape del CRT | Integrado con `rex::ppc::GuestMemcpy`; 819.200 casos y 64 retiradas de controladores pasan en PC. Sin prueba de la cinemática en carrera. |
| Backend OpenGL ES (presentador, destinos, dibujos, efectos) | Integrado; opción «API gráfica» en el launcher y oferta automática cuando Vulkan no es compatible. Los efectos CAS/FSR quedan fuera porque nuestra build no usa FidelityFX. Sin partida probada sobre GLES en nuestro APK. |
| Biblioteca GLES en el móvil | `assets/shaders/lib/shaders.js` (modo `gles`) y `build.js` la generan desde los contenedores de `nfsmw_shaders.nfsp`. En PC da la misma biblioteca, byte a byte, que `tools/biblioteca_shaders_gles.mjs` (SHA-256 `aa509b41…`). Los 152 shaders convertidos compilan y las 89 parejas enlazan en el driver Adreno 830. |
| SPIRV-Cross | Solo el backend GLSL, vendorizado en `sdk/thirdparty/spirv-cross` (`2c32b6bf`, Apache-2.0). |
| LAN | Motor (`xam_net`, `xsocket`, `xam_user`, `xgi`, `xlivebase`, `user_gamertag`) y app (`ServidorEa`, `BuscadorServidores`, `ComprobacionGamertag`, `LanSettings`, `LanServerService` en el proceso `:lan`). Desactivada por defecto. Sin carrera entre dos móviles probada en nuestro APK. |
| Afinidad 2+N | Con menos de cuatro núcleos rápidos no se restringe el proceso; `cpu_capacity` como respaldo de cpufreq. |
| Vulkan (sin regresión) | S25 Ultra: arranque, menú 3D, carrusel principal y una partida rápida (aceleración) cargada y en marcha tras su introducción, con 0 diferencias en los guardianes nativos. |
