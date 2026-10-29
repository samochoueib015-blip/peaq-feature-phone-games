"""Build the upstream Emerald preprocessing tools on Windows with LLVM/MSVC.

Leaves the pinned source checkout untouched. Optional pinned libpng/zlib source
arguments also build gbagfx; generated assets and the full game remain separate.
"""
from pathlib import Path
import argparse,subprocess,json,concurrent.futures,hashlib
from toolchain import tool
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--upstream',type=Path,required=True)
p.add_argument('--zlib',type=Path);p.add_argument('--libpng',type=Path);p.add_argument('--cmake',type=Path)
args=p.parse_args();up=args.upstream.resolve();out=ROOT/'build/emerald-tools';out.mkdir(parents=True,exist_ok=True)
pin='3c35e5b957dff6c578878031e0c4a7c8d7691401'
assert subprocess.check_output(['git','-C',str(up),'rev-parse','HEAD'],text=True).strip()==pin
compat=out/'compat';compat.mkdir(exist_ok=True)
# Only preproc needs getopt; implement its documented short-option interface
# without changing any upstream source or requiring a POSIX runtime.
(compat/'unistd.h').write_text('''#pragma once
#include <cstring>
static int optind=1;
static char *optarg=nullptr;
static int getopt(int argc,char *const argv[],const char *options) {
 static unsigned offset=1;optarg=nullptr;
 if(optind>=argc)return -1;
 const char *arg=argv[optind];
 if(offset==1 && (arg[0]!='-' || !arg[1]))return -1;
 if(offset==1 && !std::strcmp(arg,"--")){optind++;return -1;}
 int ch=(unsigned char)arg[offset++];const char *found=std::strchr(options,ch);
 if(!found){if(!arg[offset]){offset=1;optind++;}return '?';}
 if(found[1]==':'){
  if(arg[offset])optarg=argv[optind]+offset;
  else if(optind+1<argc)optarg=argv[++optind];
  else {optind++;offset=1;return '?';}
  optind++;offset=1;
 } else if(!arg[offset]){optind++;offset=1;}
 return ch;
}
''')
names=['preproc','mapjson','jsonproc','mid2agb','wav2agb','scaninc']
def build(name):
 src=up/'tools'/name;target=out/(name+'.exe')
 sources=sorted(src.glob('*.cpp'))
 # Avoid preproc/io.h shadowing the Windows SDK's <io.h>.
 includes=['-I',str(src)] if name=='jsonproc' else []
 cmd=[str(tool('clang++')),'-std=c++17','-O2','-D_CRT_SECURE_NO_WARNINGS','-I',str(compat),*includes,*(str(s) for s in sources),'-o',str(target)]
 r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace')
 (out/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise RuntimeError(name+': '+r.stderr[-2000:])
 return {'tool':name,'bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:reports=list(pool.map(build,names))
deps={};graphics_ready=False
if args.zlib or args.libpng:
 if not (args.zlib and args.libpng and args.cmake):p.error('--zlib, --libpng and --cmake are required together')
 zsrc=args.zlib.resolve();psrc=args.libpng.resolve();cmake=str(args.cmake.resolve())
 for name,src,expected in [('zlib',zsrc,'767c4c947852e143f582c85f14cf573411df1b35'),('libpng',psrc,'0b0876829d77db0827a8cc2c4326f63be22b2aa1')]:
  revision=subprocess.check_output(['git','-C',str(src),'rev-parse','HEAD'],text=True).strip()
  assert revision==expected,(name,revision);deps[name]=revision
 def run(name,cmd):
  r=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8',errors='replace');(out/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
  if r.returncode:raise RuntimeError(name+': '+(r.stdout+r.stderr)[-2500:])
 # Keep MSBuild's nested try-compile paths below legacy Windows path limits.
 zb=ROOT/'build/ez';pb=ROOT/'build/ep'
 run('zlib-configure',[cmake,'-S',str(zsrc),'-B',str(zb),'-G','Visual Studio 17 2022','-A','x64','-DZLIB_BUILD_SHARED=OFF','-DZLIB_BUILD_TESTING=OFF','-DZLIB_INSTALL=OFF'])
 run('zlib-build',[cmake,'--build',str(zb),'--config','Release','--target','zlibstatic','--parallel','3'])
 libraries=list((zb/'Release').glob('*.lib'));assert len(libraries)==1;zlib=libraries[0]
 run('png-configure',[cmake,'-S',str(psrc),'-B',str(pb),'-G','Visual Studio 17 2022','-A','x64','-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF','-DPNG_TOOLS=OFF','-DZLIB_LIBRARY='+str(zlib),'-DZLIB_INCLUDE_DIR='+str(zsrc)])
 run('png-build',[cmake,'--build',str(pb),'--config','Release','--target','png_static','--parallel','3'])
 libraries=list((pb/'Release').glob('libpng*_static.lib'));assert len(libraries)==1;png=libraries[0]
 src=up/'tools/gbagfx';target=out/'gbagfx.exe'
 run('gbagfx-build',[str(tool('clang')),'-std=c11','-O2','-D_CRT_SECURE_NO_WARNINGS','-DPNG_SKIP_SETJMP_CHECK','-DPNG_STATIC',
  '-I',str(pb),'-I',str(psrc),'-I',str(zb),'-I',str(zsrc),*(str(s) for s in sorted(src.glob('*.c'))),str(png),str(zlib),'-o',str(target)])
 run('gbagfx-smoke',[str(target),str(up/'graphics/pokemon/spinda/spots/spot_0.png'),str(out/'spinda-spot.1bpp'),'-plain','-data_width','2'])
 assert (out/'spinda-spot.1bpp').stat().st_size>0
 reports.append({'tool':'gbagfx','bytes':target.stat().st_size,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
 graphics_ready=True
report={'upstream_commit':pin,'tools':reports,'dependency_commits':deps,'graphics_converter_ready':graphics_ready,'full_engine_ready':False}
(out/'build-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
