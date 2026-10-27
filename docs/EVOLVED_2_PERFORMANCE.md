# Rendimiento y medición

No hay nuevas cifras de rendimiento en 0.8.1: no se dispone de dispositivo ni emulador. Los preajustes Batería,
Equilibrado, Rendimiento y Calidad cambian opciones existentes; sus nombres no constituyen mediciones de consumo,
temperatura o FPS. Los ajustes personales se pueden recuperar y ninguna selección es obligatoria.

Para comparar, usar la misma partida, coche, escena, ruta, resolución, API y driver; separar calentamiento y creación
de shaders de la sesión estable. Registrar tiempos por fotograma, p95/p99, pausas, uso de CPU/GPU y memoria. Repetir
la escena con el mismo estado térmico y límite de FPS. Una mejora del menú no prueba una mejora en persecuciones.

Los perfiles de `pgo/` son del flujo GCC/Switch y no se reutilizan como perfiles LLVM Android. Sigue pendiente el flujo
instrumentado ARM64, captura de perfiles reales, fusión y comparación del binario optimizado. No se incluye una build
instrumentada en el APK de esta entrega ni se fuerzan frecuencias o políticas térmicas del teléfono.
