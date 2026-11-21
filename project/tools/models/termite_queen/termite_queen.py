# -*- coding: utf-8 -*-
"""
termite_queen.py  ―  ラスボス「シロアリの女王」生成スクリプト（Blender 4.2）

  モデル生成 → パレットPNG → OBJ/MTL → ピボットJSON → .blend → OBJ読み直し検証レポート

実行例（推奨：バックグラウンド実行）
  blender -b -P termite_queen.py -- --out D:/CG2_DirectXGame/project/tools/models/termite_queen
  ・--out を省略すると「このスクリプトと同じフォルダ/termite_queen_out」に出力
  ・GUI のテキストエディタから実行しても動く（シーン内の既存オブジェクトは削除される）

座標の扱い
  ・スクリプト内部の作業座標 = エンジン座標（左手系・+Y上・頭が -Z・原点=体積重心・全長1.0）
    三角形は「右手系の外積で外向き」= 左手系DirectXの画面上で時計回り（D3D既定の表面）
  ・OBJ はエンジンの読み込み（Assimp・X反転で右手系→左手系）に合わせ、右手系で書き出す:
        OBJ = (-x, y, z)、巻き順は反転して CCW
  ・.blend は「この OBJ を Blender 既定設定（前 -Z / 上 Y）でインポートした状態」と同じ配置:
        Blender = (OBJ.x, -OBJ.z, OBJ.y)   → Blender上では頭が +Y 側を向く
  ・左右: _L = 女王自身の左。エンジン座標では +X 側（OBJ では -X 側）
"""

import bpy
import math
import os
import sys
import json
import struct
import zlib
import random
from mathutils import Vector, Matrix

# =============================================================================
# 0. 設定
# =============================================================================
MODEL = "termite_queen"
PALETTE_PX = 256                     # パレット画像サイズ（4×4マス、1マス64px）
GRID = 4

# ---- 分割数 -----------------------------------------------------------------
ABD_RADIAL = 24                      # 腹の周方向分割（偶数。0番が背中の真上）
SEG_U_VISIBLE = [0.0, 0.12, 0.35, 0.65, 0.88]   # 見える節(1〜7)の節内リング位置
SEG_U_HIDDEN = [0.0, 0.33, 0.67]                # 壁に埋まる節(8,9)
SEG_U_LAST = [0.0, 0.33, 0.60, 0.80, 0.93]      # 最終節(10)＝後端ドーム
PLATE_K = 14                         # 背板の輪郭点数
HEAD_RADIAL, HEAD_RINGS = 16, 8
THORAX_RADIAL = 16
EYE_RADIAL, EYE_RINGS = 8, 4
LIMB_RADIAL = 6                      # 脚・触角の断面（6角形＋滑らか法線）

# ---- 腹 ---------------------------------------------------------------------
A_LEN = 0.75                         # 腹の長さ（全長のおよそ75%）
A_Z0 = -0.24                         # 腹の前端 Z（正規化前）
A_RX = 0.155                         # 最大半径（横）
A_KT, A_KB = 0.90, 0.78              # 縦/横比（背中側, 腹側）… 腹側は床に乗って少し平ら
FRONT_DOME = 0.03                    # 腹の前端の丸みの長さ（胸の中に隠れる）
# 節の境目（腹の前端=0, 後端=1）。前は細かく、中央が長く張る
SEG_B = [0.0, 0.06, 0.13, 0.21, 0.305, 0.415, 0.535, 0.655, 0.77, 0.88, 1.0]
# 境目1〜9 のくびれ深さ（半径比）。前ほど深い（皮があまり伸びていない）
SEG_DEPTH = [0.085, 0.075, 0.065, 0.055, 0.050, 0.045, 0.042, 0.040, 0.040]
GROOVE_W = 0.14                      # くびれの幅（隣接節長に対する比）
# 外形（くびれ無しの包絡線）: (t, 半径比)
ENV_PTS = [(0.00, 0.42), (0.06, 0.56), (0.14, 0.70), (0.26, 0.85), (0.40, 0.94),
           (0.55, 0.985), (0.70, 1.0), (0.80, 1.0)]
DOME_T = 0.80                        # これより後ろは楕円ドームで閉じる
VISIBLE_SEGS = 7                     # 節1〜7 が見える（後ろ1/3＝節8〜10 は壁の中）
# 4区画 = 節のまとまり [開始, 終了)。境目は必ず節のくびれ
REGION_SEGS = [(0, 3), (3, 5), (5, 7), (7, 10)]
TUCK_SCALE = 0.80                    # 区画の境目で隣の区画の内側へ折り込むリングの縮小率

# ---- 背板 -------------------------------------------------------------------
PLATE_H_CENTER, PLATE_H_MID, PLATE_H_EDGE = 0.0058, 0.0052, 0.0040  # 皮からの浮き（上面）
PLATE_H_BOTTOM = -0.0030             # 下面（皮に少し沈める）
PLATE_SEED = 20261116

# ---- 胸 ---------------------------------------------------------------------
TH_Z0, TH_LEN = -0.335, 0.11
# (s, 半幅, 半高さ)  前胸(盾状)・中胸・後胸の3つのふくらみ
TH_PROFILE = [(0.00, 0.022, 0.018), (0.08, 0.036, 0.026), (0.18, 0.054, 0.035),
              (0.28, 0.060, 0.039), (0.37, 0.054, 0.037), (0.43, 0.045, 0.032),
              (0.52, 0.052, 0.035), (0.61, 0.054, 0.036), (0.69, 0.046, 0.032),
              (0.79, 0.055, 0.036), (0.89, 0.050, 0.033), (1.00, 0.030, 0.022)]
TH_RINGS_S = [0.08, 0.18, 0.28, 0.37, 0.43, 0.52, 0.61, 0.69, 0.79, 0.89, 0.96]

# ---- 頭 ---------------------------------------------------------------------
H_A, H_B, H_C = 0.050, 0.038, 0.056  # 半幅, 半高さ, 半長
H_E1, H_E2 = 0.85, 0.90              # 超楕円の指数（1未満で少し角ばる＝昆虫の頭殻らしさ）
H_PITCH = math.radians(14.0)         # 前下がり
H_CENTER = Vector((0.0, 0.034, -0.372))

# ---- 色 (sRGB 0-255) ---------------------------------------------------------
COL_SKIN = (232, 223, 194)           # 腹の皮: 黄みのある乳白色
COL_DARK = (112, 46, 26)             # 背板・頭・胸: 濃い赤茶
COL_LEG = (184, 136, 92)             # 脚・触角: 薄い茶
COL_MAND = (64, 28, 16)              # 大顎: 頭より暗い茶
COL_EYE = (14, 11, 11)               # 目: 黒
COL_SPARE = (128, 128, 128)          # 予備マス

# マス番号: 左上=0、右へ+1、行ごとに+4
# 値: (色リスト, 用途) 色が2つのマスは左半分/右半分
CELLS = {
    0: ([COL_DARK], "thorax"),
    1: ([COL_DARK], "head"),
    2: ([COL_MAND], "mandible_L(左半分) / mandible_R(右半分)"),
    3: ([COL_EYE], "eye_L(左半分) / eye_R(右半分)"),
    4: ([COL_LEG], "antenna_L(左半分) / antenna_R(右半分)"),
    5: ([COL_LEG], "leg_L1"),
    6: ([COL_LEG], "leg_L2"),
    7: ([COL_LEG], "leg_L3"),
    8: ([COL_LEG], "leg_R1"),
    9: ([COL_LEG], "leg_R2"),
    10: ([COL_LEG], "leg_R3"),
    11: ([COL_SPARE], "予備（未使用）"),
    12: ([COL_SKIN, COL_DARK], "abdomen_1 (左半分=皮, 右半分=背板)"),
    13: ([COL_SKIN, COL_DARK], "abdomen_2 (左半分=皮, 右半分=背板)"),
    14: ([COL_SKIN, COL_DARK], "abdomen_3 (左半分=皮, 右半分=背板)"),
    15: ([COL_SKIN, COL_DARK], "abdomen_4 (左半分=皮, 右半分=背板)"),
}

# パーツ定義: 名前 -> (親, マス, 矩形の種類 'full' | 'left' | 'right')
PART_DEFS = [
    ("thorax", None, 0, "full"),
    ("head", "thorax", 1, "full"),
    ("eye_L", "head", 3, "left"), ("eye_R", "head", 3, "right"),
    ("antenna_L", "head", 4, "left"), ("antenna_R", "head", 4, "right"),
    ("mandible_L", "head", 2, "left"), ("mandible_R", "head", 2, "right"),
    ("leg_L1", "thorax", 5, "full"), ("leg_L2", "thorax", 6, "full"), ("leg_L3", "thorax", 7, "full"),
    ("leg_R1", "thorax", 8, "full"), ("leg_R2", "thorax", 9, "full"), ("leg_R3", "thorax", 10, "full"),
    ("abdomen_1", "thorax", 12, "full"), ("abdomen_2", "abdomen_1", 13, "full"),
    ("abdomen_3", "abdomen_2", 14, "full"), ("abdomen_4", "abdomen_3", 15, "full"),
]
ANTENNA_PARTS = ("antenna_L", "antenna_R")
TRI_BUDGET = 6000

# 法線の自動スムーズ角（これ以上折れている辺は硬いエッジ）
SMOOTH_ALL = -1.0
SMOOTH_PLATE = math.cos(math.radians(45.0))
SMOOTH_MANDIBLE = math.cos(math.radians(50.0))


# =============================================================================
# 1. 小さな数学ヘルパ
# =============================================================================
def spow(v, e):
    return math.copysign(abs(v) ** e, v)


def lerp(a, b, t):
    return a + (b - a) * t


def pchip(xs, ys):
    """単調3次エルミート補間（オーバーシュートしない）を返す"""
    n = len(xs)
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [0.0] * n
    for i in range(1, n - 1):
        if d[i - 1] * d[i] <= 0:
            m[i] = 0.0
        else:
            w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    m[0] = d[0] if n > 1 else 0.0
    m[-1] = 0.0 if abs(d[-1]) < 1e-9 else d[-1]

    def f(x):
        if x <= xs[0]:
            return ys[0]
        if x >= xs[-1]:
            return ys[-1]
        i = 0
        while xs[i + 1] < x:
            i += 1
        t = (x - xs[i]) / h[i]
        t2, t3 = t * t, t * t * t
        return ((2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h[i] * m[i]
                + (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * h[i] * m[i + 1])
    return f


def ortho_basis(n):
    n = n.normalized()
    ref = Vector((0, 1, 0)) if abs(n.y) < 0.9 else Vector((1, 0, 0))
    t1 = ref.cross(n).normalized()
    t2 = n.cross(t1).normalized()
    return t1, t2


def transport_frames(pts):
    """中心線に沿った平行移動フレーム (T, N, B)"""
    n = len(pts)
    T = []
    for i in range(n):
        a = pts[max(i - 1, 0)]
        b = pts[min(i + 1, n - 1)]
        T.append((b - a).normalized())
    up = Vector((0, 1, 0))
    N0 = up - T[0] * up.dot(T[0])
    if N0.length < 1e-6:
        N0 = Vector((1, 0, 0)) - T[0] * T[0].x
    frames = [(T[0], N0.normalized(), T[0].cross(N0).normalized())]
    for i in range(1, n):
        q = T[i - 1].rotation_difference(T[i])
        Ni = q @ frames[-1][1]
        Ni = (Ni - T[i] * Ni.dot(T[i])).normalized()
        frames.append((T[i], Ni, T[i].cross(Ni).normalized()))
    return frames


# =============================================================================
# 2. パーツ（作業座標＝エンジン座標）
# =============================================================================
def cell_rect(cell, kind):
    """DirectX(左上原点)の UV 矩形 [u0, v0, u1, v1]"""
    cs = 1.0 / GRID
    c, r = cell % GRID, cell // GRID
    u0, v0 = c * cs, r * cs
    if kind == "left":
        return [u0, v0, u0 + cs * 0.5, v0 + cs]
    if kind == "right":
        return [u0 + cs * 0.5, v0, u0 + cs, v0 + cs]
    return [u0, v0, u0 + cs, v0 + cs]


def rect_center(rect):
    return ((rect[0] + rect[2]) * 0.5, (rect[1] + rect[3]) * 0.5)


class Part:
    def __init__(self, name, parent, cell, kind):
        self.name, self.parent, self.cell, self.kind = name, parent, cell, kind
        self.rect = cell_rect(cell, kind)
        self.verts, self.uvs, self.fixed_n = [], [], []
        self.tris, self.tri_cos = [], []
        self.shells = []          # (tri_start, tri_end)
        self.pivot = Vector((0, 0, 0))
        self._shell_start = 0

    def uv_main(self):
        """パーツの主色の UV（2色マスなら左半分の中心）"""
        if len(CELLS[self.cell][0]) == 2:
            u0, v0, u1, v1 = self.rect
            return (u0 + (u1 - u0) * 0.25, (v0 + v1) * 0.5)
        return rect_center(self.rect)

    def uv_sub(self):
        """2色マスの第2色（右半分の中心）"""
        u0, v0, u1, v1 = self.rect
        return (u0 + (u1 - u0) * 0.75, (v0 + v1) * 0.5)

    def add_v(self, p, uv, n=None):
        self.verts.append(Vector(p))
        self.uvs.append(uv)
        self.fixed_n.append(None if n is None else Vector(n).normalized())
        return len(self.verts) - 1

    def begin_shell(self):
        self._shell_start = len(self.tris)

    def tri(self, a, b, c, cos_thr):
        self.tris.append((a, b, c))
        self.tri_cos.append(cos_thr)

    def end_shell(self):
        """閉じた殻ごとに符号付き体積で向きを揃える（外向き）"""
        s, e = self._shell_start, len(self.tris)
        cen = Vector((0, 0, 0))
        ids = {i for t in self.tris[s:e] for i in t}
        for i in ids:
            cen += self.verts[i]
        cen /= max(len(ids), 1)
        vol = 0.0
        for a, b, c in self.tris[s:e]:
            pa, pb, pc = self.verts[a] - cen, self.verts[b] - cen, self.verts[c] - cen
            vol += pa.dot(pb.cross(pc))
        if vol < 0:
            for i in range(s, e):
                a, b, c = self.tris[i]
                self.tris[i] = (a, c, b)
        self.shells.append((s, e))


def loft(part, rings, uv, cos_thr, ring_normals=None,
         start=None, end=None, start_n=None, end_n=None):
    """リング列をつなぐ。start/end: 'cap'（重心で閉じる）か Vector（極点）か None"""
    part.begin_shell()
    idx = []
    for ri, ring in enumerate(rings):
        row = []
        for j, p in enumerate(ring):
            n = ring_normals[ri][j] if ring_normals and ring_normals[ri] else None
            row.append(part.add_v(p, uv, n))
        idx.append(row)
    N = len(rings[0])
    for r in range(len(rings) - 1):
        for j in range(N):
            a, b = idx[r][j], idx[r][(j + 1) % N]
            c, d = idx[r + 1][(j + 1) % N], idx[r + 1][j]
            part.tri(a, b, c, cos_thr)
            part.tri(a, c, d, cos_thr)

    def fan(ring_i, tip, tip_n, reverse):
        if isinstance(tip, str):
            cen = Vector((0, 0, 0))
            for p in rings[ring_i]:
                cen += p
            tip = cen / N
        ti = part.add_v(tip, uv, tip_n)
        row = idx[ring_i]
        for j in range(N):
            if reverse:
                part.tri(ti, row[j], row[(j + 1) % N], cos_thr)
            else:
                part.tri(ti, row[(j + 1) % N], row[j], cos_thr)
    if start is not None:
        fan(0, start, start_n, False)
    if end is not None:
        fan(len(rings) - 1, end, end_n, True)
    part.end_shell()


def ellipsoid(part, center, axis, r_axis, r_tan, uv, radial, rings, cos_thr, squash=1.0):
    t1, t2 = ortho_basis(axis)
    axis = axis.normalized()
    rr = []
    for i in range(1, rings + 1):
        a = math.pi * i / (rings + 1)
        ring = []
        for j in range(radial):
            b = 2 * math.pi * j / radial
            ring.append(center + axis * (r_axis * math.cos(a))
                        + (t1 * math.cos(b) + t2 * (math.sin(b) * squash)) * (r_tan * math.sin(a)))
        rr.append(ring)
    loft(part, rr, uv, cos_thr, start=center + axis * r_axis, end=center - axis * r_axis)


def mirror_part(src, name, parent, cell, kind):
    """X 反転コピー（巻き順も反転）"""
    p = Part(name, parent, cell, kind)
    uv = p.uv_main()
    for v, n in zip(src.verts, src.fixed_n):
        p.verts.append(Vector((-v.x, v.y, v.z)))
        p.uvs.append(uv)
        p.fixed_n.append(None if n is None else Vector((-n.x, n.y, n.z)))
    p.tris = [(a, c, b) for a, b, c in src.tris]
    p.tri_cos = list(src.tri_cos)
    p.shells = list(src.shells)
    p.pivot = Vector((-src.pivot.x, src.pivot.y, src.pivot.z))
    return p


# =============================================================================
# 3. 腹（10節・4区画・背板）
# =============================================================================
_env_mid = pchip([p[0] for p in ENV_PTS], [p[1] for p in ENV_PTS])
_SEG_L = [SEG_B[i + 1] - SEG_B[i] for i in range(10)]


def abd_env(t):
    if t < 0.0:
        s = -t / (FRONT_DOME / A_LEN)
        return ENV_PTS[0][1] * math.sqrt(max(0.0, 1.0 - s * s))
    if t > DOME_T:
        s = (t - DOME_T) / (1.0 - DOME_T)
        return _env_mid(DOME_T) * math.sqrt(max(0.0, 1.0 - s * s))
    return _env_mid(t)


def abd_groove(t):
    g = 0.0
    for i in range(1, 10):
        w = GROOVE_W * 0.5 * (_SEG_L[i - 1] + _SEG_L[i])
        g += SEG_DEPTH[i - 1] * math.exp(-((t - SEG_B[i]) / w) ** 2)
    return g


def abd_rx(t):
    return A_RX * abd_env(t) * (1.0 - abd_groove(t))


def abd_center(t):
    return Vector((0.0, 0.012 * (1.0 - t) ** 2, A_Z0 + A_LEN * t))


def abd_S(t, phi):
    r = abd_rx(t)
    s = math.sin(phi)
    ky = A_KB + (A_KT - A_KB) * (1.0 + s) * 0.5
    c = abd_center(t)
    return Vector((r * math.cos(phi), c.y + r * ky * s, c.z))


def abd_N(t, phi):
    h = 1e-4
    dp = abd_S(t, phi + h) - abd_S(t, phi - h)
    t0, t1 = max(t - h, -FRONT_DOME / A_LEN + 1e-6), min(t + h, 1.0 - 1e-6)
    dt = abd_S(t1, phi) - abd_S(t0, phi)
    n = dp.cross(dt)
    if n.length < 1e-12:
        return Vector((0, 0, 1 if t > 0.5 else -1))
    n.normalize()
    if n.dot(abd_S(t, phi) - abd_center(t)) < 0:
        n = -n
    return n


def abd_phis():
    return [math.pi * 0.5 + 2.0 * math.pi * j / ABD_RADIAL for j in range(ABD_RADIAL)]


def seg_ts(i):
    us = SEG_U_LAST if i == 9 else (SEG_U_VISIBLE if i < VISIBLE_SEGS else SEG_U_HIDDEN)
    return [SEG_B[i] + u * _SEG_L[i] for u in us]


def skin_ring(t):
    ph = abd_phis()
    return [abd_S(t, p) for p in ph], [abd_N(t, p) for p in ph]


def tuck_ring(t_valley, t_at):
    """境目のリングを縮めて、隣の区画の内側（t_at の位置）へ折り込む"""
    cv, ca = abd_center(t_valley), abd_center(t_at)
    ring = []
    for p in abd_phis():
        q = abd_S(t_valley, p) - cv
        ring.append(Vector((q.x * TUCK_SCALE, ca.y + q.y * TUCK_SCALE, ca.z)))
    return ring


def build_abdomen_skin(part, k):
    s0, s1 = REGION_SEGS[k]
    uv = part.uv_main()
    rings, norms = [], []
    start = end = None
    start_n = end_n = None
    if k == 0:
        tcf = FRONT_DOME / A_LEN
        for th in (0.55, 1.0, 1.35):
            r, n = skin_ring(-tcf * math.cos(th))
            rings.append(r)
            norms.append(n)
        start = abd_center(-tcf)
        start_n = Vector((0, 0.15, -1))
    else:
        t_prev = seg_ts(s0 - 1)[-1]              # 前の区画の最後のリング位置へ折り込む
        rings.append(tuck_ring(SEG_B[s0], t_prev))
        norms.append(None)
        start = "cap"
    ts = []
    for i in range(s0, s1):
        ts += seg_ts(i)
    if s1 < 10:
        ts.append(SEG_B[s1])                     # 終わりの境目（くびれの底）
    for t in ts:
        r, n = skin_ring(t)
        rings.append(r)
        norms.append(n)
    if s1 < 10:
        t_next = seg_ts(s1)[1]                   # 次の区画の最初の内側リング位置へ折り込む
        rings.append(tuck_ring(SEG_B[s1], t_next))
        norms.append(None)
        end = "cap"
    else:
        end = abd_center(1.0)
        end_n = Vector((0, 0, 1))
    loft(part, rings, uv, SMOOTH_ALL, norms, start=start, end=end, start_n=start_n, end_n=end_n)


def build_plate(part, seg_i, rng):
    """節 seg_i の背中に、皮の曲面に沿った角の丸い薄い板を1枚"""
    uv = part.uv_sub()
    Lz = _SEG_L[seg_i] * A_LEN
    tc = SEG_B[seg_i] + _SEG_L[seg_i] * (0.5 + rng.uniform(-0.04, 0.04))
    r = abd_rx(tc)
    flen = lerp(0.70, 0.46, min(seg_i / 4.0, 1.0))
    Lh = 0.5 * Lz * flen * rng.uniform(0.95, 1.05)         # 体軸方向の半長
    W = (0.045 + 0.15 * r) * rng.uniform(0.95, 1.05)       # 周方向の半幅（弧長）
    v_off = rng.uniform(-0.05, 0.05) * W                   # 取り残された板らしい僅かなズレ
    yaw = rng.uniform(-0.06, 0.06)
    wob = [(rng.uniform(0.02, 0.05), rng.uniform(0, 6.283)), (rng.uniform(0.01, 0.03), rng.uniform(0, 6.283))]
    n_exp = 3.2

    def shape(th):
        cu, cv = spow(math.cos(th), 2.0 / n_exp), spow(math.sin(th), 2.0 / n_exp)
        if cu < 0:                                         # 前縁を少しへこませる（そら豆形）
            cu += 0.22 * (1.0 - cv * cv) * (-cu)
        cv *= 1.0 + 0.08 * cu                              # 前縁が少し狭い台形
        k = 1.0 + wob[0][0] * math.sin(2 * th + wob[0][1]) + wob[1][0] * math.sin(3 * th + wob[1][1])
        u, v = cu * Lh * k, cv * W * k                     # 輪郭をわずかに不揃いに
        return (u * math.cos(yaw) - v * math.sin(yaw), u * math.sin(yaw) + v * math.cos(yaw))

    # 輪郭点を「弧長＋曲がり角」で等配分（丸い角に点を集め、長い辺も曲面に沿わせる）
    dense = [shape(2 * math.pi * i / 720) for i in range(720)]
    acc = [0.0]
    perim = sum(math.dist(dense[i], dense[(i + 1) % 720]) for i in range(720))
    for i in range(1, 721):
        a, b, c = dense[i - 2], dense[i - 1], dense[i % 720]
        turn = abs(math.atan2(c[1] - b[1], c[0] - b[0]) - math.atan2(b[1] - a[1], b[0] - a[0]))
        turn = min(turn, 2 * math.pi - turn)
        acc.append(acc[-1] + math.dist(b, c) / perim + turn / (2 * math.pi))
    base_pts, j = [], 0
    for k in range(PLATE_K):
        target = acc[-1] * k / PLATE_K
        while acc[j + 1] < target:
            j += 1
        f = (target - acc[j]) / max(acc[j + 1] - acc[j], 1e-12)
        a, b = dense[j % 720], dense[(j + 1) % 720]
        base_pts.append((lerp(a[0], b[0], f), lerp(a[1], b[1], f)))

    def outline(scale):
        return [(u * scale, v * scale + v_off) for u, v in base_pts]

    def surf(u, v, h):
        t = tc + u / A_LEN
        phi = math.pi * 0.5 + v / abd_rx(t)
        return abd_S(t, phi) + abd_N(t, phi) * h

    part.begin_shell()
    c_top = part.add_v(surf(0, v_off, PLATE_H_CENTER), uv)
    c_bot = part.add_v(surf(0, v_off, PLATE_H_BOTTOM), uv)
    mid = [part.add_v(surf(u, v, PLATE_H_MID), uv) for u, v in outline(0.55)]
    out = outline(1.0)
    top = [part.add_v(surf(u, v, PLATE_H_EDGE), uv) for u, v in out]
    bot = [part.add_v(surf(u, v, PLATE_H_BOTTOM), uv) for u, v in out]
    K = PLATE_K
    for k in range(K):
        k2 = (k + 1) % K
        part.tri(c_top, mid[k], mid[k2], SMOOTH_PLATE)
        part.tri(mid[k], top[k], top[k2], SMOOTH_PLATE)
        part.tri(mid[k], top[k2], mid[k2], SMOOTH_PLATE)
        part.tri(top[k], bot[k], bot[k2], SMOOTH_PLATE)
        part.tri(top[k], bot[k2], top[k2], SMOOTH_PLATE)
        part.tri(c_bot, bot[k2], bot[k], SMOOTH_PLATE)
    part.end_shell()


# =============================================================================
# 4. 胸・頭・目・触角・大顎・脚
# =============================================================================
_th_w = pchip([p[0] for p in TH_PROFILE], [p[1] for p in TH_PROFILE])
_th_h = pchip([p[0] for p in TH_PROFILE], [p[2] for p in TH_PROFILE])


def th_center(s):
    return Vector((0.0, 0.016 + 0.014 * (1.0 - s), TH_Z0 + TH_LEN * s))


def th_S(s, phi):
    w, h, c = _th_w(s), _th_h(s), th_center(s)
    e = 2.0 / 2.6
    sn = math.sin(phi)
    ky = 0.88 + 0.12 * (1.0 + sn) * 0.5
    return Vector((w * spow(math.cos(phi), e), c.y + h * ky * spow(sn, e), c.z))


def th_N(s, phi):
    hh = 1e-4
    n = (th_S(s, phi + hh) - th_S(s, phi - hh)).cross(th_S(min(s + hh, 1), phi) - th_S(max(s - hh, 0), phi))
    n.normalize()
    if n.dot(th_S(s, phi) - th_center(s)) < 0:
        n = -n
    return n


def build_thorax(part):
    uv = part.uv_main()
    rings = []
    for s in TH_RINGS_S:
        rings.append([th_S(s, math.pi * 0.5 + 2 * math.pi * j / THORAX_RADIAL) for j in range(THORAX_RADIAL)])
    loft(part, rings, uv, SMOOTH_ALL, start=th_center(0.0), end=th_center(1.0))


_H_ROT = Matrix.Rotation(-H_PITCH, 3, 'X')


def head_local(a, phi):
    sa, ca = math.sin(a), math.cos(a)
    x = H_A * spow(sa, H_E1) * spow(math.cos(phi), H_E2)
    y = H_B * spow(sa, H_E1) * spow(math.sin(phi), H_E2) * (0.96 + 0.04 * math.sin(phi))
    z = -H_C * spow(ca, H_E1)
    egg = 1.0 + 0.10 * (z / H_C)          # 後頭部をわずかに広く
    return Vector((x * egg, y * egg, z))


def head_S(a, phi):
    return H_CENTER + _H_ROT @ head_local(a, phi)


def head_N(a, phi):
    h = 1e-4
    n = (head_S(a, phi + h) - head_S(a, phi - h)).cross(head_S(a + h, phi) - head_S(a - h, phi))
    n.normalize()
    if n.dot(head_S(a, phi) - H_CENTER) < 0:
        n = -n
    return n


HEAD_FWD = (_H_ROT @ Vector((0, 0, -1))).normalized()


def build_head(part):
    uv = part.uv_main()
    rings = []
    for i in range(1, HEAD_RINGS + 1):
        a = math.pi * i / (HEAD_RINGS + 1)
        rings.append([head_S(a, math.pi * 0.5 + 2 * math.pi * j / HEAD_RADIAL) for j in range(HEAD_RADIAL)])
    loft(part, rings, uv, SMOOTH_ALL, start=head_S(0.0, 0.0), end=head_S(math.pi, 0.0))
    part.pivot = H_CENTER - HEAD_FWD * (0.75 * H_C)       # 頭と胸の境目（首）


def build_eye_L(part):
    a, phi = 1.0, 0.45
    p, n = head_S(a, phi), head_N(a, phi)
    c = p - n * 0.0020
    ellipsoid(part, c, n, 0.0072, 0.0105, part.uv_main(), EYE_RADIAL, EYE_RINGS, SMOOTH_ALL, squash=0.9)
    part.pivot = c


def bezier(p0, p1, p2, p3, n=200):
    pts = []
    for i in range(n + 1):
        t = i / n
        mt = 1 - t
        pts.append(p0 * mt ** 3 + p1 * (3 * mt * mt * t) + p2 * (3 * mt * t * t) + p3 * t ** 3)
    acc = [0.0]
    for i in range(1, len(pts)):
        acc.append(acc[-1] + (pts[i] - pts[i - 1]).length)
    return pts, acc


def sample_curve(pts, acc, s):
    s = min(max(s, 0.0), acc[-1])
    i = 0
    while i < len(acc) - 2 and acc[i + 1] < s:
        i += 1
    t = (s - acc[i]) / max(acc[i + 1] - acc[i], 1e-12)
    return pts[i].lerp(pts[i + 1], t)


def tube(part, centers, radii, uv, cos_thr, start_pole, end_pole):
    frames = transport_frames(centers)
    rings = []
    for c, r, (T, N, B) in zip(centers, radii, frames):
        rings.append([c + (N * math.cos(2 * math.pi * j / LIMB_RADIAL)
                           + B * math.sin(2 * math.pi * j / LIMB_RADIAL)) * r for j in range(LIMB_RADIAL)])
    loft(part, rings, uv, cos_thr, start=start_pole, end=end_pole)


def build_antenna_L(part):
    """数珠状の触角（10節）。付け根から前方・外側・斜め上へ"""
    a, phi = 0.70, 0.30
    base, nrm = head_S(a, phi), head_N(a, phi)
    F, O, U = Vector((0, 0, -1)), Vector((1, 0, 0)), Vector((0, 1, 0))
    L = 0.17
    p0 = base - nrm * 0.005
    d0 = (F * 0.55 + O * 0.55 + U * 0.62).normalized()
    p1 = p0 + d0 * (L * 0.36)
    d1 = (F * 0.80 + O * 0.45 + U * 0.30).normalized()
    p2 = p1 + d1 * (L * 0.34)
    d2 = (F * 0.80 + O * 0.55 - U * 0.12).normalized()
    p3 = p2 + d2 * (L * 0.34)
    pts, acc = bezier(p0, p1, p2, p3)
    total = acc[-1]
    units = [1.8] + [1.0] * 9
    seg_len = [total * u / sum(units) for u in units]
    centers, radii = [], []
    s = 0.0
    for i, sl in enumerate(seg_len):
        if i == 0:
            prof = [(0.0, 0.0052), (0.35, 0.0060), (0.72, 0.0058)]   # 柄節（長い）
        else:
            rb = 0.0058 + 0.0010 * (i / 9.0)
            prof = [(0.0, 0.0034), (0.35, rb), (0.72, rb * 0.97)]   # 数珠玉
        for u, r in prof:
            centers.append(sample_curve(pts, acc, s + u * sl))
            radii.append(r)
        s += sl
    tip = sample_curve(pts, acc, total)
    start = sample_curve(pts, acc, 0.0) - (centers[1] - centers[0]).normalized() * 0.002
    tube(part, centers, radii, part.uv_main(), SMOOTH_ALL, start, tip)
    part.pivot = base


def build_mandible_L(part):
    """内側へ湾曲した鎌形の大顎。内縁は硬いエッジ、歯を2つ"""
    a, phi = 0.36, -0.42
    base_s, nrm = head_S(a, phi), head_N(a, phi)
    B0 = base_s - nrm * 0.004
    f = HEAD_FWD
    o = Vector((1, 0, 0))
    u = o.cross(f).normalized()
    Lm = 0.072
    kin = 0.16 * math.sin(0.8 * math.pi) + (B0.x - 0.005) / Lm

    def C(s):
        return B0 + f * (Lm * s) + o * (Lm * (0.16 * math.sin(0.8 * math.pi * s) - kin * s * s)) + u * (-0.06 * Lm * s)

    ss = [0.0, 0.12, 0.25, 0.38, 0.50, 0.58, 0.68, 0.78, 0.88]
    rings = []
    for s in ss:
        T = (C(min(s + 1e-3, 1)) - C(max(s - 1e-3, 0))).normalized()
        n_in = u.cross(T).normalized()             # 内側（体の中心線側）
        up = T.cross(n_in).normalized()
        w = 0.017 * (1 - s) ** 0.7 + 0.0008
        th = 0.0085 * (1 - s) ** 0.55 + 0.0006
        tooth = 1.0 + 0.45 * math.exp(-((s - 0.58) / 0.045) ** 2) + 0.35 * math.exp(-((s - 0.78) / 0.04) ** 2)
        sec = [(0.55 * w * tooth, 0.0), (0.15 * w, 0.5 * th), (-0.30 * w, 0.45 * th),
               (-0.45 * w, 0.0), (-0.30 * w, -0.45 * th), (0.15 * w, -0.5 * th)]
        c = C(s)
        rings.append([c + n_in * x + up * y for x, y in sec])
    loft(part, rings, part.uv_main(), SMOOTH_MANDIBLE, start="cap", end=C(1.0))
    part.pivot = B0


# 脚: (胸の s 位置, 付け根→腿の付け根, 腿, すね) の各ベクトル（左側 +X）
LEG_DEFS = [
    (0.26, Vector((0.012, -0.016, -0.008)), Vector((0.040, 0.010, -0.030)), Vector((0.020, -0.045, -0.026))),
    (0.60, Vector((0.014, -0.016, 0.000)), Vector((0.048, 0.012, -0.004)), Vector((0.022, -0.048, 0.006))),
    (0.82, Vector((0.013, -0.016, 0.006)), Vector((0.045, 0.010, 0.030)), Vector((0.018, -0.046, 0.030))),
]


def build_leg_L(part, idx):
    s, v01, v12, v23 = LEG_DEFS[idx]
    phi = math.radians(-40.0)
    R = th_S(s, phi)
    nr = th_N(s, phi)
    J1 = R + v01
    J2 = J1 + v12
    T = J2 + v23
    d01, d12, d23 = v01.normalized(), v12.normalized(), v23.normalized()

    def bend_gap(da, db, r, la, lb):
        # 関節の内側でリングが重ならない距離（曲げ角に応じて広げる）
        th = da.angle(db)
        return min(r * math.tan(th * 0.5) * 1.15 + 0.002, 0.4 * min(la, lb))
    e1 = bend_gap(d01, d12, 0.0093, v01.length, v12.length)
    e2 = bend_gap(d12, d23, 0.0078, v12.length, v23.length)
    centers = [R, J1 - d01 * e1, J1 + d12 * e1, J1 + v12 * 0.5,
               J2 - d12 * e2, J2, J2 + d23 * e2, J2 + v23 * 0.45, J2 + v23 * 0.80]
    radii = [0.0095, 0.0093, 0.0086, 0.0092, 0.0078, 0.0075, 0.0068, 0.0056, 0.0044]
    tube(part, centers, radii, part.uv_main(), SMOOTH_ALL, R - nr * 0.008 - d01 * 0.002, T)
    part.pivot = R


# =============================================================================
# 5. 組み立て
# =============================================================================
def build_all():
    defs = {d[0]: d for d in PART_DEFS}
    P = {}

    def new(name):
        n, parent, cell, kind = defs[name]
        P[name] = Part(n, parent, cell, kind)
        return P[name]

    build_thorax(new("thorax"))
    build_head(new("head"))
    build_eye_L(new("eye_L"))
    build_antenna_L(new("antenna_L"))
    build_mandible_L(new("mandible_L"))
    for i in range(3):
        build_leg_L(new("leg_L%d" % (i + 1)), i)
    for src in ("eye", "antenna", "mandible"):
        n, parent, cell, kind = defs[src + "_R"]
        P[n] = mirror_part(P[src + "_L"], n, parent, cell, kind)
    for i in range(3):
        n, parent, cell, kind = defs["leg_R%d" % (i + 1)]
        P[n] = mirror_part(P["leg_L%d" % (i + 1)], n, parent, cell, kind)

    rng = random.Random(PLATE_SEED)
    for k in range(4):
        part = new("abdomen_%d" % (k + 1))
        build_abdomen_skin(part, k)
        s0, s1 = REGION_SEGS[k]
        for si in range(s0, s1):
            build_plate(part, si, rng)
        part.pivot = abd_center(SEG_B[s0])                 # 区画の前端（境目）の中心

    # 胸のピボット = 胸の体積重心
    vol, cen = shell_mass(P["thorax"])
    P["thorax"].pivot = cen
    meta = {"seg_boundary_z": [abd_center(t).z for t in SEG_B]}
    return [P[d[0]] for d in PART_DEFS], meta


def shell_mass(part):
    vol = 0.0
    m = Vector((0, 0, 0))
    for a, b, c in part.tris:
        pa, pb, pc = part.verts[a], part.verts[b], part.verts[c]
        v = pa.dot(pb.cross(pc)) / 6.0
        vol += v
        m += (pa + pb + pc) * (v / 4.0)
    return vol, (m / vol if abs(vol) > 1e-15 else Vector((0, 0, 0)))


def normalize_and_center(parts, meta):
    zs_min, zs_max = 1e9, -1e9
    for p in parts:
        if p.name in ANTENNA_PARTS:
            continue
        for v in p.verts:
            zs_min, zs_max = min(zs_min, v.z), max(zs_max, v.z)
    s = 1.0 / (zs_max - zs_min)
    for p in parts:
        p.verts = [v * s for v in p.verts]
        p.pivot = p.pivot * s
    meta["seg_boundary_z"] = [z * s for z in meta["seg_boundary_z"]]
    tv, tm = 0.0, Vector((0, 0, 0))
    for p in parts:
        v, c = shell_mass(p)
        tv += v
        tm += c * v
    com = tm / tv
    for p in parts:
        p.verts = [v - com for v in p.verts]
        p.pivot = p.pivot - com
    meta["seg_boundary_z"] = [z - com.z for z in meta["seg_boundary_z"]]
    meta["scale"], meta["com_before"], meta["volume"] = s, com, tv


def corner_normals(part):
    """角度重み付きの自動スムーズ。fixed_n を持つ頂点は解析法線をそのまま使う"""
    fn, ang = [], []
    adj = [[] for _ in part.verts]
    for fi, (a, b, c) in enumerate(part.tris):
        pa, pb, pc = part.verts[a], part.verts[b], part.verts[c]
        n = (pb - pa).cross(pc - pa)
        fn.append(n.normalized() if n.length > 1e-14 else Vector((0, 0, 1)))

        def angle(p, q, r):
            u, w = q - p, r - p
            if u.length < 1e-14 or w.length < 1e-14:
                return 0.0
            return u.angle(w)
        ang.append((angle(pa, pb, pc), angle(pb, pc, pa), angle(pc, pa, pb)))
        for k, vi in enumerate((a, b, c)):
            adj[vi].append((fi, k))
    out = []
    for fi, tri in enumerate(part.tris):
        cs = part.tri_cos[fi]
        ns = []
        for vi in tri:
            if part.fixed_n[vi] is not None:
                ns.append(part.fixed_n[vi])
                continue
            acc = Vector((0, 0, 0))
            for fj, k in adj[vi]:
                if fn[fi].dot(fn[fj]) >= cs:
                    acc += fn[fj] * ang[fj][k]
            ns.append(acc.normalized() if acc.length > 1e-14 else fn[fi])
        out.append(ns)
    return out


# =============================================================================
# 6. 書き出し
# =============================================================================
def write_png(path, w, h, pixel):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw.extend(pixel(x, y))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def read_png_rgb(path):
    with open(path, "rb") as f:
        data = f.read()
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        pos += 12 + ln
    raw = zlib.decompress(idat)
    stride = w * 3 + 1
    return w, h, lambda x, y: tuple(raw[y * stride + 1 + x * 3: y * stride + 1 + x * 3 + 3])


def palette_pixel(x, y):
    cs = PALETTE_PX // GRID
    cell = (y // cs) * GRID + (x // cs)
    cols = CELLS[cell][0]
    if len(cols) == 2:
        return cols[0] if (x % cs) < cs // 2 else cols[1]
    return cols[0]


def export_obj(parts, out_dir):
    """右手系 OBJ（エンジン側で X 反転して読む前提）"""
    obj_path = os.path.join(out_dir, MODEL + ".obj")
    lines_v, lines_vt, lines_vn, lines_f = [], [], [], []
    vt_map, vn_map = {}, {}
    v_base = 0
    for p in parts:
        cn = corner_normals(p)
        for v in p.verts:
            lines_v.append("v %.6f %.6f %.6f" % (-v.x, v.y, v.z))
        for fi, (a, b, c) in enumerate(p.tris):
            corners = []
            for vi, n in zip((a, b, c), cn[fi]):
                u, vdx = p.uvs[vi]
                kt = (round(u, 6), round(1.0 - vdx, 6))
                if kt not in vt_map:
                    vt_map[kt] = len(vt_map) + 1
                    lines_vt.append("vt %.6f %.6f" % kt)
                kn = (round(-n.x, 5), round(n.y, 5), round(n.z, 5))
                if kn not in vn_map:
                    vn_map[kn] = len(vn_map) + 1
                    lines_vn.append("vn %.5f %.5f %.5f" % kn)
                corners.append("%d/%d/%d" % (v_base + vi + 1, vt_map[kt], vn_map[kn]))
            # X 反転で鏡像になるので巻き順を反転（OBJ 内で CCW＝外向き）
            lines_f.append("f %s %s %s" % (corners[0], corners[2], corners[1]))
        v_base += len(p.verts)
    with open(obj_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("# %s - generated by termite_queen.py (Blender %s)\n" % (MODEL, bpy.app.version_string))
        f.write("# right-handed, +Y up, head = -Z. Engine loads with X flip -> left-handed.\n")
        f.write("mtllib %s.mtl\n" % MODEL)
        f.write("o %s\n" % MODEL)
        f.write("\n".join(lines_v) + "\n")
        f.write("\n".join(lines_vt) + "\n")
        f.write("\n".join(lines_vn) + "\n")
        f.write("usemtl %s_mat\n" % MODEL)
        f.write("\n".join(lines_f) + "\n")
    with open(os.path.join(out_dir, MODEL + ".mtl"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# %s\n" % MODEL)
        f.write("newmtl %s_mat\n" % MODEL)
        f.write("Ka 0.000000 0.000000 0.000000\nKd 1.000000 1.000000 1.000000\n")
        f.write("Ks 0.000000 0.000000 0.000000\nNs 10.000000\nd 1.000000\nillum 1\n")
        f.write("map_Kd %s_palette.png\n" % MODEL)
    return obj_path


def r6(v):
    return [round(float(x), 6) for x in v]


def export_json(parts, out_dir):
    cells = []
    for c in range(GRID * GRID):
        cols, usage = CELLS[c]
        e = {"cell": c, "row": c // GRID, "col": c % GRID, "uv_rect": r6(cell_rect(c, "full")),
             "usage": usage, "rgb": list(cols[0])}
        if len(cols) == 2:
            e["rgb"] = list(cols[0])
            e["rgb_right_half"] = list(cols[1])
        cells.append(e)
    data = {
        "model": MODEL,
        "coordinate_system": "engine (left-handed, +Y up, head = -Z, origin = center of mass)",
        "note": "uv_rect は左上原点(DirectX)のUV。V反転して読み込む前提。頂点のUVがこの範囲に入っていれば、そのパーツの頂点と判別できる。",
        "palette": {
            "file": MODEL + "_palette.png", "size": [PALETTE_PX, PALETTE_PX], "grid": [GRID, GRID],
            "cell_numbering": "左上=0、右へ+1、下の行へ+4",
            "two_color_cells": "abdomen_1〜4 のマスは 左半分=皮 / 右半分=背板。パーツ判別はマス全体の uv_rect で行う",
            "shared_cells": "左右対(大顎・目・触角)は同色マスを左半分=_L / 右半分=_R に分けて判別",
            "cells": cells,
        },
        "parts": [{"name": p.name, "parent": p.parent, "pivot": r6(p.pivot),
                   "palette_cell": p.cell, "uv_rect": r6(p.rect)} for p in parts],
    }
    # 読みやすさのため、パーツとマスは1行1要素で書く
    def dj(v):
        return json.dumps(v, ensure_ascii=False)
    pal = data["palette"]
    out = ["{"]
    for k in ("model", "coordinate_system", "note"):
        out.append('  %s: %s,' % (dj(k), dj(data[k])))
    out.append('  "palette": {')
    for k in ("file", "size", "grid", "cell_numbering", "two_color_cells", "shared_cells"):
        out.append('    %s: %s,' % (dj(k), dj(pal[k])))
    out.append('    "cells": [')
    out.append(",\n".join("      " + dj(c) for c in pal["cells"]))
    out.append('    ]')
    out.append('  },')
    out.append('  "parts": [')
    out.append(",\n".join("    " + dj(pt) for pt in data["parts"]))
    out.append('  ]')
    out.append('}')
    path = os.path.join(out_dir, MODEL + "_pivots.json")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    json.load(open(path, encoding="utf-8"))          # 書式チェック
    return path


def build_blend(parts, out_dir, png_path):
    # シーンを空にする
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images):
        for d in list(coll):
            if d.users == 0 or d.name.startswith(MODEL):
                coll.remove(d)

    def to_b(v):            # エンジン → OBJ(-x, y, z) → Blender(x, -z, y)
        return (-v.x, -v.z, v.y)

    verts, faces, uvs, norms, groups = [], [], [], [], []
    base = 0
    for p in parts:
        cn = corner_normals(p)
        verts += [to_b(v) for v in p.verts]
        groups.append((p.name, range(base, base + len(p.verts))))
        for fi, (a, b, c) in enumerate(p.tris):
            faces.append((base + a, base + c, base + b))
            for vi, n in ((a, cn[fi][0]), (c, cn[fi][2]), (b, cn[fi][1])):
                u, vdx = p.uvs[vi]
                uvs.append((u, 1.0 - vdx))
                norms.append(to_b(n))
        base += len(p.verts)
    me = bpy.data.meshes.new(MODEL)
    me.from_pydata(verts, [], faces)
    me.update()
    uvl = me.uv_layers.new(name="UVMap")
    for i, uv in enumerate(uvs):
        uvl.data[i].uv = uv
    for poly in me.polygons:
        poly.use_smooth = True
    me.normals_split_custom_set(norms)
    ob = bpy.data.objects.new(MODEL, me)
    bpy.context.scene.collection.objects.link(ob)
    for name, rng in groups:
        vg = ob.vertex_groups.new(name=name)
        vg.add(list(rng), 1.0, 'REPLACE')

    img = bpy.data.images.load(png_path, check_existing=False)
    img.name = MODEL + "_palette"
    mat = bpy.data.materials.new(MODEL + "_mat")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = 'Closest'
    tex.location = (-350, 250)
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.55
    me.materials.append(mat)

    # ピボット確認用の Empty（親子関係つき）
    empties = {}
    for p in parts:
        e = bpy.data.objects.new("pivot_" + p.name, None)
        e.empty_display_type = 'PLAIN_AXES'
        e.empty_display_size = 0.03
        bpy.context.scene.collection.objects.link(e)
        empties[p.name] = (e, Vector(to_b(p.pivot)))
    for p in parts:
        e, wpos = empties[p.name]
        if p.parent:
            pe, ppos = empties[p.parent]
            e.parent = pe
            e.location = wpos - ppos
        else:
            e.location = wpos
    path = os.path.join(out_dir, MODEL + ".blend")
    bpy.ops.wm.save_as_mainfile(filepath=path, check_existing=False, relative_remap=True)
    return path


# =============================================================================
# 7. 検証（書き出した OBJ / MTL / JSON / PNG を読み直して実測）
# =============================================================================
def verify(out_dir, meta):
    obj_path = os.path.join(out_dir, MODEL + ".obj")
    mtl_path = os.path.join(out_dir, MODEL + ".mtl")
    js = json.load(open(os.path.join(out_dir, MODEL + "_pivots.json"), encoding="utf-8"))
    V, VT, VN, F = [], [], [], []
    n_o = n_g = 0
    usemtl, mtllib = set(), []
    bad_poly = miss_uv = miss_n = 0
    for line in open(obj_path, encoding="utf-8"):
        sp = line.split()
        if not sp or sp[0].startswith("#"):
            continue
        k = sp[0]
        if k == "v":
            V.append(Vector(tuple(map(float, sp[1:4]))))
        elif k == "vt":
            VT.append(tuple(map(float, sp[1:3])))
        elif k == "vn":
            VN.append(Vector(tuple(map(float, sp[1:4]))))
        elif k == "o":
            n_o += 1
        elif k == "g":
            n_g += 1
        elif k == "usemtl":
            usemtl.add(sp[1])
        elif k == "mtllib":
            mtllib.append(sp[1])
        elif k == "f":
            cs = sp[1:]
            if len(cs) != 3:
                bad_poly += 1
            face = []
            for c in cs:
                q = c.split("/")
                vi = int(q[0]) - 1
                ti = int(q[1]) - 1 if len(q) > 1 and q[1] else None
                ni = int(q[2]) - 1 if len(q) > 2 and q[2] else None
                miss_uv += ti is None
                miss_n += ni is None
                face.append((vi, ti, ni))
            F.append(face)
    mtl_names, map_kd = [], []
    for line in open(mtl_path, encoding="utf-8"):
        sp = line.split()
        if sp and sp[0] == "newmtl":
            mtl_names.append(sp[1])
        if sp and sp[0] == "map_Kd":
            map_kd.append(" ".join(sp[1:]))

    E = [Vector((-v.x, v.y, v.z)) for v in V]          # エンジン座標へ（X 反転）
    piv = {p["name"]: Vector(p["pivot"]) for p in js["parts"]}
    EN = [Vector((-n.x, n.y, n.z)) for n in VN]
    parts = js["parts"]

    def uv_dx(ti):
        u, v = VT[ti]
        return u, 1.0 - v

    def part_of(ti):
        u, v = uv_dx(ti)
        hit = [p["name"] for p in parts if p["uv_rect"][0] < u < p["uv_rect"][2] and p["uv_rect"][1] < v < p["uv_rect"][3]]
        return hit[0] if len(hit) == 1 else None

    # UV がマスの境界から十分内側か（境界から 1/4 マス幅以上）
    uv_edge_bad = 0
    cs = 1.0 / GRID
    for u, v in (uv_dx(i) for i in range(len(VT))):
        fu, fv = (u / cs) % 1.0, (v / cs) % 1.0
        if min(fu, 1 - fu, fv, 1 - fv) < 0.2:
            uv_edge_bad += 1

    tri_part, straddle = [], 0
    for face in F:
        names = {part_of(c[1]) if c[1] is not None else None for c in face}
        if len(names) != 1 or None in names:
            straddle += 1
            tri_part.append(None)
        else:
            tri_part.append(names.pop())

    # 面の向き: 頂点法線と面法線（OBJ の右手系・CCW）
    flipped = 0
    part_vol = {p["name"]: 0.0 for p in parts}
    part_m = {p["name"]: Vector((0, 0, 0)) for p in parts}
    for face, pn in zip(F, tri_part):
        a, b, c = (V[x[0]] for x in face)
        fn = (b - a).cross(c - a)
        for x in face:
            if x[2] is not None and VN[x[2]].dot(fn) < 0:
                flipped += 1
        if pn:
            vol = a.dot(b.cross(c)) / 6.0
            part_vol[pn] += vol
            part_m[pn] += (a + b + c) * (vol / 4.0)
    tv = sum(part_vol.values())
    com_obj = sum(part_m.values(), Vector((0, 0, 0))) / tv
    com_eng = Vector((-com_obj.x, com_obj.y, com_obj.z))

    # パーツ別の頂点集合
    pverts = {p["name"]: set() for p in parts}
    ptris = {p["name"]: 0 for p in parts}
    skin_v = set()
    for face, pn in zip(F, tri_part):
        if not pn:
            continue
        ptris[pn] += 1
        for x in face:
            pverts[pn].add(x[0])
            if pn.startswith("abdomen"):
                u, _ = uv_dx(x[1])
                r = next(p["uv_rect"] for p in parts if p["name"] == pn)
                if u < (r[0] + r[2]) * 0.5:
                    skin_v.add(x[0])

    def bbox(ids):
        xs = [E[i].x for i in ids]
        ys = [E[i].y for i in ids]
        zs = [E[i].z for i in ids]
        return (min(xs), min(ys), min(zs)), (max(xs), max(ys), max(zs))
    all_ids = range(len(E))
    no_ant = [i for n, s in pverts.items() if n not in ANTENNA_PARTS for i in s]
    bb_all, bb_na = bbox(all_ids), bbox(no_ant)
    length_z = bb_na[1][2] - bb_na[0][2]
    width_x = bb_na[1][0] - bb_na[0][0]
    height_y = bb_na[1][1] - bb_na[0][1]
    far = max(v.length for v in E)
    zmin_owner = min(no_ant, key=lambda i: E[i].z)
    zmin_part = next(n for n, s in pverts.items() if zmin_owner in s)
    mand_tip_z = min(E[i].z for n in ("mandible_L", "mandible_R") for i in pverts[n])
    abd_end_z = max(E[i].z for i in pverts["abdomen_4"])
    eyeL_x = sum(E[i].x for i in pverts["eye_L"]) / len(pverts["eye_L"])

    sb_min, sb_max = bbox(skin_v)
    abd_dx, abd_dy = sb_max[0] - sb_min[0], sb_max[1] - sb_min[1]

    # 腹の表皮リング（Z ごとの最大半幅）からくびれ（溝）を検出
    zr = {}
    for i in skin_v:
        kz = round(E[i].z, 5)
        zr[kz] = max(zr.get(kz, 0.0), abs(E[i].x))
    zl = sorted(zr)
    grooves = []
    gval = []
    for i in range(1, len(zl) - 1):
        z0, z1, z2 = zl[i - 1], zl[i], zl[i + 1]
        expect = lerp(zr[z0], zr[z2], (z1 - z0) / (z2 - z0))
        gval.append((z1, (expect - zr[z1]) / max(zr[z1], 1e-9)))
    z_front = js["parts"][[p["name"] for p in js["parts"]].index("abdomen_1")]["pivot"][2]
    for i in range(1, len(gval) - 1):
        if gval[i][0] <= z_front + 1e-4:      # 腹の前端（胸の中のドームとのつなぎ目）は節ではない
            continue
        if gval[i][1] > 0.003 and gval[i][1] > gval[i - 1][1] and gval[i][1] > gval[i + 1][1]:
            grooves.append(gval[i])

    region_ok = []
    for k in range(2, 5):
        z = piv["abdomen_%d" % k].z
        near = min(grooves, key=lambda g: abs(g[0] - z)) if grooves else (1e9, 0)
        region_ok.append((k, z, near[0], abs(near[0] - z) < 1e-4))

    # PNG の色サンプル
    pw, ph, pix = read_png_rgb(os.path.join(out_dir, MODEL + "_palette.png"))

    # ---- 判定 ----
    ng = []
    if bad_poly: ng.append("三角形以外の面がある")
    if miss_uv: ng.append("UV の無い頂点参照がある")
    if miss_n: ng.append("法線の無い頂点参照がある")
    if n_o + n_g != 1: ng.append("o/g の数が 1 ではない")
    if len(usemtl) != 1: ng.append("usemtl が 1 種類ではない")
    if len(mtl_names) != 1: ng.append("MTL のマテリアル数が 1 ではない")
    if map_kd != [MODEL + "_palette.png"]: ng.append("map_Kd が不正")
    if abs(length_z - 1.0) > 1e-4: ng.append("全長Z が 1.0 ではない")
    if com_eng.length > 1e-3: ng.append("原点が重心からずれている")
    if not (mand_tip_z < 0 < abd_end_z): ng.append("頭の向きが逆")
    if zmin_part not in ("mandible_L", "mandible_R"): ng.append("全長の前端が大顎ではない")
    if straddle: ng.append("複数パーツの UV 矩形にまたがる面がある")
    if uv_edge_bad: ng.append("マスの境界に近い UV がある")
    if flipped: ng.append("面の向きと頂点法線が逆の箇所がある")
    if any(v <= 0 for v in part_vol.values()): ng.append("体積が負のパーツ（裏返り）がある")
    if not all(r[3] for r in region_ok): ng.append("腹の区画の境目が節のくびれと一致しない")
    if len(grooves) != 9: ng.append("腹のくびれが 9 本検出されない")
    if eyeL_x <= 0: ng.append("_L がエンジン座標の +X 側にない")
    warn = []
    if len(F) > TRI_BUDGET:
        warn.append("三角形数が目安 %d を超過" % TRI_BUDGET)

    L = []
    L.append(("NG: " + " / ".join(ng)) if ng else "OK: 規格違反なし")
    for w in warn:
        L.append("WARN: " + w)
    L.append("")
    L.append("=== termite_queen 検証レポート（書き出した OBJ/MTL/JSON/PNG を読み直して実測） ===")
    L.append("Blender %s / 出力先 %s" % (bpy.app.version_string, out_dir))
    L.append("")
    L.append("[OBJ / MTL]")
    L.append("  三角形数            : %d  (目安 %d 以下)" % (len(F), TRI_BUDGET))
    L.append("  v / vt / vn         : %d / %d / %d" % (len(V), len(VT), len(VN)))
    L.append("  三角形以外の面      : %d" % bad_poly)
    L.append("  UV 欠け / 法線欠け  : %d / %d" % (miss_uv, miss_n))
    L.append("  o の数 / g の数     : %d / %d  (合計 %d)" % (n_o, n_g, n_o + n_g))
    L.append("  usemtl の種類       : %d  %s" % (len(usemtl), sorted(usemtl)))
    L.append("  mtllib              : %s" % mtllib)
    L.append("  MTL のマテリアル数  : %d  %s" % (len(mtl_names), mtl_names))
    L.append("  map_Kd              : %s" % map_kd)
    L.append("  UV矩形をまたぐ面    : %d" % straddle)
    L.append("  マス境界に近いUV    : %d" % uv_edge_bad)
    L.append("  法線と面の向きが逆  : %d" % flipped)
    L.append("")
    L.append("[寸法]（エンジン座標）")
    L.append("  全長Z (大顎の先〜腹の後端) : %.6f   前端のパーツ=%s" % (length_z, zmin_part))
    L.append("  横幅X / 高さY (触角除く)    : %.4f / %.4f" % (width_x, height_y))
    L.append("  BBox 触角を含む  : min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f)" % (*bb_all[0], *bb_all[1]))
    L.append("  BBox 触角を含まない: min(%.4f, %.4f, %.4f) max(%.4f, %.4f, %.4f)" % (*bb_na[0], *bb_na[1]))
    L.append("  原点から最も遠い頂点までの距離 : %.4f" % far)
    L.append("  体積重心（OBJから再計算）      : (%.6f, %.6f, %.6f)  ※パーツ重なり部分は二重に数える" % tuple(com_eng))
    L.append("  正規化スケール %.5f / 総体積 %.6f（正規化後）" % (meta["scale"], tv))
    L.append("")
    L.append("[向き]")
    L.append("  大顎の先 Z = %.4f (負であること) / 腹の後端 Z = %.4f (正であること)" % (mand_tip_z, abd_end_z))
    L.append("  eye_L の X = %.4f （_L = 女王自身の左 = エンジン +X / OBJ では -X）" % eyeL_x)
    L.append("")
    L.append("[腹]")
    L.append("  最大径 X = %.4f (全長比 %.1f%%) / Y = %.4f (全長比 %.1f%%)" % (abd_dx, abd_dx * 100, abd_dy, abd_dy * 100))
    abd_len = abd_end_z - piv["abdomen_1"].z
    L.append("  腹の長さ（前端=abdomen_1ピボット〜後端） = %.4f (全長比 %.1f%%)  ※胸の中に隠れる前端の丸み %.4f は除く"
             % (abd_len, abd_len * 100, piv["abdomen_1"].z - sb_min[2]))
    L.append("  節の境目（設計値, Z）: " + ", ".join("%.4f" % z for z in meta["seg_boundary_z"]))
    L.append("  OBJ から検出したくびれ %d 本（Z, 深さ%%）: " % len(grooves)
             + ", ".join("%.4f(%.1f)" % (z, g * 100) for z, g in grooves))
    L.append("  壁面の目安: abdomen_4 の前端 Z = %.4f より後ろが壁の中" % piv["abdomen_4"].z)
    for k in range(1, 5):
        ids = pverts["abdomen_%d" % k]
        zs = [E[i].z for i in ids]
        L.append("  abdomen_%d: 頂点Z範囲 [%.4f, %.4f]（重ね代込み）  前端ピボットZ %.4f" % (k, min(zs), max(zs), piv["abdomen_%d" % k].z))
    for k, z, gz, ok in region_ok:
        L.append("  境目 abdomen_%d 前端 Z=%.4f ↔ 最寄りのくびれ Z=%.4f : %s" % (k, z, gz, "一致" if ok else "不一致"))
    L.append("")
    L.append("[パーツ]  名前 / マス / 三角形 / 親 / ピボット(エンジン座標) / 体積 / 色サンプル")
    for p in parts:
        r = p["uv_rect"]
        cx, cy = int((r[0] + r[2]) * 0.5 * pw), int((r[1] + r[3]) * 0.5 * ph)
        if p["name"].startswith("abdomen"):
            c1 = pix(int((r[0] * 0.75 + r[2] * 0.25) * pw), cy)
            c2 = pix(int((r[0] * 0.25 + r[2] * 0.75) * pw), cy)
            col = "皮%s 背板%s" % (c1, c2)
        else:
            col = str(pix(cx, cy))
        L.append("  %-11s cell %2d  tris %5d  parent %-10s pivot (% .4f, % .4f, % .4f)  vol %.6f  %s"
                 % (p["name"], p["palette_cell"], ptris[p["name"]], str(p["parent"]),
                    p["pivot"][0], p["pivot"][1], p["pivot"][2], part_vol[p["name"]], col))
    L.append("  合計 tris %d" % sum(ptris.values()))
    L.append("")
    L.append("[パレット] %dx%d, 4x4 マス（左上=0, 右へ+1, 下の行へ+4）" % (pw, ph))
    for c in range(GRID * GRID):
        cols, usage = CELLS[c]
        L.append("  cell %2d : %-28s %s" % (c, " + ".join(str(x) for x in cols), usage))
    L.append("  2色の扱い: abdomen_1〜4 は1マスの左半分=皮、右半分=背板。パーツ判別はマス全体の矩形で行う。")
    L.append("  同色の左右対（大顎・目・触角）は1マスを左右半分に分けて _L / _R を判別。")
    L.append("  ※2色マスがあるため、パレットはミップマップ無し（または Point サンプリング）推奨。")
    L.append("")
    L.append("[接合部]")
    L.append("  腹の区画境目: くびれの底のリングを両区画で共有し、そこから %.0f%% に縮めたリングで隣の区画の内側へ折り込んで閉じる。" % (TUCK_SCALE * 100))
    L.append("  頭↔胸 / 胸↔腹 / 脚・触角・大顎の付け根: 根元を相手の内側まで差し込んで重ねている。")
    L.append("  全パーツは閉じた殻（体積を計算できる）。")
    rep = os.path.join(out_dir, MODEL + "_report.txt")
    with open(rep, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(L) + "\n")
    return rep, ng, warn


# =============================================================================
# 8. main
# =============================================================================
def get_out_dir():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    if "--out" in argv:
        return os.path.abspath(argv[argv.index("--out") + 1])
    here = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else ""
    if not here or not os.path.isdir(here):
        here = os.path.dirname(bpy.data.filepath) if bpy.data.filepath else os.path.expanduser("~")
    return os.path.join(here, MODEL + "_out")


def main():
    out_dir = get_out_dir()
    os.makedirs(out_dir, exist_ok=True)
    parts, meta = build_all()
    normalize_and_center(parts, meta)
    png = os.path.join(out_dir, MODEL + "_palette.png")
    write_png(png, PALETTE_PX, PALETTE_PX, palette_pixel)
    export_obj(parts, out_dir)
    export_json(parts, out_dir)
    build_blend(parts, out_dir, png)
    rep, ng, warn = verify(out_dir, meta)
    print("[termite_queen] 出力:", out_dir)
    print("[termite_queen]", "NG: " + " / ".join(ng) if ng else "OK", *warn)
    print("[termite_queen] レポート:", rep)


if __name__ == "__main__":
    main()
