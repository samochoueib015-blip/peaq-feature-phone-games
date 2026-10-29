"""Package an ARM PIE engine with bounded relative relocations (PGE1/PGE2).

Unlike a small PGA launcher app, engine payloads may use multiple MiB. This
format grants no services by itself; the owner must supply all host callbacks.
"""
from pathlib import Path
import struct,json,hashlib,subprocess
from toolchain import tool

def fnv(data,h=2166136261):
 for b in data:h=((h^b)*16777619)&0xffffffff
 return h

def pack(elf,output,exports_symbol,exports_count):
 elf=Path(elf);output=Path(output);raw=elf.read_bytes()
 h=struct.unpack_from('<16sHHIIIIIHHHHHH',raw)
 assert raw[:7]==b'\x7fELF\x01\x01\x01' and h[1:3]==(3,40)
 sections=[struct.unpack_from('<10I',raw,h[6]+i*h[11]) for i in range(h[12])]
 st=sections[h[13]];strings=raw[st[4]:st[4]+st[5]]
 sec={strings[s[0]:].split(b'\0')[0].decode():s for s in sections}
 image_s=sec['.image'];assert image_s[3]==0
 image=raw[image_s[4]:image_s[4]+image_s[5]]
 mem=max(len(image),sec['.bss'][3]+sec['.bss'][5]) if '.bss' in sec else len(image)
 symbols={p[2]:int(p[0],16) for line in subprocess.check_output([tool('llvm-nm'),'--defined-only',elf],text=True).splitlines() if len(p:=line.split())==3}
 assert not subprocess.check_output([tool('llvm-nm'),'-u',elf],text=True).strip()
 exports=symbols[exports_symbol];assert exports%4==0 and 1<=exports_count<=32 and exports+exports_count*4<=len(image)
 functions=struct.unpack_from('<'+'I'*exports_count,image,exports)
 assert all(v&1 and (v&~1)<len(image) for v in functions)
 relocs=[]
 if '.rel.dyn' in sec:
  s=sec['.rel.dyn'];assert s[9]==8
  for at in range(s[4],s[4]+s[5],8):
   offset,kind=struct.unpack_from('<II',raw,at)
   if kind==0:
    assert offset==0,('noncanonical R_ARM_NONE',offset)
    continue
   assert kind==23,('unsupported relocation',offset,kind)
   assert offset+4<=len(image) and struct.unpack_from('<I',image,offset)[0]<mem
   relocs.append(offset)
 relocs=sorted(relocs);assert len(set(relocs))==len(relocs)
 assert all(b-a>=4 for a,b in zip(relocs,relocs[1:])), 'overlapping relocation words'
 assert all(exports+i*4 in relocs for i in range(exports_count))
 assert 2<=len(image)<=mem<=24*1024*1024 and len(relocs)<=100000
 # Original GBA command streams pack pointer words immediately after byte
 # opcodes. Version 2 preserves those layouts; version 1 remains unchanged.
 unaligned=sum(at%4!=0 for at in relocs);version=2 if unaligned else 1
 header=[0x31454750,version,len(image),mem,exports,exports_count,len(relocs),0]
 body=image+struct.pack('<'+'I'*len(relocs),*relocs)
 header[7]=fnv(struct.pack('<7I',*header[:7])+body)
 blob=struct.pack('<8I',*header)+body;output.write_bytes(blob)
 report={'format':'PGE'+str(version),'image_bytes':len(image),'memory_bytes':mem,'exports':exports,'export_offsets':list(functions),
         'unaligned_relocations':unaligned,
         'relocation_count':len(relocs),'sha256':hashlib.sha256(blob).hexdigest(),'phone_tested':False,'symbols':symbols}
 output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n')
 return report
