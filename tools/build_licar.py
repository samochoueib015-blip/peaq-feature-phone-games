"""Build the complete, unmodified Licar engine for ARMv7 Thumb (offline)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from toolchain import tool

ROOT = Path(__file__).resolve().parents[1]
PIN = 'd31205dde1ca79ce9314fe6b18cafeca1fa6f3a7'

def build(upstream,relocatable=False):
    upstream = upstream.resolve()
    commit = subprocess.check_output(['git', '-C', str(upstream), 'rev-parse', 'HEAD'], text=True).strip()
    if commit != PIN:
        raise SystemExit(f'Expected Licar {PIN}, got {commit}')
    out = ROOT / ('build/licar-module' if relocatable else 'build/licar')
    out.mkdir(parents=True, exist_ok=True)
    flags = ['--target=armv7-none-eabi', '-mcpu=cortex-a7', '-mthumb',
             '-mfloat-abi=soft', '-mno-unaligned-access', '-O2', '-ffreestanding', '-fno-builtin',
             '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
             '-I', str(upstream), '-Wall', '-Wextra', '-Wno-unused-parameter']
    if relocatable:flags+=['-fpie','-fvisibility=hidden']
    objects = []
    for source in ['experiments/licar/platform.c', 'experiments/common/landscape.c',
                   'experiments/emerald/memory.c']:
        obj = out / (Path(source).stem + '.o')
        subprocess.run([str(tool('clang')), *flags, '-c', str(ROOT / source), '-o', str(obj)], check=True)
        objects.append(str(obj))
    if relocatable:
        exports=['peaq_licar_init','peaq_licar_step','peaq_licar_audio','peaq_licar_end','peaq_licar_state','peaq_licar_speed','peaq_licar_time','peaq_licar_errors']
        source=out/'exports.c';source.write_text('#include "'+(ROOT/'experiments/licar/platform.h').as_posix()+'"\nconst void *const peaq_licar_exports[]={'+','.join('(const void *)'+s for s in exports)+'};\n')
        obj=out/'exports.o';subprocess.run([str(tool('clang')),*flags,'-c',str(source),'-o',str(obj)],check=True);objects.append(str(obj))
    script = out / 'licar.ld'
    dynamic=' .rel.dyn : { *(.rel.dyn) } .dynamic : { *(.dynamic) } .dynsym : { *(.dynsym) } .dynstr : { *(.dynstr) } .hash : { *(.hash) } .gnu.hash : { *(.gnu.hash) }' if relocatable else ''
    script.write_text('ENTRY(peaq_licar_init)\nSECTIONS { .image '+('0' if relocatable else '0x10000000')+' : { *(.text*) *(.rodata*) *(.data*) *(.got*) } .bss (NOLOAD) : { *(.bss*) *(COMMON) }'+dynamic+' /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) } }')
    elf = out / 'licar.elf'
    linkflags=['-pie','--no-dynamic-linker','-Bsymbolic','-z','norelro'] if relocatable else []
    subprocess.run([str(tool('ld.lld')),*linkflags, '-T', str(script), *objects, '-o', str(elf)], check=True)
    binary = out / 'licar.bin'
    subprocess.run([str(tool('llvm-objcopy')), '--dump-section=.image=' + str(binary), str(elf)], check=True)
    symbols = {p[2]: int(p[0], 16) for line in subprocess.check_output([str(tool('llvm-nm')), '--defined-only', str(elf)], text=True).splitlines() if len(p := line.split()) == 3}
    (out / 'symbols.json').write_text(json.dumps(symbols, indent=2))
    sizes = subprocess.check_output([str(tool('llvm-size')), str(elf)], text=True).strip()
    report = {'upstream_commit': commit, 'engine': 'Complete Licar including rendering, physics, maps, replay, menu and SFX',
              'image_bytes': binary.stat().st_size, 'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'size_output': sizes, 'phone_package_ready': False, 'hardware_tested': False}
    if relocatable:
        from pack_engine import pack
        module=pack(elf,out/'engine.pge','peaq_licar_exports',len(exports))
        report['module']={k:v for k,v in module.items() if k!='symbols'}
    (out / 'build.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    return out

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream', type=Path, required=True)
    parser.add_argument('--relocatable',action='store_true')
    args=parser.parse_args();build(args.upstream,args.relocatable)
