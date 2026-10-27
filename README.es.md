<div align="center">

# NFSMW Android Evolved

**Need for Speed: Most Wanted (2005) para Android ARM64**

APK nativo · Vulkan 1.1+ · controles táctiles · generación local de shaders

[Descargar la última versión](https://github.com/codepdbh/nfsmw-android/releases/latest) · [English](README.md)

</div>

**Estado del proyecto:** Este port recibe pocas o casi ninguna actualización debido a lo que su autor denuncia como robo de código por parte de otros «devs».

Este proyecto adapta a Android el trabajo de recompilación de [nfsmw-nx](https://github.com/StevensND/nfsmw-nx) y el SDK [ReXGlue](https://github.com/rexglue/rexglue-sdk). La aplicación usa código C++ recompilado, SDL3 y el renderizador nativo sobre Vulkan. No emula una Xbox 360 ni incluye los archivos del juego.

La versión 0.8.2 añade la [infraestructura de compilación por edición](docs/ANDROID_EDITIONS.md), selección estricta del módulo por XEX y cachés de shaders vinculadas al ejecutable. El APK publicado sigue incluyendo **solo PAL España**; las otras regiones necesitan sus ejecutables, preparación y pruebas. Esta versión no tiene prueba en teléfono ni emulador.

## Requisitos

- Android 8 o posterior y procesador ARM64 (ARMv8.0 o posterior).
- GPU con **Vulkan 1.1 o posterior** y las funciones necesarias de *descriptor indexing*, o la ruta **OpenGL ES 3.2 experimental** incorporada en 0.8.0. Vulkan expone esas funciones mediante Vulkan 1.2 o `VK_EXT_descriptor_indexing` en 1.1; una versión reciente no las garantiza. No hacen falta `shaderInt64` ni *buffer device address* en la ruta nativa. La compatibilidad de GLES requiere pruebas por dispositivo.
- Una copia propia de **Need for Speed: Most Wanted (2005), Xbox 360, edición PAL España**, extraída y con `default.xex`, `NFS/` y `Movies/`. Este APK se compila para el ejecutable de esa edición; el launcher bloquea ejecutables sin un módulo compilado incluido. Los archivos de la versión de PC, PS2 o una ISO sin extraer no sirven.
- Espacio en la memoria interna para el APK, la carpeta del juego y los archivos generados.

Probado en un **Samsung Galaxy S25 Ultra** (Adreno 830), un **Galaxy A55** (Xclipse 530) y varios Mali (G57, G615, G68, G720) según los informes de los testers.

Las GPU cuyos drivers no ofrecen *descriptor indexing* no pueden usar la ruta nativa Vulkan. Desde 0.8.0 pueden intentar **API gráfica → OpenGL ES**, con su biblioteca de shaders separada. Esto no confirma compatibilidad con todas las Mali, Adreno o PowerVR.

## Instalación y primer inicio

1. Descarga el APK de la última versión en [Releases](https://github.com/codepdbh/nfsmw-android/releases/latest) y ábrelo en el teléfono. Si Android lo solicita, permite **Instalar aplicaciones desconocidas** al navegador o al gestor de archivos.
2. Instala el APK y abre **Need for Speed Most Wanted**. Autoriza el acceso a archivos que solicita la app; en Android 11 o posterior aparece el ajuste de **acceso a todos los archivos**. Después vuelve al launcher.
3. Copia `default.xex`, `NFS/` y `Movies/` directamente dentro de `Memoria interna/nsfmw-androidevolved/`. También puedes pulsar **Elegir carpeta del juego** y seleccionar la carpeta extraída; la app la copia a ese destino.
4. Comprueba que el launcher marque los archivos como disponibles y pulsa **Jugar**. La primera vez genera `nfsmw_shaders.nfsp` a partir de tu copia y muestra el avance: desde unos segundos hasta un par de minutos en teléfonos modestos.
5. El juego se abre en horizontal. Para empezar, prueba 1280×720 y 60 FPS.

La carpeta debe quedar así:

```text
Memoria interna/nsfmw-androidevolved/
├── default.xex
├── NFS/
├── Movies/
├── nfsmw_shaders.nfsp           (generado por la app)
└── nfsmw_shaders.nfsp.version   (generado por la app)
```

No descargues ni compartas archivos del juego. La biblioteca de shaders se genera en el dispositivo a partir de tu copia.

### Actualizar desde una versión anterior

Instala el nuevo APK encima del anterior, **sin desinstalar ni borrar los datos de la app**. Las partidas y los ajustes se conservan. Si la versión nueva cambia los shaders, la app los vuelve a generar sola al pulsar **Jugar**; con la v0.3.7 ocurre una vez.

### Si no aparece Jugar

- Revisa el permiso de archivos y vuelve a abrir la app.
- Comprueba que `default.xex` esté directamente en `nsfmw-androidevolved/`, junto a `NFS/` y `Movies/`, sin una carpeta adicional en medio.
- Si la importación falla, revisa el espacio libre y selecciona la carpeta que contiene los tres elementos.

## Launcher, ajustes y controles

- **Gráficos:** resolución interna, límite de FPS, antialiasing, sombras, reflejos, resplandor del cielo, filtro de imagen y formato de pantalla.
- **Idioma de los textos** (v0.3.7): el disco PAL trae los textos de diez idiomas (español, inglés, francés, alemán, italiano, neerlandés, sueco, danés, finés y polaco). Elige uno en el launcher. Las voces y los vídeos siguen en el idioma del disco.
- **Volumen del juego**: de Normal a Máximo.
- **Controles táctiles:** dirección analógica o por inclinación, cruceta, A/B/X/Y, LB/RB, BACK, START, freno y acelerador. Desde el editor (engranaje) puedes moverlos, cambiar su tamaño, ocultarlos y ajustar su opacidad.
- **Mandos Bluetooth y USB**: funcionan como jugador 1 y ocultan los controles táctiles.
- **Teclado** (launcher **TECLADO → Jugar con teclado**): un teclado Bluetooth o USB es el mando del jugador 1 (por defecto A/D giran, E acelera, Q frena) y oculta los controles táctiles hasta el siguiente toque. **Teclas** cambia la tecla de cada botón. Android ya no recrea la pantalla del juego al conectar un mando o un teclado en plena carrera.
- **Enviar crash o log**: genera un ZIP con el informe de tu dispositivo para compartirlo por correo o en un issue de GitHub. Adjunta el ZIP al formulario. Consulta [la guía para testers](docs/android-testers.md).

## Novedades de la v0.8.1

- **Partidas guardadas:** exportar ZIP, importar, crear un respaldo privado y restaurar el último. Verifica SHA-256 y CRC, respalda antes de importar y recupera operaciones interrumpidas. Conserva ajustes, shaders y cachés. Acepta únicamente ZIPs creados por esta función; exporta una copia externa antes de desinstalar.
- **Perfil gráfico:** Batería, Equilibrado, Rendimiento y Calidad, con **Mis ajustes guardados** para volver a los valores personales. Encadenar perfiles no pisa ese respaldo. Son conjuntos de opciones, sin mejoras de FPS o consumo medidas.
- Basada en tus cambios de 0.8.0: **GLES, LAN experimental y opciones de compatibilidad** permanecen integrados. Esta entrega pasa compilación y pruebas de archivos/preajustes; no hay nueva prueba física ni en emulador. Consulta [implementación](docs/EVOLVED_2_IMPLEMENTATION.md) y [resultados](docs/EVOLVED_2_TEST_RESULTS.md).

## Novedades de la v0.5.4

- **Drivers Vulkan externos:** importa un ZIP Android ARM64 desde **Driver Vulkan → Importar ZIP**, selecciónalo y usa **Probar driver**. Puedes volver al del sistema o eliminar el paquete. Los drivers externos requieren Android 9+. Turnip es para Adreno; PanVK para Mali, con restricciones de dispositivo, Android y kernel. No se incluyen drivers en el APK.
- **Mali y otras GPU con cuatro conjuntos:** se combinan las texturas 3D y los cubos cuando la GPU permite solo cuatro conjuntos de descriptores. Se adaptan Vulkan y los shaders; las GPU con más conjuntos conservan su ruta actual.
- **Estabilidad ARM:** las órdenes se publican completas antes de ejecutarlas desde otro hilo, corrigiendo una causa del cierre al entrar a carrera con una dirección de función inválida.
- **Xclipse:** conversión conservadora de BC4/BC5 en CPU con el driver propietario de Samsung.
- **Actualizaciones opcionales:** el launcher consulta las releases oficiales de GitHub en segundo plano y ofrece **Actualizar** o **Más tarde**. **Buscar actualizaciones** fuerza una consulta manual. Puedes jugar sin actualizar ni tener conexión.

Los arreglos de compatibilidad conservan nuestro launcher. Consulta el [diagnóstico y las instrucciones de drivers](docs/compatibilidad-android-0.5.4.md). Se confirmaron la carrera y la prueba del driver del sistema en el S25 Ultra; todavía necesitamos confirmar cada combinación de Snapdragon 888/8 Gen 1, Adreno 720, Mali y Xclipse. La falta de funciones de descriptor indexing sigue siendo una limitación.

## Novedades de la v0.3.8

- Corregida la pantalla negra al volver al juego después de cambiar a otra app.

## Novedades de la v0.3.7

- **Vulkan 1.1:** el renderizador nativo funciona en controladores 1.1 con `VK_EXT_descriptor_indexing`. Los shaders (SPIR-V 1.5) se convierten a SPIR-V 1.3 al cargarse; los 152 superan el validador oficial para Vulkan 1.1.
- **Sin `shaderInt64` ni buffer device address:** los shaders ya no leen constantes mediante punteros de 64 bits. Esto abre el juego a muchos Adreno 7xx y Mali-G57/G68 que se quedaban en *«el dispositivo Vulkan no tiene shaderInt64»*. La biblioteca de shaders se regenera una vez (PAL España: SHA-256 `b84602ca…`).
- **Idioma de los textos** seleccionable en el launcher.
- **Aviso de edición:** si tu `default.xex` no es PAL España, el launcher lo explica en lugar de cerrarse con *«No function registered at 8262E768»*.
- Los informes incluyen la edición del juego, `shaderInt64`, el tipo de *descriptor indexing* y la lista de extensiones Vulkan.

Comprobado en el Galaxy S25 Ultra, también simulando un controlador Vulkan 1.1 sin punteros de 64 bits. Los detalles están en la [guía para testers](docs/android-testers.md).

## Versiones anteriores

- **v0.3.6:** conversión en CPU de las texturas BC1–BC5 que el controlador no admite; protección para Mali en **Estabilidad gráfica**.
- **v0.3.5:** modo **Compatibilidad · experimental** (backend Xenos), corrección de memoria para kernels antiguos e informes desde el launcher.
- **v0.3.4:** corrección de bloques y reflejos incorrectos en Xclipse ([diagnóstico](docs/android-xclipse-diagnostic.md)).
- **v0.3.3:** audio de intros y cinemáticas corregido ([Audio en Android](docs/android-audio.md)).

## Compilar

Requisitos: Android SDK, NDK `28.2.13676358`, JDK 17 o posterior y PowerShell.

```powershell
.\build_android.ps1
```

El APK Release se genera en `android/app/build/outputs/apk/release/app-release.apk`, optimizado y para `arm64-v8a`.

## Proyecto y licencias

- `android/`: launcher, integración SDL y build de Android.
- `app/`, `sdk/`: aplicación recompilada y ReXGlue.
- `docs/`: notas del port y compilación.

Los archivos del juego y el código generado desde `default.xex` se mantienen fuera de Git. Consulta [`LICENSE`](LICENSE) y [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Proyecto de aficionados, sin afiliación con Electronic Arts; «Need for Speed» es una marca de Electronic Arts Inc.
