"""Host tests for rejecting unprepared executables and invalid native modules."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("android_editions", ROOT / "tools/android/editions.py")
editions = importlib.util.module_from_spec(spec)
spec.loader.exec_module(editions)


class EditionTests(unittest.TestCase):
    def test_unknown_executable_never_falls_back_to_pal(self):
        with tempfile.TemporaryDirectory() as tmp:
            xex = Path(tmp) / "default.xex"
            xex.write_bytes(b"unknown executable")
            with self.assertRaisesRegex(ValueError, "Unknown XEX"):
                editions.identify(xex)

    def test_native_architecture_and_type(self):
        with tempfile.TemporaryDirectory() as tmp:
            lib = Path(tmp) / "libmain.so"
            header = bytearray(64)
            header[:6] = b"\x7fELF\x02\x01"
            struct.pack_into("<HH", header, 16, 3, 183)
            lib.write_bytes(header)
            editions.elf_arm64(lib)
            for offset, value in ((4, 1), (5, 2), (16, 2), (18, 62)):
                invalid = bytearray(header)
                invalid[offset] = value
                lib.write_bytes(invalid)
                with self.assertRaises(ValueError):
                    editions.elf_arm64(lib)
            lib.write_bytes(b"not ELF")
            with self.assertRaises(ValueError):
                editions.elf_arm64(lib)

    def test_pal_catalog_matches_launcher_and_shader_builder(self):
        import json
        known = editions.CATALOG[editions.PAL]["sha256"][0]
        manifest = json.loads((ROOT / "android/app/src/main/assets/shaders/release/manifest.json").read_text())
        self.assertIn(known, {build["xex_sha256"] for build in manifest["builds"]})
        self.assertIn(known, (ROOT / "android/app/build.gradle").read_text(encoding="utf-8"))

    def test_duplicate_and_non_pal_packaging_refused(self):
        from types import SimpleNamespace
        for ids in (["usa"], ["pal_es", "pal_es"], []):
            with self.assertRaisesRegex(ValueError, "first edition"):
                editions.build(SimpleNamespace(editions=ids))


if __name__ == "__main__":
    unittest.main()
