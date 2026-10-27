"""Copies this project into the public repository (github.com/velxio/cute-desktop-robot),
as its vocat-eyes/ folder, leaving out everything private or generated, and refuses to
finish if any value of secrets.h shows up in the copy.

    python tools/publish_repo.py [path/to/cute-desktop-robot]
"""
import fnmatch
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST_REPO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'cute-desktop-robot')
DEST = os.path.join(DEST_REPO, 'vocat-eyes')

SKIP_DIRS = {'firmware_backup', 'build', 'dist', 'captures', '__pycache__', '.git'}
SKIP_FILES = ['secrets.h', '*.o', '*.d', '*.exe', '*.bak', '*.zip', 'arduino-cli.yaml']


def skipped(name):
    return any(fnmatch.fnmatch(name, p) for p in SKIP_FILES)


def main():
    secrets = []
    sp = os.path.join(HERE, 'secrets.h')
    if os.path.exists(sp):
        txt = open(sp, encoding='utf-8').read()
        secrets = [v for v in re.findall(r'#define\s+\w+\s+"([^"]+)"', txt) if len(v) >= 6]

    if os.path.isdir(DEST):
        shutil.rmtree(DEST)
    copied = 0
    for root, dirs, files in os.walk(HERE):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        rel = os.path.relpath(root, HERE)
        for f in files:
            if skipped(f):
                continue
            src = os.path.join(root, f)
            dst = os.path.join(DEST, rel, f)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            copied += 1

    # the guard: no secret value may leave the computer
    leaks = []
    for root, _, files in os.walk(DEST):
        for f in files:
            data = open(os.path.join(root, f), 'rb').read()
            for i, v in enumerate(secrets):
                if v.encode() in data:
                    leaks.append((os.path.relpath(os.path.join(root, f), DEST), i))
    if leaks:
        shutil.rmtree(DEST)
        print('ABORTED: a value of secrets.h was found in', [l[0] for l in leaks])
        sys.exit(1)
    print('copied %d files to %s, checked for %d secret values: clean' % (copied, DEST, len(secrets)))


if __name__ == '__main__':
    main()
