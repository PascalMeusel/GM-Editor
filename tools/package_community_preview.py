"""Create paired, local Windows/source preview ZIPs from a reviewed source tree.

Never uploads, overwrites an existing output, copies Git history, or packages
campaign data. Requires a successful Release build with the static MSVC runtime.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import zipfile

from prepare_open_source import digest, is_link, safe_path

PINS = {
    "extern/glfw": "63a7e8b7f82497b0459acba5c1ce7f39aa2bc0e8",
    "extern/imgui": "a28cb615ff2c2ed8901397b00762d557cae24760",
    "_vendor/SQLiteCpp": "4cb0bf68b75b2f8b749ec5dc77f152f8aa62ec0e",
}


def reviewed_source(root: Path, audit: Path) -> list[str]:
    summary = json.loads((audit / "summary.json").read_text(encoding="utf-8"))
    if not summary.get("public_export_eligible"):
        raise ValueError("Source audit is not eligible for public export")
    with (audit / "inventory.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if not rows or len({row['path'] for row in rows}) != len(rows):
        raise ValueError("Empty or duplicate source inventory")
    for row in rows:
        relative = row["path"]
        path = root / relative
        if (not safe_path(relative) or row["status"] != "reviewed" or
                not path.resolve().is_relative_to(root) or
                any(is_link(part) for part in (path, *path.parents) if part != root and part.is_relative_to(root)) or
                digest(path) != row["sha256"]):
            raise ValueError(f"Unreviewed, unsafe or changed source: {relative}")
    paths = [row["path"] for row in rows]
    if not {"LICENSE", "LICENSING.md", "ARTWORK_LICENSE.md", "CMakeLists.txt"}.issubset(paths):
        raise ValueError("Required source/license files are missing")
    # The exporter deliberately replaces the private development README.
    if (root / "README.md").is_symlink() or not (root / "README.md").is_file():
        raise ValueError("Staged public README is missing or linked")
    expected = (root / "docs/open-source/README.md").read_text(encoding="utf-8")
    expected = expected.replace("](../../", "](")
    for name in ("BUILDING.md", "ARCHITECTURE.md", "KNOWN_LIMITATIONS.md", "HUMAN_VALIDATION.md"):
        expected = expected.replace(f"]({name})", f"](docs/open-source/{name})")
    if (root / "README.md").read_text(encoding="utf-8") != expected:
        raise ValueError("Staged README differs from the public guide")
    return sorted(set(paths + ["README.md"]))


def verify_build(root: Path, build: Path, files: list[str]) -> Path:
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    values = dict(line.split("=", 1) for line in cache.splitlines() if "=" in line and not line.startswith(("#", "//")))
    if Path(values.get("CMAKE_HOME_DIRECTORY:INTERNAL", "")).resolve() != root:
        raise ValueError("Build belongs to a different source tree")
    if values.get("CMAKE_BUILD_TYPE:STRING") != "Release":
        raise ValueError("A Release build is required")
    runtime = next((v for k, v in values.items() if k.startswith("CMAKE_MSVC_RUNTIME_LIBRARY:")), "")
    if runtime != "MultiThreaded$<$<CONFIG:Debug>:Debug>":
        raise ValueError("Build must use -StaticRuntime for the standalone Windows package")
    exe = build / "TTRPG_Editor.exe"
    if not exe.is_file() or exe.is_symlink():
        raise ValueError("Application executable is missing or linked")
    inputs = [root / f for f in files if Path(f).suffix in {".cpp", ".hpp", ".h", ".inl", ".rc", ".ico", ".cmake"} or f == "CMakeLists.txt"]
    if any(path.stat().st_mtime_ns > exe.stat().st_mtime_ns for path in inputs):
        raise ValueError("Source is newer than the executable; rebuild before packaging")
    return exe


def dependency_files(directory: Path, pin: str) -> list[Path]:
    revision = subprocess.check_output(["git", "-C", str(directory), "rev-parse", "HEAD"], text=True).strip()
    if revision != pin:
        raise ValueError(f"Dependency revision mismatch: {directory.name}")
    changed = subprocess.check_output(["git", "-C", str(directory), "status", "--porcelain", "--untracked-files=no"], text=True)
    if changed.strip():
        raise ValueError(f"Dependency has local modifications: {directory.name}")
    names = subprocess.check_output(["git", "-C", str(directory), "ls-files", "-z"]).decode("utf-8").split("\0")
    files = []
    for name in filter(None, names):
        p = directory / name
        # gitlinks are upstream optional test dependencies, not ordinary files.
        if p.is_dir():
            continue
        if (not p.is_file() or is_link(p) or not p.resolve().is_relative_to(directory.resolve()) or
                ".git" in Path(name).parts or not safe_path(name)):
            raise ValueError(f"Unsafe or missing dependency file: {name}")
        files.append(p)
    return files


def archive(path: Path, files: dict[str, Path], prefix: str) -> None:
    with zipfile.ZipFile(path, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for relative, source in sorted(files.items()):
            if not relative or ".." in Path(relative).parts or Path(relative).is_absolute():
                raise ValueError("Unsafe archive path")
            output.write(source, f"{prefix}/{relative}")
    with zipfile.ZipFile(path) as check:
        failure = check.testzip()
        if failure:
            raise ValueError(f"Archive CRC failure: {failure}")


def package(root: Path, build: Path, audit: Path, output: Path) -> dict:
    root, build, audit, output = (p.resolve() for p in (root, build, audit, output))
    if output.exists() or any(output.is_relative_to(p) or p.is_relative_to(output) for p in (root, build, audit)):
        raise ValueError("Output must be a new directory outside source, build and audit")
    source_names = reviewed_source(root, audit)
    exe = verify_build(root, build, source_names)
    dependencies = {"extern/glfw": root / "extern/glfw", "extern/imgui": root / "extern/imgui",
                    "_vendor/SQLiteCpp": build / "_deps/sqlitecpp-src"}
    source_files = {name: root / name for name in source_names}
    for destination, directory in dependencies.items():
        dependency_files(directory, PINS[destination])
    profile = json.loads((root / "distribution/community-preview-resources.json").read_text(encoding="utf-8"))
    if not set(profile["resources"]).issubset(source_files):
        raise ValueError("Runtime resources differ from the reviewed source")
    # All checks above run before creating output. Partial output is never labelled ready.
    output.mkdir(parents=True)
    # Export canonical Git blobs, avoiding CRLF checkout conversion and retaining
    # complete dependency source without publishing any Git history or remotes.
    for destination, directory in dependencies.items():
        raw = subprocess.check_output(["git", "-C", str(directory), "archive", "--format=tar", PINS[destination]])
        canonical = output / "source-dependencies" / destination
        hashes = {}
        with tarfile.open(fileobj=io.BytesIO(raw), mode="r:") as upstream:
            for member in upstream.getmembers():
                if member.isdir():
                    continue
                if not member.isfile() or not safe_path(member.name):
                    raise ValueError("Unsafe upstream archive member")
                path = canonical / member.name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(upstream.extractfile(member).read())
                hashes[member.name] = digest(path)
                source_files[f"{destination}/{member.name}"] = path
        manifest = canonical / ".gm-source-manifest.json"
        manifest.write_text(json.dumps({"revision": PINS[destination], "files": hashes}, indent=2) + "\n", encoding="utf-8")
        source_files[f"{destination}/{manifest.name}"] = manifest
    binary = output / "GM-Editor-0.9.0-community-preview-windows-x64"
    binary.mkdir()
    binary_files = {"TTRPG_Editor.exe": exe, "gm_editor.install.json": root / "distribution/gm_editor.install.json"}
    binary_files.update({name: root / name for name in profile["resources"]})
    for name in ["LICENSE", "LICENSING.md", "ARTWORK_LICENSE.md", "THIRD_PARTY_NOTICES.md", "CONTRIBUTING.md", "SECURITY.md",
                 "docs/GITHUB_TICKETS.md", "docs/MARKDOWN_DATA_EXCHANGE.md"]:
        binary_files[name] = root / name
    for path in (root / "docs/open-source").glob("*.md"):
        binary_files[f"docs/open-source/{path.name}"] = path
    binary_files["START_HERE.txt"] = root / "docs/open-source/START_HERE.md"
    for name in ["open-source-decisions.json", "community-preview-resources.json"]:
        binary_files[f"distribution/{name}"] = root / f"distribution/{name}"
    for path in (root / "distribution/licenses").glob("*"):
        if path.is_file():
            binary_files[f"distribution/licenses/{path.name}"] = path
    for destination, directory in dependencies.items():
        name = {"extern/glfw": "LICENSE.md", "extern/imgui": "LICENSE.txt", "_vendor/SQLiteCpp": "LICENSE.txt"}[destination]
        binary_files[f"LICENSES/{directory.name}-LICENSE.txt"] = directory / name
    for p in (root / "extern/imgui").glob("imstb_*.h"):
        binary_files[f"LICENSES/{p.name}.txt"] = p  # Retain complete upstream license/copyright blocks.
    for name, original in binary_files.items():
        target = binary / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, target)
        if digest(target) != digest(original):
            raise ValueError(f"Package copy mismatch: {name}")
    # The full guide remains unchanged in docs/open-source. Its root-level
    # convenience copy needs links resolved from the Windows package root.
    start_here = binary / "START_HERE.txt"
    guide = start_here.read_text(encoding="utf-8")
    for name in ("KNOWN_LIMITATIONS.md", "CONTENT_REVIEW.md"):
        guide = guide.replace(f"]({name})", f"](docs/open-source/{name})")
    start_here.write_text(guide, encoding="utf-8", newline="\n")
    binary_files["START_HERE.txt"] = start_here
    source_inventory = {name: digest(path) for name, path in sorted(source_files.items())}
    info = {"schema": 1, "release": "0.9.0-community-preview", "publication": "local candidate; owner review pending",
            "source_inventory_sha256": hashlib.sha256(json.dumps(source_inventory, sort_keys=True).encode()).hexdigest(),
            "source_files": len(source_inventory), "dependencies": PINS,
            "executable_sha256": digest(exe), "msvc_runtime": "statically linked",
            "binary_files": {name: digest(path) for name, path in sorted(binary_files.items())}}
    receipt = output / "RELEASE_MANIFEST.json"
    receipt.write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    shutil.copyfile(receipt, binary / receipt.name)
    source_manifest = output / "SOURCE_MANIFEST.json"
    source_manifest.write_text(json.dumps(source_inventory, indent=2) + "\n", encoding="utf-8")
    source_files["SOURCE_MANIFEST.json"] = source_manifest
    source_files["RELEASE_MANIFEST.json"] = receipt
    source_zip = output / "GM-Editor-0.9.0-community-preview-source.zip"
    binary_zip = output / (binary.name + ".zip")
    archive(source_zip, source_files, "GM-Editor-source")
    archive(binary_zip, {p.relative_to(binary).as_posix(): p for p in binary.rglob("*") if p.is_file()}, binary.name)
    (output / "SHA256SUMS.txt").write_text("".join(f"{digest(p)}  {p.name}\n" for p in [source_zip, binary_zip, receipt, source_manifest]), encoding="ascii")
    return {"source_zip": str(source_zip), "windows_zip": str(binary_zip), "source_files": len(source_inventory),
            "runtime_resources": len(profile["resources"]), "publication": "not published"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source", "build", "audit", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(package(args.source, args.build, args.audit, args.output), indent=2))
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Package refused: {error}", file=sys.stderr)
        sys.exit(2)
