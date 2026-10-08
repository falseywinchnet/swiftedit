"""Publish one immutable, versioned all-platform release after the native matrix passes."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess


def main() -> None:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--tag', required=True)
    arguments: argparse.Namespace = parser.parse_args()
    revision: str = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    dependencies: dict = json.loads(Path('ci/dependencies.json').read_text(encoding='utf-8'))
    tag: str = arguments.tag
    project: str = Path('CMakeLists.txt').read_text(encoding='utf-8')
    version: re.Match[str] | None = re.search(r'project\(SwiftEdit VERSION ([0-9]+\.[0-9]+\.[0-9]+)', project)
    if version is None or tag != 'v' + version.group(1):
        raise RuntimeError('Release tag must match the SwiftEdit CMake project version')
    tagged_revision: str = subprocess.check_output(['git', 'rev-parse', tag + '^{commit}'], text=True).strip()
    if tagged_revision != revision:
        raise RuntimeError('Release tag must point at the checked-out source revision')
    staging: Path = arguments.artifacts / 'publish'
    staging.mkdir()
    files: list[str] = []
    platform: str
    for platform in ['windows-x64', 'macos-arm64', 'linux-x64']:
        source: Path = arguments.artifacts / ('SwiftEdit-' + platform)
        suffix: str = '.tar.gz' if platform == 'linux-x64' else '.zip'
        name: str = 'SwiftEdit-' + platform + '-' + revision[:12] + suffix
        archive: Path = source / name
        sidecar: Path = source / (name + '.sha256')
        with archive.open('rb') as stream:
            actual: str = hashlib.file_digest(stream, 'sha256').hexdigest()
        if sidecar.read_text(encoding='utf-8').split()[0] != actual:
            raise RuntimeError('Archive digest mismatch: ' + platform)
        manifest_path: Path = source / ('manifest.json' if platform == 'macos-arm64' else platform + '-manifest.json')
        manifest: dict[str, object] = json.loads(manifest_path.read_text(encoding='utf-8'))
        if manifest['source_revision'] != revision:
            raise RuntimeError('Mixed source revisions in release: ' + platform)
        sdk: dict = manifest['sdk']
        if (sdk['provider_revision'] != dependencies['revision']
                or sdk['picker_revision'] != dependencies['picker_revision']
                or sdk['platform'] != platform):
            raise RuntimeError('Release dependencies differ from the reviewed source lock: ' + platform)
        for item in [archive, sidecar, manifest_path]:
            target: Path = staging / (platform + '-manifest.json' if item == manifest_path else item.name)
            shutil.copy2(item, target)
            files.append(str(target))
    notes: Path = staging / 'notes.md'
    changes_path: Path = Path('docs/releases') / (tag + '.md')
    changes: str = changes_path.read_text(encoding='utf-8') + '\n\n' if changes_path.is_file() else ''
    notes.write_text(
        'SwiftEdit ' + tag + ' for Windows, macOS and Linux, from commit `' + revision + '`.\n\n'
        + changes +
        'All three native build/test jobs and packaged startup checks passed before publication. '
        'Download the archive for your platform and extract it completely.\n\n'
        '- **macOS:** Apple silicon, built on macOS 15 with the GUI.Forms macOS 14 runtime contract. Ad-hoc signed; not notarized.\n'
        '- **Windows:** x64 portable ZIP, with runtime DLLs and fonts. Unsigned.\n'
        '- **Linux:** x64 portable tarball, Ubuntu 24.04-compatible runtime and X11 session. Run `SwiftEdit`.\n\n'
        'Each archive includes the GUI, terminal editor and command-session CLI. '
        'SHA-256 sidecars and source/SDK manifests accompany the downloads.\n\n'
        'The expanded feature set is still in development. The GUI currently uses its older '
        '1 MiB UTF-8 / 4096-byte line path; large-file operations are available in the terminal. '
        'The owner reports that the blank-window Mac CPU issue appears resolved. See the repository objective ledger '
        'for remaining GUI, print and responsiveness work.\n', encoding='utf-8')
    observed: subprocess.CompletedProcess[str] = subprocess.run(
        ['gh', 'release', 'view', tag, '--json', 'isDraft,targetCommitish,assets'],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if observed.returncode == 0:
        existing: dict[str, object] = json.loads(observed.stdout)
        if existing['targetCommitish'] != revision:
            raise RuntimeError('Existing release points at another commit')
        if not existing['isDraft']:
            raise RuntimeError('This immutable release is already published; do not replace its assets')
    else:
        subprocess.run(['gh', 'release', 'create', tag, '--target', revision, '--draft',
                        '--title', 'SwiftEdit ' + tag + ' — Windows, macOS and Linux',
                        '--notes-file', str(notes)], check=True)
    subprocess.run(['gh', 'release', 'upload', tag, '--clobber'] + files, check=True)
    uploaded_text: str = subprocess.check_output(['gh', 'release', 'view', tag, '--json', 'assets'], text=True)
    uploaded: dict[str, object] = json.loads(uploaded_text)
    expected: dict[str, str] = {}
    filename: str
    for filename in files:
        with Path(filename).open('rb') as stream:
            expected[Path(filename).name] = 'sha256:' + hashlib.file_digest(stream, 'sha256').hexdigest()
    asset: dict[str, object]
    for asset in uploaded['assets']:
        name = str(asset['name'])
        if name not in expected or asset.get('digest') != expected[name]:
            raise RuntimeError('Uploaded release asset mismatch: ' + name)
        del expected[name]
    if expected:
        raise RuntimeError('Release assets are incomplete')
    subprocess.run(['gh', 'release', 'edit', tag, '--draft=false'], check=True)


if __name__ == '__main__':
    main()
