"""Builds assets/3d/vocat_shell.glb from Espressif's enclosure STLs.

The STLs (source/btm.stl = body with the ears, source/top.stl = front ring) come from
Espressif's ESP-VoCat / EchoEar shell published on OSHWHub by ESP-College under GPL-3.0:
https://oshwhub.com/esp-college/echoear

Units: millimetres in, centimetres out (1 raylib unit = 1 cm). The model is centred in X
and Z and stands on Y = 0; the face looks toward -X, tilted 21 degrees up. The screen
position printed at the end is what emulator/View3D.cpp uses.

    pip install trimesh fast-simplification
    python make_glb.py [--body R,G,B] [--ring R,G,B]
"""
import argparse
import os

import numpy as np
import trimesh

HERE = os.path.dirname(os.path.abspath(__file__))


def rgb(text):
    r, g, b = (float(v) / 255.0 for v in text.split(','))
    return [r, g, b, 1.0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--body', default='46,108,214', help='colour of the body, 0-255 (default: blue)')
    ap.add_argument('--ring', default='120,170,240', help='colour of the front ring, 0-255 (default: light blue)')
    args = ap.parse_args()

    body = trimesh.load(os.path.join(HERE, 'source', 'btm.stl'))
    ring = trimesh.load(os.path.join(HERE, 'source', 'top.stl'))
    both = trimesh.util.concatenate([body, ring])
    lo, hi = both.bounds
    offset = np.array([-(lo[0] + hi[0]) / 2, -lo[1], -(lo[2] + hi[2]) / 2])
    scale = 0.1

    def prep(mesh, faces, colour):
        mesh = mesh.copy()
        mesh.apply_translation(offset)
        mesh.apply_scale(scale)
        try:
            mesh = mesh.simplify_quadric_decimation(face_count=faces)
        except Exception as e:  # fast-simplification missing: keep every triangle
            print('no decimation:', e)
        mesh.visual = trimesh.visual.TextureVisuals(material=trimesh.visual.material.PBRMaterial(
            baseColorFactor=colour, metallicFactor=0.0, roughnessFactor=0.6))
        return mesh

    scene = trimesh.Scene()
    scene.add_geometry(prep(body, 30000, rgb(args.body)), 'body')
    scene.add_geometry(prep(ring, 12000, rgb(args.ring)), 'ring')
    out = os.path.join(HERE, 'vocat_shell.glb')
    scene.export(out)
    print('wrote', out, os.path.getsize(out), 'bytes')

    # the round window of the ring, measured on top.stl: centre at 32 mm across and 36 mm up
    # the lid, the screen 7 mm behind the lid's front plane, radius 26 mm
    n = np.array([-0.934, 0.357, 0.0])
    n /= np.linalg.norm(n)
    u = np.array([0.0, 0.0, 1.0])
    v = np.cross(n, u)
    v /= np.linalg.norm(v)
    centre = (32 * u + 36 * v - 7.0 * n + offset) * scale
    print('screen centre', np.round(centre, 3).tolist(), 'normal', np.round(n, 3).tolist(),
          'up', np.round(v, 3).tolist(), 'radius', 26 * scale)


if __name__ == '__main__':
    main()
