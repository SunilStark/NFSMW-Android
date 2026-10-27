# Implementación de Evolved 2

Actualización: 27 de octubre de 2026. Base de esta entrega: `0119f7e` (0.8.0), que el usuario incorporó después
de la auditoría inicial. Se conserva ese trabajo; no se vuelve a importar el motor ni el launcher del fork.

## Ya integrado en 0.8.0

El árbol contiene el backend EGL/GLES y sus dibujos/destinos, conversión por SPIRV-Cross, biblioteca de shaders
separada, caché de programas, opciones de precisión/BC1, panorámico y servidor/cliente/descubrimiento LAN. Los
archivos proceden de la comparación con `victorgbd/NFSMW-Recompiled-Mobile` en `dd76acb`. El hook del CRT usa
`GuestMemcpy`. Esto describe integración de código: no demuestra que todos los escenarios funcionen en teléfonos.

## Incorporado en 0.8.1

- `SaveArchive.java`: ZIP limitado a los directorios de perfiles Xbox (`16` dígitos hexadecimales), `saves/` y
  `achievements/`. Rechaza rutas inválidas, duplicados, enlaces y tipos conocidos de archivos del juego. No exporta
  ajustes, logs, shaders, cachés ni drivers. Formato propio con marcador y SHA-256 por archivo, además del CRC ZIP.
- La importación se extrae primero a un directorio temporal, se valida y se respalda el contenido anterior. El
  reemplazo del directorio de usuario usa renombrados atómicos en el mismo filesystem y un marcador de commit;
  una interrupción se recupera antes de permitir el inicio. Los recursos ajenos a las partidas se conservan mediante
  enlaces duros o copia si no están disponibles. El ZIP de respaldo se verifica antes de modificar los originales.
- `LauncherSaves.java`: selector de documentos Android para exportar/importar, respaldos privados con fecha y
  restauración del último. Opera en segundo plano. Solo acepta el formato de esta función, no promete importar
  ZIPs de otros ports. Exportar permite conservar una copia fuera de la app; desinstalar elimina los respaldos privados.
- `GraphicsPresets.java` y `MainActivity`: Batería, Equilibrado, Rendimiento y Calidad. Modifican únicamente resolución,
  FPS, antialiasing, sombras y reflejos. Una transacción de preferencias guarda tanto los valores nuevos como el
  respaldo personal. Aplicar varios perfiles seguidos no sobrescribe «Mis ajustes guardados»; un cambio manual
  pasa a ser el siguiente respaldo. No cambian API, driver, idioma, audio ni controles.
- `GameActivity`: indica si hay una sesión activa para evitar importar o exportar mientras el motor escribe partidas.
  Las operaciones de archivo se serializan también entre varias instancias del launcher en el mismo proceso.

El flujo de partidas se inspira en `Partidas.java` del fork, pero se implementó de nuevo con límites, hashes y
recuperación del directorio completo. Se conserva `com.nfsmw.android`, sus rutas y la interfaz del proyecto.

## Pendiente

La detección automática Vulkan/GLES con recuperación de fallos, lectura ISO desde SAF, APK multiedición, ampliación
del editor táctil y perfiles PGO Android requieren trabajo adicional. No se muestran controles vacíos para esas
funciones. Una carrera LAN entre dos dispositivos y el comportamiento gráfico real de todos los modelos siguen
sin verificarse. El inventario inicial de `COMPARATIVA_CODEPDBH_VICTORGBD.md` es una instantánea anterior a 0.8.0.
