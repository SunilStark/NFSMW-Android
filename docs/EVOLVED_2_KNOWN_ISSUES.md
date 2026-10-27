# Límites conocidos de Evolved 2

- 0.8.1 no tiene validación física nueva: compilar y pasar pruebas de archivos no garantiza todos los escenarios Android.
- LAN y GLES conservan el carácter experimental de 0.8.0. No se garantiza compatibilidad de una familia entera de GPU.
- El selector de API sigue siendo manual. La ruta Vulkan necesita sus funciones de descriptor indexing; tener una
  versión alta de Vulkan no basta. Elegir un ZIP de driver tampoco garantiza soporte de GPU, kernel o Android.
- Las partidas solo se importan desde ZIPs creados por esta nueva función; el marcador del fork no equivale a nuestro
  manifiesto de integridad. Un archivo vacío no sustituye partidas existentes. No se importan configuraciones ni shaders.
- Los respaldos privados no sobreviven a desinstalar o borrar datos de la app. Exportar un ZIP a un documento externo
  permite conservarlo. No se eliminan respaldos antiguos automáticamente.
- La importación necesita espacio para los datos nuevos y el ZIP previo. Si no se pueden usar enlaces duros para
  conservar recursos, se copian; con cachés grandes esto requiere más espacio y puede hacer fallar la preparación.
  Si se agota el espacio, la transacción no debe cambiar los originales; si se interrumpe después de un rename,
  la recuperación se ejecuta antes de jugar. Una recuperación fallida bloquea el inicio y pide conservar los datos.
- Los perfiles son conjuntos de opciones, no ajustes adaptativos ni mejoras de rendimiento medidas. No se han
  comparado FPS o calidad visual entre ellos. No se modifican frecuencias de CPU/GPU ni el tick de simulación.
- ISO/SAF, multiedición y LAN completa requieren sus propias comprobaciones; solo se mantiene garantizada la
  selección de XEX conocida por el launcher, no una supuesta compatibilidad con ejecutables desconocidos.
