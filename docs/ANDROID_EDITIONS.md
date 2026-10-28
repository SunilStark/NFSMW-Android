# Compilaciones Android por edición

La infraestructura de 0.8.2 prepara árboles separados por ejecutable y permite empaquetar sus bibliotecas
nativas en un APK con el mismo SDK. El APK publicado **solo contiene PAL España**: es el único XEX disponible
localmente. El catálogo de hashes no es una lista de ediciones compiladas ni una certificación de compatibilidad.
No se han probado USA, Japón ni otras regiones en Android con este cambio.

Se adaptaron el emparejador de direcciones, las comprobaciones de funciones completas y el diseño de
compilación/empaquetado de
[victorgbd/NFSMW-Recompiled-Mobile](https://github.com/victorgbd/NFSMW-Recompiled-Mobile/tree/dd76acb032ccbd7b0494985242489cd9cd90a939),
GPL-3.0. El código regional está en `tools/editions/regional/`; no sustituye nuestro launcher ni nuestro SDK.

## Preparar PAL

Necesitas Python 3.11 o posterior. Primero completa la [preparación normal del proyecto](building-android.md): dependencias, generador host,
`assets/game_root/default.xex`, fuentes generadas y copias literales. Después, desde la raíz:

```powershell
python tools/android/editions.py prepare --xex assets/game_root/default.xex
python tools/android/editions.py build --editions pal_es
```

La preparación de PAL verifica la identidad y la presencia del código generado; no reemplaza `app/` ni vuelve a
traducir un ejecutable distinto sobre ese árbol. La salida es `out/editions/pal_es/edition.json` y el APK queda en
`android/app/build/outputs/apk/release/app-release.apk`.

## Añadir otra edición propia

Necesitas su `default.xex` exacto, PAL España como referencia, el generador host construido desde nuestro SDK y
las bibliotecas NFSP privadas de ambas ediciones. Las NFSP permiten verificar los contenedores originales de los
shaders especiales; no deben subirse al repositorio ni incorporarse al APK.

Ejemplo de comandos para cuando se disponga de la edición USA (no ejecutados con un XEX USA en esta entrega):

```powershell
python tools/android/editions.py prepare --xex D:/mi-copia-USA/default.xex --library D:/privado/usa.nfsp --pal-library D:/privado/pal.nfsp --rexglue sdk/out/win-amd64/rexglue.exe
python tools/android/editions.py build --editions pal_es usa
```

El preparador identifica el XEX por SHA-256, verifica que exista una receta de shaders, compara las funciones
tocadas por los hooks y sus direcciones calculadas, verifica los shaders especiales y crea `app_usa/` sin renombrar
`app/`. Ejecuta codegen, deshace/reaplica llamadas directas y genera las copias literales. Una dirección sin
traducción segura o una función distinta detiene la preparación. El descriptor solo se escribe al terminar.
Las ediciones sin receta de shaders (actualmente coreana y china tradicional en el catálogo heredado) se rechazan.

El empaquetador compila cada árbol, comprueba exportaciones del SDK y dependencias compartidas, valida ELF ARM64
y conserva `libmain.so` para PAL y `libmain_usa.so` para USA, con nombres SONAME distintos. Cada staging usa una
carpeta nueva en `out/editions/package-*`. Gradle verifica los hashes y la presencia de los módulos antes de
generar el registro del APK; no se anuncian módulos ausentes. No copia los shaders ni archivos del disco al APK.

Después de cambiar las fuentes base o las herramientas de adaptación, vuelve a preparar las ediciones. El
empaquetador rechaza descriptores antiguos. Para una compilación individual ya preparada también existe:

```powershell
cd android
./gradlew.bat :app:assembleRelease -Pnfsmw.edition=usa
```

Usa el empaquetador para publicaciones con varias ediciones: además de Gradle, comprueba coherencia con la base
actual. Ni el código generado, ni los XEX, ni las NFSP ni los árboles `app_*/` se versionan.

## Comportamiento en el teléfono

El launcher calcula el SHA-256 de `default.xex` y busca exclusivamente en el registro compilado del APK.
Un ejecutable desconocido, ilegible o sin módulo incluido se bloquea; se quitó «Jugar igualmente».
La actividad vuelve a comprobarlo antes de cargar código nativo. Nunca selecciona PAL como sustituto de otra
región. Cambiar de biblioteca requiere cerrar completamente y volver a abrir la app para evitar dos ejecutables
nativos en el mismo proceso. Se conservan el paquete, los controles, las partidas y las rutas del juego.

Los shaders Vulkan y GLES se siguen generando en el teléfono. La marca de versión queda vinculada al XEX para
las nuevas ediciones; la biblioteca PAL anterior sigue siendo válida. La caché nativa de cada nueva edición usa
un subdirectorio propio. Idioma y país parten de la edición y se mantienen las opciones de texto del launcher.

## Verificación y límites

`tools/tests/CompiledEditionsTest.java` comprueba la selección exacta, la ausencia de fallback y el rechazo de
registros duplicados o inválidos. `python -m unittest discover -s tools/tests -p test_android_editions.py -v`
comprueba ejecutables desconocidos, arquitectura/tipo ELF, coherencia PAL y argumentos de empaquetado.

Se comprobó además el emparejador sobre PAL contra sí misma: 56.865 direcciones seguras, ninguna dudosa y
1.005 direcciones calculadas verificadas en las funciones usadas por los hooks. Es una comprobación del proceso
con la referencia disponible; no valida la traducción entre dos regiones distintas.

Sin dispositivo ni emulador disponible, quedan pendientes el arranque/carrera de 0.8.2 y las pruebas reales de
selección entre dos módulos. Cuando se obtenga otro XEX hay que preparar, compilar y probar una carrera, audio,
vídeos, idioma, shaders y partidas antes de anunciar soporte para esa edición.
