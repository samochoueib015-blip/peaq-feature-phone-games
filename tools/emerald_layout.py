"""Generate a private include overlay for relocatable GBA memory regions."""
from pathlib import Path
import re, shutil

ROOT = Path(__file__).resolve().parents[1]

def include_overlay(upstream, output):
    # Copy the whole include hierarchy: quoted includes inside global.h/gba.h
    # otherwise prefer their original directory over a command-line -I path.
    dest = output / 'include'
    shutil.copytree(upstream / 'include', dest, dirs_exist_ok=True)
    header = '#include "' + (ROOT / 'experiments/emerald/memory_map.h').as_posix() + '"\n'
    path = dest / 'gba/defines.h'
    text = path.read_text()
    pairs = [('EWRAM_START','20000000','EWRAM'), ('IWRAM_START','20040000','IWRAM'),
             ('VRAM','20048000','VRAM'), ('PLTT','20060000','PALETTE'),
             ('OAM','20060400','OAM'), ('REG_BASE','20060800','REGISTERS')]
    for name, address, region in pairs:
        text, count = re.subn(r'^#define '+name+r'\s+0x'+address+r'\b[^\n]*',
                             '#define '+name+' PEAQ_'+region, text, flags=re.M)
        assert count == 1, name
    path.write_text(header + text)
    path = dest / 'gba/flash_internal.h'
    text = path.read_text(); old = '#define FLASH_BASE ((u8 *)0x10FE0000)'
    assert text.count(old) == 1
    path.write_text(header + text.replace(old, '#define FLASH_BASE ((u8 *)PEAQ_SAVE)'))
    # These two assembly symbols are integer constants, despite their C array
    # declarations. PIE's GOT treats their addresses as module-relative. Keep
    # their original values as actual integer expressions in the generated
    # boundary header, rather than relocating a count as if it were a pointer.
    constants = (upstream / 'rp2350/sound_symbols.s').read_text()
    path = dest / 'gba/m4a_internal.h'; text = path.read_text()
    for macro, symbol in [('NUM_MUSIC_PLAYERS', 'gNumMusicPlayers'), ('MAX_LINES', 'gMaxLines')]:
        values = re.findall(r'^\s*\.set\s+'+symbol+r',\s*(0x[0-9a-fA-F]+|[0-9]+)\b', constants, re.M)
        assert len(values) == 1, symbol
        value = int(values[0], 0); assert 0 <= value <= 0xffff
        text, count = re.subn(r'^#define '+macro+r'\b[^\n]*', '#define '+macro+' '+str(value)+'u', text, flags=re.M)
        assert count == 1, macro
    path.write_text(text)
    return dest
