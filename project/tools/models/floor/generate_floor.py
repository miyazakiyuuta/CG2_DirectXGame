# -*- coding: utf-8 -*-
"""
generate_floor.py  -  床(フローリング)タイル生成  (Blender 4.2)

使い方:
  blender -b -P generate_floor.py -- --out <出力フォルダ> [--preview]
  (Blender のテキストエディタから実行した場合は、.blend と同じフォルダ、
   未保存ならカレントフォルダに書き出す。プレビューは --preview のときだけ作る)

書き出すもの:
  floor_board.obj / floor_board.mtl / floor_hole.obj / floor_hole.mtl
  floor_palette.png / floor_report.txt / (--preview) floor_preview.png

座標の扱い (drone / termite_worker と同じ):
  - 形状は「エンジン座標 (左手系・+Y上)」で設計する。板の長手方向は X。
  - 原点 = 部品の上面の中心。上面 Y = 0、部品は X・Z とも -100〜+100。
  - Blender へは  blender = (-ex, -ez, ey)  で置き、
    forward_axis='NEGATIVE_Z', up_axis='Y' で書き出す。
    OBJ = (-ex, ey, ez) (反転前の右手系の値)。エンジンの Assimp 読み込みで X が反転される。
  - レポートの座標はエンジン座標で書く。
  - パレットの uv_rect は左上原点 (DirectX)。OBJ の vt は左下原点で書く。

板の並び:
  - 行は Z 方向に BOARD_ROWS 行 (ピッチ = TILE_SIZE / 行数)。
  - 各行の板の並びは X 方向に TILE_SIZE 周期。板は部品の左右の境目をまたいで続く
    (X の端には溝を作らない)。同じ部品を同じ向きで並べると板がつながる。
  - 前後の端 (Z = ±100) は長辺の溝の半分 (GROOVE_WIDTH/2) で終わる。
"""

import bpy
import bmesh
import math
import os
import sys
import random
import struct
import zlib
from collections import Counter, defaultdict
from mathutils import Vector

# ---------------------------------------------------------------------------
# 変えられる定数 (1 unit = 1 cm)
# ---------------------------------------------------------------------------
TILE_SIZE = 200.0          # 部品の大きさ (X・Z)
THICKNESS = 1.2            # 板の厚み
BOARD_WIDTH_TARGET = 15.0  # 板の幅の目安。行数 = round(TILE_SIZE / これ) (200/15 → 13行)
BOARD_LEN_MIN = 90.0       # 板の長さ
BOARD_LEN_MAX = 180.0
GROOVE_WIDTH = 0.25        # 継ぎ目の溝の幅 (2.5mm)
GROOVE_DEPTH = 0.15        # 継ぎ目の溝の深さ (1.5mm)
CHAMFER = 0.08             # 上面の角の面取り (45°, 0.8mm)
JOINT_EDGE_MARGIN = 2.0    # 板の端の継ぎ目を部品の左右の端からこれ以上離す
STAGGER_MIN = 20.0         # 隣の行との継ぎ目の最小距離
STAGGER_MIN_2 = 10.0       # 2つ隣の行との継ぎ目の最小距離
LAYOUT_SEED = 7            # 板の並びと色の乱数

HOLE_CENTER = (0.0, 0.0)   # 穴の中心 (X, Z)
HOLE_DIAMETER = 35.0       # 穴(貫通部)の基準の直径
HOLE_DIAMETER_MIN = 31.0   # 貫通部の直径の範囲 (半径をこの範囲に収める・要件 30〜40 の内側)
HOLE_DIAMETER_MAX = 39.0
HOLE_POINTS = 36           # 穴の輪郭の点数
HOLE_ZIGZAG = 0.9          # 輪郭のギザギザ (半径の交互の出入り)
HOLE_JITTER = 0.8          # 輪郭の点ごとの乱れ
HOLE_WALL_SPLIT_Y = -0.6   # 穴の内壁の塗り分けの高さ (上: 断面色 / 下: 暗い色)
BITE_COUNT = 10            # 縁のかじり跡の数
BITE_SIZE = (1.8, 3.2)     # かじり跡の大きさ (半径)
BITE_DEPTH = (0.35, 0.8)   # かじり跡の深さ (上面から)
BITE_OFFSET = 0.35         # かじり跡の中心を穴の輪郭から外へずらす量 (大きさに対する割合)
HOLE_SEED = 11

SMOOTH_ANGLE = 30.0        # これ以上の角をシャープにする

# ---------------------------------------------------------------------------
# パレット (256x256, 4x4, 1マス64px, 番号は左上から右へ)
# ---------------------------------------------------------------------------
PALETTE_NAME = "floor_palette.png"
PALETTE_SIZE = 256
GRID = 4
CELL_PX = PALETTE_SIZE // GRID

PALETTE = [
    (0,  "board_a",   (92, 58, 44),   "板の上面と面取り(標準)"),
    (1,  "board_b",   (84, 52, 40),   "板の上面と面取り(やや暗い)"),
    (2,  "board_c",   (100, 63, 47),  "板の上面と面取り(やや明るい)"),
    (3,  "board_d",   (78, 49, 38),   "板の上面と面取り(いちばん暗い)"),
    (4,  "groove",    (38, 23, 18),   "溝の底と溝の側壁"),
    (5,  "cut_face",  (118, 80, 58),  "板の断面: 穴の内壁の上側・部品の外周の側面"),
    (6,  "gnawed",    (72, 46, 33),   "穴の縁のかじり跡"),
    (7,  "hole_dark", (14, 9, 7),     "穴の内壁の下側"),
    (8,  "bottom",    (104, 88, 74),  "底面(合板の裏)"),
    (9,  "unused",    (128, 128, 128), "未使用"),
    (10, "unused",    (128, 128, 128), "未使用"),
    (11, "unused",    (128, 128, 128), "未使用"),
    (12, "unused",    (128, 128, 128), "未使用"),
    (13, "unused",    (128, 128, 128), "未使用"),
    (14, "unused",    (128, 128, 128), "未使用"),
    (15, "unused",    (128, 128, 128), "未使用"),
]
C_BOARDS = (0, 1, 2, 3)
C_GROOVE, C_CUT, C_GNAW, C_DARK, C_BOTTOM = 4, 5, 6, 7, 8

# 派生値
H = TILE_SIZE * 0.5
ROWS = max(1, int(round(TILE_SIZE / BOARD_WIDTH_TARGET)))
PITCH = TILE_SIZE / ROWS
G2 = GROOVE_WIDTH * 0.5
EPS = 1e-4


def cell_uv_rect_topleft(c):
    col, row = c % GRID, c // GRID
    return [col / GRID, row / GRID, (col + 1) / GRID, (row + 1) / GRID]


def cell_uv_center_obj(c):
    col, row = c % GRID, c // GRID
    return ((col + 0.5) / GRID, 1.0 - (row + 0.5) / GRID)


def write_png_rgb(path, width, height, getpix):
    """getpix(x, y) -> (r,g,b) 0..255, y は上から"""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(getpix(x, y))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def write_png_array(path, rgb8):
    """rgb8: (高さ, 幅, 3) uint8 の numpy 配列 (上の行から)"""
    import numpy as np
    hh, ww = rgb8.shape[:2]
    raw = np.concatenate([np.zeros((hh, 1), np.uint8), rgb8.reshape(hh, ww * 3)], axis=1).tobytes()
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", ww, hh, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as fh:
        fh.write(png)


def write_palette_png(path):
    cols = [p[2] for p in PALETTE]
    write_png_rgb(path, PALETTE_SIZE, PALETTE_SIZE,
                  lambda x, y: cols[(y // CELL_PX) * GRID + x // CELL_PX])


# ---------------------------------------------------------------------------
# 板の並び (行ごとの継ぎ目の X 位置と板の色)
# ---------------------------------------------------------------------------
def cyc_dist(a, b):
    d = abs(a - b) % TILE_SIZE
    return min(d, TILE_SIZE - d)


def wrap_x(x):
    return (x + H) % TILE_SIZE - H


def make_layout():
    """rows[i] = {"joints": [x0 < x1 < ...], "lengths": [...], "cells": [...]}
    cells[k] は joints[k] から始まる板の色 (最後の板は境目をまたいで joints[0] まで)"""
    rnd = random.Random(LAYOUT_SEED)
    counts = [n for n in range(1, 64)
              if n * BOARD_LEN_MIN <= TILE_SIZE + 1e-9 and n * BOARD_LEN_MAX >= TILE_SIZE - 1e-9
              and n >= 2]
    if not counts:
        raise RuntimeError("BOARD_LEN_MIN/MAX で TILE_SIZE を2枚以上に分けられない")
    n_boards = counts[0]

    def sample_row():
        for _ in range(2000):
            ls = [rnd.uniform(BOARD_LEN_MIN, BOARD_LEN_MAX) for _ in range(n_boards - 1)]
            last = TILE_SIZE - sum(ls)
            if BOARD_LEN_MIN <= last <= BOARD_LEN_MAX:
                ls.append(last)
                phase = rnd.uniform(-H, H)
                xs, acc = [], phase
                for L in ls:
                    xs.append(wrap_x(acc))
                    acc += L
                if all(abs(x) <= H - JOINT_EDGE_MARGIN for x in xs):
                    order = sorted(range(len(xs)), key=lambda k: xs[k])
                    k0 = order[0]
                    ls = ls[k0:] + ls[:k0]
                    xs = xs[k0:] + xs[:k0]
                    return xs, ls
        return None

    for _attempt in range(5000):
        rows = []
        ok = True
        for i in range(ROWS):
            found = None
            for _t in range(400):
                r = sample_row()
                if r is None:
                    continue
                xs, ls = r
                good = True
                checks = [(i - 1, STAGGER_MIN), (i - 2, STAGGER_MIN_2)]
                if i == ROWS - 1:
                    checks += [(0, STAGGER_MIN), (1, STAGGER_MIN_2)]
                if i == ROWS - 2:
                    checks += [(0, STAGGER_MIN_2)]
                for j, lim in checks:
                    if 0 <= j < len(rows) and j != i:
                        if min(cyc_dist(a, b) for a in xs for b in rows[j]["joints"]) < lim:
                            good = False
                            break
                if good:
                    found = (xs, ls)
                    break
            if found is None:
                ok = False
                break
            rows.append({"joints": found[0], "lengths": found[1]})
        if ok:
            break
    else:
        raise RuntimeError("継ぎ目のずらし条件を満たす並びが見つからない")

    # 色: 行内の隣の板と別の色、前の行で最も重なる板とも別の色
    def board_spans(row):
        xs, ls = row["joints"], row["lengths"]
        return [(xs[k], xs[k] + ls[k]) for k in range(len(xs))]   # 周期上の区間

    def overlap(a, b):
        tot = 0.0
        for s in (-TILE_SIZE, 0.0, TILE_SIZE):
            lo, hi = max(a[0], b[0] + s), min(a[1], b[1] + s)
            tot += max(0.0, hi - lo)
        return tot

    for i, row in enumerate(rows):
        spans = board_spans(row)
        cells = []
        for k, sp in enumerate(spans):
            cand = list(C_BOARDS)
            rnd.shuffle(cand)
            ban = set()
            if cells:
                ban.add(cells[-1])
            if k == len(spans) - 1 and cells:
                ban.add(cells[0])
            if i > 0:
                prev = rows[i - 1]
                pspans = board_spans(prev)
                best = max(range(len(pspans)), key=lambda q: overlap(sp, pspans[q]))
                ban.add(prev["cells"][best])
            pick = next((c for c in cand if c not in ban), cand[0])
            cells.append(pick)
        row["cells"] = cells
    return rows


# ---------------------------------------------------------------------------
# 形状 (エンジン座標)
# ---------------------------------------------------------------------------
class MeshBuilder:
    def __init__(self):
        self.verts = []
        self.index = {}
        self.faces = []    # (idx list, cell)

    def v(self, x, y, z):
        key = (round(x, 6), round(y, 6), round(z, 6))
        i = self.index.get(key)
        if i is None:
            i = len(self.verts)
            self.index[key] = i
            self.verts.append(Vector((x, y, z)))
        return i

    def face(self, pts, cell):
        idx = [self.v(*p) for p in pts]
        self.faces.append((idx, cell))


def row_z(i):
    z0 = -H + i * PITCH
    z1 = z0 + PITCH
    return z0, z1


def build_board_geometry(rows):
    b = MeshBuilder()
    c, D, T = CHAMFER, GROOVE_DEPTH, THICKNESS
    for i, row in enumerate(rows):
        z0, z1 = row_z(i)
        zB, zE = z0 + G2, z1 - G2
        zC, zD = zB + c, zE - c
        xs = row["joints"]
        n = len(xs)
        # 部品内の板の切れ端: [-H, xs0], [xs0, xs1], ..., [xs_{n-1}, H]
        pieces = [(-H, xs[0], False, True, row["cells"][-1])]
        for k in range(n - 1):
            pieces.append((xs[k], xs[k + 1], True, True, row["cells"][k]))
        pieces.append((xs[-1], H, True, False, row["cells"][-1]))
        for xa, xb, ja, jb, cell in pieces:
            fa = xa + G2 if ja else xa
            fb = xb - G2 if jb else xb
            ta = fa + c if ja else fa
            tb = fb - c if jb else fb
            b.face([(ta, 0, zC), (tb, 0, zC), (tb, 0, zD), (ta, 0, zD)], cell)
            b.face([(fa, -c, zB), (fb, -c, zB), (tb, 0, zC), (ta, 0, zC)], cell)
            b.face([(ta, 0, zD), (tb, 0, zD), (fb, -c, zE), (fa, -c, zE)], cell)
            b.face([(fa, -D, zB), (fb, -D, zB), (fb, -c, zB), (fa, -c, zB)], C_GROOVE)
            b.face([(fa, -c, zE), (fb, -c, zE), (fb, -D, zE), (fa, -D, zE)], C_GROOVE)
            if ja:
                b.face([(fa, -c, zB), (ta, 0, zC), (ta, 0, zD), (fa, -c, zE)], cell)
                b.face([(fa, -D, zB), (fa, -c, zB), (fa, -c, zE), (fa, -D, zE)], C_GROOVE)
            if jb:
                b.face([(tb, 0, zC), (fb, -c, zB), (fb, -c, zE), (tb, 0, zD)], cell)
                b.face([(fb, -c, zB), (fb, -D, zB), (fb, -D, zE), (fb, -c, zE)], C_GROOVE)
        for x in xs:   # 板の端の溝の底
            b.face([(x - G2, -D, zB), (x + G2, -D, zB), (x + G2, -D, zE), (x - G2, -D, zE)], C_GROOVE)

    # 長辺の溝の底 (行の境目ごと。端は半分の幅)
    for k in range(ROWS + 1):
        zb = -H + k * PITCH
        zl = zb - G2 if k > 0 else -H
        zu = zb + G2 if k < ROWS else H
        L = [-H, H] + ([x + s for x in rows[k - 1]["joints"] for s in (-G2, G2)] if k > 0 else [])
        U = [-H, H] + ([x + s for x in rows[k]["joints"] for s in (-G2, G2)] if k < ROWS else [])
        L, U = sorted(L), sorted(U)
        p, q = 0, 0
        while p < len(L) - 1 or q < len(U) - 1:
            adv_L = q >= len(U) - 1 or (p < len(L) - 1 and L[p + 1] <= U[q + 1])
            if adv_L:
                b.face([(L[p], -D, zl), (L[p + 1], -D, zl), (U[q], -D, zu)], C_GROOVE)
                p += 1
            else:
                b.face([(L[p], -D, zl), (U[q + 1], -D, zu), (U[q], -D, zu)], C_GROOVE)
                q += 1

    # 左右の側面 (X = ±H): 断面の輪郭 + 下の2角
    prof = [(-H, -D)]
    for i in range(ROWS):
        z0, z1 = row_z(i)
        zB, zE = z0 + G2, z1 - G2
        prof += [(zB, -D), (zB, -CHAMFER), (zB + CHAMFER, 0.0),
                 (zE - CHAMFER, 0.0), (zE, -CHAMFER), (zE, -D)]
    prof += [(H, -D)]
    for sx in (-H, H):
        pts = [(sx, -T, -H)] + [(sx, y, z) for z, y in prof] + [(sx, -T, H)]
        b.face(pts, C_CUT)
    # 前後の側面 (Z = ±H) と底面
    for sz in (-H, H):
        b.face([(-H, -T, sz), (H, -T, sz), (H, -D, sz), (-H, -D, sz)], C_CUT)
    b.face([(-H, -T, -H), (H, -T, -H), (H, -T, H), (-H, -T, H)], C_BOTTOM)
    return b


# ---------------------------------------------------------------------------
# 穴の形 (切り抜く形・エンジン座標)
# ---------------------------------------------------------------------------
def make_hole_shapes():
    rnd = random.Random(HOLE_SEED)
    cx, cz = HOLE_CENTER
    R = HOLE_DIAMETER * 0.5
    rmin, rmax = HOLE_DIAMETER_MIN * 0.5, HOLE_DIAMETER_MAX * 0.5
    ph1, ph2 = rnd.uniform(0, 6.28), rnd.uniform(0, 6.28)
    step = 2 * math.pi / HOLE_POINTS
    outline = []
    for k in range(HOLE_POINTS):
        th = k * step + rnd.uniform(-0.3, 0.3) * step
        r = R * (1.0 + 0.07 * math.sin(2 * th + ph1) + 0.05 * math.sin(3 * th + ph2))
        r += (HOLE_ZIGZAG if k % 2 == 0 else -HOLE_ZIGZAG) + rnd.uniform(-HOLE_JITTER, HOLE_JITTER)
        r = min(max(r, rmin), rmax)
        outline.append((th, r))
    outline_xz = [(cx + r * math.cos(th), cz + r * math.sin(th)) for th, r in outline]

    def r_at(th):
        best = min(outline, key=lambda o: abs(math.atan2(math.sin(o[0] - th), math.cos(o[0] - th))))
        return best[1]

    bites = []
    for k in range(BITE_COUNT):
        th = (k + rnd.uniform(-0.3, 0.3)) * 2 * math.pi / BITE_COUNT
        s = rnd.uniform(*BITE_SIZE)
        d = rnd.uniform(*BITE_DEPTH)
        rc = r_at(th) + BITE_OFFSET * s
        bc = (cx + rc * math.cos(th), cz + rc * math.sin(th))
        m = rnd.choice((5, 6, 7))
        ph = rnd.uniform(0, 6.28)
        poly = []
        for q in range(m):
            a = ph + q * 2 * math.pi / m + rnd.uniform(-0.25, 0.25)
            rr = s * rnd.uniform(0.6, 1.0)
            poly.append((bc[0] + rr * math.cos(a), bc[1] + rr * math.sin(a)))
        bites.append({"center": bc, "size": s, "depth": d, "poly": poly})
    return outline_xz, bites


def build_prism(poly_xz, rings, cells):
    """rings: 上から下への Y のリスト。cells[k]: rings[k]〜rings[k+1] の側面のマス"""
    b = MeshBuilder()
    n = len(poly_xz)
    for k in range(len(rings) - 1):
        ya, yb = rings[k], rings[k + 1]
        for i in range(n):
            x0, z0 = poly_xz[i]
            x1, z1 = poly_xz[(i + 1) % n]
            b.face([(x0, ya, z0), (x1, ya, z1), (x1, yb, z1), (x0, yb, z0)], cells[k])
    b.face([(x, rings[0], z) for x, z in poly_xz], cells[0])
    b.face([(x, rings[-1], z) for x, z in reversed(poly_xz)], cells[-1])
    return b


# ---------------------------------------------------------------------------
# Blender へ
# ---------------------------------------------------------------------------
def to_blender(p):
    return (-p[0], -p[2], p[1])


def carrier_materials():
    mats = []
    for i in range(GRID * GRID):
        name = "cell_%02d" % i
        m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        mats.append(m)
    return mats


def builder_to_object(b, name, mats):
    bm = bmesh.new()
    bv = [bm.verts.new(to_blender(p)) for p in b.verts]
    for idx, cell in b.faces:
        f = bm.faces.new([bv[i] for i in idx])
        f.material_index = cell
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def fix_degenerate(bm, area_eps=1e-6, max_iter=200):
    """一直線に並んだ3点の三角形 (面積ほぼ0) を、いちばん長い辺を回して解消する"""
    for _ in range(max_iter):
        bad = [f for f in bm.faces if f.calc_area() < area_eps]
        if not bad:
            return 0
        f = bad[0]
        e = max(f.edges, key=lambda ed: ed.calc_length())
        if len(e.link_faces) != 2:
            break
        bmesh.ops.rotate_edges(bm, edges=[e], use_ccw=False)
        bm.faces.ensure_lookup_table()
    return sum(1 for f in bm.faces if f.calc_area() < area_eps)


def finalize_mesh(src_me, name, palette_path):
    """マテリアル番号 = マス番号 として受け取り、三角形化・UV・法線・マテリアル1つにする"""
    bm = bmesh.new()
    bm.from_mesh(src_me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=1e-5)
    bmesh.ops.dissolve_degenerate(bm, dist=1e-5, edges=bm.edges[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    fix_degenerate(bm)
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    L_cell = bm.faces.layers.int.new("cell")
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        f[L_cell] = f.material_index
        u, v = cell_uv_center_obj(f.material_index)
        for lp in f.loops:
            lp[uv].uv = (u, v)
        f.material_index = 0
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    me.set_sharp_from_angle(angle=math.radians(SMOOTH_ANGLE))

    mat = bpy.data.materials.new(name + "_mat")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 0.6
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.get(PALETTE_NAME) or bpy.data.images.load(palette_path)
    tex.image = img
    tex.interpolation = 'Closest'
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    me.materials.append(mat)
    obj = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def export_obj(obj, out_dir):
    for o in bpy.context.scene.objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    path = os.path.join(out_dir, obj.name + ".obj")
    bpy.ops.wm.obj_export(
        filepath=path,
        export_selected_objects=True,
        forward_axis='NEGATIVE_Z', up_axis='Y',
        global_scale=1.0, apply_modifiers=True,
        export_uv=True, export_normals=True, export_colors=False,
        export_materials=True, path_mode='STRIP',
        export_triangulated_mesh=True,
        export_object_groups=False, export_material_groups=False,
        export_vertex_groups=False, export_smooth_groups=False,
    )
    return path


def make_board(rows, out_dir, palette_path, mats):
    b = build_board_geometry(rows)
    raw = builder_to_object(b, "floor_board_raw", mats)
    obj = finalize_mesh(raw.data, "floor_board", palette_path)
    return obj, raw


def make_hole(raw_board, out_dir, palette_path, mats, outline, bites):
    base = raw_board.copy()
    base.data = raw_board.data.copy()
    base.name = "floor_hole_raw"
    bpy.context.scene.collection.objects.link(base)
    cutters = []
    main = build_prism(outline, [1.0, HOLE_WALL_SPLIT_Y, -THICKNESS - 1.0], [C_CUT, C_DARK])
    cutters.append(builder_to_object(main, "cut_main", mats))
    for k, bt in enumerate(bites):
        pr = build_prism(bt["poly"], [1.0, -bt["depth"]], [C_GNAW])
        cutters.append(builder_to_object(pr, "cut_bite_%02d" % k, mats))
    for k, ct in enumerate(cutters):
        m = base.modifiers.new("bool_%02d" % k, 'BOOLEAN')
        m.operation = 'DIFFERENCE'
        m.solver = 'EXACT'
        m.object = ct
        m.material_mode = 'TRANSFER'
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(base.evaluated_get(dg))
    obj = finalize_mesh(me, "floor_hole", palette_path)
    for ct in cutters:
        bpy.data.objects.remove(ct, do_unlink=True)
    bpy.data.objects.remove(base, do_unlink=True)
    return obj


# ---------------------------------------------------------------------------
# OBJ を読み直して検査 (座標はエンジン座標に戻す: X を反転)
# ---------------------------------------------------------------------------
def read_obj(path):
    V, VT, VN, F = [], [], [], []
    o_g, usemtl, bad, mtllib = 0, set(), 0, None
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            t = line.split()
            if not t:
                continue
            if t[0] == "v":
                x, y, z = map(float, t[1:4])
                V.append((-x, y, z))
            elif t[0] == "vt":
                VT.append(tuple(map(float, t[1:3])))
            elif t[0] == "vn":
                x, y, z = map(float, t[1:4])
                VN.append((-x, y, z))
            elif t[0] in ("o", "g"):
                o_g += 1
            elif t[0] == "usemtl":
                usemtl.add(t[1])
            elif t[0] == "mtllib":
                mtllib = " ".join(t[1:])
            elif t[0] == "f":
                corners = []
                ok = len(t) == 4
                for c in t[1:]:
                    p = c.split("/")
                    if len(p) < 3 or not p[1] or not p[2]:
                        ok = False
                        p = (p + ["", "", ""])[:3]
                    corners.append(tuple(int(q) - 1 if q else -1 for q in p))
                if not ok:
                    bad += 1
                F.append(corners)
    mats, map_kd = 0, []
    if mtllib:
        with open(os.path.join(os.path.dirname(path), mtllib), encoding="utf-8") as fh:
            for line in fh:
                s = line.strip()
                if s.startswith("newmtl"):
                    mats += 1
                elif s.startswith("map_Kd"):
                    map_kd.append(s)
    return {"V": V, "VT": VT, "VN": VN, "F": F, "o_g": o_g, "usemtl": usemtl,
            "bad": bad, "mats": mats, "map_kd": map_kd, "mtllib": mtllib}


def uv_cell(uvo):
    u, v = uvo[0], 1.0 - uvo[1]
    col, row = u * GRID, v * GRID
    ci, ri = int(math.floor(col)), int(math.floor(row))
    on_center = abs(col - (ci + 0.5)) < 1e-4 and abs(row - (ri + 0.5)) < 1e-4
    on_border = abs(col - round(col)) < 1e-6 or abs(row - round(row)) < 1e-6
    return ri * GRID + ci, on_center, on_border


def tri_area(a, b, c):
    ab = Vector(b) - Vector(a)
    ac = Vector(c) - Vector(a)
    return ab.cross(ac).length * 0.5


def analyze(R):
    V, F = R["V"], R["F"]
    info = {}
    cells, not_center, border, multi = [], 0, 0, 0
    for f in F:
        cs = set()
        for _vi, ti, _ni in f:
            c, oc, ob = uv_cell(R["VT"][ti])
            cs.add(c)
            not_center += 0 if oc else 1
            border += 1 if ob else 0
        multi += 1 if len(cs) > 1 else 0
        cells.append(next(iter(cs)))
    info["cells"] = cells
    info["uv_not_center"] = not_center
    info["uv_border"] = border
    info["uv_multi"] = multi
    xs = [p[0] for p in V]; ys = [p[1] for p in V]; zs = [p[2] for p in V]
    info["bbox"] = ((min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs)))

    # 辺の数え上げ(閉じているか)
    edge_cnt = Counter()
    for f in F:
        vi = [c[0] for c in f]
        for k in range(3):
            a, b = vi[k], vi[(k + 1) % 3]
            edge_cnt[(min(a, b), max(a, b))] += 1
    info["boundary_edges"] = sum(1 for n in edge_cnt.values() if n == 1)
    info["nonmanifold_edges"] = sum(1 for n in edge_cnt.values() if n > 2)

    # 符号付き体積 (エンジン座標は左手系なので、OBJ の右手系の値で計算する)
    vol = 0.0
    for f in F:
        a, b, c = [Vector((-V[q[0]][0], V[q[0]][1], V[q[0]][2])) for q in f]
        vol += a.dot(b.cross(c)) / 6.0
    info["volume"] = vol
    info["min_area"] = min(tri_area(*[V[q[0]] for q in f]) for f in F)

    # 上面 (Y = 0) の法線
    top_tris, top_bad, top_dev = 0, 0, 0.0
    up_ledge = 0
    for f in F:
        ysf = [V[q[0]][1] for q in f]
        if all(abs(y) < 1e-5 for y in ysf):
            top_tris += 1
            for q in f:
                n = Vector(R["VN"][q[2]]).normalized()
                dev = (n - Vector((0, 1, 0))).length
                top_dev = max(top_dev, dev)
                if dev > 1e-4:
                    top_bad += 1
    bot_tris, bot_bad = 0, 0
    for f in F:
        if all(abs(V[q[0]][1] + THICKNESS) < 1e-5 for q in f):
            bot_tris += 1
            bot_bad += sum(1 for q in f if (Vector(R["VN"][q[2]]).normalized() - Vector((0, -1, 0))).length > 1e-4)
    info["bot_tris"] = bot_tris
    info["bot_bad_corners"] = bot_bad
    info["top_tris"] = top_tris
    info["top_bad_corners"] = top_bad
    info["top_max_dev"] = top_dev

    # 部品の端の面 (X = ±H, Z = ±H) に乗る辺のうち、その平面にない面に接するもの = 断面の輪郭
    face_plane = []
    for f in F:
        pts = [V[q[0]] for q in f]
        planes = set()
        for axis in (0, 2):
            for s in (-H, H):
                if all(abs(p[axis] - s) < 1e-4 for p in pts):
                    planes.add((axis, s))
        face_plane.append(planes)
    edge_faces = defaultdict(list)
    for fi, f in enumerate(F):
        vi = [c[0] for c in f]
        for k in range(3):
            a, b = vi[k], vi[(k + 1) % 3]
            edge_faces[(min(a, b), max(a, b))].append(fi)
    prof = {}
    prof_cells = {}
    for axis in (0, 2):
        for s in (-H, H):
            segs = set()
            segcells = {}
            other = 2 if axis == 0 else 0
            for (a, b), fl in edge_faces.items():
                pa, pb = V[a], V[b]
                if abs(pa[axis] - s) < 1e-4 and abs(pb[axis] - s) < 1e-4:
                    outside = [fi for fi in fl if (axis, s) not in face_plane[fi]]
                    if outside:
                        ka = (round(pa[other], 4), round(pa[1], 4))
                        kb = (round(pb[other], 4), round(pb[1], 4))
                        seg = (min(ka, kb), max(ka, kb))
                        segs.add(seg)
                        segcells[seg] = tuple(sorted(cells[fi] for fi in outside))
            prof[(axis, s)] = segs
            prof_cells[(axis, s)] = segcells
    info["prof"] = prof
    info["prof_cells"] = prof_cells
    return info


def analyze_cells(R):
    return [uv_cell(R["VT"][f[0][1]])[0] for f in R["F"]]


def ray_hits_y(R, x, z):
    """(x, z) を通る縦の線と交わる三角形の数"""
    V = R["V"]
    hits = 0
    for f in R["F"]:
        a, b, c = [(V[q[0]][0], V[q[0]][2]) for q in f]
        d = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(d) < 1e-12:
            continue
        l1 = ((b[0] - x) * (c[1] - z) - (b[1] - z) * (c[0] - x)) / d
        l2 = ((c[0] - x) * (a[1] - z) - (c[1] - z) * (a[0] - x)) / d
        l3 = 1 - l1 - l2
        if min(l1, l2, l3) > -1e-9:
            hits += 1
    return hits


# ---------------------------------------------------------------------------
# プレビュー
# ---------------------------------------------------------------------------
def look_at_rot(cam_e, tgt_e):
    d = Vector(to_blender(tgt_e)) - Vector(to_blender(cam_e))
    return d.to_track_quat('-Z', 'Y').to_euler()


def render_preview(out_dir):
    import numpy as np
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    objs = {}
    for name in ("floor_board", "floor_hole"):
        bpy.ops.wm.obj_import(filepath=os.path.join(out_dir, name + ".obj"),
                              forward_axis='NEGATIVE_Z', up_axis='Y')
        o = bpy.context.selected_objects[0]
        for m in o.data.materials:
            for nd in m.node_tree.nodes:
                if nd.type == 'TEX_IMAGE':
                    nd.interpolation = 'Closest'
        objs[name] = o
    # 2x2 に並べる: 手前(Z-)の2枚と左奥が floor_board、右奥 (+100, +100) が floor_hole
    layout = [("floor_board", -H, -H), ("floor_board", H, -H),
              ("floor_board", -H, H), ("floor_hole", H, H)]
    placed = []
    for k, (name, ox, oz) in enumerate(layout):
        src = objs[name]
        o = src if k == 3 or (name == "floor_board" and k == 0) else src.copy()
        if o is not src:
            sc.collection.objects.link(o)
        o.location = to_blender((ox, 0.0, oz))
        placed.append(o)
    # 床下の目安の面 (プレビュー用)
    bpy.ops.mesh.primitive_plane_add(size=800, location=to_blender((0, -25.0, 0)))
    under = bpy.context.active_object
    um = bpy.data.materials.new("under")
    um.use_nodes = True
    um.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.03, 0.03, 0.035, 1)
    under.data.materials.append(um)

    world = bpy.data.worlds.new("w")
    sc.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.25, 0.27, 0.32, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.6
    sun_d = bpy.data.lights.new("sun", 'SUN')
    sun_d.energy = 3.0
    sun_d.angle = math.radians(8)
    sun = bpy.data.objects.new("sun", sun_d)
    sc.collection.objects.link(sun)
    sun.rotation_euler = look_at_rot((-60, 120, -90), (0, 0, 0))

    sc.render.engine = 'CYCLES'
    sc.cycles.device = 'CPU'
    sc.cycles.samples = 24
    sc.cycles.use_denoising = False
    sc.cycles.max_bounces = 3
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.exposure = 0.3
    W, Hh = 800, 500
    sc.render.resolution_x, sc.render.resolution_y = W, Hh
    sc.render.resolution_percentage = 100
    sc.render.image_settings.file_format = 'PNG'

    cam_d = bpy.data.cameras.new("cam")
    cam = bpy.data.objects.new("cam", cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    hx, hz = H, H   # 穴の中心 (右奥の部品)
    views = [
        # (名前, 位置, 注視点, ortho_scale or None, lens, 床下の面を出す)
        ("top", (0, 600, 0), (0, 0, 0), 440.0 * W / Hh, None, True),
        ("low", (hx - 75, 5, hz - 60), (hx, 0, hz), None, 26.0, True),
        ("hole", (hx - 45, 55, hz - 45), (hx, -2, hz), None, 35.0, True),
        ("under", (hx - 70, -40, hz - 70), (hx, -1, hz), None, 28.0, False),
    ]
    up_d = bpy.data.lights.new("sun_up", 'SUN')
    up_d.energy = 2.0
    up = bpy.data.objects.new("sun_up", up_d)
    sc.collection.objects.link(up)
    up.rotation_euler = look_at_rot((-40, -120, -70), (0, 0, 0))
    imgs = []
    for name, pos, tgt, osc, lens, show_under in views:
        under.hide_render = not show_under
        up.hide_render = show_under
        sun.hide_render = not show_under
        if osc:
            cam_d.type = 'ORTHO'
            cam_d.ortho_scale = osc
            cam.location = to_blender(pos)
            cam.rotation_euler = (0.0, 0.0, math.pi)   # 上から見て +X 右・+Z 上 に近い向き
        else:
            cam_d.type = 'PERSP'
            cam_d.lens = lens
            cam.location = to_blender(pos)
            cam.rotation_euler = look_at_rot(pos, tgt)
        cam_d.clip_start = 0.5
        cam_d.clip_end = 5000
        p = os.path.join(out_dir, "_prev_%s.png" % name)
        sc.render.filepath = p
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(p)
        buf = np.empty(W * Hh * 4, dtype=np.float32)
        im.pixels.foreach_get(buf)
        a = buf.reshape(Hh, W, 4)[::-1, :, :3].copy()
        imgs.append(a)
        bpy.data.images.remove(im)
        os.remove(p)
    gap = 6
    canvas = np.full((Hh * 2 + gap, W * 2 + gap, 3), 0.1, dtype=np.float32)
    for k, a in enumerate(imgs):
        r, c = divmod(k, 2)
        canvas[r * (Hh + gap): r * (Hh + gap) + Hh, c * (W + gap): c * (W + gap) + W] = a
    c8 = (np.clip(canvas, 0, 1) * 255 + 0.5).astype(np.uint8)
    write_png_array(os.path.join(out_dir, "floor_preview.png"), c8)


# ---------------------------------------------------------------------------
# レポート
# ---------------------------------------------------------------------------
def fmt3(p):
    return "(%+.4f, %+.4f, %+.4f)" % tuple(p)


def model_report(name, R, A, rows, extra):
    L = []
    L.append("=== %s 生成レポート (Blender %s) ===" % (name, bpy.app.version_string))
    L.append("")
    L.append("[OBJ / ローダー制約]")
    L.append("  三角形数           : %d" % len(R["F"]))
    L.append("  頂点 v / vt / vn   : %d / %d / %d" % (len(R["V"]), len(R["VT"]), len(R["VN"])))
    L.append("  三角形以外・UV/法線欠けの面 : %d  (0であること)" % R["bad"])
    L.append("  o/g の数           : %d  (1であること)" % R["o_g"])
    L.append("  usemtl の種類      : %d  (1であること)" % len(R["usemtl"]))
    L.append("  MTLのマテリアル数  : %d  (1であること)" % R["mats"])
    L.append("  map_Kd             : %s" % R["map_kd"])
    L.append("  UV: 各面の3頂点ともマスの中心 (左上原点で ((col+0.5)/4, (row+0.5)/4))")
    L.append("  マスの中心にないUV / マスの境界上のUV / 1面で複数マスにまたがる面 : %d / %d / %d  (0であること)"
             % (A["uv_not_center"], A["uv_border"], A["uv_multi"]))
    L.append("  法線: スムーズ + %.0f°以上の角をシャープ (Smooth by Angle)" % SMOOTH_ANGLE)
    L.append("  面積がほぼ0の三角形 (< 1e-6) : %d  (最小面積 %.3g)"
             % (sum(1 for f in R["F"] if tri_area(*[R["V"][q[0]] for q in f]) < 1e-6), A["min_area"]))
    L.append("")
    mn, mx = A["bbox"]
    L.append("[寸法(エンジン座標・1 unit = 1 cm)]")
    L.append("  X (板の長手方向) : %.4f" % (mx[0] - mn[0]))
    L.append("  Z (行の並び)     : %.4f" % (mx[2] - mn[2]))
    L.append("  厚み Y           : %.4f" % (mx[1] - mn[1]))
    L.append("  バウンディングボックス")
    L.append("    min = %s" % fmt3(mn))
    L.append("    max = %s" % fmt3(mx))
    okx = abs(mn[0] + H) < 1e-4 and abs(mx[0] - H) < 1e-4
    okz = abs(mn[2] + H) < 1e-4 and abs(mx[2] - H) < 1e-4
    L.append("  X・Z が -100〜+100 : %s / %s" % ("OK" if okx else "NG", "OK" if okz else "NG"))
    L.append("  上面 Y の最大 = %+.6f (0であること) : %s" % (mx[1], "OK" if abs(mx[1]) < 1e-5 else "NG"))
    L.append("  原点 = 部品の上面の中心")
    L.append("")
    L.append("[閉じた形]")
    L.append("  境界エッジ / 3面以上が共有するエッジ : %d / %d  (0であること)"
             % (A["boundary_edges"], A["nonmanifold_edges"]))
    L.append("  符号付き体積 : %.2f  (外向き = 正。参考: 200x200x1.2 = %.0f)"
             % (A["volume"], TILE_SIZE * TILE_SIZE * THICKNESS))
    L.append("")
    L.append("[上面・底面の法線]")
    L.append("  上面 (3頂点とも Y = 0) の三角形 : %d" % A["top_tris"])
    L.append("  (0,1,0) からずれている頂点法線 : %d  (最大のずれ %.2e)  : %s"
             % (A["top_bad_corners"], A["top_max_dev"], "OK" if A["top_bad_corners"] == 0 else "NG"))
    L.append("  底面 (3頂点とも Y = %.1f) の三角形 : %d / (0,-1,0) からずれている頂点法線 : %d : %s"
             % (-THICKNESS, A["bot_tris"], A["bot_bad_corners"], "OK" if A["bot_bad_corners"] == 0 else "NG"))
    L.append("")
    L.extend(extra)
    return L


def layout_lines(rows):
    L = []
    L.append("[板の並び] (両モデル共通)")
    L.append("  行数 %d / ピッチ(板幅+溝) %.4f / 板の上面の幅 %.4f" % (ROWS, PITCH, PITCH - GROOVE_WIDTH - 2 * CHAMFER))
    L.append("  溝: 幅 %.3f・深さ %.3f / 面取り %.3f (45°) / 厚み %.3f"
             % (GROOVE_WIDTH, GROOVE_DEPTH, CHAMFER, THICKNESS))
    L.append("  X 方向は %.0f 周期で、板は部品の左右の境目をまたいで続く (X の端に溝はない)" % TILE_SIZE)
    L.append("  板の長さ %.0f〜%.0f の範囲で 200 を埋められる枚数: 1周期 %d 枚"
             % (BOARD_LEN_MIN, BOARD_LEN_MAX, len(rows[0]["joints"])))
    L.append("  行  Z範囲                  継ぎ目 X                板の長さ (継ぎ目から +X へ)  色(マス)")
    for i, r in enumerate(rows):
        z0, z1 = row_z(i)
        js = ", ".join("%+7.2f" % x for x in r["joints"])
        ls = ", ".join("%6.2f" % l for l in r["lengths"])
        cs = ", ".join(str(c) for c in r["cells"])
        L.append("  %2d  [%+8.3f, %+8.3f]   %s     %s             %s" % (i, z0, z1, js, ls, cs))
    dmin1 = min(min(cyc_dist(a, b) for a in rows[i]["joints"] for b in rows[(i + 1) % ROWS]["joints"])
                for i in range(ROWS))
    dmin2 = min(min(cyc_dist(a, b) for a in rows[i]["joints"] for b in rows[(i + 2) % ROWS]["joints"])
                for i in range(ROWS))
    L.append("  隣の行との継ぎ目の最小距離 (Z 方向に並べた境目も含む) : %.2f  (>= %.0f)" % (dmin1, STAGGER_MIN))
    L.append("  2つ隣の行との継ぎ目の最小距離                         : %.2f  (>= %.0f)" % (dmin2, STAGGER_MIN_2))
    L.append("  部品の左右の端から継ぎ目までの最小距離 : %.2f"
             % min(H - abs(x) for r in rows for x in r["joints"]))
    return L


def tiling_lines(name, A, other=None, other_name=None):
    L = []
    px, mxp = A["prof"][(0, -H)], A["prof"][(0, H)]
    pz, mzp = A["prof"][(2, -H)], A["prof"][(2, H)]
    L.append("[並べたときのつながり]")
    L.append("  X 方向 (左右) に同じ部品を並べる: X=+100 と X=-100 の断面の輪郭")
    L.append("    輪郭の辺の数 %d / %d, 一致 : %s" % (len(mxp), len(px), "OK" if mxp == px else "NG"))
    cx_ok = A["prof_cells"][(0, H)] == A["prof_cells"][(0, -H)]
    L.append("    境目をまたぐ板の色(輪郭の辺ごとのマス) 一致 : %s" % ("OK" if cx_ok else "NG"))
    L.append("    => 長辺の溝 %d 本の Z 位置と、境目をまたぐ %d 枚の板の上面・面取り・色がつながる"
             % (ROWS - 1, ROWS))
    L.append("  Z 方向 (前後) に同じ部品を並べる: Z=+100 と Z=-100 の断面の輪郭")
    L.append("    輪郭の辺の数 %d / %d, 一致 : %s" % (len(mzp), len(pz), "OK" if mzp == pz else "NG"))
    ys = sorted({y for seg in mzp for pt in seg for y in [pt[1]]})
    L.append("    端の断面の高さ: Y = %s (溝の底 %.3f 〜 底面 %.3f の長方形)"
             % (", ".join("%+.3f" % y for y in ys), -GROOVE_DEPTH, -THICKNESS))
    L.append("    端に残す溝の幅: 前 %.4f + 後 %.4f = %.4f (部品の中の溝の幅 %.4f と一致 : %s)"
             % (G2, G2, 2 * G2, GROOVE_WIDTH, "OK" if abs(2 * G2 - GROOVE_WIDTH) < 1e-9 else "NG"))
    if other is not None:
        okx = A["prof"][(0, H)] == other["prof"][(0, -H)] and A["prof"][(0, -H)] == other["prof"][(0, H)]
        okz = A["prof"][(2, H)] == other["prof"][(2, -H)] and A["prof"][(2, -H)] == other["prof"][(2, H)]
        okc = (A["prof_cells"][(0, H)] == other["prof_cells"][(0, -H)])
        L.append("  %s と %s を隣り合わせる" % (name, other_name))
        L.append("    X 方向 (両側とも) 輪郭一致 : %s / 色一致 : %s" % ("OK" if okx else "NG", "OK" if okc else "NG"))
        L.append("    Z 方向 (両側とも) 輪郭一致 : %s" % ("OK" if okz else "NG"))
    return L


def hole_lines(R, outline, bites):
    cx, cz = HOLE_CENTER
    rs = [math.hypot(x - cx, z - cz) for x, z in outline]
    area = 0.0
    n = len(outline)
    for i in range(n):
        x0, z0 = outline[i]; x1, z1 = outline[(i + 1) % n]
        area += x0 * z1 - x1 * z0
    area = abs(area) * 0.5
    V = R["V"]
    bot = [p for p in V if abs(p[1] + THICKNESS) < 1e-4 and math.hypot(p[0] - cx, p[2] - cz) < H * 0.5]
    br = [math.hypot(p[0] - cx, p[2] - cz) for p in bot]
    cells = analyze_cells(R)
    rim_v = {q[0] for f, c in zip(R["F"], cells) if c in (C_CUT, C_GNAW, C_DARK)
             for q in f if abs(V[q[0]][1]) < 1e-5
             and math.hypot(V[q[0]][0] - cx, V[q[0]][2] - cz) < H * 0.5}
    rim = [math.hypot(V[i][0] - cx, V[i][2] - cz) for i in rim_v]
    L = []
    L.append("[穴]")
    L.append("  中心 (X, Z) : (%+.4f, %+.4f)" % (cx, cz))
    L.append("  貫通部の輪郭 : %d 点, 半径 %.2f〜%.2f (直径 %.2f〜%.2f)"
             % (n, min(rs), max(rs), 2 * min(rs), 2 * max(rs)))
    L.append("  貫通部の面積 %.1f / 同じ面積の円の直径 %.2f" % (area, 2 * math.sqrt(area / math.pi)))
    if br:
        L.append("  底面での穴の縁の半径 (メッシュから) : %.2f〜%.2f" % (min(br), max(br)))
    if rim:
        L.append("  上面での穴の縁の半径 (かじり跡を含む・メッシュから) : %.2f〜%.2f" % (min(rim), max(rim)))
    L.append("  縁のかじり跡 : %d 個, 大きさ(半径) %.2f〜%.2f, 深さ %.2f〜%.2f"
             % (len(bites), min(b["size"] for b in bites), max(b["size"] for b in bites),
                min(b["depth"] for b in bites), max(b["depth"] for b in bites)))
    L.append("  内壁の塗り分け : Y > %.2f は cut_face, Y < %.2f は hole_dark" % (HOLE_WALL_SPLIT_Y, HOLE_WALL_SPLIT_Y))
    wall, wall_bad = 0, 0
    for f, c in zip(R["F"], cells):
        if c in (C_CUT, C_DARK):
            cen = [sum(V[q[0]][k] for q in f) / 3.0 for k in range(3)]
            if math.hypot(cen[0] - cx, cen[2] - cz) < H * 0.5:
                wall += 1
                d = Vector((cx - cen[0], 0.0, cz - cen[2])).normalized()
                if Vector(R["VN"][f[0][2]]).dot(d) < 0:
                    wall_bad += 1
    L.append("  内壁の三角形 %d / 法線が穴の中心(空洞側)を向いていないもの : %d  (0であること) : %s"
             % (wall, wall_bad, "OK" if wall_bad == 0 else "NG"))
    h = ray_hits_y(R, cx, cz)
    L.append("  貫通の確認: 中心を通る縦の線と交わる三角形 : %d  (0であること) : %s" % (h, "OK" if h == 0 else "NG"))
    return L


def palette_lines(cell_counts):
    L = []
    L.append("[パレット (%dx%d, %dx%d マス, 1マス%dpx, uv_rect は左上原点)] %s"
             % (PALETTE_SIZE, PALETTE_SIZE, GRID, GRID, CELL_PX, PALETTE_NAME))
    L.append("  cell  name        rgb               floor_board  floor_hole  用途")
    for i, n, rgb, note in PALETTE:
        L.append("  cell %2d  %-10s rgb(%3d,%3d,%3d)  tris %5d   tris %5d   %s"
                 % (i, n, rgb[0], rgb[1], rgb[2], cell_counts[0].get(i, 0), cell_counts[1].get(i, 0), note))
    return L


# ---------------------------------------------------------------------------
def parse_args():
    argv = sys.argv
    argv = argv[argv.index("--") + 1:] if "--" in argv else argv[1:]
    out, preview = None, False
    for i, a in enumerate(argv):
        if a == "--out" and i + 1 < len(argv):
            out = argv[i + 1]
        if a == "--preview":
            preview = True
    if out is None:
        out = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else os.getcwd()
    os.makedirs(out, exist_ok=True)
    return out, preview


def main():
    out_dir, preview = parse_args()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    palette_path = os.path.join(out_dir, PALETTE_NAME)
    write_palette_png(palette_path)
    mats = carrier_materials()

    rows = make_layout()
    board, raw = make_board(rows, out_dir, palette_path, mats)
    outline, bites = make_hole_shapes()
    hole = make_hole(raw, out_dir, palette_path, mats, outline, bites)
    bpy.data.objects.remove(raw, do_unlink=True)
    p_board = export_obj(board, out_dir)
    p_hole = export_obj(hole, out_dir)

    Rb, Rh = read_obj(p_board), read_obj(p_hole)
    Ab, Ah = analyze(Rb), analyze(Rh)
    cnt_b, cnt_h = Counter(Ab["cells"]), Counter(Ah["cells"])

    lines = []
    lines += model_report("floor_board", Rb, Ab, rows,
                          tiling_lines("floor_board", Ab, Ah, "floor_hole"))
    lines.append("")
    lines += model_report("floor_hole", Rh, Ah, rows,
                          tiling_lines("floor_hole", Ah, Ab, "floor_board") + [""] + hole_lines(Rh, outline, bites))
    lines.append("")
    lines += layout_lines(rows)
    lines.append("")
    lines += palette_lines((cnt_b, cnt_h))
    lines.append("")
    lines.append("[座標の扱い]")
    lines.append("  形状はエンジン座標 (左手系・+Y上) で設計。OBJ には X を反転した右手系の値を書き、")
    lines.append("  エンジンの Assimp 読み込みで X が反転されてエンジン座標に戻る。レポートの値はエンジン座標。")
    with open(os.path.join(out_dir, "floor_report.txt"), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    print("\n".join(lines))

    if preview:
        render_preview(out_dir)


main()
