"""Build Emerald's original sound pipeline and full ARM game archive locally.

Follows rp2350/gen_sound_assets.sh and build_objs.sh at the pinned revision,
changing the CPU to Cortex-A7 and using the already-built Windows host tools.
The archive still requires a PEAQ runtime; this is not a phone-ready package.
"""
from pathlib import Path
import argparse, concurrent.futures, hashlib, json, shlex, subprocess
from prepare_emerald_source import verify_prepared_source

ROOT=Path(__file__).resolve().parents[1]
PIN='3c35e5b957dff6c578878031e0c4a7c8d7691401'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--upstream',type=Path,required=True)
p.add_argument('--toolchain',type=Path,required=True)
p.add_argument('--jobs',type=int,default=4)
p.add_argument('--stage',choices=['sound','engine','all'],default='all')
p.add_argument('--relocatable',action='store_true')
a=p.parse_args();up=a.upstream.resolve();tc=a.toolchain.resolve()/'bin'
assert 1 <= a.jobs <= 16
preparation=verify_prepared_source(up)
assert preparation['upstream_commit']==PIN
out=ROOT/('build/emerald-module' if a.relocatable else 'build/emerald-engine');out.mkdir(parents=True,exist_ok=True)
objdir=out/'obj';objdir.mkdir(exist_ok=True);logdir=out/'logs';logdir.mkdir(exist_ok=True)
host=ROOT/'build/emerald-tools'
hostreport=json.loads((host/'build-report.json').read_text())
for entry in hostreport['tools']:
    assert hashlib.sha256((host/(entry['tool']+'.exe')).read_bytes()).hexdigest()==entry['sha256']
def arm(name):return str(tc/('arm-none-eabi-'+name+'.exe'))
cc=arm('gcc');assembler=arm('as');pp=str(host/'preproc.exe')
cflags=['-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access','-O2','-ffreestanding',
        '-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-Wno-builtin-declaration-mismatch',
        '-Wno-attributes','-Wno-implicit-function-declaration','-fno-common']
includes=['-iquote','include']
if a.relocatable:
    includes=['-iquote',str(up/'include')]
    cflags+=['-fpie','-fvisibility=hidden','-DPEAQ_EMERALD_RELOCATABLE=1']

def command(cmd,input=None):
    r=subprocess.run([str(c) for c in cmd],cwd=up,input=input,capture_output=True,timeout=180)
    if r.returncode:
        raise RuntimeError(str(cmd)+'\n'+(r.stdout+r.stderr).decode(errors='replace')[-7000:])
    return r.stdout,r.stderr

def parallel(label,jobs,func):
    completed=[];errors=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futures={pool.submit(func,job):job for job in jobs}
        for future in concurrent.futures.as_completed(futures):
            job=futures[future]
            try:completed.append(future.result())
            except Exception as exc:
                errors.append({'job':str(job),'error':str(exc)})
                print(label+' failed: '+str(job),flush=True)
            if (len(completed)+len(errors))%40==0:
                print(f'{label}: {len(completed)} completed, {len(errors)} failed / {len(jobs)}',flush=True)
    if errors:
        (out/(label+'-failures.json')).write_text(json.dumps(errors,indent=2)+'\n')
        raise RuntimeError(f'{label}: {len(errors)} failed; see build/emerald-engine/{label}-failures.json')
    return completed

if a.stage in ('sound','all'):
    waves=sorted((up/'sound').rglob('*.wav'))
    def wav(source):
        relative=source.relative_to(up).as_posix();dest=source.with_suffix('.bin')
        flags=['-b','-c','-l','1','--no-pad'] if 'cries' in source.parts else ['-b']
        command([host/'wav2agb.exe',*flags,relative,dest.relative_to(up).as_posix()])
        assert dest.stat().st_size>0
        return dest
    samples=parallel('samples',waves,wav)
    middir=up/'sound/songs/midi';jobs=[]
    for line in (middir/'midi.cfg').read_text().splitlines():
        line=line.strip()
        if not line or line.startswith('#'):continue
        name,flags=line.split(':',1);source=middir/name
        if source.suffix!='.mid':source=source.with_suffix('.mid')
        if source.exists():jobs.append((source,shlex.split(flags)))
    def song(job):
        source,flags=job;dest=source.with_suffix('.s')
        command([host/'mid2agb.exe',source.relative_to(up).as_posix(),dest.relative_to(up).as_posix(),*flags])
        assert dest.stat().st_size>0
        return dest
    songs=parallel('songs',jobs,song)
    missing=set(middir.glob('*.mid'))-{source for source,_ in jobs}
    assert not missing,missing
    report={'upstream_commit':PIN,'samples':len(samples),'songs':len(songs),'all_passed':True,
            'audio_on_phone_tested':False}
    (out/'sound-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)

if a.stage in ('engine','all'):
    assetreport=json.loads((ROOT/'build/emerald-assets/report.json').read_text())
    assert assetreport['graphics_pipeline_completed'] and assetreport['upstream_commit']==PIN
    sound_report=out/'sound-report.json'
    if a.relocatable and not sound_report.exists():sound_report=ROOT/'build/emerald-engine/sound-report.json'
    assert json.loads(sound_report.read_text())['all_passed']
    sources=sorted((up/'src').glob('*.c'))+[up/'rp2350'/name for name in ('bios.c','asm_stubs.c','m4a_1.c')]
    assert len({s.stem for s in sources})==len(sources)
    def compile_c(source):
        rel=source.relative_to(up).as_posix();obj=objdir/(source.stem+'.o');notes=[]
        data,err=command([cc,'-E',*includes,'-DMODERN=1','-DRP2350=1',
                         *(['-DPEAQ_EMERALD_RELOCATABLE=1'] if a.relocatable else []),*(['-DWASM=1'] if source.name=='main.c' else []),rel]);notes.append(err)
        data,err=command([pp,'-i','-g','build/assets',rel,'charmap.txt'],data);notes.append(err)
        _,err=command([cc,'-x','c',*cflags,'-c','-','-o',obj],data);notes.append(err)
        (logdir/(source.stem+'.log')).write_bytes(b''.join(notes))
        return obj
    objects=parallel('C',sources,compile_c)
    def data_asm(job):
        source,sound=job;rel=source.relative_to(up).as_posix();obj=objdir/('data_'+source.stem+'.o');notes=[]
        data,err=command([pp,rel,'charmap.txt']);notes.append(err)
        includes=['-I','include']+(['-I','.'] if sound else [])
        data,err=command([cc,'-E',*includes,'-'],data);notes.append(err)
        data,err=command([pp,'-ie',rel,'charmap.txt'],data);notes.append(err)
        inc=['-I','include']+(['-I','.','-I','sound'] if sound else [])
        _,err=command([assembler,'-mcpu=cortex-a7','-mthumb',*inc,'-o',obj,'-'],data);notes.append(err)
        (logdir/('data_'+source.stem+'.log')).write_bytes(b''.join(notes))
        return obj
    jobs=[(up/'data'/(n+'.s'),False) for n in ('maps','map_events','event_scripts','battle_scripts_1','battle_scripts_2','battle_ai_scripts','battle_anim_scripts')]
    jobs.append((up/'data/sound_data.s',True))
    objects+=parallel('data',jobs,data_asm)
    def plain_asm(source):
        obj=objdir/('song_'+source.stem+'.o')
        command([assembler,'-mcpu=cortex-a7','-mthumb','-I','sound','-o',obj,source.relative_to(up).as_posix()])
        return obj
    objects+=parallel('song_objects',sorted((up/'sound/songs/midi').glob('*.s'))+[up/'rp2350/sound_symbols.s'],plain_asm)
    archive=out/'libpokeemerald.a'
    # Use a response file so Windows' command-line limit cannot truncate members.
    response=out/'archive.rsp';response.write_text('\n'.join('"'+s.as_posix()+'"' for s in sorted(objects))+'\n')
    if archive.exists():archive.unlink()
    command([arm('ar'),'rcs',archive,'@'+str(response)])
    members=command([arm('ar'),'t',archive])[0].decode().splitlines();assert len(members)==len(objects)
    undefined=command([arm('nm'),'-u',archive])[0];(out/'undefined.txt').write_bytes(undefined)
    report={'upstream_commit':PIN,'all_compiled':True,'cpu':'cortex-a7','float_abi':'soft',
            'C_sources':len(sources),'archive_members':len(members),'archive_bytes':archive.stat().st_size,
            'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'relocatable':a.relocatable,
            'phone_package_ready':False,'engine_executed':False}
    (out/'build-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
