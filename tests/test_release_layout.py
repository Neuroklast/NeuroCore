"""Source contracts for complete, portable release packages."""
import re
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text()
label = re.search(r'set\(NEUROKORE_VERSION_LABEL "([^"]+)"\)', cmake).group(1)
for path in ['Resources/irs/American IR 01.wav', 'Resources/img/nk_logo.rc']:
    assert (root / path).is_file() and path in cmake, path
assert 'WebBinaryData::neurokore_web_dist_zip' in (root / 'src/bridge/WebAssets.cpp').read_text()
assert 'GIT_TAG        master' not in cmake
alpha = (root / 'scripts/package_alpha.py').read_text()
assert 'NEUROKORE_VERSION_LABEL' in alpha and not re.search(r'0\.\d+\.\d+-beta', alpha)
assert f'NEUROKORE {label}' in (root / 'docs/ALPHA_INSTALL.txt').read_text()
print('Portable asset paths, embedded Linux editor and pinned JIT dependency: PASS')
