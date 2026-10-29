"""Offline firmware patch build for one explicitly known PMP122 firmware.

Requires the user's own original partition backups. Never opens USB, serial,
physical drives or the SD card. Outputs contain vendor data and must not be
committed or redistributed. A successful build is not permission to flash an
unqualified handset or a substitute for a verified recovery backup.
"""
from pathlib import Path
import argparse,hashlib,json,re,shutil,struct,subprocess
from toolchain import tool
from sd_profile import load_sd_profile, sd_profile_header, sd_profile_sha256

ROOT=Path(__file__).resolve().parents[1]
KNOWN_USER='8a2997304ef2ff8bb55f9682ad5ecf5807656d797510b394732da24d19b27764'
KNOWN_OS='c576af8d7ec7217b5b029a0899109c745c52a99a4fc25e3585f9e26fac15e603'
VERIFIED_USER='58d32d66cbb91bc87d1869d48e842656b73b06084cb725ccfa8e73e065f92d4c'
sha=lambda b:hashlib.sha256(b).hexdigest()
FLAGS=['--target=armv7-none-eabi','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access']
CF=['-Oz','-ffreestanding','-fno-builtin','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-fdata-sections','-ffunction-sections','-Wall','-Wextra','-Werror']

def run(args,cwd=None):
    r=subprocess.run([str(a) for a in args],cwd=cwd,capture_output=True,text=True)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    return r.stdout

def symbols(elf):
    return {a[2]:int(a[0],16) for line in run([tool('llvm-nm'),'--defined-only',elf]).splitlines() if len(a:=line.split())==3}

def link(script,objects,elf,sections,gc=False):
    run([tool('ld.lld'),'-T',script,*(['--gc-sections'] if gc else []),*objects,'-o',elf])
    assert not run([tool('llvm-nm'),'-u',elf]).strip()
    for section,path in sections.items():run([tool('llvm-objcopy'),'--dump-section='+section+'='+str(path),elf])
    return symbols(elf)

def compile_file(src,out,flags,includes=()):
    cmd=[tool('clang'),*flags]
    for p in includes:cmd.extend(['-I',p])
    run([*cmd,'-c',src,'-o',out]);return out

def user_image(original,out):
    folder=ROOT/'firmware/menu';objs=[]
    for n in ('games_menu.c','games_hooks.S'):
        objs.append(compile_file(folder/n,out/(n+'.o'),FLAGS+(CF if n.endswith('.c') else []),(ROOT/'sdk',)))
    sy=link(folder/'games_menu.ld',objs,out/'menu.elf',{'.games':out/'menu.bin','.patch.item':out/'item.bin','.patch.image':out/'image.bin'})
    code=bytearray((out/'menu.bin').read_bytes());assert code[0x3c:0x40]==bytes.fromhex('4ff4c011');code[0x3c:0x40]=bytes.fromhex('4ff00001')
    table_at=(0xe46000+len(code)+3)&~3
    table=original[0x3de944:0x3de944+7*24]+struct.pack('<6I',0x5047,0,0,0x4f504751,sy['games_open']|1,0)
    assert table_at+len(table)<=0xe47000
    for address,length in ((0xe46000,4096),(0xf9c000,4096),(0xfb7000,4096),(0xfbe000,8192)):
        assert original[address:address+length]==b'\xff'*length
    image=bytearray(original)
    for address,data in ((0x50434,(out/'item.bin').read_bytes()),(0x50544,b'\x08\x25'),(0x505ac,struct.pack('<I',0x88000000+table_at)),(0x9ba68,(out/'image.bin').read_bytes()),(0xe46000,code),(table_at,table)):
        image[address:address+len(data)]=data
    fixed=ROOT/'firmware/qualified/launcher'
    resident_flags=['--target=armv7-none-eabi','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-Os','-std=c11','-ffreestanding','-fno-builtin','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-Wall','-Wextra','-Werror']
    resident_sources=('experiments/launcher/anchor.c','experiments/launcher/anchor_sync.S','experiments/launcher/abi5_service_segments.c','experiments/runtime/resident.c','experiments/emerald/memory.c')
    objs=[compile_file(ROOT/n,out/(Path(n).stem+'.o'),resident_flags) for n in resident_sources]
    link(fixed/'resident.ld',objs,out/'resident.elf',{'.image':out/'resident.bin'})
    resident=(out/'resident.bin').read_bytes()
    assert sha(resident)=='f0ff7da21c6d93d35aace543dd1c77d6cbe7339f22125c5a81f7359e5e2ca959','Resident layout changed; do not use stale function addresses'
    inc=(ROOT/'sdk',ROOT/'experiments/launcher',ROOT/'experiments/runtime')
    objs=[]
    for src in (fixed/'candidate_loader.c',fixed/'hooks.S',ROOT/'experiments/runtime/mocor_time.c'):
        objs.append(compile_file(src,out/(src.name+'.o'),FLAGS+(CF if src.suffix=='.c' else []),inc))
    objs.append(compile_file(fixed/'fat_mirror.c',out/'fat_mirror.o',FLAGS+['-Oz','-ffreestanding','-fno-builtin','-fno-strict-aliasing','-ffunction-sections','-fdata-sections','-Wall','-Wextra','-Werror']))
    objs.append(compile_file(fixed/'fat_hook.s',out/'fat_hook.o',FLAGS))
    sections={'.patch.fat':'fat-hook.bin','.durability':'durability.bin','.loader':'loader.bin','.resident.glue':'resident-glue.bin','.patch.window':'hook.bin'}
    link(fixed/'candidate.ld',objs,out/'launcher.elf',{s:out/n for s,n in sections.items()})
    patches=((0xfb7000,resident,4096),(0xfb7e80,(out/'resident-glue.bin').read_bytes(),384),(0xfbe000,(out/'loader.bin').read_bytes(),8192),(0xf9c000,(out/'durability.bin').read_bytes(),4096),(0x49fec,(out/'fat-hook.bin').read_bytes(),4),(0xe46000,(out/'hook.bin').read_bytes(),4))
    for address,data,limit in patches:
        assert 0<len(data)<=limit;image[address:address+len(data)]=data
    assert sha(image)==VERIFIED_USER,'Candidate does not match qualified launcher; no partition output created'
    (out/'User-Games.bin').write_bytes(image)
    return dict(original_sha256=sha(original),candidate_sha256=sha(image),matches_qualified_user=True)

def os_image(original,profile,out):
    src=ROOT/'firmware/qualified/boot';folder=out/'boot';shutil.copytree(src,folder,dirs_exist_ok=True)
    (folder/'peaq_save_probe_profile.h').write_text(sd_profile_header(profile),newline='\n')
    flags=['--target=arm-none-eabi','-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access','-Oz','-ffreestanding','-fno-builtin','-fno-PIE','-fno-strict-aliasing','-ffunction-sections','-fdata-sections','-fno-unwind-tables','-fno-asynchronous-unwind-tables','-Wall','-Wextra','-Werror']
    objs=[]
    for n in ('menu_boot_entry.s','menu_boot.c','sd_boot_board.c','fat_save_map.c','boot_ticket.c','boot_watchdog.c','support.c'):
        objs.append(compile_file(folder/n,folder/(Path(n).stem+'.o'),flags+(['-Wno-unused-command-line-argument'] if n.endswith('.s') else []),(folder,ROOT/'experiments/runtime')))
    run([tool('ld.lld'),'-T',folder/'boot.ld','--gc-sections',*objs,'-o',folder/'boot.elf'])
    assert not run([tool('llvm-nm'),'-u',folder/'boot.elf']).strip()
    run([tool('llvm-objcopy'),'-O','binary','-j','.text',folder/'boot.elf',folder/'boot.bin'])
    text=(src/'bridge.s').read_text();assert text.count('@BOOT_FILE@')==1
    (folder/'bridge.s').write_text(text.replace('@BOOT_FILE@',(folder/'boot.bin').as_posix()),newline='\n')
    gate_flags=[f for f in flags if f not in ('-mno-unaligned-access','-fno-strict-aliasing')]
    objs=[compile_file(folder/(n+'.c'),folder/(n+'.o'),gate_flags) for n in ('retained_gate','retained_request')]
    objs.insert(0,compile_file(folder/'bridge.s',folder/'bridge.o',['--target=arm-none-eabi','-mcpu=cortex-a7']))
    link(folder/'bridge.ld',objs,folder/'bridge.elf',{'.bridge':folder/'bridge.bin'},gc=True)
    bridge=(folder/'bridge.bin').read_bytes();at=0x81579e00-0x80a05e00
    assert struct.unpack_from('<I',original,0x208)[0]==0x80a06230 and at+len(bridge)<len(original) and len(bridge)%4==0
    assert original[at:at+len(bridge)]==b'\xff'*len(bridge)
    image=bytearray(original);image[at:at+len(bridge)]=bridge
    struct.pack_into('<I',image,48,at+len(bridge)-512);struct.pack_into('<I',image,0x208,0x81579e00)
    (out/'OS-MenuBoot.bin').write_bytes(image)
    return dict(original_sha256=sha(original),candidate_sha256=sha(image),bridge_bytes=len(bridge),requires_same_local_sd_profile_as_runtime=True,sd_profile_sha256=sd_profile_sha256(profile))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--original-user',type=Path,required=True);p.add_argument('--original-os',type=Path)
    p.add_argument('--sd-profile',type=Path);p.add_argument('--out',type=Path,default=ROOT/'build/known-firmware')
    a=p.parse_args();user=a.original_user.read_bytes()
    if len(user)!=16*1024*1024 or sha(user)!=KNOWN_USER:raise ValueError('Unsupported original User image; nothing written')
    original_os=None;profile=None
    if a.original_os:
        original_os=a.original_os.read_bytes()
        if len(original_os)!=14*1024*1024 or sha(original_os)!=KNOWN_OS:raise ValueError('Unsupported original OS image; nothing written')
        if a.sd_profile is None:raise ValueError('--original-os requires your own --sd-profile')
        profile=load_sd_profile(a.sd_profile)
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    report=dict(user=user_image(user,out),hardware_access=False,flash_performed=False,output_contains_vendor_data=True)
    if original_os is not None:report['os']=os_image(original_os,profile,out)
    (out/'build-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
