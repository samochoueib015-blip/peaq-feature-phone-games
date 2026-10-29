"""Link the clean Emerald archive with the exported PEAQ platform sources.

All outputs are local to build/emerald-native. This does not qualify a phone
installation and does not bundle game content for redistribution.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
from pack_engine import pack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/emerald-native'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(toolchain):
    toolchain = toolchain.resolve() / 'bin'
    suffix = '.exe' if (toolchain / 'arm-none-eabi-gcc.exe').exists() else ''
    tool = lambda name: toolchain / ('arm-none-eabi-' + name + suffix)

    def run(command):
        result = subprocess.run([str(x) for x in command], capture_output=True)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))
        return result.stdout

    archive_dir = ROOT / 'build/emerald-module'
    archive_report = json.loads((archive_dir / 'build-report.json').read_text())
    if not archive_report['all_compiled'] or not archive_report['relocatable']:
        raise ValueError('Build the complete relocatable archive first')
    archive_input = archive_dir / 'libpokeemerald.a'
    if sha(archive_input) != archive_report['sha256']:
        raise ValueError('Archive differs from its build report')
    OUT.mkdir(parents=True, exist_ok=True)
    # The linker script gives game BSS the emulated IWRAM region by archive name.
    archive = OUT / 'libpokeemerald-cooperative.a'
    shutil.copyfile(archive_input, archive)
    flags = ['-mno-unaligned-access', '-mcpu=cortex-a7', '-mthumb', '-mfloat-abi=soft',
             '-O2', '-ffreestanding', '-fno-common', '-fpie', '-fvisibility=hidden',
             '-DPEAQ_EMERALD_RELOCATABLE=1']
    includes = [item for folder in ('ports/emerald', 'experiments/emerald', 'experiments/common')
                for item in ('-I', ROOT / folder)]
    sources = [
        ('ports/emerald/platform.c', 'platform', []),
        ('experiments/emerald/video_direct.c', 'video_direct', []),
        ('ports/emerald/ppu.c', 'ppu', []),
        ('experiments/emerald/landscape_fast.c', 'landscape_fast', []),
        ('experiments/doom/runtime.c', 'libc', ['-Dpeaq_doom_io_bind=peaq_emerald_io_bind',
                                             '-Dpeaq_doom_heap_used=peaq_emerald_heap_used']),
        ('ports/emerald/exports.c', 'exports', []),
        ('experiments/emerald/audio_clock.c', 'audio_clock', []),
    ]
    objects = []
    for source, name, defines in sources:
        obj = OUT / ('runtime-' + name + '.o')
        run([tool('gcc'), *flags, *includes, *defines, '-c', ROOT / source, '-o', obj])
        objects.append(obj)
    libraries = [Path(run([tool('gcc'), '-mcpu=arm7tdmi', '-mthumb', '-mfloat-abi=soft',
                          '-print-file-name=' + name]).decode().strip())
                 for name in ('libc.a', 'libm.a', 'libgcc.a')]
    if not all(path.is_file() for path in libraries):
        raise FileNotFoundError('The Arm GNU toolchain must include the Thumb Newlib multilib')
    elf = OUT / 'emerald-engine.elf'
    run([tool('gcc'), *flags, '-pie', '-Wl,--no-dynamic-linker,-Bsymbolic', '-nostdlib',
         '-T', ROOT / 'ports/emerald/emerald.ld', *objects, '-Wl,--whole-archive', archive,
         '-Wl,--no-whole-archive', '-Wl,--start-group', *libraries, '-Wl,--end-group', '-o', elf])
    unresolved = run([tool('nm'), '-u', elf]).decode().strip()
    if unresolved:
        raise ValueError(unresolved)
    run([tool('objcopy'), '--dump-section=.image=' + str(OUT / 'emerald-engine.bin'), elf])
    result = pack(elf, OUT / 'engine.pge', 'peaq_emerald_exports', 5)
    symbols = result.pop('symbols')
    (OUT / 'symbols.json').write_text(json.dumps(symbols, indent=2) + '\n')
    result.update(upstream_commit=archive_report['upstream_commit'], gameplay='vanilla',
                  language='English', archive_sha256=sha(archive),
                  source_sha256={source: sha(ROOT / source) for source, _, _ in sources},
                  library_sha256={path.name: sha(path) for path in libraries},
                  unresolved_symbols=[], engine_executed=False, hardware_tested=False)
    (OUT / 'build-report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain', type=Path, required=True)
    build(parser.parse_args().toolchain)
