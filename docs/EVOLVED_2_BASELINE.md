# Baseline de Evolved 2

Inicio: 26 de octubre de 2026. Commit propio `026f7d07b57dcb1969349b220b945c8e24ca6a0a`; última versión 0.6.0,
`versionCode=13`. El tag 0.6.0 apunta a `507a63f7d21c9b8e2d6f15d3ed0e12b9b2c38712`; el commit posterior añade
el manifiesto de recomp.fyi. La copia extraída coincidía con Git tras recuperar el historial. Rama de trabajo:
`evolved-2/audit-20261026`. Se conserva la instantánea original en Git y el APK instalado en `out/evolved2/baseline/`.

## Entorno y referencias

- Windows; SDK Android API 35, NDK `28.2.13676358`, CMake 3.30.5, C++23, Gradle y JBR de Android Studio.
- Compilador del juego: Clang 19.0.1 del NDK, ARM64/ARMv8.0. Compilador disponible para herramientas: Clang 22.1.8,
  Visual Studio 18 Community. No se actualizó el NDK ni los requisitos de Android.
- SDK fuente fijado a ReXGlue `c94f5ebdcb3c9d1a460ca48e04f9758448f8d518`; dependencias obtenidas de sus gitlinks.
- S25 Ultra conectado, modelo SM-S938B; APK instalado 0.6.0, 31.204.437 bytes. Se copió al directorio privado de
  pruebas del PC, no a Git. Certificado SHA-256 `53f31cba4e09e3d22206639f9705f78bc8aa887f649e465d3590fe70db12b4cf`.
  La clave local de depuración tiene el mismo certificado: es posible probar una actualización sin desinstalar.
- `default.xex` y biblioteca NFSP obtenidos de los datos que el usuario tiene en su teléfono. Se mantienen ignorados.

## Primera compilación sin cambios de motor

`build_android.ps1` se ejecutó antes de modificar el motor. Falló porque falta `app/generated/rexglue.cmake` y por
ello tampoco existe `rexglue_setup_target`. Es una preparación ausente de esta copia, no una regresión del renderer.
El log queda en `out/evolved2/baseline/build-initial.log`. Advertencia preexistente: metadatos SDK XML v4 frente
al lector de Gradle que reconoce hasta v3.

La preparación de dependencias encontró además un fallo reproducible en `fetch_thirdparty.py`: intentaba resolver
como cabeceras enlaces de directorio de MoltenVK y enlaces a sus dependencias Apple opcionales, descargadas aparte.
Se distinguen esos enlaces de las cabeceras reales y se añade caché de checkout y submódulos superficiales fijados para no repetir
la descarga completa. El código generado debe reconstruirse desde el XEX propio con las herramientas de este SDK.

**Estado de build base:** reconstrucción en curso; no se autoriza interpretar el APK preexistente como build nuevo.
Antes de una integración grande del motor se debe cerrar esta referencia de compilación.

## Prueba física inicial

Se abrió 0.6.0 desde su launcher con la copia existente. Archivos y shaders aparecen disponibles y el menú principal
se dibuja (BMW, perfil y controles táctiles). Captura local `out/evolved2/baseline/screen.png`. Una muestra de memoria
en esa sesión dio PSS total 1.521.151 KB y RSS 1.868.064 KB; es una muestra, no un pico ni una medida de carrera.
No se ha medido todavía FPS de carrera, percentiles, consumo, temperatura ni audio por captura. No hay comparación
antes/después que permita anunciar una ganancia de rendimiento.

No se borraron partidas, configuraciones, drivers ni bibliotecas del teléfono. El juego es el instalado; aún no se
instaló una versión modificada. Los escenarios de conducción, guardado, importación, reconexión de mandos y LAN se
registrarán por separado cuando realmente se ejecuten.
