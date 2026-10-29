"""Compile the standalone Emerald runtime and bind it to a local engine.

The SD identity is an explicit local input because this runtime validates CID
and capacity at startup. No card or firmware is accessed by this builder.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zlib
from build_game import fnv
from toolchain import tool
from sd_profile import load_sd_profile, sd_profile_sha256, sd_profile_header

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/emerald-runtime'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def define(text, name, value):
    text, count = re.subn(r'(#define ' + name + r' )\d+u', lambda m: m[1] + str(value) + 'u', text)
    if count != 1:
        raise ValueError('Missing or duplicate manifest field: ' + name)
    return text


def build(engine_dir, sd_profile_path):
    profile = load_sd_profile(sd_profile_path)
    card_sectors = profile['capacity_sectors']
    engine_dir = engine_dir.resolve()
    engine = (engine_dir / 'engine.pge').read_bytes()
    report = json.loads((engine_dir / 'build-report.json').read_text())
    if hashlib.sha256(engine).hexdigest() != report['sha256'] or report['gameplay'] != 'vanilla':
        raise ValueError('Engine does not match its vanilla build report')
    header = struct.unpack_from('<8I', engine)
    if header[0] != 0x31454750 or header[1] not in (1, 2) or header[5] != 5:
        raise ValueError('Unexpected engine format')
    symbols = json.loads((engine_dir / 'symbols.json').read_text())
    OUT.mkdir(parents=True, exist_ok=True)
    source = OUT / 'sources'
    shutil.copytree(ROOT / 'ports/emerald/runtime', source, dirs_exist_ok=True)
    for path in source.glob('*/peaq_save_probe_profile.h'):
        path.write_text(sd_profile_header(profile), encoding='utf-8', newline='\n')
    allocation = (header[3] + 62) & ~31
    arena = allocation + 76800 + 153600 + 38400 + 4096
    values = {'FILE_BYTES': len(engine), 'FILE_CRC': zlib.crc32(engine),
              'IMAGE_BYTES': header[2], 'MEMORY_BYTES': header[3], 'RELOCATIONS': header[6],
              'ALLOCATION': allocation, 'CARD_SECTORS': card_sectors,
              'VIDEO_RVA': symbols['peaq_video'], 'AUDIO_CLOCK_RVA': symbols['peaq_audio_clock'],
              'AUDIO_SERVICE_RVA': symbols['peaq_emerald_audio_service'] | 1,
              'AUDIO_GUARD_RVA': symbols['peaq_audio_guard'],
              'DIRECT_VIDEO_RVA': symbols['peaq_host_video_direct'],
              'TUTORIAL_RVA': symbols['peaq_emerald_tutorial']}
    exports = struct.unpack_from('<5I', engine, 32 + header[4])
    for path in source.glob('*/engine_load_manifest.h'):
        text = path.read_text()
        for name, value in values.items():
            macro = 'PEAQ_ENGINE_' + name
            # Earlier translation units do not consume every newer field.
            if '#define ' + macro + ' ' in text:
                text = define(text, macro, value)
        text = re.sub(r'(peaq_engine_header\[32\]=\{)[^}]+',
                      lambda m: m[1] + ','.join(map(str, engine[:32])), text)
        text = re.sub(r'(peaq_engine_export_rva\[5\]=\{)[^}]+',
                      lambda m: m[1] + ','.join(str(x) + 'u' for x in exports), text)
        path.write_text(text, encoding='utf-8', newline='\n')
    for path in source.glob('*/intro_alignment.h'):
        path.write_text(define(path.read_text(), 'PEAQ_INTRO_ARENA_BYTES', arena),
                        encoding='utf-8', newline='\n')
    units = json.loads((ROOT / 'ports/emerald/runtime/units.json').read_text())
    flags = ['--target=arm-none-eabi', '-mcpu=cortex-a7', '-mthumb', '-mfloat-abi=soft',
             '-mno-unaligned-access', '-Oz', '-ffreestanding', '-fno-builtin', '-fno-PIE',
             '-fno-strict-aliasing', '-ffunction-sections', '-fdata-sections', '-fno-unwind-tables',
             '-fno-asynchronous-unwind-tables', '-Wall', '-Wextra', '-Werror',
             '-DFDL2=1', '-DCHIP=0', '-DWITH_EFUSE=0']
    includes = [item for folder in ('experiments/emerald', 'sdk', 'experiments/runtime', 'experiments/emerald/include')
                for item in ('-I', ROOT / folder)]
    objects = []

    def run(command):
        result = subprocess.run([str(x) for x in command], capture_output=True)
        if result.returncode:
            raise RuntimeError((result.stdout + result.stderr).decode(errors='replace'))
        return result.stdout.decode(errors='replace')

    for unit in units:
        relative = Path(unit['source']).relative_to('ports/emerald/runtime')
        path = source / relative
        obj = OUT / (unit['object'] + '.o')
        assembly = ['-Wno-unused-command-line-argument'] if path.suffix.lower() == '.s' else []
        run([tool('clang'), *flags, '-I', path.parent, *includes, *assembly, '-c', path, '-o', obj])
        objects.append(obj)
    elf = OUT / 'play.elf'
    run([tool('ld.lld'), '-T', source / 'current/play.ld', '--gc-sections',
         '-Map=' + str(OUT / 'play.map'), *objects, '-o', elf])
    unresolved = run([tool('llvm-nm'), '--undefined-only', elf]).strip()
    if unresolved:
        raise ValueError(unresolved)
    run([tool('llvm-objcopy'), '-O', 'binary', '-j', '.text', elf, OUT / 'PLAY.BIN'])
    runtime_symbols = {p[2]: int(p[0], 16) for line in run([tool('llvm-nm'), '--defined-only', elf]).splitlines()
                       if len(p := line.split()) == 3}
    blob = (OUT / 'PLAY.BIN').read_bytes()
    if len(blob) > 61440 or runtime_symbols['__bss_end'] > 0x80120000:
        raise ValueError('Runtime exceeds the existing boot gate or private RAM limit')
    if any(name.startswith(('usb_', 'nand_', 'dl_')) and name != 'usb_gate' for name in runtime_symbols):
        raise ValueError('Unexpected transport or flash symbol in standalone runtime')
    result = {'binary_bytes': len(blob), 'binary_sha256': sha(OUT / 'PLAY.BIN'), 'binary_fnv': fnv(blob),
              'engine_sha256': sha(engine_dir / 'engine.pge'), 'card_sectors': card_sectors,
              'sd_profile_sha256': sd_profile_sha256(profile),
              'arena_bytes': arena, 'bss_end': runtime_symbols['__bss_end'], 'unresolved_symbols': [],
              'gameplay': 'vanilla', 'hardware_tested': False,
              'sources': {unit['source']: sha(ROOT / unit['source']) for unit in units},
              'objects': {obj.name: sha(obj) for obj in objects}}
    (OUT / 'symbols.json').write_text(json.dumps(runtime_symbols, indent=2) + '\n')
    (OUT / 'build-report.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: result[key] for key in ('binary_bytes', 'binary_sha256', 'binary_fnv', 'arena_bytes', 'bss_end')}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/emerald-native')
    parser.add_argument('--sd-profile', type=Path, required=True,
                        help='Ignored local JSON with cid_words and capacity_sectors')
    args = parser.parse_args()
    build(args.engine, args.sd_profile)
