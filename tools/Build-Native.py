"""Build native SwiftEdit against hash-pinned installed public SDK archives."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import BinaryIO, Protocol, TextIO, TypedDict
import zipfile


class Digest(Protocol):
    def update(self, data: bytes) -> None: ...
    def hexdigest(self) -> str: ...


class PinnedArchive(TypedDict):
    name: str
    sha256: str


class SdkLock(TypedDict):
    release: str
    provider_revision: str
    archives: dict[str, PinnedArchive]


def run(arguments: list[str]) -> None:
    print('+ ' + ' '.join(arguments), flush=True)
    subprocess.run(arguments, check=True)


def download_sdk(arguments: list[str]) -> None:
    # Retry only the read-only download; compilation and checksum failures must
    # remain failures. --clobber replaces any partial archive before validation.
    attempt: int
    for attempt in range(3):
        try:
            run(arguments)
            return
        except subprocess.CalledProcessError:
            if attempt == 2:
                raise
            delay: int = 2 * (attempt + 1)
            print(f'SDK download failed; retrying in {delay} seconds.', flush=True)
            time.sleep(delay)


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


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['macos-arm64', 'windows-x64', 'linux-x64'], required=True)
    arguments: argparse.Namespace = parser.parse_args()
    lock: SdkLock = json.loads(Path('ci/native-sdk-lock.json').read_text(encoding='utf-8'))
    pinned: PinnedArchive = lock['archives'][arguments.platform]
    build: Path = Path('.build/native-' + arguments.platform).resolve()
    build.mkdir(parents=True, exist_ok=True)
    download: Path = build / 'download'
    download.mkdir(exist_ok=True)
    download_sdk(['gh', 'release', 'download', str(lock['release']), '--repo', 'falseywinchnet/swiftedit',
         '--pattern', pinned['name'], '--dir', str(download), '--clobber'])
    archive: Path = download / pinned['name']
    if sha256(archive) != pinned['sha256']:
        raise RuntimeError('SDK archive hash differs from reviewed dependency lock')
    sdk: Path = build / 'sdk'
    sdk.mkdir()  # Fail on stale SDK state; every CI run has a fresh checkout.
    package: zipfile.ZipFile
    with zipfile.ZipFile(archive) as package:
        name: str
        for name in package.namelist():
            destination: Path = (sdk / name).resolve()
            if not destination.is_relative_to(sdk):
                raise RuntimeError('SDK archive path escapes destination')
        package.extractall(sdk)
    manifest: dict[str, object] = json.loads((sdk / 'manifest.json').read_text(encoding='utf-8'))
    if manifest['provider_revision'] != lock['provider_revision'] or manifest['platform'] != arguments.platform:
        raise RuntimeError('SDK platform or revision differs from dependency lock')
    gui: Path = sdk / 'gui-forms-sdk'
    os.environ['GUI_FORMS_FONT_DIR'] = str(gui / 'share/GUIForms/fonts')
    os.environ['PATH'] = str(gui / 'bin') + os.pathsep + os.environ['PATH']
    prefix: str = str(gui) + ';' + str(sdk / 'picker-sdk')
    run(['cmake', '-S', '.', '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_PREFIX_PATH=' + prefix, '-DNOTEPAD_NATIVE_TESTS=ON'])
    run(['cmake', '--build', str(build), '--parallel', '2'])
    run(['ctest', '--test-dir', str(build), '--output-on-failure', '--timeout', '60'])
    benchmark: Path = build / ('swiftedit-terminal-end-bench.exe' if os.name == 'nt'
                               else 'swiftedit-terminal-end-bench')
    evidence: Path = build / 'terminal-end-evidence'
    evidence.mkdir()
    summary: TextIO
    with (evidence / 'summary.txt').open('w', encoding='utf-8') as summary:
        measured: subprocess.CompletedProcess[bytes] = subprocess.run(
            [str(benchmark), str(evidence / 'fixtures')], check=False, timeout=120,
            stdout=summary, stderr=subprocess.STDOUT)
    measured.check_returncode()
    revision_output: str = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True)
    revision: str = revision_output.strip()
    receipt: dict[str, object] = {
        'source_revision': revision,
        'provider_revision': lock['provider_revision'], 'platform': arguments.platform,
        'executable_sha256': sha256(benchmark), 'status': 'completed',
        'scope': 'headless read-only terminal end scan; not native input-to-screen latency',
        'fixture_bytes': 16777216, 'viewport': [80, 24], 'trials_per_fixture': 5,
        'percentiles': 'nearest rank; per-step samples pooled across trials',
        'clock_overhead': 'included', 'cache_state': 'not controlled; trials run sequentially',
        'performance_threshold': 'none'}
    (evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    if arguments.platform == 'macos-arm64':
        run([sys.executable, '-B', 'tools/Package-Mac.py', '--build', str(build), '--sdk', str(sdk)])


if __name__ == '__main__':
    main()
