# -*- coding: utf-8 -*-
"""
generate_dining.py  -  食卓(dining_table)と椅子(dining_chair)の生成  (Blender 4.2)

使い方:
  blender -b -P generate_dining.py -- --out <出力フォルダ> [--preview]
  (Blender のテキストエディタから実行した場合は、.blend と同じフォルダ、
   未保存ならカレントフォルダに書き出す。プレビューは --preview のときだけ作る)

書き出すもの:
  dining_table.obj / .mtl   dining_chair.obj / .mtl   dining_palette.png
  dining_table_colliders.json / dining_chair_colliders.json
  dining_report.txt / (--preview) dining_preview.png

座標の扱い (drone / floor と同じ):
  - 形状は「エンジン座標 (左手系・+Y上)」で設計する。1 unit = 1 cm。
  - 原点 = 床に接する面の中心 (脚の下端 Y = 0、X・Z は脚の配置の中心)。
  - Blender へは  blender = (-ex, -ez, ey)  で置き、
    forward_axis='NEGATIVE_Z', up_axis='Y' で書き出す。
    OBJ = (-ex, ey, ez) (反転前の右手系の値)。エンジンの Assimp 読み込みで X が反転される。
  - レポートと当たり判定の JSON はエンジン座標で書く。
  - パレットの uv_rect は左上原点 (DirectX)。OBJ の vt は左下原点で書く。
  - 左右の呼び方: 正面 = -Z を向いたときの左 = +X (drone と同じ)。F = -Z 側, B = +Z 側。
"""

import bpy
import bmesh
import math
import os
import sys
import json
import struct
import zlib
from collections import Counter, defaultdict
from mathutils import Vector

# ---------------------------------------------------------------------------
# 変えられる定数 (1 unit = 1 cm)
# ---------------------------------------------------------------------------
# --- 食卓 ---
TABLE_W = 150.0            # 幅 (X)
TABLE_D = 80.0             # 奥行 (Z)
TABLE_H = 70.0             # 高さ (天板の上面)
TOP_T = 3.0                # 天板の厚み
TOP_CHAMFER = 0.6          # 天板の角の面取り
TABLE_LEG = 5.5            # 脚の太さ (角材)
TABLE_LEG_INSET = 6.0      # 天板の縁から脚の外側の面までの距離
TABLE_LEG_BEVEL = 0.4
APRON_H = 8.0              # 幕板の高さ
APRON_T = 2.0              # 幕板の厚み
APRON_SETBACK = 1.0        # 脚の外側の面から幕板の外側の面までの引っ込み
APRON_BEVEL = 0.3

# --- 椅子 ---
CHAIR_W = 42.0             # 幅 (X)
CHAIR_D = 45.0             # 奥行 (Z)
SEAT_H = 42.0              # 座面の高さ (クッション上面の最も高い点)
BACK_H = 80.0              # 背もたれの上端
CHAIR_LEG = 4.0            # 脚の太さ
CHAIR_LEG_BEVEL = 0.3
SEAT_FRAME_T = 3.0         # 座枠(座板)の厚み
SEAT_FRAME_BEVEL = 0.4
CUSHION_T = 3.0            # クッションの厚み (中央)
CUSHION_SIDE_INSET = 0.5   # 座枠の左右・前の縁からクッションの縁までの引っ込み
CUSHION_BACK_GAP = 0.5     # 後ろ脚(柱)の前の面からクッションの後ろの縁までの隙間
CUSHION_SEGMENTS = 40      # クッションの輪郭の点数 (4の倍数)
CUSHION_EXPONENT = 5.0     # 輪郭の角の四角さ (超楕円の指数。大きいほど四角い)
CUSHION_DOME = 0.4         # クッション上面の中央の盛り上がり
STRETCHER_Y = 12.0         # 貫の下面の高さ
STRETCHER_H = 3.0          # 貫の高さ
STRETCHER_T = 2.0          # 貫の厚み
STRETCHER_BEVEL = 0.25
RAIL_H = 8.0               # 背もたれの上の横木の高さ
RAIL_T = 2.5               # 上の横木の厚み
RAIL_BEVEL = 0.4
SLAT_COUNT = 3             # 縦の桟の本数
SLAT_W = 3.0               # 縦の桟の幅
SLAT_T = 1.5               # 縦の桟の厚み
SLAT_BEVEL = 0.25
EMBED = 0.5                # 部材を相手の部材に差し込む深さ (継ぎ目の隙間を防ぐ)

BEVEL_SEGMENTS = 2         # 面取りの段数
WOOD_UP_NY = 0.7           # 法線の Y 成分がこれより大きい木部の面 = oak_top、-これより小さい = oak_shade
FABRIC_UP_NY = 0.5         # 法線の Y 成分がこれより大きいクッションの面 = fabric_green

# --- プレビュー ---
PREVIEW_CHAIR_X = 37.5     # 椅子の X 位置 (長辺側に2脚ずつ)
PREVIEW_CHAIR_Z = 60.0     # 椅子の Z 位置 (テーブル中心から)
PREVIEW_SIZE = (800, 540)  # 1枚の大きさ (2x2 で並べる)
PREVIEW_SAMPLES = 32
PREVIEW_EXPOSURE = 0.0     # プレビューの露出
PREVIEW_FLOOR_RGB = (92, 58, 44)   # プレビューにだけ敷く床の色 (floor_palette の board_a)

PALETTE_SIZE = 256
PALETTE_GRID = 4
PALETTE = [
    # (名前, sRGB, 用途)
    ("oak_top",           (222, 186, 136), "上を向いた木部の面(天板の上面・横木の上面など)"),
    ("oak_side",          (200, 160, 108), "横を向いた木部の面(脚・幕板・天板の側面・桟)"),
    ("oak_shade",         (150, 112,  72), "下を向いた木部の面・内側の陰(天板の裏・幕板の内側)"),
    ("fabric_green",      ( 56,  92,  70), "座面クッションの上面"),
    ("fabric_green_dark", ( 38,  64,  48), "クッションの側面・下側"),
] + [("unused", (128, 128, 128), "未使用")] * 11
C_TOP, C_SIDE, C_SHADE, C_FAB, C_FAB_DARK = 0, 1, 2, 3, 4

OUT_NAMES = dict(
    palette="dining_palette.png",
    report="dining_report.txt",
    preview="dining_preview.png",
)


# ---------------------------------------------------------------------------
# 小物
# ---------------------------------------------------------------------------
def e2b(p):
    """エンジン座標 -> Blender 座標"""
    return (-p[0], -p[2], p[1])


def cell_uv_blender(i):
    """マスの中心 (Blender / OBJ の UV は左下原点)"""
    col, row = i % PALETTE_GRID, i // PALETTE_GRID
    return ((col + 0.5) / PALETTE_GRID, 1.0 - (row + 0.5) / PALETTE_GRID)


def cell_rect_dx(i):
    """uv_rect (左上原点)"""
    col, row = i % PALETTE_GRID, i // PALETTE_GRID
    s = 1.0 / PALETTE_GRID
    return [col * s, row * s, (col + 1) * s, (row + 1) * s]


def write_png(path, w, h, rows):
    """rows: 上から順の RGB バイト列"""
    raw = b"".join(b"\x00" + bytes(r) for r in rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

    data = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def fmt3(p, nd=4):
    return "(" + ", ".join(("%+." + str(nd) + "f") % x for x in p) + ")"


# ---------------------------------------------------------------------------
# パーツ (エンジン座標で作る)
# ---------------------------------------------------------------------------
class Part:
    def __init__(self, name, verts, faces, cells, smooth):
        self.name = name
        self.verts = verts      # [(x, y, z)]
        self.faces = faces      # [[i, ...]]
        self.cells = cells      # [cell]
        self.smooth = smooth    # [bool]


def bm_to_part(bm, name, cell_fn, smooth_fn):
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.normal_update()
    bm.verts.index_update()
    bm.faces.index_update()
    verts = [tuple(v.co) for v in bm.verts]
    faces, cells, smooth = [], [], []
    for f in bm.faces:
        faces.append([v.index for v in f.verts])
        cells.append(cell_fn(f))
        smooth.append(smooth_fn(f))
    bm.free()
    return Part(name, verts, faces, cells, smooth)


def wood_cells(inner_dir=None):
    def fn(f):
        n = f.normal
        if n.y > WOOD_UP_NY:
            return C_TOP
        if n.y < -WOOD_UP_NY:
            return C_SHADE
        if inner_dir is not None and n.dot(Vector(inner_dir)) > 0.7:
            return C_SHADE
        return C_SIDE
    return fn


def make_box(name, mn, mx, bevel, inner_dir=None):
    """面取りした角材 (閉じた殻)"""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector([mn[k] if v.co[k] < 0 else mx[k] for k in range(3)])
    if bevel > 0:
        bmesh.ops.bevel(bm, geom=bm.verts[:] + bm.edges[:], offset=bevel,
                        offset_type='OFFSET', segments=BEVEL_SEGMENTS, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
    return bm_to_part(bm, name, wood_cells(inner_dir), lambda f: False)


def make_cushion(name, x_half, z_min, z_max, y_bottom, y_top):
    """布張り風のクッション。超楕円の輪郭を同心に重ねた閉じた殻。
       底面だけ平ら、側面〜上面はなめらか。"""
    a0 = x_half
    b0 = (z_max - z_min) * 0.5
    zc = (z_min + z_max) * 0.5
    t = y_top - y_bottom
    plateau = y_top - CUSHION_DOME
    # 縁の丸み: (内側への引っ込み, 高さ)
    profile = [(0.00, y_bottom),
               (0.00, y_bottom + 0.30 * t),
               (0.12, y_bottom + 0.57 * t),
               (0.40, y_bottom + 0.75 * t),
               (0.85, plateau - 0.075 * t),
               (1.50, plateau)]
    # 上面の盛り上がり
    dmax = min(a0, b0)
    for d in (4.5, 8.5, 13.0, 17.0):
        if d < dmax - 1.0:
            u = (d - 1.5) / (dmax - 1.5)
            profile.append((d, plateau + CUSHION_DOME * (1.0 - (1.0 - u) ** 2)))
    n = CUSHION_SEGMENTS
    e = 2.0 / CUSHION_EXPONENT

    def outline(d, y):
        pts = []
        a, b = a0 - d, b0 - d
        for i in range(n):
            ang = 2.0 * math.pi * i / n
            c, s = math.cos(ang), math.sin(ang)
            x = a * math.copysign(abs(c) ** e, c)
            z = b * math.copysign(abs(s) ** e, s)
            pts.append((x, y, zc + z))
        return pts

    bm = bmesh.new()
    rings = [[bm.verts.new(p) for p in outline(d, y)] for d, y in profile]
    top_center = bm.verts.new((0.0, y_top, zc))
    for k in range(len(rings) - 1):
        r0, r1 = rings[k], rings[k + 1]
        for i in range(n):
            j = (i + 1) % n
            bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
    last = rings[-1]
    for i in range(n):
        bm.faces.new((last[i], last[(i + 1) % n], top_center))
    bottom = bm.faces.new(list(reversed(rings[0])))
    bm.faces.index_update()
    bottom_idx = bottom.index

    def cell_fn(f):
        return C_FAB if f.normal.y > FABRIC_UP_NY else C_FAB_DARK

    def smooth_fn(f):
        return f.index != bottom_idx
    return bm_to_part(bm, name, cell_fn, smooth_fn)


# ---------------------------------------------------------------------------
# モデルの組み立て
# ---------------------------------------------------------------------------
def table_parts():
    hw, hd = TABLE_W * 0.5, TABLE_D * 0.5
    y_top_bot = TABLE_H - TOP_T
    x_out, z_out = hw - TABLE_LEG_INSET, hd - TABLE_LEG_INSET
    x_in, z_in = x_out - TABLE_LEG, z_out - TABLE_LEG
    x_mid, z_mid = x_out - TABLE_LEG * 0.5, z_out - TABLE_LEG * 0.5
    y_ap = y_top_bot - APRON_H
    parts = [make_box("top", (-hw, y_top_bot, -hd), (hw, TABLE_H, hd), TOP_CHAMFER)]
    for nm, sx, sz in (("leg_FL", 1, -1), ("leg_FR", -1, -1), ("leg_BL", 1, 1), ("leg_BR", -1, 1)):
        xs = sorted((sx * x_in, sx * x_out))
        zs = sorted((sz * z_in, sz * z_out))
        parts.append(make_box(nm, (xs[0], 0.0, zs[0]), (xs[1], y_top_bot, zs[1]), TABLE_LEG_BEVEL))
    ao = APRON_SETBACK
    for nm, sz in (("apron_F", -1), ("apron_B", 1)):
        zs = sorted((sz * (z_out - ao), sz * (z_out - ao - APRON_T)))
        parts.append(make_box(nm, (-x_mid, y_ap, zs[0]), (x_mid, y_top_bot, zs[1]),
                              APRON_BEVEL, inner_dir=(0, 0, -sz)))
    for nm, sx in (("apron_L", 1), ("apron_R", -1)):
        xs = sorted((sx * (x_out - ao), sx * (x_out - ao - APRON_T)))
        parts.append(make_box(nm, (xs[0], y_ap, -z_mid), (xs[1], y_top_bot, z_mid),
                              APRON_BEVEL, inner_dir=(-sx, 0, 0)))
    return parts


def chair_parts():
    hw, hd = CHAIR_W * 0.5, CHAIR_D * 0.5
    L = CHAIR_LEG
    y_frame_top = SEAT_H - CUSHION_T
    y_frame_bot = y_frame_top - SEAT_FRAME_T
    x_in = hw - L
    z_in = hd - L
    parts = []
    # 脚 (前脚は座枠の下まで、後ろ脚はそのまま背もたれの柱)
    for nm, sx, sz in (("leg_FL", 1, -1), ("leg_FR", -1, -1), ("leg_BL", 1, 1), ("leg_BR", -1, 1)):
        xs = sorted((sx * x_in, sx * hw))
        zs = sorted((sz * z_in, sz * hd))
        top = y_frame_bot if sz < 0 else BACK_H
        parts.append(make_box(nm, (xs[0], 0.0, zs[0]), (xs[1], top, zs[1]), CHAIR_LEG_BEVEL))
    # 座枠
    parts.append(make_box("seat_frame", (-hw, y_frame_bot, -hd), (hw, y_frame_top, hd), SEAT_FRAME_BEVEL))
    # クッション
    parts.append(make_cushion("cushion", hw - CUSHION_SIDE_INSET,
                              -hd + CUSHION_SIDE_INSET, z_in - CUSHION_BACK_GAP,
                              y_frame_top, SEAT_H))
    # 貫 (四方・同じ高さ。脚の中心まで差し込む)
    x_mid, z_mid = hw - L * 0.5, hd - L * 0.5
    y0, y1 = STRETCHER_Y, STRETCHER_Y + STRETCHER_H
    for nm, sz in (("stretcher_F", -1), ("stretcher_B", 1)):
        c = sz * z_mid
        parts.append(make_box(nm, (-x_mid, y0, c - STRETCHER_T * 0.5), (x_mid, y1, c + STRETCHER_T * 0.5),
                              STRETCHER_BEVEL))
    for nm, sx in (("stretcher_L", 1), ("stretcher_R", -1)):
        c = sx * x_mid
        parts.append(make_box(nm, (c - STRETCHER_T * 0.5, y0, -z_mid), (c + STRETCHER_T * 0.5, y1, z_mid),
                              STRETCHER_BEVEL))
    # 背もたれ: 上の横木 + 縦の桟
    zc = z_mid
    parts.append(make_box("back_rail", (-x_in - EMBED * 2, BACK_H - RAIL_H, zc - RAIL_T * 0.5),
                          (x_in + EMBED * 2, BACK_H, zc + RAIL_T * 0.5), RAIL_BEVEL))
    pitch = 2.0 * x_in / (SLAT_COUNT + 1)
    for k in range(SLAT_COUNT):
        x = -x_in + pitch * (k + 1)
        parts.append(make_box("back_slat_%d" % k, (x - SLAT_W * 0.5, y_frame_top, zc - SLAT_T * 0.5),
                              (x + SLAT_W * 0.5, BACK_H - RAIL_H + EMBED, zc + SLAT_T * 0.5), SLAT_BEVEL))
    return parts


def build_object(obj_name, parts, mat):
    bm = bmesh.new()
    lc = bm.faces.layers.int.new("cell")
    lp = bm.faces.layers.int.new("part")
    for pi, p in enumerate(parts):
        vs = [bm.verts.new(c) for c in p.verts]
        for fi, c, s in zip(p.faces, p.cells, p.smooth):
            f = bm.faces.new([vs[i] for i in fi])
            f[lc] = c
            f[lp] = pi
            f.smooth = s
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')

    # パーツごとの情報 (エンジン座標)
    info = []
    for pi, p in enumerate(parts):
        fs = [f for f in bm.faces if f[lp] == pi]
        vs = {v for f in fs for v in f.verts}
        mn = [min(v.co[k] for v in vs) for k in range(3)]
        mx = [max(v.co[k] for v in vs) for k in range(3)]
        cells = Counter(f[lc] for f in fs)
        info.append(dict(name=p.name, tris=len(fs), min=mn, max=mx, cells=cells,
                         verts=[tuple(v.co) for v in vs]))

    # Blender 座標へ (鏡映なので面の向きを反転して外向きに戻す)
    for v in bm.verts:
        v.co = Vector(e2b(v.co))
    bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        u = cell_uv_blender(f[lc])
        for l in f.loops:
            l[uv].uv = u
    me = bpy.data.meshes.new(obj_name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(obj_name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob, info


# ---------------------------------------------------------------------------
# 当たり判定の箱
# ---------------------------------------------------------------------------
def aabb_of(info_by_name, names, y_lo=-1e9, y_hi=1e9):
    """パーツの頂点のうち Y が範囲内のものの AABB。Y は範囲で切る (角材は Y 方向の柱なので正確)"""
    pts = [p for n in names for p in info_by_name[n]["verts"] if y_lo - 1e-6 <= p[1] <= y_hi + 1e-6]
    mn = [min(p[k] for p in pts) for k in range(3)]
    mx = [max(p[k] for p in pts) for k in range(3)]
    pmn = min(info_by_name[n]["min"][1] for n in names)
    pmx = max(info_by_name[n]["max"][1] for n in names)
    mn[1] = max(pmn, y_lo)
    mx[1] = min(pmx, y_hi)
    return mn, mx


def box_from_minmax(name, mn, mx):
    return dict(name=name,
                center=[round((mn[k] + mx[k]) * 0.5, 4) for k in range(3)],
                size=[round(mx[k] - mn[k], 4) for k in range(3)])


def table_collider_spec():
    """設計の寸法から直接求めた箱 (形状からは求めない。レポートで形状と比べる)"""
    hw, hd = TABLE_W * 0.5, TABLE_D * 0.5
    y_top_bot = TABLE_H - TOP_T
    y_ap = y_top_bot - APRON_H
    x_out, z_out = hw - TABLE_LEG_INSET, hd - TABLE_LEG_INSET
    x_in, z_in = x_out - TABLE_LEG, z_out - TABLE_LEG
    boxes = [(box_from_minmax("top", (-hw, y_ap, -hd), (hw, TABLE_H, hd)),
              ["top", "apron_F", "apron_B", "apron_L", "apron_R"], None)]
    for nm, sx, sz in (("leg_FL", 1, -1), ("leg_FR", -1, -1), ("leg_BL", 1, 1), ("leg_BR", -1, 1)):
        xs = sorted((sx * x_in, sx * x_out))
        zs = sorted((sz * z_in, sz * z_out))
        boxes.append((box_from_minmax(nm, (xs[0], 0, zs[0]), (xs[1], y_top_bot, zs[1])), [nm], None))
    return boxes


def chair_collider_spec():
    hw, hd = CHAIR_W * 0.5, CHAIR_D * 0.5
    L = CHAIR_LEG
    y_frame_top = SEAT_H - CUSHION_T
    y_frame_bot = y_frame_top - SEAT_FRAME_T
    x_in, z_in = hw - L, hd - L
    x_mid, z_mid = hw - L * 0.5, hd - L * 0.5
    boxes = [(box_from_minmax("seat", (-hw, y_frame_bot, -hd), (hw, SEAT_H, hd)),
              ["seat_frame", "cushion"], None),
             (box_from_minmax("back", (-hw, SEAT_H, z_in), (hw, BACK_H, hd)),
              ["leg_BL", "leg_BR", "back_rail"] + ["back_slat_%d" % k for k in range(SLAT_COUNT)],
              (SEAT_H, BACK_H))]
    for nm, sx, sz in (("leg_FL", 1, -1), ("leg_FR", -1, -1), ("leg_BL", 1, 1), ("leg_BR", -1, 1)):
        xs = sorted((sx * x_in, sx * hw))
        zs = sorted((sz * z_in, sz * hd))
        boxes.append((box_from_minmax(nm, (xs[0], 0, zs[0]), (xs[1], y_frame_bot, zs[1])), [nm],
                      (0.0, y_frame_bot)))
    y0, y1 = STRETCHER_Y, STRETCHER_Y + STRETCHER_H
    t = STRETCHER_T * 0.5
    boxes.append((box_from_minmax("stretcher_F", (-x_mid, y0, -z_mid - t), (x_mid, y1, -z_mid + t)), ["stretcher_F"], None))
    boxes.append((box_from_minmax("stretcher_B", (-x_mid, y0, z_mid - t), (x_mid, y1, z_mid + t)), ["stretcher_B"], None))
    boxes.append((box_from_minmax("stretcher_L", (x_mid - t, y0, -z_mid), (x_mid + t, y1, z_mid)), ["stretcher_L"], None))
    boxes.append((box_from_minmax("stretcher_R", (-x_mid - t, y0, -z_mid), (-x_mid + t, y1, z_mid)), ["stretcher_R"], None))
    return boxes


def write_colliders(path, model, boxes):
    data = dict(model=model, unit="cm",
                coordinate="engine (left-handed, +Y up), offset from model origin",
                boxes=[b for b, _, _ in boxes])
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


# ---------------------------------------------------------------------------
# パレット・マテリアル・書き出し
# ---------------------------------------------------------------------------
def make_palette(path):
    cs = PALETTE_SIZE // PALETTE_GRID
    rows = []
    for y in range(PALETTE_SIZE):
        row = bytearray()
        r = y // cs
        for x in range(PALETTE_SIZE):
            row += bytes(PALETTE[r * PALETTE_GRID + x // cs][1])
        rows.append(row)
    write_png(path, PALETTE_SIZE, PALETTE_SIZE, rows)


def make_material(img):
    mat = bpy.data.materials.new("dining_palette")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = 'Closest'
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.75
    return mat


def export_obj(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=True,
                          forward_axis='NEGATIVE_Z', up_axis='Y', global_scale=1.0,
                          export_uv=True, export_normals=True, export_colors=False,
                          export_materials=True, path_mode='STRIP',
                          export_triangulated_mesh=True, export_object_groups=False,
                          export_material_groups=False, export_smooth_groups=False,
                          apply_modifiers=True)


# ---------------------------------------------------------------------------
# 検証 (書き出した OBJ を読み直して調べる)
# ---------------------------------------------------------------------------
def parse_obj(path):
    r = dict(v=[], vt=[], vn=[], faces=[], bad=0, o=0, g=0, usemtl=set(), mtllib=[])
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.split()
            if not s:
                continue
            k = s[0]
            if k == "v":
                r["v"].append(tuple(map(float, s[1:4])))
            elif k == "vt":
                r["vt"].append(tuple(map(float, s[1:3])))
            elif k == "vn":
                r["vn"].append(tuple(map(float, s[1:4])))
            elif k == "f":
                corners = []
                ok = len(s) == 4
                for c in s[1:]:
                    p = c.split("/")
                    if len(p) != 3 or not p[1] or not p[2]:
                        ok = False
                        p = (p + ["", "", ""])[:3]
                    corners.append(tuple(int(x) - 1 if x else -1 for x in p))
                if not ok:
                    r["bad"] += 1
                r["faces"].append(corners)
            elif k == "o":
                r["o"] += 1
            elif k == "g":
                r["g"] += 1
            elif k == "usemtl":
                r["usemtl"].add(s[1])
            elif k == "mtllib":
                r["mtllib"].append(" ".join(s[1:]))
    return r


def parse_mtl(path):
    n, maps = 0, []
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if s.startswith("newmtl"):
                n += 1
            elif s.startswith("map_Kd"):
                maps.append(s)
    return n, maps


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(a):
    l = math.sqrt(dot(a, a))
    return (a[0] / l, a[1] / l, a[2] / l) if l > 0 else a


def analyze(obj):
    """OBJ の中身を調べる。座標はエンジン座標 (-x, y, z) に直して返す"""
    V = obj["v"]
    E = [(-x, y, z) for x, y, z in V]
    res = {}
    # UV
    cell_tris = Counter()
    off_center = 0
    multi = 0
    for fc in obj["faces"]:
        cs = set()
        for _, ti, _ in fc:
            if ti < 0:
                continue
            u, v = obj["vt"][ti]
            cu, cv = u * PALETTE_GRID, (1.0 - v) * PALETTE_GRID
            if abs(cu - math.floor(cu) - 0.5) > 1e-4 or abs(cv - math.floor(cv) - 0.5) > 1e-4:
                off_center += 1
            cs.add(int(math.floor(cv)) * PALETTE_GRID + int(math.floor(cu)))
        if len(cs) > 1:
            multi += 1
        cell_tris[min(cs) if cs else -1] += 1
    res.update(cell_tris=cell_tris, uv_off_center=off_center, uv_multi=multi)
    # 法線: 平らな面 (3頂点の法線が同じ) はその法線が面の向きと一致するか
    flat, smooth, flat_bad = 0, 0, 0
    for fc in obj["faces"]:
        ns = [norm(obj["vn"][ni]) for _, _, ni in fc]
        p = [V[vi] for vi, _, _ in fc]
        fn = norm(cross(sub(p[1], p[0]), sub(p[2], p[0])))
        if all(dot(ns[0], n) > 0.99999 for n in ns[1:]):
            flat += 1
            if dot(fn, ns[0]) < 0.9999:
                flat_bad += 1
        else:
            smooth += 1
    res.update(flat=flat, smooth=smooth, flat_bad=flat_bad)
    # 殻 (頂点を共有する面のまとまり) と向き、境界エッジ
    parent = list(range(len(V)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    edges = Counter()
    for fc in obj["faces"]:
        ids = [vi for vi, _, _ in fc]
        for i in range(3):
            a, b = ids[i], ids[(i + 1) % 3]
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[ra] = rb
            edges[(min(a, b), max(a, b))] += 1
    vol = defaultdict(float)
    for fc in obj["faces"]:
        p = [V[vi] for vi, _, _ in fc]
        vol[find(fc[0][0])] += dot(p[0], cross(p[1], p[2])) / 6.0
    res.update(shells=len(vol), shells_inward=sum(1 for x in vol.values() if x <= 0),
               boundary_edges=sum(1 for c in edges.values() if c == 1),
               nonmanifold_edges=sum(1 for c in edges.values() if c > 2))
    res["E"] = E
    res["min"] = [min(p[k] for p in E) for k in range(3)]
    res["max"] = [max(p[k] for p in E) for k in range(3)]
    res["far"] = max(math.sqrt(dot(p, p)) for p in E)
    return res


def collider_lines(boxes, info, E, model_min, model_max):
    by = {d["name"]: d for d in info}
    lines = []
    worst = 0.0
    for b, parts, yr in boxes:
        c, s = b["center"], b["size"]
        bmn = [c[k] - s[k] * 0.5 for k in range(3)]
        bmx = [c[k] + s[k] * 0.5 for k in range(3)]
        if yr:
            tmn, tmx = aabb_of(by, parts, yr[0], yr[1])
        else:
            tmn, tmx = aabb_of(by, parts)
        d = max(max(abs(bmn[k] - tmn[k]), abs(bmx[k] - tmx[k])) for k in range(3))
        worst = max(worst, d)
        rng = "" if not yr else "  (Y %.1f〜%.1f の部分)" % yr
        lines.append("  %-12s center %s  size %s" % (b["name"], fmt3(c, 2), fmt3(s, 2).replace("+", "")))
        lines.append("  %-12s   対象: %s%s  形状との差の最大 %.4f" % ("", ", ".join(parts), rng, d))
    # どの箱にも入らない頂点
    outside = 0
    eps = 1e-3
    bl = []
    for b, _, _ in boxes:
        c, s = b["center"], b["size"]
        bl.append(([c[k] - s[k] * 0.5 - eps for k in range(3)], [c[k] + s[k] * 0.5 + eps for k in range(3)]))
    for p in E:
        if not any(all(mn[k] <= p[k] <= mx[k] for k in range(3)) for mn, mx in bl):
            outside += 1
    umn = [min(m[k] + eps for m, _ in bl) for k in range(3)]
    umx = [max(m[k] - eps for _, m in bl) for k in range(3)]
    ud = max(max(abs(umn[k] - model_min[k]), abs(umx[k] - model_max[k])) for k in range(3))
    lines.append("  箱の形状との差の最大 (全箱)          : %.4f  (0.5 以内であること)" % worst)
    lines.append("  どの箱にも入らない頂点               : %d  (0であること)" % outside)
    lines.append("  全箱を合わせた範囲 min %s max %s" % (fmt3(umn, 2), fmt3(umx, 2)))
    lines.append("  見た目のバウンディングボックスとの差 : %.4f" % ud)
    return lines, worst, outside


def obj_block(name, obj, mtl_n, mtl_maps, an, info):
    L = []
    L.append("[OBJ / ローダー制約]")
    L.append("  三角形数           : %d" % len(obj["faces"]))
    L.append("  頂点 v / vt / vn   : %d / %d / %d" % (len(obj["v"]), len(obj["vt"]), len(obj["vn"])))
    L.append("  三角形以外・UV/法線欠けの面 : %d  (0であること)" % obj["bad"])
    L.append("  o/g の数           : %d  (1であること)" % (obj["o"] + obj["g"]))
    L.append("  usemtl の種類      : %d  (1であること)" % len(obj["usemtl"]))
    L.append("  MTLのマテリアル数  : %d  (1であること)" % mtl_n)
    L.append("  map_Kd             : %s" % mtl_maps)
    L.append("  UV: 各面の3頂点ともマスの中心 (左上原点で ((col+0.5)/4, (row+0.5)/4))")
    L.append("  マスの境界上のUV / 1面で複数マスにまたがる面 : %d / %d  (0であること)"
             % (an["uv_off_center"], an["uv_multi"]))
    L.append("  法線: 平らな面はすべてシャープ、座面クッションの側面〜上面だけスムーズ")
    L.append("    平らな面 / なめらかな面 : %d / %d" % (an["flat"], an["smooth"]))
    L.append("    平らな面で法線が面の向きとずれている面 : %d  (0であること)" % an["flat_bad"])
    L.append("  殻の数 / 外向き(符号付き体積 > 0)でない殻 : %d / %d" % (an["shells"], an["shells_inward"]))
    L.append("  境界エッジ / 3面以上が共有するエッジ : %d / %d  (0であること。すべて閉じた殻)"
             % (an["boundary_edges"], an["nonmanifold_edges"]))
    L.append("")
    L.append("[パーツ (一体の1オブジェクト内の内訳)]")
    for d in info:
        cells = ", ".join("%d:%d" % (c, n) for c, n in sorted(d["cells"].items()))
        L.append("  %-12s tris %4d  min %s max %s  cells {%s}"
                 % (d["name"], d["tris"], fmt3(d["min"], 2), fmt3(d["max"], 2), cells))
    L.append("  三角形数の合計 : %d" % sum(d["tris"] for d in info))
    return L


def dims_block(an):
    mn, mx = an["min"], an["max"]
    L = ["[寸法(エンジン座標・1 unit = 1 cm)]"]
    L.append("  幅 X   : %.4f" % (mx[0] - mn[0]))
    L.append("  高さ Y : %.4f" % (mx[1] - mn[1]))
    L.append("  奥行 Z : %.4f" % (mx[2] - mn[2]))
    L.append("  バウンディングボックス")
    L.append("    min = %s" % fmt3(mn))
    L.append("    max = %s" % fmt3(mx))
    L.append("  脚の下端 Y = %.4f  (0であること)" % mn[1])
    L.append("  X・Z の中心 (bbox) = (%+.4f, %+.4f)" % ((mn[0] + mx[0]) * 0.5, (mn[2] + mx[2]) * 0.5))
    L.append("  原点から最も遠い頂点までの距離 : %.4f" % an["far"])
    return L


def leg_rects(info, names):
    by = {d["name"]: d for d in info}
    return [(by[n]["min"], by[n]["max"]) for n in names]


def table_space_block(an, info):
    """脚以外のパーツ (天板・幕板) の最も低い点と、それより下の頂点がすべて脚の中にあるかを調べる。
       幕板は端を脚に差し込んでいるので、頂点の位置ではなくパーツで分ける。"""
    E = an["E"]
    leg_names = ["leg_FL", "leg_FR", "leg_BL", "leg_BR"]
    legs = leg_rects(info, leg_names)
    clear_h = min(min(p[1] for p in d["verts"]) for d in info if d["name"] not in leg_names)
    eps = 1e-4

    def in_leg(p):
        return any(all(mn[k] - eps <= p[k] <= mx[k] + eps for k in range(3)) for mn, mx in legs)
    low = [p for p in E if p[1] < clear_h - 1e-3]
    low_not_leg = sum(1 for p in low if not in_leg(p))
    gap_x = min(p[0] for p in low if p[0] > 0) - max(p[0] for p in low if p[0] < 0)
    gap_z = min(p[2] for p in low if p[2] > 0) - max(p[2] for p in low if p[2] < 0)
    by = {d["name"]: d for d in info}
    L = ["[テーブルの下のくぐれる空間]"]
    L.append("  床から幕板の下まで (天板・幕板の最も低い点) : %.4f" % clear_h)
    L.append("  その高さより下で脚の外にある頂点 : %d  (0であること。下は脚以外何もない)" % low_not_leg)
    L.append("  脚の間の幅 X (左右の脚の内側の面の間・長辺側から抜ける幅) : %.4f" % gap_x)
    L.append("  脚の間の幅 Z (前後の脚の内側の面の間・短辺側から抜ける幅) : %.4f" % gap_z)
    L.append("  脚の外側の面の位置 : X = ±%.4f, Z = ±%.4f (天板の縁から %.1f 内側)"
             % (max(m[0] for _, m in legs), max(m[2] for _, m in legs), TABLE_LEG_INSET))
    L.append("  幕板の外側の面は脚の外側の面から %.1f 内側 (X = ±%.4f, Z = ±%.4f)"
             % (APRON_SETBACK, by["apron_L"]["max"][0], by["apron_B"]["max"][2]))
    return L


def chair_space_block(an, info):
    by = {d["name"]: d for d in info}
    E = an["E"]
    back_pts = [p for p in E if p[1] > SEAT_H + 1.0]
    zmean = sum(p[2] for p in back_pts) / len(back_pts)
    cush = by["cushion"]
    L = ["[椅子の正面の確認]"]
    L.append("  座面より上 (Y > %.1f) の頂点の Z 平均 = %+.4f  (正 = 背もたれが +Z 側であること)"
             % (SEAT_H + 1.0, zmean))
    L.append("  座面クッション: 前の縁 Z = %+.4f (負であること), 後ろの縁 Z = %+.4f"
             % (cush["min"][2], cush["max"][2]))
    L.append("  背もたれ (上の横木): Z %+.4f〜%+.4f, Y %.4f〜%.4f"
             % (by["back_rail"]["min"][2], by["back_rail"]["max"][2], by["back_rail"]["min"][1], by["back_rail"]["max"][1]))
    L.append("  座面の高さ (クッション上面の最も高い点) : %.4f" % cush["max"][1])
    L.append("  背もたれの上端                           : %.4f" % an["max"][1])
    L.append("")
    sF = by["stretcher_F"]
    L.append("[椅子の下の抜けられる空間]")
    L.append("  貫の下 (床〜貫の下面)          : %.4f" % sF["min"][1])
    L.append("  貫の上 (貫の上面〜座枠の下面)  : %.4f" % (by["seat_frame"]["min"][1] - sF["max"][1]))
    L.append("  脚の間の幅 X (左右の脚の内側)  : %.4f" % (by["leg_FL"]["min"][0] - by["leg_FR"]["max"][0]))
    L.append("  脚の間の幅 Z (前後の脚の内側)  : %.4f" % (by["leg_BL"]["min"][2] - by["leg_FL"]["max"][2]))
    L.append("  ※ 貫は四方とも同じ高さ (Y %.1f〜%.1f)。どの向きから見ても同じ形" % (STRETCHER_Y, STRETCHER_Y + STRETCHER_H))
    return L


# ---------------------------------------------------------------------------
# プレビュー
# ---------------------------------------------------------------------------
def make_vis_material(name, col, alpha_emit, strength):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    mix = nt.nodes.new("ShaderNodeMixShader")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = (*col, 1.0)
    em.inputs["Strength"].default_value = strength
    mix.inputs["Fac"].default_value = alpha_emit
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], out.inputs["Surface"])
    return m


def add_box_object(name, center_e, size_e, mat, wire=False):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    c = e2b(center_e)
    s = (size_e[0], size_e[2], size_e[1])
    for v in bm.verts:
        v.co = Vector((c[0] + v.co[0] * s[0], c[1] + v.co[1] * s[1], c[2] + v.co[2] * s[2]))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    if wire:
        mod = ob.modifiers.new("wire", 'WIREFRAME')
        mod.thickness = 0.5
        mod.use_replace = True
    return ob


def look_at(cam, loc_e, target_e):
    cam.location = Vector(e2b(loc_e))
    d = Vector(e2b(target_e)) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def render_preview(out_dir, table_ob, chair_ob, table_boxes, chair_boxes):
    import numpy as np
    scene = bpy.context.scene
    # 床 (プレビューだけ)
    fm = bpy.data.materials.new("preview_floor")
    fm.use_nodes = True
    fb = fm.node_tree.nodes["Principled BSDF"]
    fb.inputs["Base Color"].default_value = (*[srgb_to_linear(c) for c in PREVIEW_FLOOR_RGB], 1.0)
    fb.inputs["Roughness"].default_value = 0.55
    add_box_object("preview_floor", (0, -0.6, 0), (600, 1.0, 600), fm)

    chairs = []
    poses = [((PREVIEW_CHAIR_X, 0, PREVIEW_CHAIR_Z), False), ((-PREVIEW_CHAIR_X, 0, PREVIEW_CHAIR_Z), False),
             ((PREVIEW_CHAIR_X, 0, -PREVIEW_CHAIR_Z), True), ((-PREVIEW_CHAIR_X, 0, -PREVIEW_CHAIR_Z), True)]
    for i, (pos, flip) in enumerate(poses):
        ob = bpy.data.objects.new("chair_%d" % i, chair_ob.data)
        scene.collection.objects.link(ob)
        ob.location = Vector(e2b(pos))
        ob.rotation_euler = (0, 0, math.pi if flip else 0)
        chairs.append(ob)
    chair_ob.hide_render = True

    # 当たり判定の箱 (半透明 + 輪郭)
    vis = []
    mt = make_vis_material("col_table", (1.0, 0.42, 0.08), 0.28, 1.0)
    mtw = make_vis_material("col_table_w", (1.0, 0.55, 0.15), 1.0, 2.0)
    mc = make_vis_material("col_chair", (0.10, 0.75, 1.0), 0.28, 1.0)
    mcw = make_vis_material("col_chair_w", (0.30, 0.85, 1.0), 1.0, 2.0)
    for b, _, _ in table_boxes:
        vis.append(add_box_object("tb_" + b["name"], b["center"], b["size"], mt))
        vis.append(add_box_object("tbw_" + b["name"], b["center"], b["size"], mtw, wire=True))
    for i, (pos, flip) in enumerate(poses):
        for b, _, _ in chair_boxes:
            c = b["center"]
            if flip:
                c = (-c[0], c[1], -c[2])
            c = (c[0] + pos[0], c[1] + pos[1], c[2] + pos[2])
            vis.append(add_box_object("cb%d_%s" % (i, b["name"]), c, b["size"], mc))
            vis.append(add_box_object("cbw%d_%s" % (i, b["name"]), c, b["size"], mcw, wire=True))

    # 光
    world = bpy.data.worlds.new("preview_world")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.36, 0.36, 0.38, 1.0)
    bg.inputs["Strength"].default_value = 0.8
    scene.world = world
    sun_d = bpy.data.lights.new("sun", 'SUN')
    sun_d.energy = 3.0
    sun_d.angle = math.radians(8)
    sun = bpy.data.objects.new("sun", sun_d)
    scene.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(42), 0, math.radians(-38))

    cam_d = bpy.data.cameras.new("cam")
    cam_d.clip_start = 0.5
    cam_d.clip_end = 5000
    cam = bpy.data.objects.new("cam", cam_d)
    scene.collection.objects.link(cam)
    scene.camera = cam

    r = scene.render
    r.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = PREVIEW_SAMPLES
    scene.cycles.use_denoising = True
    scene.cycles.max_bounces = 4
    scene.cycles.transparent_max_bounces = 32
    r.resolution_x, r.resolution_y = PREVIEW_SIZE
    r.resolution_percentage = 100
    r.image_settings.file_format = 'PNG'
    r.image_settings.color_mode = 'RGB'
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.exposure = PREVIEW_EXPOSURE

    views = [
        ("oblique", (-190, 175, -215), (0, 30, 0), 38, False),
        ("under", (-150, 10, -16), (0, 27, 6), 18, False),
        ("oblique_col", (-190, 175, -215), (0, 30, 0), 38, True),
        ("under_col", (-150, 10, -16), (0, 27, 6), 18, True),
    ]
    imgs = []
    for nm, loc, tgt, lens, show_col in views:
        for o in vis:
            o.hide_render = not show_col
        cam_d.lens = lens
        look_at(cam, loc, tgt)
        path = os.path.join(out_dir, "_preview_%s.png" % nm)
        r.filepath = path
        bpy.ops.render.render(write_still=True)
        im = bpy.data.images.load(path)
        w, h = im.size
        a = np.empty(w * h * 4, dtype=np.float32)
        im.pixels.foreach_get(a)
        a = a.reshape(h, w, 4)[::-1, :, :3]
        imgs.append((np.clip(a * 255.0 + 0.5, 0, 255)).astype(np.uint8))
        bpy.data.images.remove(im)
        os.remove(path)
    top = np.concatenate([imgs[0], imgs[1]], axis=1)
    bot = np.concatenate([imgs[2], imgs[3]], axis=1)
    full = np.concatenate([top, bot], axis=0)
    H, W = full.shape[:2]
    write_png(os.path.join(out_dir, OUT_NAMES["preview"]), W, H, [full[y].tobytes() for y in range(H)])
    return views, poses


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out, preview = None, False
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out = argv[i + 1]
            i += 2
        elif argv[i] == "--preview":
            preview = True
            i += 1
        else:
            i += 1
    if out is None:
        out = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else os.getcwd()
    return os.path.abspath(out), preview


def main():
    out_dir, preview = parse_args()
    os.makedirs(out_dir, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)

    pal_path = os.path.join(out_dir, OUT_NAMES["palette"])
    make_palette(pal_path)
    img = bpy.data.images.load(pal_path)
    mat = make_material(img)

    models = {}
    for name, fn, spec in (("dining_table", table_parts, table_collider_spec),
                           ("dining_chair", chair_parts, chair_collider_spec)):
        ob, info = build_object(name, fn(), mat)
        obj_path = os.path.join(out_dir, name + ".obj")
        export_obj(ob, obj_path)
        boxes = spec()
        write_colliders(os.path.join(out_dir, name + "_colliders.json"), name, boxes)
        models[name] = dict(ob=ob, info=info, boxes=boxes, obj_path=obj_path,
                            mtl_path=os.path.join(out_dir, name + ".mtl"))

    # レポート
    R = []
    R.append("=== dining 生成レポート (Blender %s) ===" % bpy.app.version_string)
    R.append("")
    R.append("[作り直し]")
    R.append("  blender -b -P generate_dining.py -- --out <出力フォルダ> [--preview]")
    R.append("  例: blender -b -P generate_dining.py -- --out ./out --preview")
    R.append("  主な寸法は generate_dining.py の先頭の定数で変えられる")
    R.append("")
    R.append("[共通]")
    R.append("  座標: エンジン座標 (左手系・+Y上)。1 unit = 1 cm。OBJ = (-ex, ey, ez) で書き出し")
    R.append("  原点: 床に接する面の中心 (脚の下端 Y = 0、X・Z は脚の配置の中心)")
    R.append("  左右: 正面 (-Z) を向いたときの左 = +X。F = -Z 側, B = +Z 側")
    R.append("  パレット: %s (2つのモデルで共有)" % OUT_NAMES["palette"])
    R.append("  当たり判定: <モデル名>_colliders.json  (name / center / size、cm、エンジン座標)")
    all_ok = True
    for name in ("dining_table", "dining_chair"):
        m = models[name]
        obj = parse_obj(m["obj_path"])
        mtl_n, maps = parse_mtl(m["mtl_path"])
        an = analyze(obj)
        m["an"] = an
        R.append("")
        R.append("=" * 72)
        R.append("  %s" % name)
        R.append("=" * 72)
        R += obj_block(name, obj, mtl_n, maps, an, m["info"])
        R.append("")
        R += dims_block(an)
        R.append("")
        if name == "dining_table":
            R += table_space_block(an, m["info"])
        else:
            R += chair_space_block(an, m["info"])
        R.append("")
        R.append("[当たり判定の箱 (%s_colliders.json, %d 個)]" % (name, len(m["boxes"])))
        cl, worst, outside = collider_lines(m["boxes"], m["info"], an["E"], an["min"], an["max"])
        R += cl
        ok = (obj["bad"] == 0 and obj["o"] + obj["g"] == 1 and len(obj["usemtl"]) == 1 and mtl_n == 1
              and an["uv_off_center"] == 0 and an["uv_multi"] == 0 and an["flat_bad"] == 0
              and an["shells_inward"] == 0 and an["boundary_edges"] == 0 and abs(an["min"][1]) < 1e-6
              and worst <= 0.5 and outside == 0)
        all_ok = all_ok and ok
        R.append("")
        R.append("  確認項目の判定 : %s" % ("OK" if ok else "NG"))

    R.append("")
    R.append("[パレット (%dx%d, %dx%d マス, 1マス%dpx, uv_rect は左上原点)]"
             % (PALETTE_SIZE, PALETTE_SIZE, PALETTE_GRID, PALETTE_GRID, PALETTE_SIZE // PALETTE_GRID))
    ct = models["dining_table"]["an"]["cell_tris"]
    cc = models["dining_chair"]["an"]["cell_tris"]
    for i, (nm, rgb, desc) in enumerate(PALETTE):
        R.append("  cell %2d  %-18s rgb(%3d,%3d,%3d)  uv_rect %s  tris table %4d / chair %4d  %s"
                 % (i, nm, rgb[0], rgb[1], rgb[2], cell_rect_dx(i), ct.get(i, 0), cc.get(i, 0), desc))
    R.append("  木部の塗り分け: 法線の Y 成分 > %.2f → oak_top、< -%.2f → oak_shade、それ以外 → oak_side"
             % (WOOD_UP_NY, WOOD_UP_NY))
    R.append("                  (幕板の内側を向いた面は oak_shade)")
    R.append("  クッション    : 法線の Y 成分 > %.2f → fabric_green、それ以外 → fabric_green_dark" % FABRIC_UP_NY)
    R.append("  床 (floor_palette) の板の色 rgb(78〜100, 49〜63, 38〜47) とは別の色")

    if preview:
        views, poses = render_preview(out_dir, models["dining_table"]["ob"], models["dining_chair"]["ob"],
                                      models["dining_table"]["boxes"], models["dining_chair"]["boxes"])
        R.append("")
        R.append("[プレビュー (%s, 2x2 枚)]" % OUT_NAMES["preview"])
        R.append("  左上: 斜め上から  右上: 床上10cmからテーブルの下  左下・右下: 同じ視点で当たり判定の箱を重ねた図")
        R.append("  (橙 = テーブルの箱、水色 = 椅子の箱)")
        R.append("  椅子の配置 (エンジン座標・プレビューだけ):")
        for i, (pos, flip) in enumerate(poses):
            R.append("    chair_%d  pos %s  Y軸回転 %d°  (正面がテーブルを向く)" % (i, fmt3(pos, 1), 180 if flip else 0))
        R.append("  床の板 rgb%s はプレビューにだけ置いたもので、OBJ には含まない" % (PREVIEW_FLOOR_RGB,))
        R.append("  光は形が分かりやすい昼光 (太陽 + 空の明かり) で、夜の照明での見え方とは異なる")

    R.append("")
    R.append("総合判定 : %s" % ("OK" if all_ok else "NG"))
    with open(os.path.join(out_dir, OUT_NAMES["report"]), "w", encoding="utf-8") as f:
        f.write("\n".join(R) + "\n")
    print("\n".join(R))


if __name__ == "__main__":
    main()
