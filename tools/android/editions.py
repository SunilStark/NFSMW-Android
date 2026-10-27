#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare own XEX editions and package their separately compiled Android modules.

Build/staging design adapted from victorgbd/NFSMW-Recompiled-Mobile
tools/android/{preparar_nativo,apk_ediciones}.py at dd76acb032ccbd7b0494985242489cd9cd90a939.
Unlike that packager, no NFSP libraries or game assets are placed in the APK.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "out" / "editions"
APP = ROOT / "android" / "app"
PAL = "pal_es"
CATALOG = json.loads((ROOT / "tools/editions/regional/ediciones.json").read_text(encoding="utf-8"))["ediciones"]


def sha(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def run(*args, cwd=ROOT, env=None):
    subprocess.run([str(x) for x in args], cwd=cwd, env=env, check=True)


def identify(xex):
    digest = sha(xex)
    for ident, edition in CATALOG.items():
        if digest in edition.get("sha256", []):
            return ident, edition, digest
    raise ValueError("Unknown XEX SHA-256; inspect its addresses before adding it to the catalog: " + digest)


def source_hash():
    """Invalidate prepared regional hooks when the base app or adaptation tools change."""
    digest = hashlib.sha256()
    paths = list((ROOT / "app").rglob("*")) + list((ROOT / "tools/editions").rglob("*"))
    for p in sorted(paths):
        rel = p.relative_to(ROOT)
        if not p.is_file() or any(x in rel.parts for x in ("generated", "copias_literales", "__pycache__")):
            continue
        if p.suffix not in (".cpp", ".h", ".hpp", ".inl", ".c", ".toml", ".py", ".json", ".tsv", ".ld", ".txt"):
            continue
        digest.update(rel.as_posix().encode())
        digest.update(p.read_bytes())
    return digest.hexdigest()


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temp.replace(path)


def prepare(args):
    xex = Path(args.xex).resolve()
    ident, data, digest = identify(xex)
    manifest = json.loads((APP / "src/main/assets/shaders/release/manifest.json").read_text(encoding="utf-8"))
    if digest not in {entry["xex_sha256"] for entry in manifest["builds"]}:
        raise ValueError("This edition has no verified on-device shader recipe yet: " + ident)
    output = OUT / ident
    descriptor = output / "edition.json"
    # A failed preparation must never leave a previously successful descriptor usable.
    descriptor.unlink(missing_ok=True)
    destination = ROOT / "assets" / ("game_root" if ident == PAL else "game_root_" + ident)
    if ident == PAL:
        current = destination / "default.xex"
        if not current.is_file() or sha(current) != digest:
            raise ValueError("Prepare PAL with the normal project bootstrap first; this tool does not replace app/.")
        app = ROOT / "app"
    else:
        if not args.library or not args.pal_library:
            raise ValueError("Other editions require --library and --pal-library (private NFSP shader containers).")
        if not args.rexglue or not Path(args.rexglue).is_file():
            raise ValueError("--rexglue must point to the host generator built from this project's SDK")
        reference = ROOT / "assets/game_root/default.xex"
        if identify(reference)[0] != PAL:
            raise ValueError("PAL reference required")
        sys.path.insert(0, str(ROOT / "tools/editions/regional"))
        import native_checks
        destination.mkdir(parents=True, exist_ok=True)
        own_xex = destination / "default.xex"
        if own_xex != xex:
            shutil.copyfile(xex, own_xex)
        table = output / "table.tsv"
        matcher, addresses = native_checks.tabla_de_edicion(ROOT, reference, own_xex, table)
        native_checks.comprobar_funciones(matcher, addresses)
        fingerprints = native_checks.huellas_shaders(ROOT, data, Path(args.library), Path(args.pal_library))
        app = ROOT / ("app_" + ident)
        corrected = ROOT / "tools/editions" / ident / "parejas_corregidas.json"
        extra = ["--parejas", corrected] if corrected.is_file() else []
        run(sys.executable, "tools/editions/crear_arbol.py", ident, table,
            "../assets/" + destination.name + "/default.xex", *extra,
            *(f"{old}={new}" for old, new in fingerprints.items()))
        with (app / "codegen.log").open("w", encoding="utf-8") as log:
            subprocess.run([str(Path(args.rexglue).resolve()), "codegen", "nfsmw_manifest.toml"],
                           cwd=app, stdout=log, stderr=subprocess.STDOUT, check=True)
        generated = app / "generated/default"
        run(sys.executable, "tools/llamadas_directas.py", "--deshacer", "--gen", generated)
        run(sys.executable, "tools/llamadas_directas.py", "--gen", generated, "--enganchadas", app / "enganchadas.txt")
        run(sys.executable, "tools/copia_literal.py", generated, app / "src/copias_literales", "--tabla", table)
    if not any((app / "generated/default").glob("nfsmw_recomp.*.cpp")):
        raise ValueError("Missing generated code; run codegen first")
    write_json(descriptor, dict(id=ident, name=data["nombre"], sha256=digest,
                               language=data["idioma"], country=data["pais"], app=app.name,
                               source_sha256=source_hash()))
    print("Prepared:", descriptor)


def elf_arm64(path):
    """Reject mislabeled 32-bit/x86/non-ELF staging inputs."""
    with Path(path).open("rb") as f:
        header = f.read(64)
    if len(header) != 64 or header[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", header, 16) != (3, 183):
        raise ValueError("Expected an AArch64 ELF shared library: " + str(path))


def native_objects(ident):
    app_name = "app" if ident == PAL else "app_" + ident
    library = "main" if ident == PAL else "main_" + ident
    candidates = []
    for cache in (APP / ".cxx/Release").glob("*/arm64-v8a/CMakeCache.txt"):
        text = cache.read_text(encoding="utf-8")
        if f"NFSMW_EDITION_APP:STRING={app_name}\n" not in text:
            continue
        if f"NFSMW_EDITION_LIBRARY:STRING={library}\n" not in text:
            continue
        obj = APP / "build/intermediates/cxx/Release" / cache.parent.parent.name / "obj/arm64-v8a"
        if (obj / ("lib" + library + ".so")).is_file():
            candidates.append(obj)
    if not candidates:
        raise ValueError("No compiled native module for " + ident)
    return max(candidates, key=lambda p: (p / ("lib" + library + ".so")).stat().st_mtime_ns)


def build(args):
    ids = args.editions
    if not ids or ids[0] != PAL or len(set(ids)) != len(ids):
        raise ValueError("The first edition must be pal_es; no duplicates")
    current_source = source_hash()
    for ident in ids:
        if not re.fullmatch(r"[a-z][a-z0-9_]{0,31}", ident) or ident not in CATALOG:
            raise ValueError("Invalid edition: " + ident)
        d = json.loads((OUT / ident / "edition.json").read_text(encoding="utf-8"))
        if d.get("source_sha256") != current_source:
            raise ValueError("Base app changed: prepare " + ident + " again")
        expected_app = "app" if ident == PAL else "app_" + ident
        if d.get("id") != ident or d.get("app") != expected_app or d.get("sha256") not in CATALOG[ident]["sha256"]:
            raise ValueError("Invalid edition descriptor: " + ident)
    env = dict(os.environ)
    studio = Path(r"C:\Program Files\Android\Android Studio\jbr")
    if not env.get("JAVA_HOME") and studio.is_dir():
        env["JAVA_HOME"] = str(studio)
    env.setdefault("ANDROID_HOME", str(Path.home() / "AppData/Local/Android/Sdk"))
    gradlew = ROOT / "android" / ("gradlew.bat" if os.name == "nt" else "gradlew")
    def gradle(*arguments):
        run(gradlew, *arguments, "--console=plain", cwd=ROOT / "android", env=env)
    for ident in ids:
        gradle(":app:externalNativeBuildRelease", "-Pnfsmw.edition=" + ident)
    sdk = Path(env["ANDROID_HOME"])
    nm = next((sdk / "ndk/28.2.13676358/toolchains/llvm/prebuilt").glob(
        "*/bin/" + ("llvm-nm.exe" if os.name == "nt" else "llvm-nm")))
    def exports(path):
        return set(subprocess.check_output([str(nm), "-D", "--defined-only", "--format=just-symbols", str(path)], text=True).split())
    base = native_objects(PAL)
    symbols = exports(base / "librexruntime.so")
    # Never delete a user supplied directory. Each build gets a new, private staging directory.
    OUT.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix="package-", dir=OUT))
    libs = staging / "jniLibs/arm64-v8a"
    libs.mkdir(parents=True)
    entries = []
    for ident in ids[1:]:
        obj = native_objects(ident)
        if exports(obj / "librexruntime.so") != symbols:
            raise ValueError("Edition SDK exports differ: " + ident)
        # The SDK and SDL are shared by every edition. Refuse other differing dependencies too.
        for name in ("libSDL3.so", "libc++_shared.so"):
            if (obj / name).is_file() and (base / name).is_file() and sha(obj / name) != sha(base / name):
                raise ValueError("Different shared dependency: " + name)
        library = "main_" + ident
        src = obj / ("lib" + library + ".so")
        elf_arm64(src)
        dst = libs / src.name
        shutil.copy2(src, dst)
        data = json.loads((OUT / ident / "edition.json").read_text(encoding="utf-8"))
        data.update(library=library, native_sha256=sha(dst))
        entries.append(data)
    write_json(staging / "editions.json", entries)
    gradle(":app:assembleRelease", ":app:lintVitalRelease", "-Pnfsmw.edition=pal_es", "-Pnfsmw.editionsDir=" + str(staging))
    print("APK:", APP / "build/outputs/apk/release/app-release.apk")
    print("Compiled editions:", ", ".join(ids))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare")
    prep.add_argument("--xex", required=True)
    prep.add_argument("--library", help="Private NFSP of this edition, for special-shader matching")
    prep.add_argument("--pal-library", help="Private reference PAL NFSP")
    prep.add_argument("--rexglue", help="Host code generator from the local SDK")
    pack = commands.add_parser("build")
    pack.add_argument("--editions", nargs="+", default=[PAL])
    args = parser.parse_args()
    try:
        (prepare if args.command == "prepare" else build)(args)
    except (ValueError, OSError, StopIteration, subprocess.CalledProcessError) as e:
        parser.exit(1, "ERROR: " + str(e) + "\n")


if __name__ == "__main__":
    main()
