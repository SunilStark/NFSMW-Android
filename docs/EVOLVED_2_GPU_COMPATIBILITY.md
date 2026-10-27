# Estado de compatibilidad GPU

La integración 0.8.0 conserva Vulkan y añade el backend GLES del fork. Es una ampliación de rutas de código, no una
lista de teléfonos confirmados. Vulkan mantiene cuatro conjuntos, UBO sin Int64/BDA y conversiones BC cuando proceden.
GLES traduce una biblioteca distinta con SPIRV-Cross y tiene sus propios recursos y límites de driver.

Adreno 5xx/6xx/7xx/8xx, Mali-G52/G57/G68/G7xx/G6xx, Xclipse y PowerVR siguen siendo objetivos de prueba. Hay que registrar
por modelo: versión de API, extensiones, unidades de textura, UBOs, formatos, límite de descriptores, driver y errores.
Una prueba del sistema en Adreno no prueba Mali ni Xclipse. Turnip y PanVK no son intercambiables.

Esta entrega 0.8.1 agrega administración de partidas y perfiles gráficos. No modifica los requisitos de GPU ni añade
compatibilidad medida con un nuevo procesador. El perfil Batería o Rendimiento no soluciona capacidades ausentes.
