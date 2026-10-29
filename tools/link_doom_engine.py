"""Link the compiled engine with Newlib and host-owned I/O, without phone I/O."""
from pathlib import Path
import argparse,subprocess,json,hashlib,re
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--toolchain',type=Path,required=True)
p.add_argument('--relocatable',action='store_true')
p.add_argument('--guarded',action='store_true',help='Managed create/tick/quit; safe exit boundary, separate export ABI')
p.add_argument('--engine-build',type=Path,help='Existing compiled engine directory; never modified')
p.add_argument('--upstream',type=Path,default=ROOT/'deps/doomgeneric',help='Pinned checkout used only to verify existing object order')
p.add_argument('--out',type=Path,help='Separate output directory')
args=p.parse_args();tc=args.toolchain.resolve()
default=ROOT/('build/doom-module' if args.relocatable else 'build/doom-engine')
guard_default=ROOT/('build/doom-guarded' if args.relocatable else 'build/doom-guarded-fixed')
engine_build=(args.engine_build or default).resolve();out=(args.out or (guard_default if args.guarded else default)).resolve()
if args.guarded and out==engine_build:raise ValueError('Guarded experiments require a separate --out directory')
if args.guarded and out==(ROOT/'build/doom-runtime-guard').resolve():raise ValueError('Preserve the historical guard build; use build/doom-guarded')
out.mkdir(parents=True,exist_ok=True)
engine=engine_build/'doom-engine.o'
compiled=json.loads((engine_build/'report.json').read_text())
assert hashlib.sha256(engine.read_bytes()).hexdigest()==compiled['engine_object_sha256']
ext='.exe' if (tc/'bin/arm-none-eabi-gcc.exe').exists() else ''
def tool(n):return str(tc/('bin/arm-none-eabi-'+n+ext))
flags=['-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access','-Os','-ffunction-sections','-fdata-sections']
if args.relocatable:flags+=['-fpie','-fvisibility=hidden']
if args.guarded:flags+=['-DPEAQ_DOOM_GUARDED_EXIT=1']
subprocess.run([tool('gcc'),*flags,'-Wall','-Wextra','-Werror','-c',str(ROOT/'experiments/doom/runtime.c'),'-o',str(out/'runtime.o')],check=True)
script=out/'engine.ld'
dynamic=' .rel.dyn : { *(.rel.dyn) } .dynamic : { *(.dynamic) } .dynsym : { *(.dynsym) } .dynstr : { *(.dynstr) } .hash : { *(.hash) } .gnu.hash : { *(.gnu.hash) }' if args.relocatable else ''
entry='peaq_doom_create' if args.guarded else 'doomgeneric_Create'
script.write_text('ENTRY('+entry+')\nSECTIONS { .image '+('0' if args.relocatable else '0x10000000')+' : { *(.text*) *(.rodata*) *(.data*) *(.got*) } . = ALIGN(16); .bss (NOLOAD) : { *(.bss*) *(COMMON) }'+dynamic+' /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) } }')
elf=out/'doom-engine.elf'
exports=['doomgeneric_Tick','peaq_doom_bind','peaq_doom_buttons','peaq_doom_overflows','peaq_doom_io_bind','peaq_doom_heap_used']
objects=[engine,out/'runtime.o'];linkflags=[];libraries=['-lc','-lm','-lgcc']
if args.guarded:
 # GNU --wrap cannot redirect calls that -r already resolved inside the
 # aggregate. Reuse the exact original constituent objects instead; verify
 # their provenance by reconstructing that aggregate byte for byte first.
 upstream=args.upstream.resolve()
 assert subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()==compiled['upstream_commit']
 names=re.search(r'^SRC_DOOM = (.*)$',(upstream/'doomgeneric/Makefile').read_text(),re.M)[1].split()
 engine_objects=[engine_build/name for name in names if name!='doomgeneric_xlib.o']+[engine_build/'platform.o']
 assert len(engine_objects)==compiled['compiled_sources'] and all(path.is_file() for path in engine_objects)
 check=out/'engine-object-check.o'
 subprocess.run([tool('ld'),'-r',*map(str,engine_objects),'-o',str(check)],check=True)
 assert hashlib.sha256(check.read_bytes()).hexdigest()==compiled['engine_object_sha256']
 objects=[*engine_objects,out/'runtime.o']
 exports[0]='peaq_doom_tick'
 exports+=['peaq_doom_quit','peaq_doom_exit_status','peaq_doom_runtime_abi']
 obj=out/'guard.o'
 subprocess.run([tool('gcc'),*flags,'-Wall','-Wextra','-Werror','-c',str(ROOT/'experiments/doom/guard.c'),'-o',str(obj)],check=True)
 objects.append(obj)
 obj=out/'guard-context.o'
 subprocess.run([tool('gcc'),*flags,'-c',str(ROOT/'experiments/doom/guard_context.S'),'-o',str(obj)],check=True)
 objects.append(obj)
 obj=out/'video-init.o'
 subprocess.run([tool('gcc'),*flags,'-Wall','-Wextra','-Werror','-c',str(ROOT/'experiments/doom/video_init.c'),'-o',str(obj)],check=True)
 objects.append(obj)
if args.relocatable:
 entries=[entry,*exports]
 source=out/'exports.c';source.write_text('\n'.join('extern void '+n+'(void);' for n in entries)+'\nconst void *const peaq_doom_exports[]={'+','.join('(const void *)'+n for n in entries)+'};\n')
 obj=out/'exports.o';subprocess.run([tool('gcc'),*flags,'-c',str(source),'-o',str(obj)],check=True);objects.append(obj)
 exports+=['peaq_doom_exports'];linkflags=['-pie','-Wl,--no-dynamic-linker,-Bsymbolic']
 # The Thumb-1 multilib is instruction-compatible with Cortex-A7 and uses
 # relocatable literal words rather than unrelocatable MOVW/MOVT addresses.
 # Only libc/math/compiler helpers use it; game code remains Cortex-A7 PIE.
 libraries=[subprocess.check_output([tool('gcc'),'-mcpu=arm7tdmi','-mthumb','-mfloat-abi=soft','-print-file-name='+n],text=True).strip() for n in ('libc.a','libm.a','libgcc.a')]
 assert all(Path(n).is_file() for n in libraries)
if args.guarded:linkflags+=['-Wl,--wrap=I_Quit,--wrap=I_InitGraphics']
cmd=[tool('gcc'),*flags,*linkflags,'-nostdlib','-T',str(script),'-Wl,--gc-sections',*(f'-Wl,-u,{s}' for s in exports),*(str(o) for o in objects),'-Wl,--start-group',*libraries,'-Wl,--end-group','-o',str(elf)]
linked=subprocess.run(cmd,capture_output=True,text=True);(out/'link.log').write_text(linked.stdout+linked.stderr)
if linked.returncode:raise RuntimeError('Doom link failed; see '+str(out/'link.log')+'\n'+linked.stderr[:1500])
binary=out/'doom-engine.bin'
subprocess.run([tool('objcopy'),'--dump-section=.image='+str(binary),str(elf)],check=True)
symbols={f[2]:int(f[0],16) for line in subprocess.check_output([tool('nm'),'--defined-only',str(elf)],text=True).splitlines() if len(f:=line.split())==3}
(out/'symbols.json').write_text(json.dumps(symbols,indent=2)+'\n')
undefined=subprocess.check_output([tool('nm'),'-u',str(elf)],text=True).strip();assert not undefined,undefined
report={'fully_linked':True,'image_bytes':binary.stat().st_size,'image_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
 'size_output':subprocess.check_output([tool('size'),str(elf)],text=True).strip(),'unresolved_symbols':[],
 'phone_ready':False,'sound_backend_ready':False,'hardware_tested':False,
 'guarded_runtime':args.guarded,'runtime_abi':1 if args.guarded else 0,
 'engine_object_sha256':compiled['engine_object_sha256'],
 'exports':[entry,*[name for name in exports if name!='peaq_doom_exports']]}
tracked=[ROOT/'experiments/doom/runtime.c',Path(__file__)]
if args.guarded:
 tracked += [ROOT/'experiments/doom'/name for name in ('guard.c','guard.h','guard_context.S','video_init.c')]
 report['guard_link_revision']=2
 report['initial_video_clear']={'pixels':64000,'pixel_bytes':1,'value':0,'method':'link-wrapped I_InitGraphics'}
 report['engine_single_objects_verified_against_original_aggregate']=True
 report['engine_sources_recompiled']=False
report['source_sha256']={f.relative_to(ROOT).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in tracked}
if args.relocatable:
 from pack_engine import pack
 module=pack(elf,out/'engine.pge','peaq_doom_exports',len(entries))
 report['module']={k:v for k,v in module.items() if k!='symbols'}
 report['multilib']={'cpu':'arm7tdmi','thumb':True,'libraries':{Path(n).name:hashlib.sha256(Path(n).read_bytes()).hexdigest() for n in libraries}}
(out/'link-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
