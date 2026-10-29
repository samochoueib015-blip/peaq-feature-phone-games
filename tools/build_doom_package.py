"""Build the DOOM SD folder for the qualified segmented ABI5-v2 launcher.

This offline command writes only build/doom-pga. Firmware qualification is a
separate requirement; this build does not connect to a phone. Supply Freedoom
Phase 1 separately as freedoom1.wad. Native audio/save integration is absent.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
import os
from build_game import fnv
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'build/doom-pga'
ENGINE_SHA='280c057a38d74e315408c9ebbee5dfb1bb6bc6717a24d6a8c9b4dabee2e2012a'
TOOLCHAIN=None
def tool(name):
 root=TOOLCHAIN/'bin'
 suffix='.exe' if (root/'arm-none-eabi-gcc.exe').exists() else ''
 return str(root/('arm-none-eabi-'+name+suffix))
PROFILE=ROOT/'ports/doom/aligned_profile.h'
SOURCES=['ports/doom/worker.c', 'experiments/doom/read_cache.c', 'experiments/doom/fileio.c', 'experiments/licar/io_channel.c', 'experiments/runtime/module.c', 'experiments/runtime/mocor_files.c', 'experiments/emerald/memory.c', 'experiments/doom/mocor_host.c', 'experiments/runtime/resident_tx_join.c', 'experiments/licar/mocor_host_sync.S', 'ports/doom/adapter.c', 'experiments/runtime/mocor_display.c', 'experiments/runtime/mocor_time.c', 'experiments/runtime/native_controls.c']
HEADERS=[]

def sha(b):return hashlib.sha256(b).hexdigest()
def build(toolchain):
 global TOOLCHAIN
 TOOLCHAIN=toolchain.resolve()
 engine=ROOT/'build/doom-guarded/engine.pge'
 if sha(engine.read_bytes())!=ENGINE_SHA:
  raise ValueError('DOOM engine differs from the qualified strict-alignment build')
 OUT.mkdir(parents=True,exist_ok=True);objects=[]
 flags=['-include',str(PROFILE),'-I',str(ROOT/'experiments/doom'),'-I',str(ROOT/'experiments/runtime'),'-mno-unaligned-access','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-Os','-std=c11','-ffreestanding','-fno-builtin',
  '-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fpie','-fvisibility=hidden',
  '-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
 for i,name in enumerate(SOURCES):
  obj=OUT/(str(i)+'-'+Path(name).stem+'.o')
  subprocess.run([tool('gcc'),*flags,'-c',ROOT/name,'-o',obj],check=True);objects.append(obj)
 script=OUT/'game.ld';script.write_text('''ENTRY(pga_event)
SECTIONS {
 .image 0 : { *(.text*) *(.rodata*) *(.data*) *(.got*) }
 .bss ALIGN(4096) (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; }
 .rel.dyn : { *(.rel.dyn) } .dynamic : { *(.dynamic) } .dynsym : { *(.dynsym) }
 .dynstr : { *(.dynstr) } .hash : { *(.hash) } .gnu.hash : { *(.gnu.hash) }
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.note*) }
}
''')
 elf=OUT/'game.elf'
 subprocess.run([tool('gcc'),*flags,'-nostdlib','-pie','-Wl,--no-dynamic-linker,-Bsymbolic,-z,norelro',
  '-T',script,*objects,'-o',elf],check=True)
 assert not subprocess.check_output([tool('nm'),'-u',elf],text=True).strip()
 symbols={p[2]:int(p[0],16) for line in subprocess.check_output([tool('nm'),'--defined-only',elf],text=True).splitlines() if len(p:=line.split())==3}
 raw=elf.read_bytes();h=struct.unpack_from('<16sHHIIIIIHHHHHH',raw);assert h[1:3]==(3,40)
 sh=[struct.unpack_from('<10I',raw,h[6]+i*h[11]) for i in range(h[12])]
 s=sh[h[13]];names=raw[s[4]:s[4]+s[5]]
 sections={names[s[0]:].split(b'\0')[0].decode():s for s in sh}
 s=sections['.image'];assert s[3]==0;image=bytearray(raw[s[4]:s[4]+s[5]])
 memory=max(len(image),symbols['__bss_end']);state=struct.unpack_from('<I',image,symbols['pga_state_bytes'])[0]
 descriptor=symbols['peaq_doom_pga_descriptor'];table=symbols['peaq_doom_pga_segments']
 offsets=[symbols[n]|1 for n in ('peaq_doom_pga_start','pga_event','peaq_doom_pga_close','peaq_doom_pga_poll')]
 struct.pack_into('<10I',image,descriptor,0x35414750,2,40,2,7,table,*offsets)
 specs=[struct.unpack_from('<4I',image,table+i*16) for i in range(7)]
 relocs=[]
 if '.rel.dyn' in sections:
  s=sections['.rel.dyn'];assert s[9]==8
  for off in range(s[4],s[4]+s[5],8):
   at,info=struct.unpack_from('<II',raw,off);assert info==23,('not relative',hex(info))
   assert at%4==0 and at+4<=len(image) and struct.unpack_from('<I',image,at)[0]<memory
   assert not(descriptor<=at<descriptor+40 or table<=at<table+112)
   relocs.append(at)
 assert len(set(relocs))==len(relocs);relocs.sort()
 assert 2<=len(image)<=65536 and memory<=131072 and state<=32768 and len(relocs)<=512
 assert sum(s[1] for s in specs)==3616021
 assert struct.unpack_from('<I',image,symbols['pga_required_abi'])[0]==5
 # Tiny code-native thumbnail; no borrowed Doom artwork or proprietary assets.
 icon=bytearray()
 for y in range(40):
  for x in range(40):
   wall=(x<7 or x>=33 or y<6 or y>=34)
   aim=(x==20 and 14<=y<=26) or (y==20 and 14<=x<=26)
   icon+=struct.pack('<H',0xffe0 if aim else 0x7800 if wall else 0x18c3)
 meta=b'DOOM'.ljust(32,b'\0')+icon
 header=[0x32414750,5,len(image),memory,offsets[1],state,5,len(relocs),0,descriptor]
 blob=bytearray(struct.pack('<10I',*header)+meta+image+struct.pack('<'+'I'*len(relocs),*relocs))
 struct.pack_into('<I',blob,32,fnv(blob[:32]+blob[36:]))
 (OUT/'game.pga').write_bytes(blob);(OUT/'image.bin').write_bytes(image)
 (OUT/'offsets.json').write_text(json.dumps(symbols,indent=2)+'\n')
 # Relocated debug fixture consumed by the qualified native-host harness.
 base=0x30000000;relocated=bytearray(image)
 for at in relocs:struct.pack_into('<I',relocated,at,struct.unpack_from('<I',image,at)[0]+base)
 (OUT/'worker.bin').write_bytes(relocated)
 (OUT/'symbols.json').write_text(json.dumps({k:v+base for k,v in symbols.items()},indent=2)+'\n')
 # Dedicated package directory is local output only; no SD path is accessed.
 package=OUT/'Games/Doom';package.mkdir(parents=True,exist_ok=True)
 (package/'game.pga').write_bytes(blob);(package/'engine.pge').write_bytes(engine.read_bytes())
 (package/'LICENSE-DoomGeneric.txt').write_bytes((ROOT/'licenses/DoomGeneric-GPL-2.0.txt').read_bytes())
 report={'built':True,'abi':5,'descriptor_revision':2,'title':'DOOM','package_sha256':sha(blob),
  'image_sha256':sha(image),'image_bytes':len(image),'memory_bytes':memory,'state_bytes':state,
  'tick_ms':5,'relocations':relocs,'descriptor_offset':descriptor,'segment_table_offset':table,'segments':specs,
  'allocation_payload_bytes':{'worker':24576,'module':628779,'heap':2621447,'frames':307231,'tcb':196,'stack':32768,'native_host':1024},
  'allocation_payload_total':3616021,'raw_segment_requests':3616046,
  'engine_sha256':sha(engine.read_bytes()),'pga_clock':'original SCI_GetTickCount via mocor_time',
  'source_sha256':{s:sha((ROOT/s).read_bytes()) for s in [*SOURCES,*HEADERS,'tools/build_doom_package.py']},
  'hardware_access':False,'hardware_tested_by_this_build':False,'audio':False,'saves':False,
  'required_launcher':{'abi':5,'descriptor_revision':2,'firmware_qualification_required':True},
  'assets':'Requires freedoom1.wad beside engine.pge; no proprietary assets included'}
 (OUT/'build-report.json').write_text(json.dumps(report,indent=2)+'\n');return OUT
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--toolchain',type=Path,default=os.environ.get('PEAQ_GNU_ARM'))
 args=parser.parse_args()
 if args.toolchain is None:parser.error('Pass --toolchain or set PEAQ_GNU_ARM to the Arm GNU installation root')
 print(build(args.toolchain))
