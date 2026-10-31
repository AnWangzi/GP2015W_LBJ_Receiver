"""Record public build identity without host paths or installed-device data."""
import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent.parent
version = (ROOT / 'VERSION').read_text(encoding='ascii').strip()
assert re.fullmatch(r'\d+\.\d+\.\d+', version), 'Invalid VERSION'
try:
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=str(ROOT),
                                      stderr=subprocess.DEVNULL).decode().strip()
    dirty = bool(subprocess.check_output(['git', 'status', '--porcelain'], cwd=str(ROOT)))
except (OSError, subprocess.CalledProcessError):
    revision, dirty = None, None
folder = ROOT / 'keilproj/iap/release'
files = {'application': ROOT / 'keilproj/iap/ObjectsApp/lbj_app.bin',
         'boot': ROOT / 'keilproj/iap/ObjectsBoot/lbj_boot.bin',
         'uart_package': folder / 'lbj-update.lbju'}
info = {'version': version, 'git_commit': revision, 'working_tree_dirty': dirty,
        'built_at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'artifacts': {name: {'bytes': path.stat().st_size,
                            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                      for name, path in files.items()}}
(folder / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n', encoding='utf-8')
print('Build identity recorded: version', version, 'commit', revision or 'unversioned')
