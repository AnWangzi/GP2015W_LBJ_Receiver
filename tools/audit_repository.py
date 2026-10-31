"""Audit the Git index snapshot; never print matched sensitive contents."""
import argparse
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PATTERNS = {
    'access-token': r'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|AKIA[A-Z0-9]{16})',
    'private-key': r'-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----',
    'personal-path': r'(?i)(?:[a-z]:[\\/](?:Users|Documents)[\\/]|/mnt/[a-z]/|/home/[\w.-]+/)',
    'private-email': r'(?i)\b[A-Z0-9._%+-]+@(?:qq|163|126|gmail|hotmail|outlook)\.com\b',
    'credential-url': r'(?i)https?://[^\s/@:]+:[^\s/@]+@',
    'credential-assignment': r'''(?i)(?:api_key|access_token|auth_token|password)\s*[:=]\s*["'][A-Za-z0-9_+/=-]{20,}["']''',
}
BAD_PARTS = {'refs', 'local-archive', 'diagnostics', 'serial_deps', 'openocd',
             'restore_original', 'field-results', '.aws', '.codex', '.agents', '__pycache__'}
BAD_SUFFIXES = {'.bin', '.lbju', '.axf', '.elf', '.o', '.dll', '.exe', '.log',
                '.jpg', '.jpeg', '.png', '.pdf', '.zip', '.pyc', '.uvoptx', '.pem', '.key'}


def violations(name, data):
    path = PurePosixPath(name)
    if (set(path.parts) & BAD_PARTS or path.suffix.lower() in BAD_SUFFIXES or
            '.uvguix.' in name or path.name.startswith('.env')):
        return [(name, 0, 'local-or-generated-file')]
    try:
        text = data.decode('utf-8-sig')
    except UnicodeDecodeError:
        return [(name, 0, 'non-UTF8-file')]
    found = []
    for number, line in enumerate(text.splitlines(), 1):
        for category, pattern in PATTERNS.items():
            if re.search(pattern, line):
                found.append((name, number, category))
    return found


def index_files():
    entries = subprocess.check_output(['git', 'ls-files', '--stage', '-z'], cwd=str(ROOT))
    rows = []
    for record in entries.split(b'\0'):
        if not record:
            continue
        header, name = record.split(b'\t', 1)
        mode, sha, stage = header.split()
        if stage != b'0' or mode not in (b'100644', b'100755'):
            raise ValueError('Unmerged or non-regular index entry')
        rows.append((name.decode('utf-8'), sha))
    if not rows:
        raise ValueError('Git index is empty; stage the reviewed files first')
    objects = subprocess.check_output(['git', 'cat-file', '--batch'],
        input=b'\n'.join(sha for _, sha in rows) + b'\n', cwd=str(ROOT))
    offset = 0
    result = {}
    for name, sha in rows:
        end = objects.index(b'\n', offset)
        header = objects[offset:end].split()
        if len(header) != 3 or header[0] != sha or header[1] != b'blob':
            raise ValueError('Unexpected Git object')
        size = int(header[2]); offset = end + 1
        result[name] = objects[offset:offset + size]; offset += size + 1
    return result


def self_test():
    token = 'gh' + 'p_' + 'X' * 32
    assert violations('src/x.c', token.encode())
    assert violations('docs/x.md', ('demo' + '@' + 'qq.com').encode())
    assert violations('docs/x.md', bytes([67, 58, 92]) + b'Users/private/file')
    assert violations('keilproj/diagnostics/a.c', b'void f(void) {}')
    assert violations('backup.bin', b'\0')
    assert not violations('src/main.c', b'const char *status = "RX08";')
    assert not violations('LICENSE', b'Copyright holder <contact@example.org>')
    print('PASS repository audit self-tests')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='scan the complete Git index snapshot')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    self_test()
    if args.self_test and not args.staged:
        return 0
    files = index_files()
    found = [item for name, data in files.items() for item in violations(name, data)]
    required = ['README.md', 'VERSION', 'CHANGELOG.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md',
                'LICENSES/Apache-2.0.txt', 'LICENSES/Unifont-LICENSE.txt',
                'assets/fonts/unifont-subset.hex', 'keilproj/src/main.c']
    for name in required:
        if name not in files:
            found.append((name, 0, 'missing-required-file'))
    version = files.get('VERSION', b'').decode().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        found.append(('VERSION', 0, 'invalid-version'))
    if ('**' + version + '**').encode() not in files.get('README.md', b''):
        found.append(('README.md', 0, 'version-mismatch'))
    if ('## ' + version + ' ').encode() not in files.get('CHANGELOG.md', b''):
        found.append(('CHANGELOG.md', 0, 'version-mismatch'))
    for name, line, category in found:
        print('%s:%d: %s' % (name, line, category))
    if found:
        print('FAIL %d repository audit findings; matched contents were not printed' % len(found))
        return 1
    print('PASS Git index: %d UTF-8 files, version %s; no configured sensitive-content findings' %
          (len(files), version))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, subprocess.CalledProcessError) as error:
        print('Repository audit could not complete:', str(error))
        sys.exit(1)
