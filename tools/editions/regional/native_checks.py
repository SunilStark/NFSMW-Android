# SPDX-License-Identifier: GPL-3.0-or-later
# Adapted from victorgbd/NFSMW-Recompiled-Mobile @ dd76acb032ccbd7b0494985242489cd9cd90a939.
"""Regional address and whole-function checks. No fetching or native tree replacement."""
import glob
import json
import re
import struct
from pathlib import Path
import emparejar
import imagen

class ediciones:
    CACHE = Path(__file__).resolve().parents[3] / 'out' / 'editions' / 'images'

def fallar(message):
    raise ValueError(message)

DIRECCION = re.compile(r"(?<![0-9A-Fa-f])82[0-9A-Fa-f]{6}(?![0-9A-Fa-f])")

PARTIDA = re.compile(r"sub_(82[0-9A-Fa-f]{0,6})\s*,\s*([0-9A-Fa-f]{1,6})(?![0-9A-Fa-f])")

BINARIOS = ("_spirv.h", "_ps.h", "_vs.h")

FUENTES = (".cpp", ".h", ".hpp", ".inl", ".c")

OTROS = ("CMakeLists.txt", "CMakePresets.json", "nfsmw.toml", "orden_funciones.ld", "overrides.toml",
         "huecos.toml", "generated/rexglue.cmake")

CONSTANTES = (0x82000000, 0x82CD0000)

FIN_IMAGEN = 0x82D20000

SEGURAS = ("exacta", "misma direccion", "constante", "referencias")

def _direcciones(texto):
    out = {int(x, 16) for x in DIRECCION.findall(texto)}
    for alta, baja in PARTIDA.findall(texto):
        if len(alta) + len(baja) == 8:
            out.add(int(alta + baja, 16))
    return out

COMENTARIO = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)

def _direcciones_en_codigo(texto):
    """Las direcciones fuera de los comentarios: las que el codigo usa de verdad."""
    return _direcciones(COMENTARIO.sub("", texto))

def tabla_de_edicion(arbol, xex_ref, xex_otra, salida):
    """
    Traduce todas las direcciones que crear_arbol.py, llamadas_directas.py y
    copia_literal.py necesitan, y las escribe con el formato de su
    tools/editions/emparejar.py. Devuelve las que usa el codigo de app/src
    (sin las de los comentarios, que tambien se traducen).
    """
    app = arbol / "app"
    todas, en_codigo = set(), set()
    for ruta in (app / "src").rglob("*"):
        if ruta.is_file() and ruta.name.endswith(FUENTES) and not ruta.name.endswith(BINARIOS):
            texto = ruta.read_text(encoding="utf-8")
            todas |= _direcciones(texto)
            en_codigo |= _direcciones_en_codigo(texto)
    for rel in OTROS:
        todas |= _direcciones((app / rel).read_text(encoding="utf-8"))
    generado = app / "generated" / "default"
    with open(generado / "codegen.partition.json", encoding="utf-8") as fh:
        todas |= {int(d, 16) for d in json.load(fh)["assignments"]}
    debil = re.compile(r"(?<![\w])sub_([0-9A-F]{8})\(ctx, base\);")
    for ruta in glob.glob(str(generado / "nfsmw_recomp.*.cpp")):
        with open(ruta, encoding="utf-8") as fh:
            todas |= {int(x, 16) for x in debil.findall(fh.read())}
    todas |= _direcciones((arbol / "tools" / "copia_literal.py").read_text(encoding="utf-8"))

    img_ref, _ = imagen.cargar(str(xex_ref), str(ediciones.CACHE))
    img_otra, _ = imagen.cargar(str(xex_otra), str(ediciones.CACHE))
    e = emparejar.Emparejador(img_ref, img_otra)
    filas, cuenta = [], {}
    for d in sorted(todas):
        # Sin la busqueda por contenido: "exacta" para crear_arbol.py, y puede
        # caer en otra funcion. Lo que no se ancla se queda en dudosa.
        seccion, destino, estado = e.traducir(d, por_contenido=False)
        if destino is None and (d in CONSTANTES or d >= FIN_IMAGEN):
            destino, estado = d, "constante (misma en las dos)"
        filas.append("%08X\t%s\t%s\t%s" % (d, seccion, "%08X" % destino if destino else "-", estado))
        clave = "segura" if estado.startswith(SEGURAS) else "dudosa"
        cuenta[clave] = cuenta.get(clave, 0) + 1
    salida.parent.mkdir(parents=True, exist_ok=True)
    salida.write_text("pal\tseccion\totra\testado\n" + "\n".join(filas) + "\n", encoding="utf-8")
    print(f"[ok] {len(todas)} direcciones: {cuenta.get('segura', 0)} seguras, {cuenta.get('dudosa', 0)} dudosas"
          f" (solo valen en el reparto de funciones) -> {salida}")
    return e, en_codigo

def comprobar_funciones(e, en_codigo):
    """Las funciones que el codigo nativo de su app toca, enteras: tienen que ser
    las mismas en las dos ediciones salvo direcciones (emparejar.py)."""
    malas, avisos, parejas = [], [], 0
    for d in sorted(en_codigo):
        if e.traducir(d)[0] != ".text":
            continue
        # Enteras, y cada direccion que calculan (lis + parte baja) con su
        # traduccion: asi se comprueba tambien la de .data donde se mueve.
        problemas = e.comparar_funcion(d)
        n, otros, constantes = e.comprobar_parejas(d)
        parejas += n
        problemas += otros
        avisos += constantes
        if problemas:
            malas.append("%08X: %s" % (d, "; ".join(problemas[:4])))
    if malas:
        fallar("Funciones que la app toca y que en esta edicion son distintas:\n        "
               + "\n        ".join(malas))
    print(f"[ok] las funciones que toca la app son las mismas en las dos ediciones ({parejas} direcciones"
          " calculadas, todas con su traduccion)")
    if avisos:
        # Su codigo lee sus constantes; la app no las copia.
        print(f"     aviso: {len(avisos)} constantes con otro valor en esta edicion, en funciones que la app"
              " envuelve o solo nombra:")
        for aviso in sorted(set(avisos))[:8]:
            print(f"       {aviso}")

HUELLAS_ESPECIALES = ("kHuellaBrightPass", "kHuellaCielo", "kHuellaComposicion")

def _biblioteca(ruta):
    """{huella: contenedor original} de una biblioteca NFSSPV (su nfsmw_shader_library.cpp)."""
    datos = ruta.read_bytes()
    if datos[:8] != b"NFSSPV\0\0":
        fallar(f"{ruta} no es una biblioteca de shaders.")
    _version, cantidad = struct.unpack_from("<II", datos, 8)
    pos, contenedores = 24, {}
    for _ in range(cantidad):
        original, palabras, huella = struct.unpack_from("<IIQ", datos, pos)
        pos += 16
        contenedores[huella] = datos[pos:pos + original]
        pos += original + palabras * 4
    return contenedores

def huellas_shaders(arbol, ficha, biblioteca, biblioteca_pal):
    """
    Las huellas de los shaders especiales que cambian en esa edicion, {PAL: suya}:
    en su biblioteca, el contenedor con el mismo microcodigo que el de la PAL
    (como su tools/editions/shaders_especiales.py, que lo saca de los
    contenedores del disco). Si ediciones.json ya las tiene ("nativo"), tienen
    que coincidir.
    """
    fuente = (arbol / "app" / "src" / "nfsmw_nativo_dibujos.cpp").read_text(encoding="utf-8")
    pal = _biblioteca(biblioteca_pal)
    suya = _biblioteca(biblioteca)
    cambian = {}
    for nombre in HUELLAS_ESPECIALES:
        m = re.search(rf"constexpr uint64_t {nombre} = 0x([0-9A-Fa-f]{{16}})ull;", fuente)
        if not m:
            fallar(f"No encuentro {nombre} en app/src/nfsmw_nativo_dibujos.cpp.")
        huella = int(m.group(1), 16)
        if huella not in pal:
            fallar(f"{nombre} ({huella:016X}) no esta en la biblioteca de la PAL Espana.")
        b = pal[huella]
        # La palabra 1 del contenedor (big-endian) dice donde empieza el microcodigo.
        inicio = int.from_bytes(b[4:8], "big")
        iguales = sorted(h for h, u in suya.items() if u[4:8] == b[4:8] and u[inicio:] == b[inicio:])
        if len(iguales) != 1:
            fallar(f"{nombre}: en la biblioteca de {ficha['nombre']} hay {len(iguales)} contenedores con su "
                   "microcodigo, y tiene que haber uno.\n"
                   "        Esa edicion trae otro shader: hay que mirarlo a mano (ver "
                   "tools/editions/shaders_especiales.py).")
        if iguales[0] != huella:
            cambian["%016X" % huella] = "%016X" % iguales[0]
    anotadas = (ficha.get("nativo") or {}).get("huellas_shaders")
    if anotadas is not None and anotadas != cambian:
        fallar(f"Las huellas de shaders de {ficha['nombre']} no son las de tools/ediciones/ediciones.json: "
               f"salen {cambian or 'las de la PAL'}, y alli dice {anotadas or 'las de la PAL'}.")
    print("[ok] huellas de los shaders especiales: "
          + (", ".join(f"{v} -> {n}" for v, n in cambian.items()) if cambian else "las mismas que en la PAL"))
    return cambian
