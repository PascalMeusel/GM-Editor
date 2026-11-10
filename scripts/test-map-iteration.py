"""Negative controls for local batch review; no renderer or Pillow required."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('iteration', Path(__file__).with_name('run-map-iteration.py'))
iteration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(iteration)


class ReviewGateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.batch = self.root/'batch-001'
        self.attempt = self.batch/'attempt-001'
        self.attempt.mkdir(parents=True)
        self.manifest = self.attempt/'manifest.json'
        iteration.write_json(self.manifest, dict(batch=1, automated_passed=True, cases=[{}]*100))
        self.review = dict(batch=1, engineering_review='accepted', manifest='attempt-001/manifest.json',
                           manifest_sha256=iteration.digest(self.manifest), open_issues=[])

    def save_review(self):
        iteration.write_json(self.batch/'review.json', self.review)

    def test_missing_review_blocks_advance(self):
        with self.assertRaises(FileNotFoundError):
            iteration.validate_previous(self.root, 2)

    def test_unresolved_issue_blocks_advance(self):
        self.review['open_issues'] = ['Disconnected bridge']
        self.save_review()
        with self.assertRaisesRegex(RuntimeError, 'unresolved'):
            iteration.validate_previous(self.root, 2)

    def test_changed_manifest_invalidates_review(self):
        self.save_review()
        self.manifest.write_text('{}', encoding='utf-8')
        with self.assertRaisesRegex(RuntimeError, 'immutable'):
            iteration.validate_previous(self.root, 2)

    def test_incomplete_batch_cannot_advance(self):
        iteration.write_json(self.manifest, dict(batch=1, automated_passed=True, cases=[{}]*99))
        self.review['manifest_sha256'] = iteration.digest(self.manifest)
        self.save_review()
        with self.assertRaisesRegex(RuntimeError, 'all 100'):
            iteration.validate_previous(self.root, 2)

    def test_valid_review_unlocks_exactly_next_batch(self):
        self.save_review()
        self.assertEqual(iteration.validate_previous(self.root, 2)['sha256'], iteration.digest(self.batch/'review.json'))
        with self.assertRaises(FileNotFoundError):
            iteration.validate_previous(self.root, 3)

    def test_corpus_has_no_dropped_or_repeated_seed(self):
        cases = [iteration.case_spec(index) for index in range(iteration.TOTAL_MAPS)]
        self.assertEqual(len({case['seed'] for case in cases}), 500)
        self.assertEqual(len({case['id'] for case in cases}), 500)
        self.assertEqual({case['kind'] for case in cases}, {'world','region','tactical'})
        self.assertEqual({case['locale'] for case in cases}, {'en','de'})


if __name__ == '__main__':
    unittest.main()
