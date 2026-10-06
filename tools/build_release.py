"""Build, verify and package a native Windows/Linux release with the current Python.

No cross-compilation. A successful build requires Python 3.12+, a discoverable
shared Python library and the dependencies in requirements-build.txt.
"""
import argparse
import hashlib
import importlib.metadata
import importlib.util
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED_RUNTIME_CHECKS = frozenset({
    'bundled_data', 'isolated_save', 'font_fallback', 'missing_image_fallback',
    'bundled_sound_effects', 'music_menu', 'music_battle', 'all_screens',
    'menu_play_event', 'start_match_event', 'first_round_event', 'checkpoint_restore',
    'no_reward_replay', 'quick_match_complete', 'classic_match_complete',
    'practice_no_rewards', 'journal', 'pack_purchase', 'achievement_claim',
    'duplicate_claim_blocked', 'card_upgrade', 'deck_workshop', 'resolution_1280',
    'resolution_1920', 'save_write', 'save_reload',
})


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix+'.tmp')
    temp.write_text(json.dumps(data, indent=2, allow_nan=False)+'\n', encoding='utf-8')
    temp.replace(path)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as handle:
        for block in iter(lambda: handle.read(1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


def preflight():
    checks = []

    def add(name, passed, detail):
        checks.append({'name': name, 'passed': bool(passed), 'detail': detail})

    add('python_version', sys.version_info >= (3, 12),
        f'Python {platform.python_version()}; install Python 3.12+ to build a supported release.')
    add('host_platform', sys.platform in ('win32', 'linux'),
        'This release builder targets its current Windows/Linux host, not another OS.')
    for module in ('pygame', 'PyInstaller'):
        add(module, importlib.util.find_spec(module) is not None,
            'Install build dependencies: python -m pip install -r requirements-build.txt')
    if checks[-1]['passed']:
        try:
            probe = subprocess.run([sys.executable, '-c',
                'from PyInstaller.depend.bindepend import get_python_library_path; print(get_python_library_path())'],
                text=True, capture_output=True, timeout=30)
            library = probe.stdout.strip().splitlines()[-1] if probe.stdout.strip() else ''
            found = probe.returncode == 0 and bool(library) and Path(library).is_file()
            detail = library if found else (
                'Shared Python library unavailable. Use a standard python.org Windows installation, '
                'a setup-python CI runtime, or install the matching libpython package on Linux. '
                + probe.stderr[-1200:])
            add('python_shared_library', found, detail)
        except (OSError, subprocess.TimeoutExpired) as error:
            add('python_shared_library', False, str(error))
    else:
        add('python_shared_library', False, 'Install PyInstaller first to check its Python-library discovery.')
    return checks


def validate_runtime_report(path, frozen):
    with Path(path).open(encoding='utf-8') as handle:
        report = json.load(handle)
    if (not isinstance(report, dict) or type(report.get('schema_version')) is not int
            or report['schema_version'] != 1):
        raise ValueError('Unsupported runtime report')
    if report.get('status') != 'passed' or report.get('frozen') is not frozen:
        raise ValueError('Runtime check failed or ran the wrong (source/frozen) target')
    checks = report.get('checks')
    if not isinstance(checks, list) or not all(isinstance(c, dict) and c.get('passed') is True for c in checks):
        raise ValueError('Runtime report contains failed or malformed checks')
    names = [check.get('name') for check in checks]
    if not all(isinstance(name, str) for name in names):
        raise ValueError('Runtime check names must be strings')
    if len(names) != len(set(names)) or not REQUIRED_RUNTIME_CHECKS.issubset(names):
        raise ValueError('Runtime report is incomplete or contains duplicate checks')
    return report


def run_step(report, name, command, cwd, log, timeout=300):
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    entry = {'name': name, 'status': 'running', 'command': [str(arg) for arg in command], 'log': str(log)}
    report['steps'].append(entry)
    env = {**os.environ, 'PYGAME_HIDE_SUPPORT_PROMPT': '1'}
    try:
        with log.open('wb') as output:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=timeout)
        entry['exit_code'] = result.returncode
        entry['status'] = 'passed' if result.returncode == 0 else 'failed'
        if result.returncode:
            raise RuntimeError(f'{name} failed (exit {result.returncode}); see {log}')
    except (OSError, subprocess.TimeoutExpired) as error:
        entry['status'] = 'failed'
        entry['error'] = str(error)
        raise RuntimeError(f'{name} could not finish: {error}') from error


def publish_release(binary, runtime_report, output, root=ROOT):
    """Only called after the executable's own runtime checks have passed."""
    binary, output = Path(binary), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    manifest = {'schema_version': 1, 'created_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
                'platform': platform.system(), 'architecture': platform.machine(),
                'python': platform.python_version(), 'pygame': importlib.metadata.version('pygame'),
                'pyinstaller': importlib.metadata.version('pyinstaller'),
                'executable': binary.name, 'size_bytes': binary.stat().st_size, 'sha256': sha256(binary),
                'runtime_status': runtime_report['status'], 'runtime_checks': len(runtime_report['checks'])}
    stem = f'CardGame-{platform.system()}-{platform.machine()}'
    # Assemble a complete delivery ZIP before replacing a previous ZIP.
    with tempfile.TemporaryDirectory(prefix='arcana-publish-', dir=output) as temporary:
        stage = Path(temporary)
        shutil.copy2(binary, stage/binary.name)
        shutil.copy2(root/'docs/DISTRIBUTION_README.txt', stage/'README.txt')
        write_json(stage/'manifest.json', manifest)
        write_json(stage/'runtime-report.json', runtime_report)
        with zipfile.ZipFile(stage/(stem+'.zip'), 'w', compression=zipfile.ZIP_DEFLATED) as bundle:
            for path in (stage/binary.name, stage/'README.txt', stage/'manifest.json', stage/'runtime-report.json'):
                bundle.write(path, arcname=path.name)
        for path in (stage/binary.name, stage/'manifest.json', stage/'runtime-report.json', stage/(stem+'.zip')):
            path.replace(output/path.name)
    archive = output/(stem+'.zip')
    checksum = output/(stem+'.zip.sha256')
    checksum.write_text(sha256(archive)+'  '+archive.name+'\n', encoding='utf-8')
    return {'executable': str(output/binary.name), 'manifest': str(output/'manifest.json'),
            'archive': str(archive), 'checksum': str(checksum), 'sha256': manifest['sha256']}


def build(check_only=False, output=None, report_path=None, root=ROOT):
    root = Path(root)
    output = Path(output or root/'dist').resolve()
    report_path = Path(report_path or root/'build/release-report.json').resolve()
    report = {'schema_version': 1, 'status': 'failed', 'python': platform.python_version(),
              'platform': platform.system(), 'preflight': [], 'steps': []}
    try:
        report['preflight'] = preflight()
        if not all(item['passed'] for item in report['preflight']):
            raise RuntimeError('Build prerequisites are missing; inspect the preflight checks.')
        if check_only:
            report['status'] = 'preflight_passed'
        else:
            logs = root/'build/release-checks'
            logs.mkdir(parents=True, exist_ok=True)
            run_step(report, 'data_validation', [sys.executable, 'tools/validate_data.py', '--json'],
                     root, logs/'data.log')
            run_step(report, 'unit_tests', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-q'],
                     root, logs/'tests.log', timeout=600)
            source_report = logs/'source-runtime.json'
            source_report.unlink(missing_ok=True)
            run_step(report, 'source_runtime', [sys.executable, str(root/'main.py'), '--self-test',
                     '--report', str(source_report)], root, logs/'source-runtime.log')
            validate_runtime_report(source_report, frozen=False)
            stage = root/'build/release-stage'
            stage.mkdir(parents=True, exist_ok=True)
            exe_name = 'CardGame.exe' if sys.platform == 'win32' else 'CardGame'
            binary = stage/exe_name
            binary.unlink(missing_ok=True)
            run_step(report, 'pyinstaller', [sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean',
                     '--distpath', str(stage), '--workpath', str(root/'build/pyinstaller'), str(root/'CardGame.spec')],
                     root, logs/'pyinstaller.log', timeout=1200)
            if not binary.is_file():
                raise RuntimeError('PyInstaller did not create the expected executable')
            frozen_report = logs/'packaged-runtime.json'
            frozen_report.unlink(missing_ok=True)
            # A temporary cwd proves that the binary does not depend on the source directory.
            with tempfile.TemporaryDirectory(prefix='arcana-exe-check-') as working:
                run_step(report, 'packaged_runtime', [str(binary), '--self-test', '--report', str(frozen_report)],
                         working, logs/'packaged-runtime.log')
            runtime_report = validate_runtime_report(frozen_report, frozen=True)
            report['artifacts'] = publish_release(binary, runtime_report, output, root)
            report['status'] = 'passed'
    except (OSError, ValueError, RuntimeError, importlib.metadata.PackageNotFoundError) as error:
        report['error'] = str(error)
    write_json(report_path, report)
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Check prerequisites only; no build or artifact')
    parser.add_argument('--output', type=Path, help='Release output directory (default: dist/)')
    parser.add_argument('--report', type=Path, help='Build report path (default: build/release-report.json)')
    args = parser.parse_args(argv)
    try:
        report = build(args.check, args.output, args.report)
    except OSError as error:
        print(f'Cannot write build report: {error}', file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2))
    return 0 if report['status'] in ('passed', 'preflight_passed') else 1


if __name__ == '__main__':
    raise SystemExit(main())
