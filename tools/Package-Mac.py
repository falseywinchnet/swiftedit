"""Stage, resolve, sign and archive the tested macOS SwiftEdit application."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import time
from typing import BinaryIO, Protocol


class Digest(Protocol):
    def update(self, data: bytes) -> None: ...
    def hexdigest(self) -> str: ...


def run(arguments: list[str]) -> str:
    result: subprocess.CompletedProcess[str] = subprocess.run(
        arguments, check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout, end='')
    result.check_returncode()
    return result.stdout


def sha256(path: Path) -> str:
    digest: Digest = hashlib.sha256()
    stream: BinaryIO
    with path.open('rb') as stream:
        data: bytes = stream.read(1024 * 1024)
        while data:
            digest.update(data)
            data = stream.read(1024 * 1024)
    result: str = digest.hexdigest()
    return result


def verify_load_paths(binary: Path, executable: Path, app: Path) -> None:
    listing: str = run(['otool', '-L', str(binary)])
    identity_listing: str = run(['otool', '-D', str(binary)])
    identities: list[str] = identity_listing.splitlines()[1:]
    line: str
    for line in listing.splitlines()[1:]:
        dependency: str = line.strip().split(' (', 1)[0]
        if dependency in identities or dependency.startswith(('/System/', '/usr/lib/')):
            continue
        expanded: str = dependency.replace('@loader_path', str(binary.parent))
        expanded = expanded.replace('@executable_path', str(executable.parent))
        resolved: Path = Path(expanded).resolve()
        if '@' in expanded or not resolved.is_relative_to(app) or not resolved.is_file():
            raise RuntimeError('Unresolved external runtime dependency: ' + dependency)


def cpu_seconds(value: str) -> float:
    fields: list[str] = value.strip().split(':')
    if len(fields) < 2 or len(fields) > 3:
        raise RuntimeError('Unexpected process CPU time: ' + value)
    result: float = 0.0
    field: str
    for field in fields:
        result = result * 60.0 + float(field)
    return result


def read_cpu_seconds(pid: int) -> float:
    sample: str = run(['/bin/ps', '-p', str(pid), '-o', 'time='])
    result: float = cpu_seconds(sample)
    return result


def wait_alive(child: subprocess.Popen[bytes], seconds: int, log: Path) -> None:
    try:
        child.wait(timeout=seconds)
    except subprocess.TimeoutExpired:
        return
    raise RuntimeError('Packaged app exited during observation; inspect ' + str(log))


def verify_startup(executable: Path, stage: Path) -> dict[str, object]:
    """Own one child and a disposable fixture; always reap the child on exit."""
    fixture: Path = stage / 'startup-fixture.txt'
    fixture.write_text('', encoding='utf-8')
    log: Path = stage / 'startup.log'
    environment: dict[str, str] = dict(os.environ)
    variable: str
    for variable in ['GUI_FORMS_FONT_DIR', 'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH']:
        environment.pop(variable, None)
    stream: BinaryIO
    with log.open('wb') as stream:
        child: subprocess.Popen[bytes] = subprocess.Popen(
            [str(executable), str(fixture)], cwd=stage, env=environment,
            stdout=stream, stderr=subprocess.STDOUT)
        try:
            wait_alive(child, 5, log)
            before: float = read_cpu_seconds(child.pid)
            started: float = time.monotonic()
            wait_alive(child, 10, log)
            after: float = read_cpu_seconds(child.pid)
            elapsed: float = time.monotonic() - started
            consumed: float = after - before
            if consumed < 0:
                raise RuntimeError('Process CPU time moved backwards')
            observation: dict[str, object] = {
                'warmup_seconds': 5, 'elapsed_seconds': elapsed,
                'process_cpu_seconds': consumed,
                'percent_of_one_core': 100.0 * consumed / elapsed,
                'scope': 'ten quiet seconds with an empty text fixture on the CI runner; '
                         'focus and occlusion not controlled; no idle-performance pass threshold'}
            print('Packaged idle CPU: ' + json.dumps(observation))
            return observation
        finally:
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    arguments: argparse.Namespace = parser.parse_args()
    if platform.system() != 'Darwin':
        raise RuntimeError('This packager requires a native macOS runner')
    build: Path = arguments.build.resolve(strict=True)
    sdk: Path = arguments.sdk.resolve(strict=True)
    revision: str = run(['git', 'rev-parse', 'HEAD']).strip()
    status: str = run(['git', 'status', '--porcelain'])
    if status:
        raise RuntimeError('Dogfood archives require a clean checkout')
    output: Path = build / 'dist'
    output.mkdir(exist_ok=True)
    stage: Path = Path(tempfile.mkdtemp(prefix='mac-package-', dir=build))
    app: Path = stage / 'SwiftEdit.app'
    shutil.copytree(build / 'SwiftEdit.app', app, symlinks=True)
    resources: Path = app / 'Contents/Resources'
    notices: Path = resources / 'licenses'
    shutil.copytree(sdk / 'gui-forms-sdk/share/licenses', notices, dirs_exist_ok=True)
    shutil.copytree(sdk / 'picker-sdk/share/licenses', notices, dirs_exist_ok=True)
    shutil.copy2('third_party/md4c/LICENSE.md', notices / 'MD4C.md')
    shutil.copy2('third_party/unicode/LICENSE.txt', notices / 'Unicode.txt')
    fixup: Path = stage / 'fixup.cmake'
    fixup.write_text('include(BundleUtilities)\nfixup_bundle("' + app.as_posix() +
                     '" "" "' + (sdk / 'gui-forms-sdk/lib').as_posix() + '")\n', encoding='utf-8')
    run(['cmake', '-P', str(fixup)])
    executable: Path = app / 'Contents/MacOS/SwiftEdit'
    verify_load_paths(executable, executable, app)
    frameworks: Path = app / 'Contents/Frameworks'
    library: Path
    for library in frameworks.iterdir():
        if library.is_file() and not library.is_symlink():
            verify_load_paths(library, executable, app)
            run(['codesign', '--force', '--sign', '-', str(library)])
    run(['codesign', '--force', '--deep', '--sign', '-', str(app)])
    run(['codesign', '--verify', '--deep', '--strict', str(app)])
    idle_cpu: dict[str, object] = verify_startup(executable, stage)
    manifest: dict[str, object] = {
        'product': 'SwiftEdit', 'source_revision': revision,
        'platform': platform.platform(), 'architecture': platform.machine(),
        'signature': 'ad-hoc; not Developer ID signed or notarized',
        'packaged_startup': 'remained running for five seconds on a disposable text fixture',
        'idle_cpu_observation': idle_cpu,
        'sdk': json.loads((sdk / 'manifest.json').read_text(encoding='utf-8')),
        'executable_sha256': sha256(executable)}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    (output / 'README.txt').write_text(
        'SwiftEdit development build for Apple silicon\n\n'
        'Extract the ZIP, then open SwiftEdit.app. The app includes its required private '
        'libraries, fonts and license notices. This build is ad-hoc signed, not notarized.\n'
        'Built and tested on macOS 26; Intel Macs and older macOS versions are not validated.\n\n'
        'This is an early dogfood build. The expanded feature set is still in development. '
        'The GUI currently uses its older editor path, limited to 1 MiB of UTF-8 text '
        'and 4096 UTF-8 bytes per logical line; '
        'the newer large-file session and terminal work are not yet integrated into the GUI. '
        'Use copies of important documents while testing.\n'
        'See manifest.json for the exact source and dependency revisions.\n', encoding='utf-8')
    archive: Path = output / ('SwiftEdit-macos-arm64-' + revision[:12] + '.zip')
    run(['ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', str(app), str(archive)])
    checksum: str = sha256(archive)
    archive.with_suffix('.zip.sha256').write_text(checksum + '  ' + archive.name + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
