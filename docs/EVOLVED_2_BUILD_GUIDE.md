# Compilar y comprobar Evolved 2

Se conservan ARM64, minSdk 26, targetSdk 35, NDK `28.2.13676358`, CMake 3.30.5 y C++23. Hace falta JDK 17+ y el SDK
de Android. Primero preparar dependencias y el código recompilado desde la copia propia, como explica `building.md`.
El código generado, XEX, NFSP y datos comerciales no se añaden a Git.

```powershell
python tools/fetch_thirdparty.py --cache-dir out/evolved2/dependency-cache
.\build_android.ps1
```

La salida sigue en `android/app/build/outputs/apk/release/app-release.apk`. No se actualizan las dependencias grandes
solo por modernizarlas. SPIRV-Cross está fijado al commit que figura en `sdk/thirdparty/spirv-cross/README.nfsmw`.

Las pruebas puras de esta entrega no requieren Android:

```powershell
javac -encoding UTF-8 -d out/save-tests android/app/src/main/java/com/nfsmw/android/SaveArchive.java android/app/src/main/java/com/nfsmw/android/GraphicsPresets.java tools/tests/SaveArchiveTest.java tools/tests/GraphicsPresetsTest.java
java -cp out/save-tests com.nfsmw.android.SaveArchiveTest
java -cp out/save-tests com.nfsmw.android.GraphicsPresetsTest
```

Usar el JBR/JDK 17+ del equipo para estos tests. Verificar la firma con `apksigner verify --print-certs`, el paquete,
ABI y SDK con `aapt dump badging`, y la alineación de los segmentos ELF para páginas de 16 KB. El firmado actual usa
la clave local de depuración: mantener el certificado es necesario para actualizar sin desinstalar. No sustituirlo
sin una estrategia explícita de continuidad/rotación compatible. Las claves privadas no se incluyen en Git; una clave
de producción debe custodiarse fuera del repositorio y suministrarse mediante configuración local o secretos del CI.

No hay publicación automática desde el build. Publicar exige la instrucción del propietario y notas que distingan
pruebas de código, pruebas físicas y limitaciones experimentales.
