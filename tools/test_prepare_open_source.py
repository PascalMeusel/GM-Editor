"""Regression checks for the public-source trust boundary, using disposable files."""
import hashlib
from pathlib import Path
import tempfile
import unittest

import prepare_open_source as export


class SourceExportTests(unittest.TestCase):
    def test_private_data_and_workflows_are_excluded(self):
        for path in ("TTRPG.db", "Models/test.db-wal", "Resources/Campaigns/secret.json",
                     "docs/kickstarter/index.html", "docs/business/plan.md", "outputs/art.png",
                     "Resources/ArtCandidates/example.png", ".github/workflows/build.yml",
                     ".github/FUNDING.yml", ".env", "tools/.env.production", "../secret.cpp"):
            with self.subTest(path=path):
                self.assertFalse(export.selected(path))
        self.assertTrue(export.selected("Models/new_uncommitted.cpp"))
        self.assertTrue(export.selected("scripts/build-windows-msvc.ps1"))
        self.assertTrue(export.selected("Models/Map/MapSettlementCompositionV10.inl"))
        self.assertTrue(export.selected("scripts/ci/run-linux-check.sh"))
        self.assertTrue(export.selected(".github/workflows/portable-core.yml"))

    def test_secret_report_never_contains_value(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sample.cpp"
            secret = "gh" + "p_" + "z" * 36
            path.write_text('const char* token = "' + secret + '";\n', encoding="utf-8")
            results = export.findings(path, "sample.cpp")
            self.assertEqual(results[0]["rule"], "github-token")
            self.assertNotIn(secret, str(results))

    def test_unknown_art_and_changed_font_remain_blocked(self):
        self.assertEqual(export.classification("Resources/Portraits/Company/human.png", "x")[0], "pending")
        font = next(iter(export.FONT_HASHES))
        self.assertEqual(export.classification(font, "x")[0], "blocked")

    def test_public_export_refuses_pending_before_copying(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            root.mkdir()
            art = root / "Resources/Portraits/test.png"
            art.parent.mkdir(parents=True)
            art.write_bytes(b"unreviewed art")
            rows, alerts, _ = export.inventory(root)
            destination = Path(temporary) / "public"
            with self.assertRaisesRegex(ValueError, "unresolved"):
                export.stage(root, destination, rows, alerts, True)
            self.assertFalse(destination.exists())

    def test_private_stage_preserves_current_work_and_has_no_history(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            root.mkdir()
            (root / "main.cpp").write_text("// latest local code\n", encoding="utf-8")
            (root / ".git").mkdir()
            (root / ".git/config").write_text("private history", encoding="utf-8")
            (root / "TTRPG.db").write_bytes(b"private campaign")
            rows, alerts, _ = export.inventory(root)
            destination = Path(temporary) / "review"
            export.stage(root, destination, rows, alerts, False)
            self.assertEqual((root / "main.cpp").read_bytes(), (destination / "main.cpp").read_bytes())
            self.assertTrue((destination / "PRIVATE_REVIEW_ONLY.txt").exists())
            self.assertFalse((destination / ".git").exists())
            self.assertFalse((destination / "TTRPG.db").exists())

    def test_source_change_or_existing_destination_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            root.mkdir()
            code = root / "main.cpp"
            code.write_text("before", encoding="utf-8")
            rows, alerts, _ = export.inventory(root)
            code.write_text("after", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "changed"):
                export.stage(root, Path(temporary) / "review", rows, alerts, False)
            with self.assertRaisesRegex(ValueError, "outside"):
                export.stage(root, root / "nested", rows, alerts, False)
            existing = Path(temporary) / "existing"
            existing.mkdir()
            with self.assertRaisesRegex(ValueError, "already exists"):
                export.stage(root, existing, rows, alerts, False)

    def test_public_stage_rewrites_human_protocol_link_only_in_root_readme(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            docs = root / "docs/open-source"
            docs.mkdir(parents=True)
            for name in ("LICENSE", "LICENSING.md", "CMakeLists.txt"):
                (root / name).write_text("Fixture\n", encoding="utf-8")
            guide = "[Human validation protocol](HUMAN_VALIDATION.md).\n"
            (docs / "README.md").write_text(guide, encoding="utf-8")
            (docs / "HUMAN_VALIDATION.md").write_text("# Pending human validation\n", encoding="utf-8")
            rows, alerts, _ = export.inventory(root)
            destination = Path(temporary) / "public"
            export.stage(root, destination, rows, alerts, True)
            self.assertEqual((destination / "README.md").read_text(encoding="utf-8"),
                             "[Human validation protocol](docs/open-source/HUMAN_VALIDATION.md).\n")
            self.assertEqual((destination / "docs/open-source/README.md").read_text(encoding="utf-8"), guide)
            self.assertTrue((destination / "docs/open-source/HUMAN_VALIDATION.md").is_file())
            self.assertFalse((destination / "HUMAN_VALIDATION.md").exists())

    def test_decisions_are_bound_to_file_bytes(self):
        import json
        with tempfile.TemporaryDirectory() as temporary:
            decision_file = Path(temporary) / "decisions.json"
            decision_file.write_text(json.dumps({"schema": 1, "files": [{
                "path": "Resources/test.png", "sha256": "old", "license": "CC0-1.0",
                "evidence": "Source record", "reviewer": "Maintainer", "date": "2026-09-30"}]}))
            rows = [{"path": "Resources/test.png", "sha256": "changed", "status": "pending"}]
            with self.assertRaisesRegex(ValueError, "current file bytes"):
                export.apply_decisions(rows, decision_file)

    def test_forged_path_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            root.mkdir()
            outside = Path(temporary) / "secret.cpp"
            outside.write_text("private", encoding="utf-8")
            rows = [{"path": "../secret.cpp", "sha256": hashlib.sha256(b"private").hexdigest(),
                     "status": "reviewed"}]
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                export.stage(root, Path(temporary) / "review", rows, [], False)

    def test_missing_compiled_source_is_detected_before_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / "cmake/sources/Models.cmake"
            manifest.parent.mkdir(parents=True)
            manifest.write_text('set(SOURCES "${GM_APPLICATION_SOURCE_ROOT}/Models/missing.cpp")')
            _, alerts, _ = export.inventory(root)
            self.assertEqual(alerts, [{"path": "Models/missing.cpp", "rule": "missing-production-source"}])

    def test_linked_directory_is_not_traversed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "source"
            root.mkdir()
            outside = Path(temporary) / "private"
            outside.mkdir()
            (outside / "private.cpp").write_text("private content")
            try:
                (root / "Models").symlink_to(outside, target_is_directory=True)
            except OSError:
                self.skipTest("Creating symlinks is not permitted on this host")
            rows, alerts, _ = export.inventory(root)
            self.assertEqual(rows, [])
            self.assertEqual(alerts, [{"path": "Models", "rule": "linked-directory"}])

    def test_resource_profile_preserves_code_and_does_not_approve_art(self):
        import json
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "profile.json"
            path.write_text(json.dumps({"schema": 1, "resources": ["Resources/Portraits/human.png"]}))
            rows = [{"path": "Models/main.cpp", "status": "reviewed"},
                    {"path": "Resources/Portraits/human.png", "status": "pending"},
                    {"path": "Resources/Portraits/unknown.png", "status": "pending"}]
            kept, omitted = export.apply_resource_profile(rows, path)
            self.assertEqual(kept, rows[:2])
            self.assertEqual(omitted, [rows[2]["path"]])
            self.assertEqual(kept[1]["status"], "pending")
            for resources in [["Models/main.cpp"], ["Resources/Portraits/missing.png"],
                              ["Resources/../secret.cpp"], [rows[1]["path"], rows[1]["path"]]]:
                path.write_text(json.dumps({"schema": 1, "resources": resources}))
                with self.assertRaises(ValueError):
                    export.apply_resource_profile(rows, path)


if __name__ == "__main__":
    unittest.main()
