# Resultados de pruebas

## Entrega 0.8.1 — 27 de octubre de 2026

No hay dispositivo físico ni emulador disponible para esta entrega. No se ejecutó ADB ni se reutilizan las pruebas
del 26 de octubre como si comprobaran las funciones nuevas.

- Compilación Release de la base 0.8.0: correcta antes de editar.
- Compilación Release de 0.8.1 y lint vital de Android: correctos con NDK 28.2 y API 35.
- APK con paquete `com.nfsmw.android`, versión 0.8.1/code 15, minSdk 26 y targetSdk 35. Sus 11 bibliotecas son ARM64
  y sus segmentos de carga tienen alineación de al menos 16 KB. El certificado coincide con el de las instalaciones
  anteriores; no se cambió la clave. No se detectaron XEX, ISO ni NFSP en el APK.
- `SaveArchiveTest`: exportación/importación y restauración del respaldo; exclusión de configuración/caché/logs/drivers;
  conservación de ajustes y recursos; alias legítimo de la carpeta de usuario, ZIP truncado, rutas externas, tipos prohibidos, colisiones de nombres e integridad
  SHA-256 y archivo comprimido que supera el límite de 64 MiB al expandirse. Simula interrupciones antes de instalar,
  después de instalar sin commit y después del commit.
- `GraphicsPresetsTest`: conservación del respaldo al encadenar preajustes, captura de cambios manuales y exclusión de
  API/idioma. Los valores se validan además contra `GameOptions` antes de aplicarlos en Android.
- Tests Java ejecutados en el JBR del PC. Las transacciones se probaron en directorios temporales propios en Windows.

## Evidencia histórica separada

El 26 de octubre se compilaron 152 shaders GLSL ES y se enlazaron 89 parejas en un Adreno 830; se probaron también
819.200 casos de copias solapadas en ARM64. Eso valida esos componentes aislados. No valida una carrera completa
GLES, la LAN, la nueva interfaz de partidas o su recuperación sobre el filesystem de Android.

## Pendiente físico

Crear una partida y exportarla, importar y restaurar en Android, proveedor SAF remoto/no disponible, corte de proceso
durante importación y recuperación, restauración tras actualizar APK y conservación de controles. Para LAN hace
falta completar una carrera real entre dos dispositivos. No se midieron FPS, temperatura, batería ni latencia.
