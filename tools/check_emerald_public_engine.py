"""Execute a bounded clean-engine smoke test in Unicorn, without phone I/O.

This checks relocation, initialization, frame callbacks and the cartridge RAM
save boundary. It does not test the SD runtime, physical audio or firmware.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.arm_const import (UC_CPU_ARM_CORTEX_A7, UC_ARM_REG_R0, UC_ARM_REG_R1,
                               UC_ARM_REG_R2, UC_ARM_REG_R3, UC_ARM_REG_SP,
                               UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_CPSR)
from build_game import fnv

ROOT = Path(__file__).resolve().parents[1]


def check(folder, frame_count):
    if not 1 <= frame_count <= 1600:
        raise ValueError('Use 1 through 1600 frames')
    folder = folder.resolve()
    blob = (folder / 'engine.pge').read_bytes()
    h = struct.unpack_from('<8I', blob)
    if h[0] != 0x31454750 or h[1] not in (1, 2) or h[5] != 5:
        raise ValueError('Unexpected engine header')
    if len(blob) != 32 + h[2] + h[6] * 4 or fnv(blob[:28] + blob[32:]) != h[7]:
        raise ValueError('Invalid engine size/checksum')
    base, ram = 0x10002000, 0x40000000
    image = bytearray(blob[32:32 + h[2]])
    relocs = struct.unpack_from('<' + 'I' * h[6], blob, 32 + h[2])
    if tuple(sorted(set(relocs))) != relocs:
        raise ValueError('Relocations are not unique and sorted')
    for at in relocs:
        if at + 4 > len(image):
            raise ValueError('Relocation outside image')
        value = struct.unpack_from('<I', image, at)[0]
        if value >= h[3]:
            raise ValueError('Relocation outside module memory')
        struct.pack_into('<I', image, at, value + base)
    m = Uc(UC_ARCH_ARM, UC_MODE_THUMB)
    m.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_A7)
    m.mem_map(base, (h[3] + 32 + 4095) & ~4095)
    m.mem_map(ram, 0x400000)
    m.mem_write(base, bytes(image))
    m.mem_write(base + h[3], b'\xcc' * 32)
    sy = {name: value + base for name, value in json.loads((folder / 'symbols.json').read_text()).items()}
    gba, portrait, layers, host = ram + 0x100000, ram + 0x120000, ram + 0x150000, ram + 0x180000
    stop, stack = ram + 0x3ff000, ram + 0x3fe000
    regs = [UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3]
    clock = 0
    frames = []
    audio = bytearray()
    dirty = []
    invalid = []

    def bad_access(uc, access, address, size, value, user):
        invalid.append({'access': access, 'address': hex(address), 'size': size})
        return False

    m.hook_add(UC_HOOK_MEM_INVALID, bad_access)

    def callback(uc, address, size, user):
        args = [uc.reg_read(register) for register in regs]
        result = 0
        if address == host + 0x100:
            assert args[:3] == [portrait, 240, 320]
            frames.append(hashlib.sha256(uc.mem_read(portrait, 153600)).hexdigest())
        elif address == host + 0x110:
            assert 0 < args[1] <= 256 and args[2] == 13440
            audio.extend(uc.mem_read(args[0], args[1]))
        elif address == host + 0x120:
            dirty.append(1)
        elif address == host + 0x130:
            result = clock
        uc.reg_write(UC_ARM_REG_R0, result)
        uc.reg_write(UC_ARM_REG_PC, uc.reg_read(UC_ARM_REG_LR))

    for address in range(host + 0x100, host + 0x140, 16):
        m.hook_add(UC_HOOK_CODE, callback, begin=address, end=address)
    m.mem_write(host, struct.pack('<4I', host + 0x101, host + 0x111, host + 0x121, host + 0x131))
    save = sy['peaq_emerald_save']
    m.mem_write(save, b'\xff' * 131072)

    def call(name, *args):
        m.reg_write(UC_ARM_REG_CPSR, 0x33)
        m.reg_write(UC_ARM_REG_SP, stack)
        m.reg_write(UC_ARM_REG_LR, stop | 1)
        for register, value in zip(regs, args):
            m.reg_write(register, value)
        m.emu_start(sy[name] | 1, stop, count=100000000)
        if m.reg_read(UC_ARM_REG_PC) != stop or m.reg_read(UC_ARM_REG_SP) != stack:
            raise ValueError('Engine call did not return within its instruction budget: ' + name)
        return m.reg_read(UC_ARM_REG_R0)

    if call('peaq_emerald_save_memory') != save or call('peaq_emerald_init', host, gba, portrait, layers) != 1:
        raise ValueError('Engine initialization failed')
    callbacks = []
    names = {value & ~1: name for name, value in sy.items() if name.startswith(('CB2_', 'MainCB2'))}
    for frame in range(frame_count):
        clock = (frame + 1) * 1000 // 60
        buttons = 8 if frame in (180, 181, 360, 361) else 0
        if frame >= 540 and frame % 60 in (0, 1):
            buttons = 1
        call('peaq_emerald_step', buttons)
        callback_address = struct.unpack('<I', m.mem_read(sy['gMain'] + 4, 4))[0] & ~1
        name = names.get(callback_address, hex(callback_address))
        if not callbacks or callbacks[-1]['name'] != name:
            callbacks.append({'frame': frame, 'name': name})
        if frame % 100 == 0:
            print(json.dumps({'frame': frame, 'callback': name}), flush=True)
    if call('peaq_emerald_frames') != frame_count or len(frames) != frame_count:
        raise ValueError('Missing game/frame callbacks')
    pattern = bytes((i * 37 + 9) & 255 for i in range(4096))
    m.mem_write(host + 0x1000, pattern)
    for sector in (0, 15, 16, 31):
        assert call('Rp2350SaveEraseSector', sector) == 0
        assert call('Rp2350SaveProgramSector', sector, host + 0x1000) == 0
        assert bytes(m.mem_read(save + sector * 4096, 4096)) == pattern
        assert call('Rp2350SaveProgramByte', sector, 17, 0) == 0
        assert m.mem_read(save + sector * 4096 + 17, 1) == b'\0'
    before = bytes(m.mem_read(save, 131072))
    assert call('Rp2350SaveEraseSector', 32) != 0
    assert call('Rp2350SaveProgramByte', 31, 4096, 0) != 0
    assert bytes(m.mem_read(save, 131072)) == before
    assert bytes(m.mem_read(base + h[3], 32)) == b'\xcc' * 32
    result = {'all_passed': True, 'engine_sha256': hashlib.sha256(blob).hexdigest(),
              'frames': len(frames), 'distinct_frame_hashes': len(set(frames)),
              'audio_samples': len(audio), 'distinct_audio_values': len(set(audio)),
              'callbacks': callbacks, 'invalid_memory_accesses': invalid,
              'save_mirror_bounds_and_programming': True, 'allocation_end_guard_preserved': True,
              'relocation_method': 'Python reference relocation; not the production ARM loader',
              'sd_runtime_tested': False, 'hardware_tested': False, 'full_playthrough_tested': False}
    (folder / 'public-engine-checks.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/emerald-native')
    parser.add_argument('--frames', type=int, default=120)
    args = parser.parse_args()
    check(args.engine, args.frames)
