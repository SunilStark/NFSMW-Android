#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Extracts the SPIR-V of every shader in the native shader library (nfsmw_shaders.nfsp), numbered as the renderer
numbers them (its position in the library, EntradaShader::numero), to measure them with Arm's Mali Offline
Compiler (malioc) against the tablet's Mali-G68.

    adb pull /sdcard/nsfmw-androidevolved/nfsmw_shaders.nfsp out/tablet/
    python tools/android/extraer_spirv.py out/tablet/nfsmw_shaders.nfsp out/tablet/spirv

Writes ps_NNN.spv and vs_NNN.spv. The library is made from the game: its output goes to out/, which git
ignores, and is never committed. The format is the one nfsmw_shader_library.cpp reads (little-endian):
"NFSSPV\\0\\0", u32 version 1, u32 count, u64 XXH3 of the rest; per shader u32 container bytes, u32 SPIR-V
words, u64 container hash, the container, the SPIR-V.
"""
import os
import struct
import sys


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    datos = open(sys.argv[1], 'rb').read()
    if datos[:8] != b'NFSSPV\0\0':
        print('no es una biblioteca de shaders (firma)')
        return 1
    version, cantidad = struct.unpack_from('<II', datos, 8)
    if version != 1:
        print('version no admitida:', version)
        return 1
    os.makedirs(sys.argv[2], exist_ok=True)
    p = 24
    pixel = vertices = 0
    for numero in range(cantidad):
        original, palabras = struct.unpack_from('<II', datos, p)
        p += 16
        contenedor = datos[p:p + original]
        p += original
        spirv = datos[p:p + palabras * 4]
        p += palabras * 4
        es_vertices = (struct.unpack_from('>I', contenedor, 0)[0] & 1) != 0
        nombre = ('vs_%03d.spv' if es_vertices else 'ps_%03d.spv') % numero
        with open(os.path.join(sys.argv[2], nombre), 'wb') as f:
            f.write(spirv)
        if es_vertices:
            vertices += 1
        else:
            pixel += 1
    print('%d shaders: %d pixel, %d vertex -> %s' % (cantidad, pixel, vertices, sys.argv[2]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
