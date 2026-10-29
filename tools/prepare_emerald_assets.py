"""Use the pinned upstream asset pipeline with locally built Windows tools.

Generated files stay in the ignored upstream checkout; original game sources
and conversion rules are not changed. No ROM or physical handset is accessed.
"""
from pathlib import Path
import argparse,subprocess,json,os,shutil,importlib.util,hashlib,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--upstream',type=Path,required=True);p.add_argument('--make',type=Path,required=True)
p.add_argument('--shell-tools',type=Path,required=True);p.add_argument('--toolchain',type=Path,required=True)
args=p.parse_args();up=args.upstream.resolve();host=ROOT/'build/emerald-tools';out=ROOT/'build/emerald-assets';out.mkdir(exist_ok=True)
pin='3c35e5b957dff6c578878031e0c4a7c8d7691401'
assert subprocess.check_output(['git','-C',str(up),'rev-parse','HEAD'],text=True).strip()==pin
tools=json.loads((host/'build-report.json').read_text());assert tools['graphics_converter_ready']
for item in tools['tools']:
 src=host/(item['tool']+'.exe');assert hashlib.sha256(src.read_bytes()).hexdigest()==item['sha256']
 shutil.copy2(src,up/'tools'/item['tool']/src.name)
make=host/'make.exe';shutil.copy2(args.make.resolve(),make)
os.environ['PATH']=os.pathsep.join([str(host),str(args.toolchain.resolve()/'bin'),str(args.shell_tools.resolve()),os.environ['PATH']])
os.chdir(up)
def run(cmd):
 r=subprocess.run([str(s) for s in cmd],capture_output=True,text=True)
 with (out/'commands.log').open('a',encoding='utf-8') as f:f.write(str(cmd)+'\n'+r.stdout+r.stderr+'\n')
 if r.returncode:raise RuntimeError((r.stdout+r.stderr)[-3000:])
run([make,'generated','NODEP=1','SETUP_PREREQS=0'])
maps=sorted(Path('data/maps').glob('*/map.json'))
# `generated` produces group/event constants, but not the per-map leaves
# needed by the original maps.s and map_events.s assembly pipelines.
for start in range(0,len(maps),40):
 targets=[f.with_name('header.inc').as_posix() for f in maps[start:start+40]]
 run([make,'-j4','NODEP=1','SETUP_PREREQS=0',*targets])
for f in maps:
 for name in ('header.inc','events.inc','connections.inc'):assert f.with_name(name).is_file()
print('Upstream generated map and text dependencies',flush=True)
spec=importlib.util.spec_from_file_location('emerald_assets',up/'tools/generate_wasm_assets.py')
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
def ensure(target):
 if not Path(target).exists():run([make,'NODEP=1','SETUP_PREREQS=0',target])
mod.ensure_make_target=ensure
mod.GFX=up/'tools/gbagfx/gbagfx.exe'
mod.main()
report={'upstream_commit':pin,'graphics_pipeline_completed':True,'maps_generated':len(maps),
 'make_sha256':hashlib.sha256(make.read_bytes()).hexdigest(),
 'asset_files':sum(1 for f in (up/'build/assets').rglob('*') if f.is_file()),
 'assets_bytes':sum(f.stat().st_size for f in (up/'build/assets').rglob('*') if f.is_file()),'full_engine_ready':False}
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
