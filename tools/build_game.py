"""Build an ARM/Thumb C game into a self-contained, relocatable PGA2 package."""
from pathlib import Path
import argparse,hashlib,json,struct,subprocess
from toolchain import tool
HERE=Path(__file__).resolve().parents[1]/'sdk'

def run(args):
 p=subprocess.run([str(x) for x in args],capture_output=True,text=True)
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
 return p.stdout
def fnv(b):
 h=2166136261
 for x in b:h=((h^x)*16777619)&0xffffffff
 return h
def build(source,output,title,icon):
 from PIL import Image
 source=Path(source).resolve();output=Path(output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
 obj=output.with_suffix('.o');elf=output.with_suffix('.elf')
 flags=['--target=armv7-none-eabi','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access',
  '-Oz','-ffreestanding','-fno-builtin','-fpie','-fvisibility=hidden','-fno-unwind-tables',
  '-fno-asynchronous-unwind-tables','-fdata-sections','-ffunction-sections','-Wall','-Wextra','-Werror']
 run([tool('clang'),*flags,'-I',HERE,'-c',source,'-o',obj])
 run([tool('ld.lld'),'-pie','--no-dynamic-linker','-Bsymbolic','-z','norelro','-T',HERE/'module.ld',obj,'-o',elf])
 raw=elf.read_bytes();assert raw[:7]==b'\x7fELF\x01\x01\x01'
 hdr=struct.unpack_from('<16sHHIIIIIHHHHHH',raw)
 assert hdr[2]==40 and hdr[1]==3
 shoff,shentsize,shnum,shstrndx=hdr[6],hdr[11],hdr[12],hdr[13]
 sections=[struct.unpack_from('<10I',raw,shoff+i*shentsize) for i in range(shnum)]
 st=sections[shstrndx];strings=raw[st[4]:st[4]+st[5]]
 sections={strings[s[0]:].split(b'\0')[0].decode():s for s in sections}
 image_s=sections['.image'];assert image_s[3]==0
 image=raw[image_s[4]:image_s[4]+image_s[5]]
 mem=len(image)
 if '.bss' in sections:mem=max(mem,sections['.bss'][3]+sections['.bss'][5])
 symbols={p[2]:int(p[0],16) for line in run([tool('llvm-nm'),'--defined-only',elf]).splitlines() if len(p:=line.split())==3}
 entry=hdr[4];assert entry==(symbols['pga_event']|1)
 state=struct.unpack_from('<I',image,symbols['pga_state_bytes'])[0]
 tick=struct.unpack_from('<I',image,symbols['pga_tick_ms'])[0]
 abi=struct.unpack_from('<I',image,symbols['pga_required_abi'])[0] if 'pga_required_abi' in symbols else 2
 assert abi in (2,3,4)
 relocs=[]
 if '.rel.dyn' in sections:
  s=sections['.rel.dyn'];assert s[9]==8
  for off in range(s[4],s[4]+s[5],8):
   at,info=struct.unpack_from('<II',raw,off);assert info==23,('Only R_ARM_RELATIVE supported',hex(info))
   assert at%4==0 and at+4<=len(image) and struct.unpack_from('<I',image,at)[0]<mem
   relocs.append(at)
 relocs=sorted(relocs);assert len(set(relocs))==len(relocs)
 assert 2<=len(image)<=65536 and len(image)<=mem<=131072 and state<=32768 and len(relocs)<=512
 assert tick==0 or 50<=tick<=1000
 assert 0<len(title)<=24 and all(32<=ord(c)<=126 for c in title)
 im=Image.open(icon).convert('RGB');assert im.size==(40,40),'Thumbnail must be 40x40 pixels'
 pixels=b''.join(struct.pack('<H',((r>>3)<<11)|((g>>2)<<5)|(b>>3)) for r,g,b in im.getdata())
 meta=title.encode('ascii').ljust(32,b'\0')+pixels
 body=meta+image+b''.join(struct.pack('<I',r) for r in relocs)
 header=[0x32414750,abi,len(image),mem,entry,state,tick,len(relocs),0,0]
 h=struct.pack('<10I',*header);header[8]=fnv(h[:32]+h[36:]+body)
 package=struct.pack('<10I',*header)+body;output.write_bytes(package)
 report={'source':str(source),'sha256':hashlib.sha256(package).hexdigest(),'image_bytes':len(image),
  'memory_bytes':mem,'state_bytes':state,'tick_ms':tick,'entry':hex(entry),'relocations':relocs,
  'symbols':{k:hex(v) for k,v in symbols.items()},'format':'PGA2','abi':abi,'title':title,
  'header_bytes':3272,'thumbnail_sha256':hashlib.sha256(pixels).hexdigest()}
 output.with_suffix('.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
 return report
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('output')
 p.add_argument('--title',required=True);p.add_argument('--icon',required=True)
 a=p.parse_args();build(a.source,a.output,a.title,a.icon)
