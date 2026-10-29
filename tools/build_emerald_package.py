"""Assemble local Emerald folders with fresh blank records and an update set.

The output still requires matching, qualified firmware. This command never
accesses a phone or a mounted card. Fresh saves are generated, not imported.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
from build_game import build as build_game, fnv
from sd_profile import load_sd_profile, sd_profile_sha256

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/emerald-package'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def record(magic, payload, generation):
    header = struct.pack('<4I', magic, 1, len(payload), generation)
    return header + struct.pack('<I', fnv(header + payload)) + payload


def build(profile_path, sd_profile_path):
    from PIL import Image, ImageDraw
    profile = json.loads(profile_path.read_text())
    if profile.get('game_folder') != 'Smaragd' or not profile.get('qualified_firmware_required'):
        raise ValueError('The current boot gate and filesystem paths require the Smaragd folder profile')
    runtime_dir = ROOT / 'build/emerald-runtime'
    engine_dir = ROOT / 'build/emerald-native'
    runtime = (runtime_dir / 'PLAY.BIN').read_bytes()
    engine = (engine_dir / 'engine.pge').read_bytes()
    report = json.loads((runtime_dir / 'build-report.json').read_text())
    sd_profile = load_sd_profile(sd_profile_path)
    if report.get('sd_profile_sha256') != sd_profile_sha256(sd_profile):
        raise ValueError('The supplied SD profile differs from the runtime build profile')
    if sha(runtime) != report['binary_sha256'] or sha(engine) != report['engine_sha256']:
        raise ValueError('The engine/runtime pair differs from the runtime build report')
    if report['gameplay'] != 'vanilla':
        raise ValueError('Only the vanilla public configuration is packaged')
    launcher = OUT / 'launcher'
    launcher.mkdir(parents=True, exist_ok=True)
    for name in ('menu_game.c', 'boot_ticket.c', 'boot_ticket.h', 'retained_request.c', 'retained_request.h'):
        shutil.copyfile(ROOT / 'ports/emerald/launcher' / name, launcher / name)
    fields = {'BRIDGE': profile['bridge_base'], 'PLAY_FNV': fnv(runtime),
              'GATE_PROFILE': profile['gate_profile'], 'OS_BODY_BYTES': profile['os_body_bytes']}
    if any(not isinstance(value, int) or not 0 < value <= 0xffffffff for value in fields.values()):
        raise ValueError('Invalid boot profile constants')
    (launcher / 'menu_profile.h').write_text(''.join('#define PEAQ_MENU_' + key + ' ' + str(value) + 'u\n'
                                                 for key, value in fields.items()))
    icon = Image.new('RGB', (40, 40), (8, 23, 25))
    draw = ImageDraw.Draw(icon)
    draw.polygon([(20, 3), (33, 14), (30, 29), (20, 37), (10, 29), (7, 14)],
                 fill=(30, 170, 120), outline=(160, 255, 220))
    draw.polygon([(20, 3), (20, 37), (10, 29), (7, 14)], fill=(15, 110, 95))
    draw.line([(7, 14), (20, 19), (33, 14)], fill=(160, 255, 220))
    draw.line([(20, 3), (20, 37)], fill=(160, 255, 220))
    icon.save(launcher / 'icon.png')
    launcher_report = build_game(launcher / 'menu_game.c', launcher / 'game.pga', 'Emerald', launcher / 'icon.png')
    request = struct.pack('<5I', 0x31514750, 1, 0, fnv(runtime), 6) + bytes(472)
    common = {'ADAPT.PGE': engine, 'PLAY.BIN': runtime, 'game.pga': (launcher / 'game.pga').read_bytes(),
              'save0.dat': record(0x31534750, request, 0), 'save1.dat': record(0x31534750, request, 1)}
    fresh_only = {'ENGINE0.SAV': record(0x31534c50, bytes([255]) * 131072, 0),
                  'ENGINE1.SAV': record(0x31534c50, bytes([255]) * 131072, 1), 'REPORT.BIN': bytes(512)}
    fixture = bytes(((p * 73 + (p >> 8) * 19 + 0x35) ^ (p >> 3)) & 255 for p in range(32768))
    if sha(fixture) != '5e32bd3f1e1ff09927b1c94ad3974e03f72dcb0224b81820505b4417a44084b9':
        raise ValueError('Runtime reference pattern mismatch')
    inventories = {}
    for variant in ('fresh', 'update'):
        games = OUT / variant / 'Games'
        folder = games / 'Smaragd'
        folder.mkdir(parents=True, exist_ok=True)
        files = dict(common)
        if variant == 'fresh':
            files.update(fresh_only)
        # Never silently overwrite records that someone may have populated.
        for name, payload in files.items():
            path = folder / name
            if name in fresh_only and path.exists() and path.read_bytes() != payload:
                raise ValueError('Existing local save/settings record differs: ' + str(path))
        for name, payload in files.items():
            (folder / name).write_bytes(payload)
        (games / 'COLDCHK.BIN').write_bytes(fixture)
        inventories[variant] = {path.relative_to(games).as_posix(): {'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes())}
                                for path in sorted(games.rglob('*')) if path.is_file()}
    result = {'gameplay': 'vanilla', 'language': 'English', 'engine_sha256': sha(engine),
              'runtime_sha256': sha(runtime), 'launcher_sha256': launcher_report['sha256'],
              'firmware_profile': profile, 'card_sectors': report['card_sectors'],
              'sd_profile_sha256': sd_profile_sha256(sd_profile),
              'fresh_saves': 'Generated blank 128 KiB game data in valid dual-slot wrappers',
              'hardware_tested': False, 'firmware_verified': False,
              'installation_complete': False, 'files': inventories}
    (OUT / 'build-report.json').write_text(json.dumps(result, indent=2) + '\n')
    (OUT / 'README.txt').write_text(
        'Local build output only. The clean public engine/runtime has not been qualified on a phone.\n'
        'Matching firmware, SD CID and capacity are required; no firmware is included.\n'
        'fresh/ contains blank game saves. Never copy those over an existing game save.\n'
        'update/ omits real game saves and REPORT.BIN preferences.\n'
        'The legacy Smaragd folder name is required by the existing boot gate.\n'
        'Generated game content remains a local input/output and must not be committed.\n', encoding='utf-8')
    print(json.dumps({key: result[key] for key in ('gameplay', 'engine_sha256', 'runtime_sha256', 'launcher_sha256', 'hardware_tested')}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware-profile', type=Path,
                        default=ROOT / 'ports/emerald/launcher/firmware-profile.json')
    parser.add_argument('--sd-profile', type=Path, required=True,
                        help='The same ignored local SD profile used for the runtime and firmware')
    args = parser.parse_args()
    build(args.firmware_profile, args.sd_profile)
