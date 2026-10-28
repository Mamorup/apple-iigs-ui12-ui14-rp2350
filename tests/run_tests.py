"""Run native HAL-stub regressions; no hardware access or device resets."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cc', default=os.environ.get('CC') or shutil.which('gcc') or shutil.which('cc'))
args = parser.parse_args()
if not args.cc:
    parser.error('A native C compiler is required; install GCC or use --cc')
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='iigs-native-') as folder:
    for name in ['input_regression', 'watchdog_health_test']:
        output = Path(folder) / (name + ('.exe' if os.name == 'nt' else ''))
        subprocess.run([args.cc, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                        str(root / 'tests' / (name + '.c')), '-o', str(output)], check=True)
        subprocess.run([str(output)], check=True)
