"""Refuse mismatched or unsafe release inputs using disposable fixtures."""
import csv
import io
import json
from pathlib import Path
import posixpath
import re
import tarfile
import tempfile
import unittest
from unittest import mock
from urllib.parse import unquote, urlsplit
import zipfile
import package_community_preview as package
import prepare_open_source as export


class PackageBoundaryTests(unittest.TestCase):
    def fixture(self, directory, guide='Fixture\n', root_readme='Fixture\n'):
        root, audit = Path(directory) / 'source', Path(directory) / 'audit'
        root.mkdir(); audit.mkdir()
        paths = ['LICENSE', 'LICENSING.md', 'ARTWORK_LICENSE.md', 'CMakeLists.txt',
                 'docs/open-source/README.md', 'docs/open-source/HUMAN_VALIDATION.md', 'docs/GITHUB_TICKETS.md']
        for name in paths:
            p = root / name; p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text('Fixture\n', encoding='utf-8')
        (root / 'docs/open-source/README.md').write_text(guide, encoding='utf-8')
        (root / 'README.md').write_text(root_readme, encoding='utf-8', newline='\n')
        (audit / 'summary.json').write_text(json.dumps({'public_export_eligible': True}))
        with (audit / 'inventory.csv').open('w', newline='', encoding='utf-8') as handle:
            writer = csv.DictWriter(handle, fieldnames=['path', 'status', 'sha256'])
            writer.writeheader()
            writer.writerows({'path': p, 'status': 'reviewed', 'sha256': package.digest(root / p)} for p in paths)
        return root.resolve(), audit

    def test_changed_or_pending_source_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, audit = self.fixture(directory)
            self.assertIn('README.md', package.reviewed_source(root, audit))
            (root / 'CMakeLists.txt').write_text('Changed')
            with self.assertRaisesRegex(ValueError, 'changed'):
                package.reviewed_source(root, audit)
            (audit / 'summary.json').write_text(json.dumps({'public_export_eligible': False}))
            with self.assertRaisesRegex(ValueError, 'not eligible'):
                package.reviewed_source(root, audit)

    def test_private_readme_cannot_replace_public_guide(self):
        with tempfile.TemporaryDirectory() as directory:
            root, audit = self.fixture(directory)
            (root / 'README.md').write_text('Private owner notes')
            with self.assertRaisesRegex(ValueError, 'public guide'):
                package.reviewed_source(root, audit)

    def test_human_protocol_root_link_matches_staging_and_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            guide = '[Human validation protocol](HUMAN_VALIDATION.md).\n'
            public_readme = '[Human validation protocol](docs/open-source/HUMAN_VALIDATION.md).\n'
            root, audit = self.fixture(directory, guide, public_readme)
            selected = package.reviewed_source(root, audit)
            self.assertIn('docs/open-source/HUMAN_VALIDATION.md', selected)
            output = Path(directory) / 'source.zip'
            package.archive(output, {name: root / name for name in selected}, 'Preview')
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read('Preview/README.md').decode('utf-8'), public_readme)
                self.assertIn('Preview/docs/open-source/HUMAN_VALIDATION.md', archive.namelist())
            (root / 'README.md').write_text(guide, encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'public guide'):
                package.reviewed_source(root, audit)

    def test_other_checkout_or_dynamic_runtime_build_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root, audit = self.fixture(directory)
            build = Path(directory) / 'build'; build.mkdir()
            cache = build / 'CMakeCache.txt'
            cache.write_text('CMAKE_HOME_DIRECTORY:INTERNAL=elsewhere\nCMAKE_BUILD_TYPE:STRING=Release\n')
            with self.assertRaisesRegex(ValueError, 'different source tree'):
                package.verify_build(root, build, [])
            cache.write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={root}\nCMAKE_BUILD_TYPE:STRING=Release\n')
            with self.assertRaisesRegex(ValueError, 'StaticRuntime'):
                package.verify_build(root, build, [])

    def test_both_archive_document_links_resolve_and_original_guides_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root, audit = self.fixture(directory)
            guide = '[Limits](KNOWN_LIMITATIONS.md) and [content](CONTENT_REVIEW.md).\n'
            exchange_guide = '# Markdown campaign-note exchange\n\nOnly supported private note text is updated.\n'
            documents = {
                'CONTRIBUTING.md': '[Ticket workflow](docs/GITHUB_TICKETS.md).\n',
                'THIRD_PARTY_NOTICES.md': 'Dependency fixture notices\n',
                'SECURITY.md': 'Security fixture\n',
                'docs/open-source/START_HERE.md': guide,
                'docs/open-source/KNOWN_LIMITATIONS.md': '[Exchange contract](../MARKDOWN_DATA_EXCHANGE.md).\n',
                'docs/open-source/CONTENT_REVIEW.md': 'Fixture content\n',
                'docs/MARKDOWN_DATA_EXCHANGE.md': exchange_guide,
                'distribution/community-preview-resources.json': '{"schema":1,"resources":[]}',
                'distribution/open-source-decisions.json': '{"schema":1,"files":[]}',
                'distribution/gm_editor.install.json': '{}',
            }
            for relative, text in documents.items():
                path = root / relative; path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding='utf-8', newline='\n')
            paths = sorted(path.relative_to(root).as_posix() for path in root.rglob('*')
                           if path.is_file() and export.selected(path.relative_to(root).as_posix()))
            self.assertIn('docs/MARKDOWN_DATA_EXCHANGE.md', paths)
            with (audit / 'inventory.csv').open('w', newline='', encoding='utf-8') as handle:
                writer = csv.DictWriter(handle, fieldnames=['path', 'status', 'sha256'])
                writer.writeheader()
                writer.writerows({'path': p, 'status': 'reviewed', 'sha256': package.digest(root / p)} for p in paths)
            build = base / 'build'; build.mkdir()
            cache = f'CMAKE_HOME_DIRECTORY:INTERNAL={root}\nCMAKE_BUILD_TYPE:STRING=Release\n'
            cache += 'CMAKE_MSVC_RUNTIME_LIBRARY:STRING=MultiThreaded$<$<CONFIG:Debug>:Debug>\n'
            (build / 'CMakeCache.txt').write_text(cache)
            (build / 'TTRPG_Editor.exe').write_bytes(b'Packaging-only fixture; never execute')
            for path in (root / 'extern/glfw/LICENSE.md', root / 'extern/imgui/LICENSE.txt', build / '_deps/sqlitecpp-src/LICENSE.txt'):
                path.parent.mkdir(parents=True, exist_ok=True); path.write_text('Fixture license\n')
            upstream = io.BytesIO()
            with tarfile.open(fileobj=upstream, mode='w') as archive:
                body = b'Fixture upstream source and license\n'
                member = tarfile.TarInfo('LICENSE.txt'); member.size = len(body)
                archive.addfile(member, io.BytesIO(body))
            # Synthetic upstream bytes avoid cloning or compiling dependencies;
            # production source validation, copying, manifests and ZIPs stay real.
            with mock.patch.object(package, 'dependency_files', return_value=[]), mock.patch.object(
                    package.subprocess, 'check_output', return_value=upstream.getvalue()):
                result = package.package(root, build, audit, base / 'output')
            prefix = 'GM-Editor-0.9.0-community-preview-windows-x64/'
            with zipfile.ZipFile(result['windows_zip']) as archive:
                names = {name[len(prefix):] for name in archive.namelist()}
                self.assertIn('docs/GITHUB_TICKETS.md', names)
                self.assertEqual(archive.read(prefix + 'docs/open-source/START_HERE.md'), guide.encode())
                rebased = archive.read(prefix + 'START_HERE.txt').decode('utf-8')
                self.assertIn('](docs/open-source/KNOWN_LIMITATIONS.md)', rebased)
                self.assertIn('](docs/open-source/CONTENT_REVIEW.md)', rebased)
                manifest = json.loads(archive.read(prefix + 'RELEASE_MANIFEST.json'))
                self.assertEqual(manifest['binary_files']['START_HERE.txt'],
                                 package.hashlib.sha256(rebased.encode()).hexdigest())
            for key, prefix in (('windows_zip', prefix), ('source_zip', 'GM-Editor-source/')):
                with self.subTest(archive=key), zipfile.ZipFile(result[key]) as archive:
                    names = {name[len(prefix):] for name in archive.namelist()}
                    self.assertIn('docs/MARKDOWN_DATA_EXCHANGE.md', names)
                    self.assertEqual(archive.read(prefix + 'docs/MARKDOWN_DATA_EXCHANGE.md'), exchange_guide.encode())
                    for name in names:
                        if name.endswith('.md') or name == 'START_HERE.txt':
                            body = archive.read(prefix + name).decode('utf-8')
                            for link in re.findall(r'\[[^\]]*\]\(([^)]+)\)', body):
                                target = urlsplit(link)
                                if target.scheme or target.netloc or not target.path:
                                    continue
                                resolved = posixpath.normpath(posixpath.join(posixpath.dirname(name), unquote(target.path)))
                                self.assertIn(resolved, names, f'{name}: unresolved {link}')
    def test_existing_output_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory); output = base / 'output'; output.mkdir()
            sentinel = output / 'keep.txt'; sentinel.write_text('Existing work')
            with self.assertRaisesRegex(ValueError, 'new directory'):
                package.package(base / 'source', base / 'build', base / 'audit', output)
            self.assertEqual(sentinel.read_text(), 'Existing work')

    def test_archive_has_known_root_and_rejects_path_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory); source = base / 'input.txt'; source.write_text('checked bytes')
            package.archive(base / 'good.zip', {'docs/start.txt': source}, 'Preview')
            with zipfile.ZipFile(base / 'good.zip') as archive:
                self.assertEqual(archive.namelist(), ['Preview/docs/start.txt'])
                self.assertEqual(archive.read('Preview/docs/start.txt'), source.read_bytes())
            with self.assertRaisesRegex(ValueError, 'Unsafe archive path'):
                package.archive(base / 'bad.zip', {'../../secret.txt': source}, 'Preview')


if __name__ == '__main__':
    unittest.main()
