"""Build the Licar SD folder for the qualified segmented ABI5-v2 launcher.

This offline command writes only build/licar-pga. Firmware qualification is a
separate requirement; this build does not connect to a phone. PCM and native
save/replay writes remain unavailable in this integration.
"""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
from toolchain import tool

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/licar-pga'
PIN='d31205dde1ca79ce9314fe6b18cafeca1fa6f3a7'
ENGINE_SHA='4a224904e8e37eaac116d7e0e27eb646ba6cfe21aa07096cf4aefd96564f89ea'
SOURCES=['ports/licar/adapter.c', 'experiments/licar/pga_memory.c', 'experiments/runtime/mocor_time.c', 'experiments/licar/mocor_host.c', 'experiments/licar/mocor_host_sync.S', 'experiments/licar/io_channel.c', 'experiments/licar/io_bridge.c', 'ports/licar/runtime.c', 'experiments/runtime/resident_tx_join.c', 'experiments/runtime/mocor_files.c', 'experiments/runtime/mocor_display.c', 'experiments/runtime/module.c', 'experiments/emerald/memory.c', 'experiments/runtime/native_controls.c']
TRACKED=SOURCES

def sha(b):return hashlib.sha256(b).hexdigest()
def fnv(b):
 h=2166136261
 for x in b:h=((h^x)*16777619)&0xffffffff
 return h
def run(args):
 p=subprocess.run([str(x) for x in args],text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 return p.stdout
def build(upstream):
 upstream=upstream.resolve()
 commit=subprocess.check_output(['git','-C',str(upstream),'rev-parse','HEAD'],text=True).strip()
 if commit!=PIN:raise ValueError(f'Expected Licar {PIN}, got {commit}')
 assets=[('data','data'),('assets/music','music.u8'),('license','LICENSE')]
 asset_bytes={}
 for source,destination in assets:
  payload=subprocess.check_output(['git','-C',str(upstream),'show',PIN+':'+source])
  # The qualified Windows folder used CRLF for text. Read pinned Git blobs
  # and reproduce that representation independently of checkout settings.
  if source in ('data','license'):payload=payload.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')
  asset_bytes[destination]=payload
 engine=(ROOT/'build/licar-module/engine.pge').read_bytes()
 if sha(engine)!=ENGINE_SHA:raise ValueError('Licar engine differs from the qualified strict-alignment build')
 OUT.mkdir(parents=True,exist_ok=True);folder=OUT/'Racer';folder.mkdir(exist_ok=True)
 flags=['-I',str(ROOT/'experiments/licar'),'-I',str(ROOT/'experiments/runtime'),'--target=armv7-none-eabi','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft',
  '-mno-unaligned-access','-Oz','-std=c11','-ffreestanding','-fno-builtin','-fpie',
  '-fvisibility=hidden','-fno-unwind-tables','-fno-asynchronous-unwind-tables',
  '-fdata-sections','-ffunction-sections','-Wall','-Wextra','-Werror']
 objects=[]
 for i,source in enumerate(SOURCES):
  obj=OUT/(str(i)+'-'+Path(source).stem+'.o')
  run([tool('clang'),*flags,'-I',ROOT/'sdk','-c',ROOT/source,'-o',obj]);objects.append(obj)
 elf=OUT/'adapter.elf'
 run([tool('ld.lld'),'-pie','--no-dynamic-linker','-Bsymbolic','-z','norelro',
      '-T',ROOT/'sdk/module.ld',*objects,'-o',elf])
 assert not run([tool('llvm-nm'),'-u',elf]).strip()
 raw=elf.read_bytes();assert raw[:7]==b'\x7fELF\x01\x01\x01'
 hdr=struct.unpack_from('<16sHHIIIIIHHHHHH',raw);assert hdr[1:3]==(3,40)
 shoff,shentsize,shnum,shstrndx=hdr[6],hdr[11],hdr[12],hdr[13]
 entries=[struct.unpack_from('<10I',raw,shoff+i*shentsize) for i in range(shnum)]
 st=entries[shstrndx];strings=raw[st[4]:st[4]+st[5]]
 sections={strings[s[0]:].split(b'\0')[0].decode():s for s in entries}
 im=sections['.image'];assert im[3]==0
 image=bytearray(raw[im[4]:im[4]+im[5]])
 mem=len(image)
 if '.bss' in sections:mem=max(mem,sections['.bss'][3]+sections['.bss'][5])
 symbols={p[2]:int(p[0],16) for line in run([tool('llvm-nm'),'--defined-only',elf]).splitlines() if len(p:=line.split())==3}
 state=struct.unpack_from('<I',image,symbols['pga_state_bytes'])[0]
 tick=struct.unpack_from('<I',image,symbols['pga_tick_ms'])[0]
 assert struct.unpack_from('<I',image,symbols['pga_required_abi'])[0]==5 and tick==5
 entry=symbols['pga_event']|1;assert hdr[4]==entry
 descriptor_at=symbols['peaq_licar_pga_descriptor'];table_at=symbols['peaq_licar_pga_segments']
 assert descriptor_at%4==table_at%4==0
 assert image[descriptor_at:descriptor_at+40]==b'\0'*40
 callbacks=[symbols[n]|1 for n in ('peaq_licar_pga_start','pga_event','peaq_licar_pga_close','peaq_licar_pga_poll')]
 descriptor=[0x35414750,2,40,2,7,table_at,*callbacks]
 struct.pack_into('<10I',image,descriptor_at,*descriptor)
 specs=[struct.unpack_from('<4I',image,table_at+16*i) for i in range(7)]
 assert [x[0] for x in specs]==[1,3,4,7,8,5,6]
 assert all(x[3]==0 and x[2] in (4,8) for x in specs)
 relocs=[]
 if '.rel.dyn' in sections:
  s=sections['.rel.dyn'];assert s[9]==8
  for off in range(s[4],s[4]+s[5],8):
   at,info=struct.unpack_from('<II',raw,off);assert info==23
   assert at%4==0 and at+4<=len(image) and struct.unpack_from('<I',image,at)[0]<mem
   assert not(descriptor_at<=at<descriptor_at+40 or table_at<=at<table_at+112)
   relocs.append(at)
 relocs=sorted(relocs);assert len(set(relocs))==len(relocs)
 assert 2<=len(image)<=65536 and len(image)<=mem<=131072 and state<=32768 and len(relocs)<=512
 assert all(x&1 and (x&~1)+2<=len(image) for x in callbacks)
 assert sum(x[1]+x[2]-1 for x in specs)<4*1024*1024
 # Original code-native icon; no third-party artwork or copyrighted game data.
 pixels=[]
 for y in range(40):
  for x in range(40):
   c=0x0842
   if 8<x<31:c=0x39e7
   if x in (9,30):c=0xffff
   if x in (19,20) and y%12<6:c=0xffe0
   if 14<=x<=26 and 11<=y<=31:c=0xf800
   if 16<=x<=24 and 15<=y<=21:c=0x07ff
   if x in (13,14,26,27) and (14<=y<=18 or 26<=y<=30):c=0
   pixels.append(c)
 icon=struct.pack('<1600H',*pixels)
 meta=b'Racer'.ljust(32,b'\0')+icon
 body=meta+image+b''.join(struct.pack('<I',x) for x in relocs)
 header=[0x32414750,5,len(image),mem,entry,state,tick,len(relocs),0,descriptor_at]
 h=struct.pack('<10I',*header);header[8]=fnv(h[:32]+h[36:]+body)
 package=struct.pack('<10I',*header)+body
 (folder/'game.pga').write_bytes(package);(OUT/'adapter.bin').write_bytes(image)
 (OUT/'symbols.json').write_text(json.dumps(symbols,indent=2)+'\n')
 (folder/'engine.pge').write_bytes(engine)
 for destination,payload in asset_bytes.items():
  (folder/destination).write_bytes(payload)
 report={'built':True,'format':'PGA2 header ABI5 descriptor revision2','header':header,
  'image_bytes':len(image),'memory_bytes':mem,'state_bytes':state,'bss':list(sections['.bss'][3:6:2]),
  'tick_ms':tick,'descriptor_offset':descriptor_at,'descriptor':descriptor,'segments':specs,
  'usable_segment_bytes':sum(x[1] for x in specs),'raw_segment_bytes':sum(x[1]+x[2]-1 for x in specs),
  'relocations':relocs,'image_sha256':sha(image),'package_sha256':sha(package),
  'files':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for p in sorted(folder.iterdir()) if p.is_file()},
  'source_sha256':{s:sha((ROOT/s).read_bytes()) for s in TRACKED},
  'upstream_pin':commit,'engine_sha256':sha(engine),
  'required_launcher':{'abi':5,'descriptor_revision':2,'firmware_qualification_required':True},
  'folder_access':'copied selected UTF16 D/E Games folder, only engine.pge/data/music.u8 read-existing',
  'native_save_write':False,'native_audio_output':False,'hardware_access':False,'hardware_tested_by_this_build':False,
  'service_tick_ms':5,'redraw_interval_ms':50,
  'limits':['Requires the separately qualified PEAQ firmware and native launcher services.',
   'Native PCM and nonempty Licar data_append are explicit unsupported/backpressure, not silent success.',
   'An offline build does not establish compatibility with a different handset or firmware.']}
 (OUT/'build-report.json').write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:report[k] for k in ('built','image_bytes','memory_bytes','state_bytes','package_sha256','raw_segment_bytes')},indent=2))
 return report
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--upstream',type=Path,default=ROOT/'deps/licar')
 build(parser.parse_args().upstream)
