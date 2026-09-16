"""Package only native product artifacts; never include the license issuer/key."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--build', type=Path, required=True)
p.add_argument('--platform', choices=['linux-x86_64', 'windows-x86_64', 'macos-universal'], required=True)
p.add_argument('--commit', required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
artifacts = a.build.resolve() / 'NeuroKore_artefacts' / 'Release'
if not artifacts.is_dir():
    raise SystemExit(f'Missing native Release artifacts: {artifacts}')
output = root / 'release'
output.mkdir(exist_ok=True)
stem = f'NEUROKORE-0.6.4-beta-{a.platform}'
with tempfile.TemporaryDirectory() as temp:
    package = Path(temp) / stem
    package.mkdir()
    formats = ['VST3', 'Standalone'] + (['AU'] if a.platform == 'macos-universal' else [])
    for fmt in formats:
        src = artifacts / fmt
        suffix = {'VST3': '.vst3', 'AU': '.component', 'Standalone': '.app' if a.platform == 'macos-universal' else '.exe' if a.platform == 'windows-x86_64' else ''}[fmt]
        candidates = [x for x in src.iterdir() if x.name.startswith('NEUROKORE') and (x.suffix == suffix if suffix else x.is_file())]
        if len(candidates) != 1:
            raise SystemExit(f'Expected exactly one {fmt} artifact: {candidates}')
        binary = candidates[0]
        dest = package / fmt / binary.name
        dest.parent.mkdir()
        if binary.is_dir():
            shutil.copytree(binary, dest, symlinks=True)
        else:
            shutil.copy2(binary, dest)
        if a.platform == 'macos-universal':
            # Ad-hoc integrity signature; Developer ID notarisation needs owner credentials.
            subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(dest)], check=True)
            subprocess.run(['codesign', '--verify', '--deep', '--strict', str(dest)], check=True)
    for src, name in [('LICENSE', 'LICENSE.txt'), ('installer/EULA.txt', 'EULA.txt'), ('docs/ALPHA_TESTER_AGREEMENT.txt', 'ALPHA_TESTER_AGREEMENT.txt'), ('docs/ALPHA_INSTALL.txt', 'INSTALL.txt')]:
        shutil.copy2(root / src, package / name)
    third_party = package / 'ThirdPartyLicenses'
    third_party.mkdir()
    juce = a.build / '_deps/juce-src'
    if not (juce / 'LICENSE.md').is_file():
        raise SystemExit('JUCE license missing from fetched dependency')
    shutil.copy2(juce / 'LICENSE.md', third_party / 'JUCE-LICENSE.md')
    # Preserve upstream licence notices for shipped source dependencies/assets.
    for base in [juce / 'modules', a.build / '_deps/asmjit-src', root / 'web/public', root / 'src/third_party']:
        if base.exists():
            for f in base.rglob('*'):
                if f.is_file() and f.name.lower().startswith(('license', 'licence', 'copying', 'ofl')):
                    name = base.name + '-' + '-'.join(f.relative_to(base).parts)
                    shutil.copy2(f, third_party / name)
    (package / 'BUILD.json').write_text(json.dumps({'version':'0.6.4-beta','commit':a.commit,'platform':a.platform,'formats':formats,'signing':'ad-hoc; not notarised' if a.platform == 'macos-universal' else 'unsigned'}, indent=2))
    archive = shutil.make_archive(str(output / stem), 'zip' if a.platform == 'windows-x86_64' else 'gztar', temp, stem)
    file = Path(archive)
    (output / (file.name + '.sha256')).write_text(hashlib.sha256(file.read_bytes()).hexdigest() + '  ' + file.name + '\n')
    print(file)
