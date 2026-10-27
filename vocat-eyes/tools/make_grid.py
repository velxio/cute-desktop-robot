"""Puts every expression (or every act) in one animated grid, all moving at once, with the
name under each face. Input: the per-face GIFs of `facetool all` / `facetool acts`.

    python tools/make_grid.py <gif folder> <out base name> [columns] [Name,Name,...]

writes <out>.gif (128 colours, no dither: smaller than an animated WebP here). Needs Pillow and ffmpeg.
"""
import glob
import os
import shutil
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont, ImageSequence

CELL = 100          # face diameter in pixels
PAD_X, PAD_Y = 14, 30
FRAMES = 90         # 90 frames at 50 ms: 4.5 s loop
BG = (13, 16, 22)


def load(path, skip):
    im = Image.open(path)
    frames = [f.convert('RGB').resize((CELL, CELL), Image.LANCZOS) for f in ImageSequence.Iterator(im)]
    return frames[int(len(frames) * skip):] or frames


def main():
    src, out = sys.argv[1], sys.argv[2]
    cols = int(sys.argv[3]) if len(sys.argv) > 3 else 8
    order_file = os.path.join(src, 'order.txt')  # optional: one name per line
    if len(sys.argv) > 4:
        names = sys.argv[4].split(',')
    elif os.path.exists(order_file):
        names = [l.strip() for l in open(order_file, encoding='utf-8') if l.strip()]
    else:
        names = sorted(os.path.splitext(os.path.basename(p))[0] for p in glob.glob(os.path.join(src, '*.gif')))
    # SKIP=0.4 drops the first 40 % of each clip: the expression GIFs start with the change
    # from the normal face, and without it every cell of a grid would open on the same face
    skip = float(os.environ.get('SKIP', '0'))
    clips = {n: load(os.path.join(src, n + '.gif'), skip) for n in names}

    cols = min(cols, len(names))
    rows = (len(names) + cols - 1) // cols
    w, h = cols * (CELL + PAD_X) + PAD_X, rows * (CELL + PAD_Y) + 10
    mask = Image.new('L', (CELL, CELL), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, CELL - 1, CELL - 1), fill=255)
    try:
        font = ImageFont.truetype('segoeui.ttf', 13)
    except OSError:
        font = ImageFont.load_default()

    # the frame without faces: rings and names
    base = Image.new('RGB', (w, h), BG)
    d = ImageDraw.Draw(base)
    pos = {}
    for i, n in enumerate(names):
        x = PAD_X + (i % cols) * (CELL + PAD_X)
        y = 8 + (i // cols) * (CELL + PAD_Y)
        pos[n] = (x, y)
        d.ellipse((x - 3, y - 3, x + CELL + 2, y + CELL + 2), fill=(42, 48, 64))
        tw = d.textlength(n, font=font)
        d.text((x + (CELL - tw) / 2, y + CELL + 6), n, fill=(170, 180, 196), font=font)

    tmp = tempfile.mkdtemp()
    for f in range(FRAMES):
        im = base.copy()
        for n in names:
            clip = clips[n]
            im.paste(clip[f % len(clip)], pos[n], mask)
        im.save(os.path.join(tmp, 'f%03d.png' % f))

    pattern = os.path.join(tmp, 'f%03d.png')
    subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y', '-framerate', '20', '-i', pattern,
                    '-vf', 'split[a][b];[a]palettegen=max_colors=128:stats_mode=full[p];[b][p]paletteuse=dither=none',
                    '-loop', '0', out + '.gif'], check=True)
    shutil.rmtree(tmp)
    print(out + '.gif', os.path.getsize(out + '.gif') // 1024, 'KB', '%dx%d' % (w, h))


if __name__ == '__main__':
    main()
