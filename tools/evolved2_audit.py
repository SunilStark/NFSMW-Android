"""Compare tracked source inventories without copying code or touching either checkout.

Usage: python tools/evolved2_audit.py --reference ../nfsmw-references/victorgbd-mobile
The JSON contains hashes and paths, never game files or local absolute paths.
"""
import argparse
import hashlib
import json
import subprocess
from collections import Counter
from pathlib import Path


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def digest(data):
    # Keep binary hashes exact; normalize only UTF-8 text for Windows checkouts.
    try:
        data.decode("utf-8")
        if b"\0" not in data:
            data = data.replace(b"\r\n", b"\n")
    except UnicodeDecodeError:
        pass
    return hashlib.sha256(data).hexdigest()


def compare(primary, reference, base):
    primary_paths = git(primary, "ls-files", "-z").decode().split("\0")
    reference_paths = set(git(reference, "ls-files", "-z").decode().split("\0"))
    rows = []
    mapped = set()
    for path in primary_paths:
        if not path.startswith(("app/", "sdk/", "shaders/", "android/", "tools/", "docs/", "pgo/")):
            continue
        refpath = "nativo/" + path if path.startswith(("app/", "sdk/", "shaders/", "pgo/")) else path
        ours = primary / path
        if not ours.is_file():
            continue
        row = {"primary": path, "reference": refpath, "primary_sha256": digest(ours.read_bytes())}
        if refpath in reference_paths and (reference / refpath).is_file():
            mapped.add(refpath)
            row["reference_sha256"] = digest((reference / refpath).read_bytes())
            row["status"] = "identical" if row["primary_sha256"] == row["reference_sha256"] else "modified"
        else:
            # Distinct Java packages are not evidence that a feature was removed.
            row["status"] = "no_equivalent_path"
        rows.append(row)
    for path in sorted(reference_paths - mapped):
        if path.startswith(("nativo/app/", "nativo/sdk/", "nativo/shaders/")) and (reference / path).is_file():
            if any(row["reference"] == path for row in rows):
                continue
            rows.append({"reference": path, "status": "reference_only", "reference_sha256": digest((reference / path).read_bytes())})
    return {
        "primary_commit": git(primary, "rev-parse", "HEAD").decode().strip(),
        "reference_commit": git(reference, "rev-parse", "HEAD").decode().strip(),
        "historical_base": git(primary, "rev-parse", base).decode().strip(),
        "hash_format": "SHA-256; UTF-8 text normalized to LF, binary data unchanged",
        "scope": "tracked path comparison; renamed packages and semantic differences require manual review",
        "counts": dict(Counter(row["status"] for row in rows)),
        "files": rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--base", default="6df1501")
    parser.add_argument("--output", type=Path, default=Path("out/evolved2/comparison.json"))
    args = parser.parse_args()
    primary = Path(__file__).resolve().parents[1]
    report = compare(primary, args.reference.resolve(), args.base)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("primary_commit", "reference_commit", "historical_base", "counts")}))


if __name__ == "__main__":
    main()
