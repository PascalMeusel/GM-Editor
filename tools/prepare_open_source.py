"""Inventory and stage a private source review; never publish or modify the input.

The public mode fails closed on unresolved rights, unsafe paths or secret findings.
This is a packaging preflight, not a legal opinion or a complete secret scanner.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys

CODE_ROOTS = {"Controllers", "Models", "Views", "cmake", "scripts", "tools", "distribution"}
ROOT_FILES = {"AGENTS.md", "CMakeLists.txt", "CMakePresets.json", "TTRPG_Editor.rc",
              ".gitattributes", ".gitignore", ".gitmodules", "LICENSE", "LICENSING.md",
              "THIRD_PARTY_NOTICES.md", "CONTRIBUTING.md", "SECURITY.md", "ARTWORK_LICENSE.md"}
TEXT_EXTENSIONS = {".cpp", ".hpp", ".h", ".c", ".inl", ".sh", ".cjs", ".mjs", ".cmake", ".ps1", ".py", ".json",
                   ".md", ".txt", ".yml", ".yaml", ".patch", ".psv", ".rc"}
FORBIDDEN_PARTS = {".git", "__pycache__", "node_modules", "dist", "outputs", "out",
                   "campaigns", "backups", "recovery", "logs"}
FORBIDDEN_SUFFIXES = {".db", ".sqlite", ".sqlite3", ".log", ".pem", ".pfx", ".key",
                      ".exe", ".dll", ".obj", ".zip", ".7z", ".tar", ".bak"}
PRIVATE_WORKFLOWS = {"build.yml", "extended-qa.yml", "release-acceptance.yml"}
FONT_HASHES = {
    "Resources/Fonts/Inter/Inter-Regular.ttf": "40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82",
    "Resources/Fonts/Inter/Inter-SemiBold.ttf": "78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3",
    "Resources/Fonts/Inter/LICENSE.txt": "262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a",
    "Resources/Fonts/SourceSerif4/SourceSerif4-Regular.ttf": "e5a4ee6a3d87bb9024796be390c6771e2a0eb1883dae25effaf57ca01668e24b",
    "Resources/Fonts/SourceSerif4/LICENSE.md": "75784a295293a8992f5a8d99210566e0064a012e6dab6731305e3787f15896c7",
}
SECRET_PATTERNS = {
    "private-key": re.compile(r"-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----"),
    "github-token": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{50,})\b"),
    "cloud-access-id": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
    "api-key": re.compile(r"\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{40,}\b"),
}


def digest(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def is_link(path: Path) -> bool:
    # Path.is_junction only exists on newer Python versions. Reject all Windows
    # reparse points as well as POSIX links, including links to files inside root.
    attributes = getattr(path.lstat(), "st_file_attributes", 0)
    return path.is_symlink() or bool(attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400))


def safe_path(relative: str) -> bool:
    path = Path(relative)
    parts = relative.replace("\\", "/").split("/")
    return (not path.is_absolute() and not re.match(r"^[A-Za-z]:", relative)
            and all(p not in {"", ".", ".."} for p in parts)
            and not any(p.lower() in FORBIDDEN_PARTS for p in parts)
            and not any(p.lower() == "build" or p.lower().startswith("build-") for p in parts[:-1])
            and not path.name.lower().startswith(".env")
            and path.name.lower() not in {"gmeditor.settings", "imgui.ini"}
            and path.suffix.lower() not in FORBIDDEN_SUFFIXES
            and not re.search(r"\.(?:db|sqlite|sqlite3)(?:-wal|-shm|-journal)?$", relative, re.I))


def selected(relative: str) -> bool:
    if not safe_path(relative):
        return False
    p = Path(relative)
    parts = p.parts
    if len(parts) == 1:
        return relative in ROOT_FILES or p.suffix in {".cpp", ".hpp"}
    if parts[0] in CODE_ROOTS:
        return p.suffix.lower() in TEXT_EXTENSIONS
    if relative.startswith("docs/open-source/") or relative in {"docs/GITHUB_TICKETS.md", "docs/MARKDOWN_DATA_EXCHANGE.md"}:
        return p.suffix in {".md", ".json"}
    if parts[0] == ".github":
        return p.name not in PRIVATE_WORKFLOWS and p.name != "FUNDING.yml"
    if parts[0] == "Resources":
        return len(parts) > 1 and parts[1] in {"AppIcon", "Audio", "Fonts", "Maps", "Portraits", "Samples"}
    return False


def classification(relative: str, sha: str) -> tuple[str, str, str]:
    if relative in FONT_HASHES:
        if sha != FONT_HASHES[relative]:
            return "blocked", "OFL-1.1", "Pinned upstream font/license hash mismatch"
        return "reviewed", "OFL-1.1", "Pinned upstream hash matches retained source record"
    if relative.startswith("Resources/Fonts/") and relative.endswith("/SOURCE.md"):
        return "reviewed", "GPL-3.0-or-later", "Project provenance record"
    if relative.startswith("Resources/Samples/"):
        return "reviewed", "GPL-3.0-or-later", "Original sample notice; no campaign database included"
    if relative.startswith("Resources/"):
        return "pending", "see source ledger", "Asset-family redistribution decision required"
    if relative.startswith("Models/Rulesets/DND/") or relative == "Models/Character/PortraitRecipe.hpp":
        return "pending", "GPL-3.0-or-later; CC-BY-4.0 where applicable", "Review SRD scope, non-SRD names and complete attribution"
    return "reviewed", "GPL-3.0-or-later unless separately noticed", "Project source or documentation; retain embedded upstream notices"


def findings(path: Path, relative: str) -> list[dict]:
    if path.suffix.lower() not in TEXT_EXTENSIONS and path.name != "LICENSE":
        return []
    # Read text only. Never place a matched secret, full line or excerpt in output.
    body = path.read_text(encoding="utf-8-sig", errors="replace")
    result = []
    for rule, pattern in SECRET_PATTERNS.items():
        for match in pattern.finditer(body):
            result.append({"path": relative, "line": body.count("\n", 0, match.start()) + 1,
                           "rule": rule})
    return result


def inventory(root: Path, resource_paths: set[str] | None = None) -> tuple[list[dict], list[dict], list[str]]:
    rows, alerts, excluded = [], [], []
    for directory, directories, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in list(directories):
            child = parent / name
            relative = child.relative_to(root).as_posix()
            if is_link(child):
                alerts.append({"path": relative, "rule": "linked-directory"})
                directories.remove(name)
            elif name.lower() in FORBIDDEN_PARTS or name.lower().startswith("build") or name == "extern":
                directories.remove(name)
            elif parent == root and name not in CODE_ROOTS | {"docs", "Resources", ".github"}:
                directories.remove(name)
        for name in files:
            path = parent / name
            relative = path.relative_to(root).as_posix()
            if not selected(relative) or (resource_paths is not None and relative.startswith("Resources/") and relative not in resource_paths):
                excluded.append(relative)
                continue
            if is_link(path) or not path.resolve().is_relative_to(root):
                alerts.append({"path": relative, "rule": "linked-file"})
                continue
            sha = digest(path)
            state, license_id, evidence = classification(relative, sha)
            rows.append({"path": relative, "bytes": path.stat().st_size, "sha256": sha,
                         "status": state, "license": license_id, "evidence": evidence})
            alerts.extend(findings(path, relative))
    selected_paths = {row["path"] for row in rows}
    for manifest in sorted((root / "cmake/sources").glob("*.cmake")):
        for relative in re.findall(r'\$\{GM_APPLICATION_SOURCE_ROOT\}/([^"\s]+)',
                                   manifest.read_text(encoding="utf-8-sig")):
            if relative not in selected_paths:
                alerts.append({"path": relative, "rule": "missing-production-source"})
    return sorted(rows, key=lambda row: row["path"]), alerts, sorted(excluded)


def stage(root: Path, destination: Path, rows: list[dict], alerts: list[dict], public: bool) -> None:
    root, destination = root.resolve(), destination.resolve()
    if destination == root or destination.is_relative_to(root) or root.is_relative_to(destination):
        raise ValueError("Staging must be outside the source checkout")
    if destination.exists():
        raise ValueError("Destination already exists; use a new directory")
    if alerts:
        raise ValueError("Resolve secret/path findings before staging")
    if public and any(row["status"] != "reviewed" for row in rows):
        raise ValueError("Public export blocked by unresolved redistribution decisions")
    if public and not {"LICENSE", "LICENSING.md", "CMakeLists.txt", "docs/open-source/README.md"}.issubset(
            {row["path"] for row in rows}):
        raise ValueError("Required public source/license files are missing")
    # Validate all paths and hashes before creating any destination.
    for row in rows:
        source = root / row["path"]
        if (not selected(row["path"]) or not source.resolve().is_relative_to(root)
                or any(is_link(part) for part in (source, *source.parents) if part != root and part.is_relative_to(root))):
            raise ValueError("Unsafe or unselected source path")
        if digest(source) != row["sha256"]:
            raise ValueError("Source changed after inventory; run a fresh audit")
    destination.mkdir(parents=True)
    for row in rows:
        source = root / row["path"]
        target = destination / row["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        if digest(target) != row["sha256"]:
            raise ValueError("Copy hash mismatch; incomplete destination is not a release")
    readme = root / "docs/open-source/README.md"
    if readme.is_file():
        text = readme.read_text(encoding="utf-8")
        text = text.replace("](../../", "](")
        for filename in ("BUILDING.md", "ARCHITECTURE.md", "KNOWN_LIMITATIONS.md", "HUMAN_VALIDATION.md"):
            text = text.replace(f"]({filename})", f"](docs/open-source/{filename})")
        (destination / "README.md").write_text(text, encoding="utf-8", newline="\n")
    if not public:
        (destination / "PRIVATE_REVIEW_ONLY.txt").write_text(
            "Private source review. Unresolved content rights remain. Do not upload.\n",
            encoding="utf-8")


def git_info(root: Path) -> dict:
    result = {}
    for label, arguments in (("base_commit", ["rev-parse", "HEAD"]),
                             ("branch", ["branch", "--show-current"]),
                             ("submodules", ["submodule", "status"])):
        run = subprocess.run(["git", "-C", str(root), *arguments], capture_output=True, text=True)
        result[label] = run.stdout.strip() if run.returncode == 0 else "unavailable"
    return result


def apply_decisions(rows: list[dict], decision_path: Path) -> None:
    """A maintainer can record a supported decision for exact bytes, not a glob."""
    payload = json.loads(decision_path.read_text(encoding="utf-8-sig"))
    if payload.get("schema") != 1 or not isinstance(payload.get("files"), list):
        raise ValueError("Unsupported redistribution decision file")
    indexed = {row["path"]: row for row in rows}
    seen = set()
    for decision in payload["files"]:
        path = decision.get("path")
        required = ("sha256", "license", "evidence", "reviewer", "date")
        if path in seen or path not in indexed or any(not decision.get(key) for key in required):
            raise ValueError("Invalid, duplicate or unselected redistribution decision")
        seen.add(path)
        row = indexed[path]
        if row["sha256"] != decision["sha256"] or row["status"] == "blocked":
            raise ValueError("Redistribution decision does not match current file bytes")
        row.update(status="reviewed", license=decision["license"],
                   evidence=f'{decision["evidence"]}; {decision["reviewer"]}; {decision["date"]}')


def read_resource_profile(profile_path: Path) -> set[str]:
    payload = json.loads(profile_path.read_text(encoding="utf-8-sig"))
    paths = payload.get("resources")
    if payload.get("schema") != 1 or not isinstance(paths, list):
        raise ValueError("Unsupported resource profile")
    if (any(not isinstance(path, str) or not path.startswith("Resources/") or
            not selected(path) for path in paths) or len(paths) != len(set(paths))):
        raise ValueError("Resource profile contains unsafe, duplicate or non-resource paths")
    return set(paths)


def apply_resource_profile(rows: list[dict], profile_path: Path) -> tuple[list[dict], list[str]]:
    """Select resources without granting permissions or hiding application code."""
    paths = read_resource_profile(profile_path)
    available = {row["path"] for row in rows}
    if not set(paths).issubset(available):
        raise ValueError("Resource profile references missing files")
    keep = set(paths)
    omitted = [row["path"] for row in rows if row["path"].startswith("Resources/") and row["path"] not in keep]
    omitted_set = set(omitted)
    return [row for row in rows if row["path"] not in omitted_set], omitted


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--report-dir", type=Path, required=True)
    parser.add_argument("--stage", type=Path, help="Create a new private review source directory")
    parser.add_argument("--public", action="store_true", help="Reject any unresolved rights; never uploads")
    parser.add_argument("--decisions", type=Path, help="Maintainer-reviewed, hash-bound redistribution decisions")
    parser.add_argument("--resource-profile", type=Path, help="Explicit resource selection; originals are preserved")
    args = parser.parse_args()
    root = args.root.resolve()
    report = args.report_dir.resolve()
    if report == root or report.is_relative_to(root):
        parser.error("Keep private audit evidence outside the source checkout")
    if args.stage and (report == args.stage.resolve() or report.is_relative_to(args.stage.resolve())):
        parser.error("Audit evidence must not be placed inside staged source")
    if report.exists():
        parser.error("Report directory already exists; preserve old evidence and use a new directory")
    resource_paths = read_resource_profile(args.resource_profile) if args.resource_profile else None
    rows, alerts, excluded = inventory(root, resource_paths)
    if args.resource_profile:
        rows, omitted = apply_resource_profile(rows, args.resource_profile)
        excluded.extend(omitted)
    if args.decisions:
        apply_decisions(rows, args.decisions)
    required_files = {"LICENSE", "LICENSING.md", "CMakeLists.txt", "docs/open-source/README.md"}
    missing = sorted(required_files - {row["path"] for row in rows})
    report.mkdir(parents=True)
    summary = {"schema": 1, "source": git_info(root), "selected_files": len(rows),
               "selected_bytes": sum(row["bytes"] for row in rows),
               "pending_files": sum(row["status"] != "reviewed" for row in rows),
               "secret_or_path_findings": len(alerts),
               "missing_required_files": missing,
               "public_export_eligible": not missing and not alerts and all(row["status"] == "reviewed" for row in rows),
               "history_clearance": "not established; use a new public history after reviewing the snapshot",
               "limitations": "Heuristic current-file scan only; use a dedicated full-history secret scanner too"}
    (report / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    (report / "findings.json").write_text(json.dumps(alerts, indent=2), encoding="utf-8")
    (report / "excluded.json").write_text(json.dumps(excluded, indent=2), encoding="utf-8")
    with (report / "inventory.csv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["path", "bytes", "sha256", "status", "license", "evidence"])
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps(summary, indent=2))
    if args.public and not summary["public_export_eligible"]:
        print("Public export refused: see private inventory and findings.", file=sys.stderr)
        return 2
    if args.stage:
        stage(root, args.stage, rows, alerts, args.public)
        print("Staged source only; initialize pinned submodules in the new checkout before building.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print(f"Preparation failed: {error}", file=sys.stderr)
        sys.exit(2)
