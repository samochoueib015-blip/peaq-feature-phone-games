"""Create an isolated, original-gameplay source copy and apply platform edits.

The supplied upstream checkout is read only. Its working-tree changes and
untracked files are not copied. Game content remains local under build/.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PIN = '3c35e5b957dff6c578878031e0c4a7c8d7691401'
OUT = ROOT / 'build/emerald-source'
PATCHES = ROOT / 'ports/emerald/platform-patches.json'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify_prepared_source(source):
    source = source.resolve()
    manifest = json.loads((source / 'peaq-platform.json').read_text(encoding='utf-8'))
    commit = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != PIN or manifest.get('upstream_commit') != PIN or manifest.get('gameplay') != 'vanilla':
        raise ValueError('Prepared source revision/configuration mismatch')
    if manifest['patches_sha256'] != digest(PATCHES.read_bytes()):
        raise ValueError('Prepared source uses a different platform patch set')
    patches = json.loads(PATCHES.read_text(encoding='utf-8'))['files']
    expected = {patch['path']: patch['result_sha256'] for patch in patches}
    if manifest['patched_files'] != expected:
        raise ValueError('Prepared source manifest differs from the published patch set')
    for name, checksum in expected.items():
        if digest((source / name).read_text(encoding='utf-8').encode()) != checksum:
            raise ValueError('Prepared platform source changed: ' + name)
    changed = set(subprocess.check_output(['git', '-C', str(source), 'diff', '--name-only', 'HEAD'], text=True).splitlines())
    if changed - expected.keys():
        raise ValueError('Unexpected tracked source changes: ' + ', '.join(sorted(changed - expected.keys())))
    memory = (source / 'include/gba/peaq_memory_map.h').read_bytes()
    if digest(memory) != manifest['memory_header_sha256'] or memory != (ROOT / 'experiments/emerald/memory_map.h').read_bytes():
        raise ValueError('Prepared memory mapping changed')
    return manifest


def prepare(upstream):
    upstream = upstream.resolve()
    commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != PIN:
        raise ValueError(f'Expected upstream {PIN}, got {commit}')
    if OUT.exists():
        raise FileExistsError(f'{OUT} already exists; use a new publication checkout for a clean preparation')
    payload = json.loads(PATCHES.read_text(encoding='utf-8'))
    if payload['upstream_commit'] != PIN or payload['gameplay'] != 'vanilla':
        raise ValueError('Unexpected platform patch configuration')
    OUT.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', '-c', 'core.autocrlf=false', 'clone', '--no-hardlinks', '--no-checkout',
                    '--', str(upstream), str(OUT)], check=True)
    subprocess.run(['git', '-C', str(OUT), 'config', 'core.autocrlf', 'false'], check=True)
    subprocess.run(['git', '-C', str(OUT), 'checkout', '--detach', PIN], check=True)
    records = {}
    for patch in payload['files']:
        relative = Path(patch['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Patch path must stay inside the prepared tree')
        path = OUT / relative
        original = path.read_text(encoding='utf-8')
        if digest(original.encode()) != patch['source_sha256']:
            raise ValueError(f'Original source mismatch: {relative}')
        lines = original.splitlines(keepends=True)
        previous = len(lines) + 1
        for edit in reversed(patch['edits']):
            start, end = edit['start'], edit['end']
            if not 0 <= start <= end < previous:
                raise ValueError(f'Overlapping or invalid edits: {relative}')
            lines[start:end] = [edit['replacement']]
            previous = start
        result = ''.join(lines)
        if digest(result.encode()) != patch['result_sha256']:
            raise ValueError(f'Patched source mismatch: {relative}')
        path.write_text(result, encoding='utf-8', newline='\n')
        records[relative.as_posix()] = patch['result_sha256']
    memory = (ROOT / 'experiments/emerald/memory_map.h').read_bytes()
    (OUT / 'include/gba/peaq_memory_map.h').write_bytes(memory)
    manifest = {'upstream_commit': PIN, 'gameplay': 'vanilla', 'language': 'English',
                'patches_sha256': digest(PATCHES.read_bytes()), 'patched_files': records,
                'memory_header_sha256': digest(memory), 'personal_saves_included': False,
                'hardware_tested': False}
    (OUT / 'peaq-platform.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    verify_prepared_source(OUT)
    print(json.dumps({'prepared': str(OUT), 'platform_files': len(records), 'gameplay': 'vanilla'}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--verify', action='store_true', help='Verify an existing prepared source instead of creating one')
    args = parser.parse_args()
    if args.verify:
        result = verify_prepared_source(args.upstream)
        print(json.dumps({'verified': True, 'upstream_commit': result['upstream_commit'], 'gameplay': result['gameplay']}))
    else:
        prepare(args.upstream)
