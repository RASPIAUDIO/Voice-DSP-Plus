"""Run portable USB state/filter tests without accessing hardware."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default='cl' if os.name == 'nt' else 'cc')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    compiler = shutil.which(args.cc)
    if not compiler:
        raise RuntimeError('C compiler unavailable; use a developer shell or --cc')
    user = ROOT/'XMOS/overlay/sources/app_xvf3800/src/user_config'
    # Isolate the actual tile-1 pure filter math, excluding hardware I/O.
    source = (user/'voicedsp_v2_equalizer.c').read_text()
    pure = source.split('#if ON_TILE(1)\n', 1)[1].split('\n#else', 1)[0]
    math = output/'filter_math.c'
    math.write_text(pure, encoding='utf-8')
    msvc = Path(compiler).stem.lower() == 'cl'
    for test in sorted((ROOT/'tests').glob('test_*.c')):
        exe = output/(test.stem + ('.exe' if os.name == 'nt' else ''))
        inputs = [str(test)]
        if test.stem in ('test_fixed_filter', 'test_parametric'):
            inputs.append(str(math))
        includes = [ROOT, ROOT/'include', ROOT/'overlay', user]
        if msvc:
            command = [compiler, '/nologo', '/std:c11', '/W4']
            command += [f'/I{p}' for p in includes] + inputs + [f'/Fe:{exe}', f'/Fo:{output}/']
        else:
            command = [compiler, '-std=c11', '-Wall', '-Wextra']
            command += [f'-I{p}' for p in includes] + inputs + ['-lm', '-o', str(exe)]
        built = subprocess.run(command, cwd=output, text=True, capture_output=True)
        (output/(test.stem+'.build.log')).write_text(built.stdout+built.stderr)
        if built.returncode:
            print(built.stdout+built.stderr)
        built.check_returncode()
        result = subprocess.run([str(exe)], text=True, capture_output=True, check=True)
        (output/(test.stem+'.log')).write_text(result.stdout+result.stderr)
        print(result.stdout.strip())


if __name__ == '__main__':
    main()
