"""Release plumbing tests. Mocked packaging tests do not claim a native build."""
import copy
import hashlib
import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile
from contextlib import redirect_stderr
from pathlib import Path
from unittest.mock import patch

os.environ['PYGAME_HIDE_SUPPORT_PROMPT'] = '1'
os.environ['SDL_VIDEODRIVER'] = 'dummy'
os.environ['SDL_AUDIODRIVER'] = 'dummy'

from main import main, positive_frames
from src.systems.resources import ROOT
from src.systems.self_test import run_self_test, write_report
from tools import build_release as release


def successful_report(frozen=False):
    return {'schema_version': 1, 'status': 'passed', 'frozen': frozen,
            'checks': [{'name': name, 'passed': True, 'detail': ''}
                       for name in sorted(release.REQUIRED_RUNTIME_CHECKS)]}


class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_runtime_self_test_passes_without_touching_personal_save(self):
        personal = ROOT/'saves/player_save.json'
        before = personal.read_bytes() if personal.exists() else None
        report = run_self_test()
        self.assertEqual(report['status'], 'passed', report.get('error'))
        self.assertFalse(report['frozen'])
        self.assertEqual(len(report['checks']), 26)
        self.assertTrue(all(check['passed'] for check in report['checks']))
        after = personal.read_bytes() if personal.exists() else None
        self.assertEqual(before, after)
        path = self.root/'nested folder'/'report.json'
        write_report(path, report)
        self.assertEqual(release.validate_runtime_report(path, False), report)
        self.assertFalse(path.with_suffix('.json.tmp').exists())

    def test_runtime_failure_is_reported(self):
        with patch('src.game.Game', side_effect=RuntimeError('simulated launch failure')):
            report = run_self_test()
        self.assertEqual(report['status'], 'failed')
        self.assertIn('simulated launch failure', report['error'])
        self.assertIn('RuntimeError', report['traceback'])

    def test_cli_self_test_from_unrelated_directory(self):
        report_path = self.root/'report with spaces.json'
        result = subprocess.run([sys.executable, str(ROOT/'main.py'), '--self-test', '--report', str(report_path)],
                                cwd=self.root, capture_output=True, text=True, timeout=90)
        self.assertEqual(result.returncode, 0, result.stderr+result.stdout)
        report = release.validate_runtime_report(report_path, False)
        self.assertEqual(json.loads(result.stdout), report)
        self.assertFalse((self.root/'saves').exists())

    def test_invalid_cli_combinations_exit_before_launch(self):
        for args in (['--smoke', '0'], ['--smoke', '-2'], ['--smoke', 'bad'],
                     ['--headless'], ['--report', 'unused.json'],
                     ['--self-test', '--smoke', '1'], ['--self-test', '--screenshot', 'unused.png']):
            with self.subTest(args=args), redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                main(args)
            self.assertEqual(error.exception.code, 2)
        self.assertEqual(positive_frames('15'), 15)

    def test_cli_failure_exit_code(self):
        failed = {'schema_version': 1, 'status': 'failed', 'checks': [], 'frozen': False}
        with patch('src.systems.self_test.run_self_test', return_value=failed), patch('sys.stdout', None):
            self.assertEqual(main(['--self-test', '--report', str(self.root/'failed.json')]), 1)
        self.assertEqual(json.loads((self.root/'failed.json').read_text()), failed)

    def test_windowed_stdout_none_still_writes_report(self):
        with patch('src.systems.self_test.run_self_test', return_value=successful_report()), patch('sys.stdout', None):
            result = main(['--self-test', '--report', str(self.root/'windowed.json')])
        self.assertEqual(result, 0)
        self.assertTrue((self.root/'windowed.json').exists())

    def test_unwritable_runtime_report_has_distinct_failure_code(self):
        with patch('src.systems.self_test.run_self_test', return_value=successful_report()), \
             patch('src.systems.self_test.write_report', side_effect=PermissionError('read-only')), \
             redirect_stderr(io.StringIO()):
            self.assertEqual(main(['--self-test', '--report', str(self.root/'denied.json')]), 2)


class BuildToolTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.output = self.root/'dist'
        self.report_path = self.root/'build-report.json'
        (self.root/'docs').mkdir()
        (self.root/'docs/DISTRIBUTION_README.txt').write_text('Unit-test fixture, not a release.')

    def tearDown(self):
        self.temp.cleanup()

    def test_preflight_rejects_unsupported_python_and_missing_modules(self):
        with patch.object(release.sys, 'version_info', (3, 11, 2)), \
             patch('importlib.util.find_spec', return_value=None):
            checks = release.preflight()
        checks = {row['name']: row for row in checks}
        self.assertFalse(checks['python_version']['passed'])
        self.assertFalse(checks['PyInstaller']['passed'])
        self.assertFalse(checks['python_shared_library']['passed'])

    def test_preflight_accepts_discovered_library(self):
        library = self.root/'libpython.mock'
        library.write_bytes(b'unit-test fixture')
        probe = subprocess.CompletedProcess([], 0, stdout=str(library)+'\n', stderr='')
        with patch.object(release.sys, 'version_info', (3, 12, 9)), \
             patch('importlib.util.find_spec', return_value=object()), \
             patch('subprocess.run', return_value=probe):
            checks = release.preflight()
        self.assertTrue(all(check['passed'] for check in checks))

    def test_library_probe_failure_is_not_a_builder_crash(self):
        for value in (subprocess.CompletedProcess([], 1, stdout='', stderr='library unavailable'),
                      subprocess.TimeoutExpired('probe', 30)):
            kwargs = {'side_effect': value} if isinstance(value, Exception) else {'return_value': value}
            with patch('importlib.util.find_spec', return_value=object()), patch('subprocess.run', **kwargs):
                checks = release.preflight()
            self.assertFalse(checks[-1]['passed'])

    def test_blocked_build_reports_failure_without_artifacts(self):
        failed = [{'name': 'python_version', 'passed': False, 'detail': 'upgrade Python'}]
        with patch.object(release, 'preflight', return_value=failed), patch.object(release, 'run_step') as run:
            report = release.build(output=self.output, report_path=self.report_path, root=self.root)
        self.assertEqual(report['status'], 'failed')
        run.assert_not_called()
        self.assertNotIn('artifacts', report)
        self.assertFalse(self.output.exists())
        self.assertEqual(json.loads(self.report_path.read_text()), report)

    def test_check_only_never_creates_a_release(self):
        with patch.object(release, 'preflight', return_value=[{'passed': True}]), patch.object(release, 'run_step') as run:
            report = release.build(True, self.output, self.report_path, self.root)
        self.assertEqual(report['status'], 'preflight_passed')
        self.assertNotIn('artifacts', report)
        run.assert_not_called()

    def test_runtime_report_validation_rejects_false_success(self):
        original = successful_report(True)
        bad_reports = [[], {}, {**original, 'frozen': False}, {**original, 'status': 'failed'},
                       {**original, 'checks': []}, {**original, 'schema_version': 2}, {**original, 'schema_version': True}]
        changed = copy.deepcopy(original)
        changed['checks'][0]['passed'] = 'true'
        bad_reports.append(changed)
        changed = copy.deepcopy(original)
        changed['checks'].append(copy.deepcopy(changed['checks'][0]))
        bad_reports.append(changed)
        for report in bad_reports:
            release.write_json(self.report_path, report)
            with self.assertRaises(ValueError):
                release.validate_runtime_report(self.report_path, True)
        release.write_json(self.report_path, original)
        self.assertEqual(release.validate_runtime_report(self.report_path, True), original)

    def test_sha256_and_json_write(self):
        path = self.root/'sample.bin'
        path.write_bytes(b'abc')
        self.assertEqual(release.sha256(path), hashlib.sha256(b'abc').hexdigest())
        release.write_json(self.report_path, {'ok': True})
        self.assertEqual(json.loads(self.report_path.read_text()), {'ok': True})
        self.assertFalse(self.report_path.with_suffix('.json.tmp').exists())

    def test_failed_or_timed_out_step_is_recorded(self):
        for value in (subprocess.CompletedProcess([], 5), subprocess.TimeoutExpired('command', 1)):
            report = {'steps': []}
            kwargs = {'side_effect': value} if isinstance(value, Exception) else {'return_value': value}
            with patch('subprocess.run', **kwargs), self.assertRaises(RuntimeError):
                release.run_step(report, 'example', ['unused'], self.root, self.root/'step.log')
            self.assertEqual(report['steps'][0]['status'], 'failed')
            self.assertTrue((self.root/'step.log').exists())

    def fake_step(self, report, name, command, cwd, log, timeout=300):
        """Only exercises orchestration; it never purports to compile an executable."""
        report['steps'].append({'name': name, 'status': 'passed'})
        if name == 'source_runtime':
            release.write_json(Path(command[-1]), successful_report(False))
        elif name == 'pyinstaller':
            output = Path(command[command.index('--distpath')+1])
            output.mkdir(parents=True, exist_ok=True)
            (output/('CardGame.exe' if sys.platform=='win32' else 'CardGame')).write_bytes(b'mock executable')
        elif name == 'packaged_runtime':
            self.assertNotEqual(Path(cwd), self.root)
            self.assertTrue(Path(cwd).is_dir())
            release.write_json(Path(command[-1]), successful_report(True))

    def test_pipeline_order_and_archive_integrity_with_mock_build(self):
        with patch.object(release, 'preflight', return_value=[{'passed': True}]), \
             patch.object(release, 'run_step', side_effect=self.fake_step), \
             patch('importlib.metadata.version', return_value='unit-test-only'):
            report = release.build(output=self.output, report_path=self.report_path, root=self.root)
        self.assertEqual(report['status'], 'passed', report.get('error'))
        self.assertEqual([step['name'] for step in report['steps']],
                         ['data_validation', 'unit_tests', 'source_runtime', 'pyinstaller', 'packaged_runtime'])
        artifacts = report['artifacts']
        binary = Path(artifacts['executable'])
        manifest = json.loads(Path(artifacts['manifest']).read_text())
        self.assertEqual(manifest['sha256'], release.sha256(binary))
        self.assertTrue(Path(artifacts['checksum']).read_text().startswith(release.sha256(artifacts['archive'])))
        with zipfile.ZipFile(artifacts['archive']) as bundle:
            self.assertEqual(set(bundle.namelist()), {binary.name, 'manifest.json', 'runtime-report.json', 'README.txt'})
            self.assertIsNone(bundle.testzip())
            self.assertEqual(hashlib.sha256(bundle.read(binary.name)).hexdigest(), manifest['sha256'])

    def test_stale_runtime_report_cannot_publish_an_unverified_binary(self):
        stale = self.root/'build/release-checks/packaged-runtime.json'
        release.write_json(stale, successful_report(True))

        def missing_report(report, name, command, cwd, log, timeout=300):
            if name != 'packaged_runtime':
                self.fake_step(report, name, command, cwd, log, timeout)
            else:
                report['steps'].append({'name': name, 'status': 'passed'})

        with patch.object(release, 'preflight', return_value=[{'passed': True}]), \
             patch.object(release, 'run_step', side_effect=missing_report), \
             patch.object(release, 'publish_release') as publish:
            report = release.build(output=self.output, report_path=self.report_path, root=self.root)
        self.assertEqual(report['status'], 'failed')
        self.assertNotIn('artifacts', report)
        publish.assert_not_called()
        self.assertFalse(stale.exists())

    def test_failed_frozen_report_prevents_publication(self):
        def wrong_target(report, name, command, cwd, log, timeout=300):
            self.fake_step(report, name, command, cwd, log, timeout)
            if name == 'packaged_runtime':
                release.write_json(Path(command[-1]), successful_report(False))

        with patch.object(release, 'preflight', return_value=[{'passed': True}]), \
             patch.object(release, 'run_step', side_effect=wrong_target), \
             patch.object(release, 'publish_release') as publish:
            report = release.build(output=self.output, report_path=self.report_path, root=self.root)
        self.assertEqual(report['status'], 'failed')
        publish.assert_not_called()


if __name__ == '__main__':
    unittest.main()
