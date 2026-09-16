"""Source contracts for complete, portable release packages."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
cmake = (root / 'CMakeLists.txt').read_text()
for path in ['Resources/irs/American IR 01.wav', 'Resources/img/nk_logo.rc']:
    assert (root / path).is_file() and path in cmake, path
assert 'WebBinaryData::neurokore_web_dist_zip' in (root / 'src/bridge/WebAssets.cpp').read_text()
assert 'GIT_TAG        master' not in cmake
print('Portable asset paths, embedded Linux editor and pinned JIT dependency: PASS')
