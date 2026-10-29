"""Build the complete DoomGeneric engine for the PEAQ ARM architecture.

Uses the pinned upstream checkout and an Arm GNU toolchain with Newlib.
This produces a relocatable engine object and an explicit list of runtime
imports; a successful compile alone is not a runnable phone package.
"""
from pathlib import Path
import argparse,subprocess,json,re,hashlib,concurrent.futures
ROOT=Path(__file__).resolve().parents[1]
PIN='e257a09527062bfb986b6e5e0780839b6cbd5b71'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--upstream',type=Path,required=True)
parser.add_argument('--toolchain',type=Path,required=True)
parser.add_argument('--relocatable',action='store_true')
args=parser.parse_args();up=args.upstream.resolve();tc=args.toolchain.resolve()
commit=subprocess.check_output(['git','-C',str(up),'rev-parse','HEAD'],text=True).strip()
assert commit==PIN
src=up/'doomgeneric';out=ROOT/('build/doom-module' if args.relocatable else 'build/doom-engine');out.mkdir(parents=True,exist_ok=True)
suffix='.exe' if (tc/'bin/arm-none-eabi-gcc.exe').exists() else ''
gcc=tc/('bin/arm-none-eabi-gcc'+suffix);ld=tc/('bin/arm-none-eabi-ld'+suffix)
names=re.search(r'^SRC_DOOM = (.*)$',(src/'Makefile').read_text(),re.M)[1].split()
sources=[src/(s[:-2]+'.c') for s in names if s!='doomgeneric_xlib.o']
sources.append(ROOT/'experiments/doom/platform.c')
flags=['-mcpu=cortex-a7','-mthumb','-mfloat-abi=soft','-mno-unaligned-access','-Os','-ffunction-sections','-fdata-sections',
 '-fno-strict-aliasing','-fwrapv','-DNORMALUNIX','-DLINUX','-DSNDSERV','-D_DEFAULT_SOURCE',
 '-DDOOMGENERIC_RESX=320','-DDOOMGENERIC_RESY=200','-I',str(src)]
if args.relocatable:flags+=['-fpie','-fvisibility=hidden']

def compile_one(path):
 obj=out/(path.stem+'.o')
 p=subprocess.run([str(gcc),*flags,'-c',str(path),'-o',str(obj)],capture_output=True,text=True)
 return path,obj,p

results=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 for path,obj,p in pool.map(compile_one,sources):
  results.append((path,obj,p))
  if p.returncode:print(path.name+': '+p.stderr[:1800],flush=True)
(out/'compile.log').write_text('\n'.join(str(p)+'\n'+r.stdout+r.stderr for p,o,r in results))
failed=[p.name for p,o,r in results if r.returncode]
if failed:raise SystemExit('Compile failures: '+', '.join(failed))
engine=out/'doom-engine.o'
subprocess.run([str(ld),'-r',*(str(o) for p,o,r in results),'-o',str(engine)],check=True)
nm=tc/('bin/arm-none-eabi-nm'+suffix)
imports=subprocess.check_output([str(nm),'-u',str(engine)],text=True)
size=subprocess.check_output([str(tc/('bin/arm-none-eabi-size'+suffix)),str(engine)],text=True)
report={'compiled_sources':len(sources),'upstream_commit':commit,'engine_object_sha256':hashlib.sha256(engine.read_bytes()).hexdigest(),
 'size_output':size.strip(),'runtime_imports':imports.splitlines(),'phone_ready':False,'hardware_tested':False}
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
