"""Run a pinned external host validator against the actual native bundles."""
import argparse
from pathlib import Path
import platform
import subprocess
import urllib.request
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, default=Path('build'))
a = p.parse_args()
os_name = {'Darwin': 'macOS', 'Windows': 'Windows', 'Linux': 'Linux'}[platform.system()]
root = Path(__file__).resolve().parents[1]
work = root / 'build-validator'
work.mkdir(exist_ok=True)
archive = work / 'pluginval.zip'
url = f'https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_{os_name}.zip'
urllib.request.urlretrieve(url, archive)
with zipfile.ZipFile(archive) as z:
    z.extractall(work)
executable = work / ('pluginval.app/Contents/MacOS/pluginval' if os_name == 'macOS' else 'pluginval.exe' if os_name == 'Windows' else 'pluginval')
executable.chmod(0o755)
logs = a.build.resolve() / 'validation'
logs.mkdir(exist_ok=True)
artifacts = a.build.resolve() / 'NeuroKore_artefacts' / 'Release'
plugins = list((artifacts / 'VST3').glob('*.vst3'))
if os_name == 'macOS':
    plugins += list((artifacts / 'AU').glob('*.component'))
if not plugins:
    raise SystemExit('No native plugins to validate')
for plugin in plugins:
    if os_name == 'macOS':
        subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(plugin)], check=True)
    command = [str(executable), '--strictness-level', '5', '--output-dir', str(logs), '--validate', str(plugin)]
    if os_name == 'Linux':
        command = ['xvfb-run', '-a'] + command
    subprocess.run(command, check=True, timeout=600)
