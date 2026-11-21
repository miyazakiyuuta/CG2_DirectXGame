# -*- coding: utf-8 -*-
"""
generate_drone.py  -  自機(超小型の駆除ドローン)生成スクリプト  (Blender 4.2)

使い方:
  blender -b -P generate_drone.py -- --out <出力フォルダ>
  (Blender のテキストエディタから実行した場合は、.blend と同じフォルダ、
   未保存ならカレントフォルダに書き出す)

書き出すもの:
  drone.obj / drone.mtl / drone_palette.png / drone_pivots.json / drone_report.txt

座標の扱い (termite_worker と同じ):
  - 形状は「エンジン座標 (左手系・+Y上・機首 -Z・機体の左 = +X)」で設計する。
  - Blender へは  blender = (-ex, -ez, ey)  で置き、
    forward_axis='NEGATIVE_Z', up_axis='Y' で書き出す。
    OBJ = (bx, bz, -by) = (-ex, ey, ez)  (反転前の右手系の値)
    エンジンの Assimp 読み込みで X が反転され、エンジン座標に戻る。
  - JSON とレポートの座標はエンジン座標で書く。
  - パレットの uv_rect は左上原点 (DirectX)。OBJ の vt は左下原点で書く。
"""

import bpy
import bmesh
import math
import json
import os
import sys
import struct
import zlib
from mathutils import Vector

MODEL = "drone"

# ---------------------------------------------------------------------------
# パレット (256x256, 4x4, 1マス64px, 番号は左上から右へ)
# ---------------------------------------------------------------------------
PALETTE_SIZE = 256
GRID = 4
CELL_PX = PALETTE_SIZE // GRID

PALETTE = [
    # idx  name              sRGB              note
    (0,  "body_white",     (236, 239, 242), "胴体の主色"),
    (1,  "body_grey",      (168, 176, 186), "胴体の下面・タンクの端"),
    (2,  "accent_lime",    (178, 255, 20),  "アクセント(蛍光の黄緑)・回さないパーツで共有"),
    (3,  "dark",           (44, 50, 58),    "暗部(穴・継ぎ目・ハウジング)・回さないパーツで共有"),
    (4,  "arm_L",          (200, 206, 214), "左腕"),
    (5,  "arm_R",          (200, 206, 214), "右腕"),
    (6,  "guard_L",        (222, 226, 231), "左ガード"),
    (7,  "guard_R",        (222, 226, 231), "右ガード"),
    (8,  "rotor_L",        (146, 160, 176), "左ローター(回す・専用)"),
    (9,  "rotor_R",        (146, 160, 176), "右ローター(回す・専用)"),
    (10, "sensor_lens",    (30, 224, 255),  "センサーのレンズ(発光用・専用)"),
    (11, "nozzle_front",   (128, 136, 146), "殺虫剤の噴射口"),
    (12, "thruster",       (156, 164, 174), "推進器のハウジング"),
    (13, "thruster_glow",  (165, 238, 255), "推進器の噴射口の発光面(発光用・専用)"),
    (14, "unused",         (128, 128, 128), "未使用"),
    (15, "unused",         (128, 128, 128), "未使用"),
]
C_BODY, C_GREY, C_ACC, C_DARK = 0, 1, 2, 3
C_ARM_L, C_ARM_R, C_GUARD_L, C_GUARD_R = 4, 5, 6, 7
C_ROTOR_L, C_ROTOR_R, C_LENS, C_NOZZLE = 8, 9, 10, 11
C_THR, C_GLOW = 12, 13

ROTATING_CELLS = (C_ROTOR_L, C_ROTOR_R)


def cell_uv_rect_topleft(c):
    col, row = c % GRID, c // GRID
    return [col / GRID, row / GRID, (col + 1) / GRID, (row + 1) / GRID]


def cell_uv_center_obj(c):
    """OBJ / Blender の UV (左下原点) でのマスの中心"""
    col, row = c % GRID, c // GRID
    return ((col + 0.5) / GRID, 1.0 - (row + 0.5) / GRID)


def write_palette_png(path):
    rows = []
    for y in range(PALETTE_SIZE):            # PNG は上の行から
        row = bytearray(b"\x00")             # filter: None
        r = y // CELL_PX
        for x in range(PALETTE_SIZE):
            c = r * GRID + x // CELL_PX
            row += bytes(PALETTE[c][2])
        rows.append(bytes(row))
    raw = b"".join(rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", PALETTE_SIZE, PALETTE_SIZE, 8, 2, 0, 0, 0))
    png += chunk(b"sRGB", b"\x00")
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


# ---------------------------------------------------------------------------
# 形状ビルダー (エンジン座標・設計単位)
#   コンポーネント = 1つの閉じた殻。パーツは複数のコンポーネントを持てる。
# ---------------------------------------------------------------------------
class Comp:
    def __init__(self, part):
        self.part = part
        self.verts = []      # Vector (engine)
        self.faces = []      # (tuple(idx), cell)

    def v(self, p):
        self.verts.append(Vector(p))
        return len(self.verts) - 1

    def f(self, idx, cell):
        self.faces.append((tuple(idx), cell))


COMPS = []


def new_comp(part):
    c = Comp(part)
    COMPS.append(c)
    return c


def lathe(part, profile, cells, seg, origin, axis, ref, closed=False, theta0=0.0):
    """profile: [(r, a)] 軸方向 a・半径 r。r=0 は軸上の1点(極)。
    cells[i] は profile[i] -> profile[i+1] の帯の色。"""
    c = new_comp(part)
    O = Vector(origin)
    A = Vector(axis).normalized()
    U = Vector(ref)
    U = (U - A * A.dot(U)).normalized()
    W = A.cross(U)
    rings = []
    for r, a in profile:
        if r < 1e-9:
            rings.append([c.v(O + A * a)])
        else:
            ring = []
            for k in range(seg):
                t = theta0 + 2.0 * math.pi * k / seg
                ring.append(c.v(O + A * a + (U * math.cos(t) + W * math.sin(t)) * r))
            rings.append(ring)
    n = len(profile)
    m = n if closed else n - 1
    for i in range(m):
        R0, R1 = rings[i], rings[(i + 1) % n]
        cell = cells[i]
        if len(R0) == 1 and len(R1) == 1:
            continue
        for k in range(seg):
            k1 = (k + 1) % seg
            if len(R0) == 1:
                c.f((R0[0], R1[k1], R1[k]), cell)
            elif len(R1) == 1:
                c.f((R0[k], R0[k1], R1[0]), cell)
            else:
                c.f((R0[k], R0[k1], R1[k1], R1[k]), cell)
    return c


def loft(part, rings_pts, cellf, start_apex=None, end_apex=None,
         start_cell=None, end_cell=None):
    """rings_pts: 同じ点数の輪の列。cellf(seg_index, k) -> cell。
    端は apex(点)で閉じるか、None なら n角形で閉じる。"""
    c = new_comp(part)
    rings = [[c.v(p) for p in ring] for ring in rings_pts]
    n = len(rings[0])
    for i in range(len(rings) - 1):
        R0, R1 = rings[i], rings[i + 1]
        for k in range(n):
            k1 = (k + 1) % n
            c.f((R0[k], R0[k1], R1[k1], R1[k]), cellf(i, k))
    R0, RN = rings[0], rings[-1]
    if start_apex is not None:
        P = c.v(start_apex)
        for k in range(n):
            c.f((P, R0[(k + 1) % n], R0[k]), start_cell)
    else:
        c.f(tuple(reversed(R0)), start_cell)
    if end_apex is not None:
        P = c.v(end_apex)
        for k in range(n):
            c.f((RN[k], RN[(k + 1) % n], P), end_cell)
    else:
        c.f(tuple(RN), end_cell)
    return c


def box(part, p0, p1, half_w, half_h, up, cell):
    """p0->p1 を軸とする四角柱"""
    d = (Vector(p1) - Vector(p0)).normalized()
    v = Vector(up)
    v = (v - d * d.dot(v)).normalized()
    u = d.cross(v)
    def ring(p):
        p = Vector(p)
        return [p + u * half_w + v * half_h, p - u * half_w + v * half_h,
                p - u * half_w - v * half_h, p + u * half_w - v * half_h]
    return loft(part, [ring(p0), ring(p1)], lambda i, k: cell,
                start_cell=cell, end_cell=cell)


def mirror_part(src_part, dst_part, cell_map):
    """X を反転して左右反対のパーツを作る(面の向きも反転)"""
    for c in [c for c in COMPS if c.part == src_part]:
        m = new_comp(dst_part)
        m.verts = [Vector((-p.x, p.y, p.z)) for p in c.verts]
        m.faces = [(tuple(reversed(idx)), cell_map.get(cell, cell)) for idx, cell in c.faces]


# ---------------------------------------------------------------------------
# 設計 (エンジン座標・設計単位。後で全長1.0に正規化し重心を原点へ移す)
# ---------------------------------------------------------------------------
SEG_BODY = 24
SEG_GUARD = 64
SEG_HUB = 32
SEG_POD = 24
SEG_SENSOR = 28
SEG_NOZZLE = 20
SEG_THR = 36
SEG_TANK = 20

ROTOR_C = Vector((0.33, 0.05, 0.0))   # 左ローターの中心(羽根の回転面の中心)
GUARD_R_IN = 0.150
BLADE_TIP = 0.138

PIVOTS = {}   # name -> Vector (設計単位・エンジン座標)
POINTS = {}   # name -> (pos, dir)


def build_body():
    # (z, 半幅, 半高, 中心y)
    st = [
        (-0.500, 0.030, 0.026, -0.010),
        (-0.485, 0.058, 0.048, -0.008),
        (-0.460, 0.085, 0.068, -0.005),
        (-0.420, 0.108, 0.084, 0.000),
        (-0.360, 0.122, 0.094, 0.004),
        (-0.280, 0.130, 0.100, 0.007),
        (-0.180, 0.134, 0.104, 0.009),
        (-0.060, 0.134, 0.104, 0.010),
        (0.060, 0.130, 0.102, 0.010),
        (0.160, 0.122, 0.096, 0.008),
        (0.220, 0.114, 0.091, 0.006),
        (0.270, 0.106, 0.086, 0.004),
        (0.310, 0.098, 0.082, 0.002),
        (0.340, 0.092, 0.078, 0.000),
    ]
    N = SEG_BODY
    ex = 2.0 / 2.6   # 少し角ばった断面(超楕円)

    def sp(x):
        return math.copysign(abs(x) ** ex, x)

    rings = []
    for z, hw, hh, yc in st:
        ring = []
        for k in range(N):
            t = 2.0 * math.pi * k / N - math.pi / N   # 面の中心が 0°,15°,30°…に来る
            s = math.sin(t)
            hy = hh if s >= 0 else hh * 0.9           # 下面は少し平ら
            ring.append((hw * sp(math.cos(t)), yc + hy * sp(s), z))
        rings.append(ring)

    def cellf(i, k):
        z0, z1 = st[i][0], st[i + 1][0]
        if i == 10:                                   # 後ろ寄りの帯
            return C_ACC
        if k == 6 and -0.43 < z0 and z1 < -0.05:       # 背中のライン(タンクの前まで)
            return C_ACC
        if k in (1, 11) and -0.37 < z0 and z1 < 0.17:  # 側面のライン(赤道の少し上)
            return C_ACC
        if 13 <= k <= 23:                             # 下面
            return C_GREY
        return C_BODY

    loft("body", rings, cellf,
         start_apex=(0.0, -0.010, -0.506), start_cell=C_BODY,
         end_apex=(0.0, 0.000, 0.345), end_cell=C_DARK)

    # 背中の殺虫剤タンク(カプセル)
    R, z0, z1, ty = 0.046, -0.03, 0.15, 0.115
    prof, cells = [(0.0, z0 - R)], []
    for a in (67.5, 45.0, 22.5):
        t = math.radians(a)
        prof.append((R * math.cos(t), z0 - R * math.sin(t)))
    prof += [(R, z0), (R, z0 + 0.015), (R, z1 - 0.015), (R, z1)]
    for a in (22.5, 45.0, 67.5):
        t = math.radians(a)
        prof.append((R * math.cos(t), z1 + R * math.sin(t)))
    prof.append((0.0, z1 + R))
    cells = [C_GREY] * 4 + [C_DARK, C_ACC, C_DARK] + [C_GREY] * 4
    lathe("body", prof, cells, SEG_TANK, (0, ty, 0), (0, 0, 1), (1, 0, 0))

    PIVOTS["body"] = Vector((0, 0, 0))   # 後で重心(=原点)に置き換える


def build_sensor():
    O = Vector((0.0, 0.030, -0.400))
    A = (0, 0, -1)
    housing = [(0.0, 0.0), (0.045, 0.0), (0.047, 0.080), (0.047, 0.092),
               (0.043, 0.105), (0.034, 0.105), (0.034, 0.097), (0.0, 0.097)]
    hcells = [C_DARK, C_DARK, C_ACC, C_ACC, C_ACC, C_DARK, C_DARK]
    lathe("sensor", housing, hcells, SEG_SENSOR, O, A, (1, 0, 0))
    lens = [(0.0, 0.095), (0.035, 0.095), (0.035, 0.101), (0.030, 0.109),
            (0.019, 0.114), (0.0, 0.116)]
    lathe("sensor", lens, [C_LENS] * 5, SEG_SENSOR, O, A, (1, 0, 0))
    PIVOTS["sensor"] = O + Vector(A) * 0.105
    POINTS["light"] = (O + Vector(A) * 0.126, Vector((0, 0, -1)))


def build_nozzle():
    O = Vector((0.0, -0.045, -0.380))
    A = (0, 0, -1)
    prof = [(0.0, 0.0), (0.026, 0.0), (0.026, 0.070), (0.019, 0.080),
            (0.019, 0.130), (0.023, 0.135), (0.023, 0.150), (0.012, 0.150),
            (0.012, 0.140), (0.0, 0.140)]
    cells = [C_NOZZLE, C_NOZZLE, C_NOZZLE, C_NOZZLE, C_ACC, C_ACC, C_ACC, C_DARK, C_DARK]
    lathe("nozzle_front", prof, cells, SEG_NOZZLE, O, A, (1, 0, 0))
    PIVOTS["nozzle_front"] = O.copy()
    POINTS["muzzle"] = (O + Vector(A) * 0.150, Vector((0, 0, -1)))


def build_thruster():
    O = Vector((0.0, 0.0, 0.280))
    A = (0, 0, 1)
    prof = [(0.0, 0.0), (0.075, 0.0), (0.080, 0.060), (0.086, 0.120),
            (0.092, 0.130), (0.092, 0.170), (0.088, 0.180), (0.090, 0.215),
            (0.084, 0.220), (0.070, 0.220), (0.066, 0.160), (0.060, 0.140),
            (0.022, 0.140), (0.0, 0.185)]
    cells = [C_THR, C_THR, C_THR, C_DARK, C_DARK, C_DARK, C_THR, C_THR,
             C_ACC, C_DARK, C_DARK, C_GLOW, C_DARK]
    lathe("thruster", prof, cells, SEG_THR, O, A, (1, 0, 0))
    PIVOTS["thruster"] = O.copy()
    POINTS["thruster_exhaust"] = (O + Vector(A) * 0.220, Vector((0, 0, 1)))


def build_left_rotor_unit():
    C = ROTOR_C
    # --- 腕(六角柱・上面にアクセントのライン)
    root = Vector((0.080, 0.000, -0.010))
    end = Vector((C.x, C.y - 0.033, C.z))
    d = (end - root).normalized()
    vv = Vector((0, 1, 0))
    vv = (vv - d * d.dot(vv)).normalized()
    uu = vv.cross(d)          # だいたい Z 方向
    NS = 8
    rings = []
    for i in range(NS):
        t = i / (NS - 1)
        p = root.lerp(end, t)
        hw = 0.025 + (0.018 - 0.025) * t
        hh = 0.017 + (0.013 - 0.017) * t
        ring = []
        for k in range(6):
            a = math.radians(60 * k)
            ring.append(p + uu * (hw * math.cos(a)) + vv * (hh * math.sin(a)))
        rings.append(ring)
    loft("arm_L", rings,
         lambda i, k: C_ACC if (k == 1 and 1 <= i <= 4) else C_ARM_L,
         start_cell=C_ARM_L, end_cell=C_ARM_L)
    # モーターの台座
    pod = [(0.0, -0.060), (0.030, -0.060), (0.036, -0.052), (0.036, -0.020),
           (0.030, -0.014), (0.0, -0.014)]
    lathe("arm_L", pod, [C_DARK, C_ARM_L, C_ARM_L, C_DARK, C_DARK],
          SEG_POD, C, (0, 1, 0), (1, 0, 0))
    PIVOTS["arm_L"] = root.copy()

    # --- ガード(ダクト状の輪・上の縁をアクセント)
    g = [(0.150, -0.030), (0.148, 0.022), (0.154, 0.032), (0.168, 0.032),
         (0.174, 0.022), (0.174, -0.022), (0.168, -0.030)]
    lathe("guard_L", g, [C_GUARD_L, C_GUARD_L, C_ACC, C_ACC, C_GUARD_L, C_GUARD_L, C_GUARD_L],
          SEG_GUARD, C, (0, 1, 0), (1, 0, 0), closed=True, theta0=math.pi / SEG_GUARD)
    # ガードを支えるステー(外側・前・後ろ。内側は腕が支える)
    sy = C.y - 0.026
    for dirv in ((1, 0, 0), (0, 0, -1), (0, 0, 1)):
        dv = Vector(dirv)
        p0 = Vector((C.x, sy, C.z)) + dv * 0.030
        p1 = Vector((C.x, sy, C.z)) + dv * 0.162
        box("guard_L", p0, p1, 0.006, 0.005, (0, 1, 0), C_GUARD_L)
    PIVOTS["guard_L"] = C.copy()

    # --- ローター(ハブ + 3枚羽根)。専用マスのみ使用
    hub = [(0.0, -0.012), (0.028, -0.012), (0.028, 0.006), (0.022, 0.013),
           (0.012, 0.018), (0.0, 0.020)]
    lathe("rotor_L", hub, [C_ROTOR_L] * 5, SEG_HUB, C, (0, 1, 0), (1, 0, 0))
    # 羽根: 上から見て時計回り(XMMatrixRotationY の正の角度)で上向きの推力になるピッチ
    #   +X にある羽根は回ると -Z 側へ進む -> 前縁を -Z 側に置き、前縁を高くする
    span = [0.022, 0.040, 0.060, 0.080, 0.100, 0.118, 0.132, BLADE_TIP]
    chord = [0.026, 0.036, 0.040, 0.040, 0.037, 0.033, 0.028, 0.016]
    pitch = [16.0, 15.0, 13.5, 12.0, 10.5, 9.5, 9.0, 9.0]
    thick = 0.006
    for b in range(3):
        phi = math.radians(30.0 + 120.0 * b)
        cph, sph = math.cos(phi), math.sin(phi)
        rings = []
        for r, ch, pd in zip(span, chord, pitch):
            pa = math.radians(pd)
            hc = ch * 0.5
            local = [
                (r, hc * math.sin(pa), -hc * math.cos(pa)),     # 前縁(高い)
                (r, thick * 0.5, 0.0),                          # 上
                (r, -hc * math.sin(pa), hc * math.cos(pa)),     # 後縁(低い)
                (r, -thick * 0.5, 0.0),                         # 下
            ]
            ring = []
            for x, y, z in local:
                # DirectX の RotationY (行ベクトル): x' = x c + z s, z' = -x s + z c
                ring.append(C + Vector((x * cph + z * sph, y, -x * sph + z * cph)))
            rings.append(ring)
        loft("rotor_L", rings, lambda i, k: C_ROTOR_L,
             start_cell=C_ROTOR_L, end_cell=C_ROTOR_L)
    PIVOTS["rotor_L"] = C.copy()


PARTS = [
    # name,          parent,   cell,        rotation_axis, spin
    ("body",         None,     C_BODY,      None, None),
    ("arm_L",        "body",   C_ARM_L,     None, None),
    ("arm_R",        "body",   C_ARM_R,     None, None),
    ("guard_L",      "arm_L",  C_GUARD_L,   None, None),
    ("guard_R",      "arm_R",  C_GUARD_R,   None, None),
    ("rotor_L",      "arm_L",  C_ROTOR_L,   (0, 1, 0), +1),
    ("rotor_R",      "arm_R",  C_ROTOR_R,   (0, 1, 0), -1),
    ("sensor",       "body",   C_LENS,      None, None),
    ("nozzle_front", "body",   C_NOZZLE,    None, None),
    ("thruster",     "body",   C_THR,       None, None),
]


def build_all():
    COMPS.clear()
    PIVOTS.clear()
    POINTS.clear()
    build_body()
    build_sensor()
    build_nozzle()
    build_thruster()
    build_left_rotor_unit()
    mirror_part("arm_L", "arm_R", {C_ARM_L: C_ARM_R})
    mirror_part("guard_L", "guard_R", {C_GUARD_L: C_GUARD_R})
    mirror_part("rotor_L", "rotor_R", {C_ROTOR_L: C_ROTOR_R})
    for n in ("arm", "guard", "rotor"):
        p = PIVOTS[n + "_L"]
        PIVOTS[n + "_R"] = Vector((-p.x, p.y, p.z))


# ---------------------------------------------------------------------------
# 体積・重心(各殻の重なりは二重に数える近似)
# ---------------------------------------------------------------------------
def comp_volume_centroid(c):
    vol = 0.0
    cen = Vector((0, 0, 0))
    for idx, _ in c.faces:
        p0 = c.verts[idx[0]]
        for j in range(1, len(idx) - 1):
            p1, p2 = c.verts[idx[j]], c.verts[idx[j + 1]]
            v6 = p0.dot(p1.cross(p2))
            vol += v6
            cen += (p0 + p1 + p2) * v6
    if abs(vol) < 1e-15:
        return 0.0, Vector((0, 0, 0))
    return abs(vol) / 6.0, cen / (4.0 * vol)


# ---------------------------------------------------------------------------
# Blender へ
# ---------------------------------------------------------------------------
def to_blender(p):
    return (-p.x, -p.z, p.y)


def make_object(out_dir, palette_path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bm = bmesh.new()
    L_cell = bm.faces.layers.int.new("cell")
    L_part = bm.faces.layers.int.new("part")
    L_comp = bm.faces.layers.int.new("comp")
    part_index = {p[0]: i for i, p in enumerate(PARTS)}
    for ci, c in enumerate(COMPS):
        bv = [bm.verts.new(to_blender(p)) for p in c.verts]
        for idx, cell in c.faces:
            f = bm.faces.new([bv[i] for i in idx])
            f[L_cell] = cell
            f[L_part] = part_index[c.part]
            f[L_comp] = ci
    bm.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        u, v = cell_uv_center_obj(f[L_cell])
        for lp in f.loops:
            lp[uv].uv = (u, v)

    # 外向きの確認(殻ごとの符号付き体積 > 0)
    comp_vol = [0.0] * len(COMPS)
    for f in bm.faces:
        a, b, c = [lp.vert.co for lp in f.loops]
        comp_vol[f[L_comp]] += a.dot(b.cross(c)) / 6.0
    part_tris = {p[0]: 0 for p in PARTS}
    for f in bm.faces:
        part_tris[PARTS[f[L_part]][0]] += 1

    me = bpy.data.meshes.new(MODEL)
    bm.to_mesh(me)
    bm.free()
    me.shade_smooth()
    me.set_sharp_from_angle(angle=math.radians(35.0))

    mat = bpy.data.materials.new(MODEL + "_mat")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = bpy.data.images.load(palette_path)
    tex.interpolation = 'Closest'
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    me.materials.append(mat)

    obj = bpy.data.objects.new(MODEL, me)
    bpy.context.scene.collection.objects.link(obj)

    obj_path = os.path.join(out_dir, MODEL + ".obj")
    bpy.ops.wm.obj_export(
        filepath=obj_path,
        export_selected_objects=False,
        forward_axis='NEGATIVE_Z', up_axis='Y',
        global_scale=1.0, apply_modifiers=True,
        export_uv=True, export_normals=True, export_colors=False,
        export_materials=True, path_mode='STRIP',
        export_triangulated_mesh=True,
        export_object_groups=False, export_material_groups=False,
        export_vertex_groups=False, export_smooth_groups=False,
    )
    return obj_path, comp_vol, part_tris


# ---------------------------------------------------------------------------
# OBJ を読み直して検査
# ---------------------------------------------------------------------------
def inspect_obj(obj_path, mtl_path):
    V, VT, VN = [], [], []
    faces = []
    o_g = 0
    usemtl = set()
    bad = 0
    with open(obj_path, encoding="utf-8") as fh:
        for line in fh:
            s = line.split()
            if not s:
                continue
            t = s[0]
            if t == "v":
                V.append(tuple(float(x) for x in s[1:4]))
            elif t == "vt":
                VT.append(tuple(float(x) for x in s[1:3]))
            elif t == "vn":
                VN.append(tuple(float(x) for x in s[1:4]))
            elif t in ("o", "g"):
                o_g += 1
            elif t == "usemtl":
                usemtl.add(" ".join(s[1:]))
            elif t == "f":
                cs = s[1:]
                ok = len(cs) == 3
                fv = []
                for cc in cs:
                    p = cc.split("/")
                    if len(p) != 3 or not p[1] or not p[2]:
                        ok = False
                    fv.append(tuple(int(x) - 1 if x else -1 for x in (p + ["", ""])[:3]))
                if not ok:
                    bad += 1
                faces.append(fv)
    mats, map_kd = 0, []
    with open(mtl_path, encoding="utf-8") as fh:
        for line in fh:
            ls = line.strip()
            if ls.startswith("newmtl"):
                mats += 1
            if ls.startswith("map_Kd"):
                map_kd.append(ls)
    # エンジン座標へ (Assimp の X 反転と同じ)
    E = [Vector((-x, y, z)) for x, y, z in V]
    return dict(E=E, VT=VT, VN=VN, faces=faces, o_g=o_g, usemtl=usemtl,
                bad=bad, mats=mats, map_kd=map_kd)


def uv_cell(uv):
    """OBJ の vt (左下原点) -> マス番号。マスの境界上なら -1"""
    u, v = uv
    vt = 1.0 - v                      # 左上原点へ
    cu, cv = u * GRID, vt * GRID
    col, row = int(math.floor(cu)), int(math.floor(cv))
    if not (0 <= col < GRID and 0 <= row < GRID):
        return -1
    if abs(cu - round(cu)) < 1e-6 or abs(cv - round(cv)) < 1e-6:
        return -1
    return row * GRID + col


# ---------------------------------------------------------------------------
def fmt3(v):
    return "(%+.4f, %+.4f, %+.4f)" % tuple(round(float(x), 4) + 0.0 for x in v[:3])


def main():
    argv = sys.argv
    out_dir = None
    if "--" in argv:
        rest = argv[argv.index("--") + 1:]
        if "--out" in rest:
            out_dir = rest[rest.index("--out") + 1]
    if out_dir is None:
        try:
            out_dir = os.path.dirname(os.path.abspath(__file__))
            if not os.path.isdir(out_dir):
                raise ValueError
        except Exception:
            out_dir = bpy.path.abspath("//") or os.getcwd()
    os.makedirs(out_dir, exist_ok=True)

    # 1) 設計
    build_all()

    # 2) 正規化: 全長(機首の先〜推進器の後端, Z) = 1.0
    zs = [p.z for c in COMPS for p in c.verts]
    length_design = max(zs) - min(zs)
    scale = 1.0 / length_design

    # 3) 重心(体積)を原点へ
    tv, acc = 0.0, Vector((0, 0, 0))
    for c in COMPS:
        vol, cen = comp_volume_centroid(c)
        tv += vol
        acc += cen * vol
    com = acc / tv
    com.x = 0.0   # 設計は左右対称。四角形の分割の向きによる微小な左右差を消す

    def T(p):
        return (Vector(p) - com) * scale

    for c in COMPS:
        c.verts = [T(p) for p in c.verts]
    PIVOTS["body"] = com.copy()
    pivots = {k: T(v) for k, v in PIVOTS.items()}
    points = {k: (T(p), d.normalized()) for k, (p, d) in POINTS.items()}
    rotor_c = T(ROTOR_C)

    # 4) パレット・Blender・書き出し
    pal_path = os.path.join(out_dir, MODEL + "_palette.png")
    write_palette_png(pal_path)
    obj_path, comp_vol, part_tris = make_object(out_dir, pal_path)
    mtl_path = os.path.join(out_dir, MODEL + ".mtl")

    # 5) 検査
    R = inspect_obj(obj_path, mtl_path)
    E = R["E"]
    n_tri = len(R["faces"])
    mn = Vector((min(p.x for p in E), min(p.y for p in E), min(p.z for p in E)))
    mx = Vector((max(p.x for p in E), max(p.y for p in E), max(p.z for p in E)))
    far = max(p.length for p in E)

    # マスごとの三角形数・混在チェック
    cell_tris = {i: 0 for i in range(GRID * GRID)}
    mixed, on_border = 0, 0
    cell_vidx = {i: set() for i in range(GRID * GRID)}
    for fv in R["faces"]:
        cs = [uv_cell(R["VT"][t]) for (_, t, _) in fv]
        if -1 in cs:
            on_border += 1
            continue
        if len(set(cs)) != 1:
            mixed += 1
            continue
        cell_tris[cs[0]] += 1
        for (vi, _, _) in fv:
            cell_vidx[cs[0]].add(vi)

    # ローター(回す頂点)のチェック
    rotor_check = {}
    guard_in_scaled = GUARD_R_IN * scale
    for name, cell, center in (("rotor_L", C_ROTOR_L, rotor_c),
                               ("rotor_R", C_ROTOR_R, Vector((-rotor_c.x, rotor_c.y, rotor_c.z)))):
        vs = [E[i] for i in cell_vidx[cell]]
        mean_x = sum(p.x for p in vs) / len(vs)
        rmax = max(math.hypot(p.x - center.x, p.z - center.z) for p in vs)
        shared = 0
        for other in range(GRID * GRID):
            if other != cell:
                shared += len(cell_vidx[cell] & cell_vidx[other])
        rotor_check[name] = dict(verts=len(vs), mean_x=mean_x, rmax=rmax, shared=shared,
                                 tris=cell_tris[cell])

    # 寸法の内訳
    nose_z = mn.z
    tail_z = mx.z
    body_w = max(abs(E[i].x) for i in cell_vidx[C_BODY] | cell_vidx[C_GREY]) * 2.0

    # 6) JSON
    part_cells_used = {p[0]: set() for p in PARTS}
    for c in COMPS:
        for _, cell in c.faces:
            part_cells_used[c.part].add(cell)

    def r6(v):
        return [round(float(x), 6) + 0.0 for x in v]

    parts_json = []
    for name, parent, cell, axis, spin in PARTS:
        extra = sorted(part_cells_used[name] - {cell})
        d = {
            "name": name,
            "parent": parent,
            "pivot": r6(pivots[name]),
            "rotation_axis": list(axis) if axis else None,
            "palette_cell": cell,
            "uv_rect": cell_uv_rect_topleft(cell),
            "extra_cells": extra,
            "extra_uv_rects": [cell_uv_rect_topleft(e) for e in extra],
            "triangles": part_tris[name],
            "rotates": axis is not None,
        }
        if spin is not None:
            d["spin"] = {
                "seen_from_above": "clockwise" if spin > 0 else "counterclockwise",
                "sign_about_axis_XMMatrixRotationY": spin,
                "note": "羽根のピッチはこの向きで上向きの推力になる形",
            }
        parts_json.append(d)

    points_json = [{"name": k, "position": r6(p), "direction": r6(d)}
                   for k, (p, d) in sorted(points.items())]

    js = {
        "model": MODEL,
        "generator": "generate_drone.py (Blender %s)" % bpy.app.version_string,
        "coordinate_system": "engine (left-handed, +Y up, head = -Z, origin = center of mass)",
        "unit": "全長(機首の先〜推進器の後端)を 1.0 に正規化 (1 unit = 1 cm)",
        "obj": MODEL + ".obj",
        "palette": {
            "image": MODEL + "_palette.png",
            "size": [PALETTE_SIZE, PALETTE_SIZE],
            "grid": [GRID, GRID],
            "cell_px": CELL_PX,
            "cell_order": "左上から右へ。col = index % 4, row = index / 4",
            "uv_origin": "top-left (DirectX)",
            "cells": [{"index": i, "name": n, "srgb": list(rgb), "note": note,
                       "uv_rect": cell_uv_rect_topleft(i)}
                      for i, n, rgb, note in PALETTE],
            "rotating_cells": list(ROTATING_CELLS),
            "emissive_cells": [C_LENS, C_GLOW],
        },
        "parts": parts_json,
        "points": points_json,
    }
    with open(os.path.join(out_dir, MODEL + "_pivots.json"), "w", encoding="utf-8") as fh:
        json.dump(js, fh, ensure_ascii=False, indent=2)

    # 7) レポート
    L = []
    L.append("=== %s 生成レポート (Blender %s) ===" % (MODEL, bpy.app.version_string))
    L.append("")
    L.append("[OBJ / ローダー制約]")
    L.append("  三角形数           : %d" % n_tri)
    L.append("  頂点 v / vt / vn   : %d / %d / %d" % (len(E), len(R["VT"]), len(R["VN"])))
    L.append("  三角形以外・UV/法線欠けの面 : %d  (0であること)" % R["bad"])
    L.append("  o/g の数           : %d  (1であること)" % R["o_g"])
    L.append("  usemtl の種類      : %d  (1であること)" % len(R["usemtl"]))
    L.append("  MTLのマテリアル数  : %d  (1であること)" % R["mats"])
    L.append("  map_Kd             : %s" % R["map_kd"])
    L.append("  UV: 各面の3頂点ともマスの中心 (左上原点で ((col+0.5)/4, (row+0.5)/4))")
    L.append("  マスの境界上のUV / 1面で複数マスにまたがる面 : %d / %d  (0であること)" % (on_border, mixed))
    L.append("  法線: スムーズ + 35°以上の角をシャープ (Smooth by Angle)")
    L.append("")
    L.append("[寸法(エンジン座標・縮尺は全長1.0基準・1 unit = 1 cm)]")
    L.append("  全長(機首の先〜推進器の後端, Z): %.4f" % (tail_z - nose_z))
    L.append("  横幅 X (ガードを含む)          : %.4f   胴体のみ: %.4f" % (mx.x - mn.x, body_w))
    L.append("  高さ Y                         : %.4f" % (mx.y - mn.y))
    L.append("  奥行 Z                         : %.4f" % (mx.z - mn.z))
    L.append("  バウンディングボックス")
    L.append("    min = %s" % fmt3(mn))
    L.append("    max = %s" % fmt3(mx))
    L.append("  原点から最も遠い頂点までの距離 : %.4f" % far)
    L.append("  機首の向き確認: 機首の先 Z = %+.4f (負であること), 推進器の後端 Z = %+.4f"
             % (nose_z, tail_z))
    L.append("  左右の確認: rotor_L の頂点の X 平均 = %+.4f (正 = 機体の左 = +X であること)"
             % rotor_check["rotor_L"]["mean_x"])
    L.append("")
    L.append("[重心]")
    L.append("  原点 = 体積から求めた重心(各パーツの殻の重なりは二重に数える近似)")
    L.append("  設計単位からの縮尺 : %.5f" % scale)
    L.append("  殻の数 / 外向き(符号付き体積 > 0)でない殻 : %d / %d"
             % (len(comp_vol), sum(1 for v in comp_vol if v <= 0)))
    L.append("")
    L.append("[パーツ / ピボット(エンジン座標)]")
    for d in parts_json:
        L.append("  %-12s cell %2d  tris %4d  parent %-8s pivot %s%s"
                 % (d["name"], d["palette_cell"], d["triangles"], str(d["parent"]),
                    fmt3(d["pivot"]),
                    ("  axis (%+d, %+d, %+d)" % tuple(d["rotation_axis"])) if d["rotation_axis"] else ""))
    L.append("  追加で使うマス(extra_cells):")
    for d in parts_json:
        L.append("    %-12s %s" % (d["name"], d["extra_cells"] if d["extra_cells"] else "なし"))
    L.append("  三角形数の合計 : %d" % sum(d["triangles"] for d in parts_json))
    L.append("")
    L.append("[回すパーツの見分け(UVのマス)]")
    for name in ("rotor_L", "rotor_R"):
        rc = rotor_check[name]
        cell = C_ROTOR_L if name == "rotor_L" else C_ROTOR_R
        L.append("  %-8s cell %2d  uv_rect %s" % (name, cell, cell_uv_rect_topleft(cell)))
        L.append("    このマスの三角形 %d (パーツの三角形数と一致: %s) / 頂点 %d"
                 % (rc["tris"], "OK" if rc["tris"] == part_tris[name] else "NG", rc["verts"]))
        L.append("    ほかのマスの面と共有している頂点 : %d  (0であること)" % rc["shared"])
        L.append("    回転軸からの最大半径 %.4f < ガード内径 %.4f : %s"
                 % (rc["rmax"], guard_in_scaled, "OK" if rc["rmax"] < guard_in_scaled else "NG"))
        sp = [d for d in parts_json if d["name"] == name][0]["spin"]
        L.append("    回転の向き: 上から見て%s (XMMatrixRotationY の角度の符号 %+d)"
                 % ("時計回り" if sp["sign_about_axis_XMMatrixRotationY"] > 0 else "反時計回り",
                    sp["sign_about_axis_XMMatrixRotationY"]))
    L.append("")
    L.append("[点(エンジン座標)]")
    for p in points_json:
        L.append("  %-17s pos %s  dir %s" % (p["name"], fmt3(p["position"]), fmt3(p["direction"])))
    L.append("")
    L.append("[パレット (%dx%d, %dx%d マス, 1マス%dpx, uv_rect は左上原点)]"
             % (PALETTE_SIZE, PALETTE_SIZE, GRID, GRID, CELL_PX))
    for i, n, rgb, note in PALETTE:
        L.append("  cell %2d  %-14s rgb(%3d,%3d,%3d)  tris %4d  %s"
                 % (i, n, rgb[0], rgb[1], rgb[2], cell_tris[i], note))
    rep = "\n".join(L) + "\n"
    with open(os.path.join(out_dir, MODEL + "_report.txt"), "w", encoding="utf-8") as fh:
        fh.write(rep)
    print(rep)


if __name__ == "__main__":
    main()
