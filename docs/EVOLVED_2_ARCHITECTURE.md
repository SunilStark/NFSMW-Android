# Arquitectura de Evolved 2

El paquete Android sigue siendo `com.nfsmw.android`. `MainActivity` comprueba los archivos, prepara shaders y opciones
y abre `GameActivity`/SDL3. JNI conecta controles y opciones con el runtime ReXGlue; el código PowerPC recompilado
se genera desde el XEX PAL España y queda fuera de Git. La entrada y las funciones registradas conservan sus hooks.

El renderer nativo interpreta el estado gráfico y las órdenes del juego. `DestinosNativos` separa las copias y los
destinos de la API: Vulkan conserva sus imágenes y descriptores; GLES tiene contextos EGL compartidos, texturas,
FBOs y sincronización propios. La biblioteca `nfsmw_shaders.nfsp` continúa para Vulkan y `nfsmw_shaders_gles.nfsp`
usa recursos por registro traducidos a GLSL ES. Xenos continúa como ruta experimental independiente.

El SDK gestiona perfiles, contenedores de contenido y logros en `files/nfsmw/user`; los respaldos legibles del juego
están en `saves/`. La gestión nueva no cambia esa raíz. Los ZIPs privados van a `files/save-backups`, separados de
los datos activos. El directorio temporal hermano `.nfsmw-save-transaction` guarda las dos versiones durante un
reemplazo. Antes del commit se recupera la original; después se conserva la instalada y se limpia la anterior.

Los preajustes viven en las preferencias existentes de gráficos. Se guardan junto con los seis valores aplicados
y el respaldo personal mediante un solo `SharedPreferences.Editor.commit`. Ningún preajuste cambia el backend ni
modifica parámetros globales del sistema. LAN sigue desactivada por defecto y su servidor usa un servicio separado.
