# -*- coding: utf-8 -*-
"""
シロアリ(職蟻)モデル生成スクリプト

実行方法(プロジェクトの project/ フォルダを基準にしたパス例):
    blender --background --python tools/models/termite_worker/generate_termite_worker.py

出力:
    resources/termite_worker/termite_worker.obj / .mtl
    resources/termite_worker/termite_worker_palette.png
    resources/termite_worker/termite_worker_pivots.json
    tools/models/termite_worker/termite_worker.blend
    tools/models/termite_worker/termite_worker_report.txt

座標系:
    Blender上では 頭 = +Y、背中 = +Z で作る。
    OBJ書き出し(forward=-Z, up=Y)で 頭 = -Z、背中 = +Y になる。
    エンジンは読み込み時にX反転するので、Blender (x, y, z) -> エンジン (-x, z, -y)。

オプション(Blenderの引数 "--" の後ろに書く):
    --project-root <path>   project/ フォルダの場所(省略時はこのファイルの位置から算出)
"""

import bpy
import bmesh
import json
import math
import struct
import sys
import zlib
from pathlib import Path
from mathutils import Vector, Matrix

# =====================================================================
# 調整用の定数
# =====================================================================

MODEL_NAME = "termite_worker"
BODY_LENGTH = 1.0            # 顎の先〜腹の端の長さ(触角は含めない)

# パレット(4x4マス、1マス64px)
PALETTE_SIZE = 256
PALETTE_GRID = 4
UV_SPREAD = 0.30             # マス中央から、マス幅の何割までUVを広げるか(にじみ防止)
MANDIBLE_UV_MARGIN = 10      # 顎のグラデーション用UVの左右余白(px)

# 分割数 (segments=経度方向, rings=緯度方向)
ABDOMEN_SEG, ABDOMEN_RINGS = 32, 16
HEAD_SEG, HEAD_RINGS = 28, 14
THORAX_SEG, THORAX_RINGS = 16, 10
EYE_SEG, EYE_RINGS = 10, 6
BEAD_SEG, BEAD_RINGS = 8, 5
JOINT_SEG, JOINT_RINGS = 6, 4
LEG_SIDES = 8
MANDIBLE_SIDES, MANDIBLE_RINGS = 8, 6

ANTENNA_BEADS = 7
ANTENNA_START = 0.020      # 触角の付け根から最初の玉までの距離(設計単位)
ANTENNA_SPACING = 0.038    # 玉の間隔(設計単位)


def hex_color(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


# 色(明るめ。HPが減ったときの赤の乗算が見えやすいように)
COL_HEAD = hex_color("#F3DFA0")      # 頭:淡い黄色
COL_BODY = hex_color("#F4EEDD")      # 胸・腹:乳白色
COL_LEG = hex_color("#FAF5E8")       # 脚:体より少し明るいクリーム
COL_ANTENNA = hex_color("#F1DDA4")   # 触角:頭と同系色
COL_MANDIBLE_BASE = hex_color("#F6EBCF")
COL_MANDIBLE_TIP = hex_color("#D9A25A")  # 顎の先端:明るめのアンバー
COL_EYE = hex_color("#74554A")       # 目:濃すぎない茶色
COL_SPARE = (255, 255, 255)

# パーツごとのパレットのマス番号(左上が0、右へ進み、次の行へ。DirectXのUV向き基準)
CELL = {
    "head": 0, "thorax": 1, "abdomen": 2,          # 3 = 予備(白)
    "leg_L1": 4, "leg_R1": 5, "leg_L2": 6, "leg_R2": 7, "leg_L3": 8, "leg_R3": 9,
    "antenna_L": 10, "antenna_R": 11,
    "mandible_L": 12, "mandible_R": 13,
    "eye_L": 14, "eye_R": 15,
}

# =====================================================================
# 形状生成のヘルパー
# =====================================================================


def add_shell(bm, verts, faces):
    """頂点リストと面(インデックスのタプル)を bmesh に追加する。"""
    bv = [bm.verts.new(v) for v in verts]
    for f in faces:
        bm.faces.new([bv[i] for i in f])


def ellipsoid(bm, center, radii, seg, rings, rot=None, deform=None):
    """
    楕円体を追加する。極はローカルY軸方向(前後方向)。
    deform(p) は単位球上の点 p を受け取り、変形した点を返す関数。
    """
    rot = rot or Matrix.Identity(3)
    center = Vector(center)
    verts, faces = [], []

    def place(p):
        if deform:
            p = deform(p)
        p = Vector((p.x * radii[0], p.y * radii[1], p.z * radii[2]))
        return center + rot @ p

    verts.append(place(Vector((0, 1, 0))))            # 前の極
    for i in range(1, rings):
        phi = math.pi * i / rings
        for j in range(seg):
            th = 2 * math.pi * j / seg
            verts.append(place(Vector((math.sin(phi) * math.cos(th),
                                       math.cos(phi),
                                       math.sin(phi) * math.sin(th)))))
    verts.append(place(Vector((0, -1, 0))))           # 後ろの極
    last = len(verts) - 1

    def ring_idx(i, j):
        return 1 + (i - 1) * seg + (j % seg)

    for j in range(seg):
        faces.append((0, ring_idx(1, j + 1), ring_idx(1, j)))
    for i in range(1, rings - 1):
        for j in range(seg):
            faces.append((ring_idx(i, j), ring_idx(i, j + 1),
                          ring_idx(i + 1, j + 1), ring_idx(i + 1, j)))
    for j in range(seg):
        faces.append((last, ring_idx(rings - 1, j), ring_idx(rings - 1, j + 1)))
    add_shell(bm, verts, faces)


def sphere(bm, center, r, seg, rings):
    ellipsoid(bm, center, (r, r, r), seg, rings)


def tube(bm, centers, radii, sides, tip_point=None):
    """
    中心点の列に沿った管を追加する。両端はふさぐ。
    tip_point を指定すると、末端は面ではなくその1点に集める(とがった先端)。
    向きのねじれを防ぐため、断面の向きは平行移動法で引き継ぐ。
    """
    n = len(centers)
    tangents = []
    for i in range(n):
        if i == 0:
            t = centers[1] - centers[0]
        elif i == n - 1:
            t = centers[-1] - centers[-2]
        else:
            t = (centers[i] - centers[i - 1]).normalized() + (centers[i + 1] - centers[i]).normalized()
        tangents.append(t.normalized())

    ref = Vector((0, 0, 1)) if abs(tangents[0].z) < 0.9 else Vector((1, 0, 0))
    nrm = tangents[0].cross(ref).normalized()
    verts, faces = [], []
    for i in range(n):
        if i > 0:  # 平行移動法:前の法線を新しい接線に直交するよう補正
            nrm = (nrm - tangents[i] * nrm.dot(tangents[i])).normalized()
        bin_ = tangents[i].cross(nrm).normalized()
        for k in range(sides):
            a = 2 * math.pi * k / sides
            verts.append(centers[i] + (nrm * math.cos(a) + bin_ * math.sin(a)) * radii[i])

    for i in range(n - 1):
        for k in range(sides):
            a, b = i * sides + k, i * sides + (k + 1) % sides
            faces.append((a, b, b + sides, a + sides))

    verts.append(centers[0].copy())
    start_c = len(verts) - 1
    for k in range(sides):
        faces.append((start_c, (k + 1) % sides, k))

    end_c_pos = tip_point if tip_point is not None else centers[-1].copy()
    verts.append(end_c_pos)
    end_c = len(verts) - 1
    base = (n - 1) * sides
    for k in range(sides):
        faces.append((end_c, base + k, base + (k + 1) % sides))
    add_shell(bm, verts, faces)


def bezier(p0, p1, p2, t):
    return p0 * ((1 - t) ** 2) + p1 * (2 * (1 - t) * t) + p2 * (t * t)


def ellipsoid_surface(center, radii, direction):
    d = Vector(direction).normalized()
    return Vector(center) + Vector((d.x * radii[0], d.y * radii[1], d.z * radii[2]))


def lerp(a, b, t):
    return a + (b - a) * t


# =====================================================================
# パーツ定義(この段階の寸法は「設計用の単位」。後で全長1.0に合わせて縮尺する)
# =====================================================================

HEAD_C, HEAD_R = Vector((0, 0.27, 0.05)), (0.18, 0.16, 0.15)
ABD_C, ABD_R = Vector((0, -0.40, 0.03)), (0.22, 0.29, 0.19)


class Part:
    def __init__(self, name, parent, pivot):
        self.name = name
        self.parent = parent
        self.pivot = Vector(pivot)
        self.cell = CELL[name] if name in CELL else CELL[name.split("_")[0]]
        self.bm = bmesh.new()
        self.axis = None          # 顎のグラデーション用 (根元, 先端)
        self.exclude_from_length = False


def build_parts():
    parts = []

    # ---- 胸(ルート):前胸 + 中後胸 ----
    thorax = Part("thorax", None, (0, -0.04, 0.0))
    ellipsoid(thorax.bm, (0, 0.08, 0.02), (0.085, 0.085, 0.08), THORAX_SEG, THORAX_RINGS)
    ellipsoid(thorax.bm, (0, -0.04, 0.0), (0.10, 0.11, 0.085), THORAX_SEG, THORAX_RINGS)
    parts.append(thorax)

    # ---- 腹:大きな楕円体。前を細く、体節のゆるい凹凸、後ろが少し垂れる ----
    def abd_deform(p):
        t = p.y                                  # -1(後ろ)〜+1(前)
        xz = 1.0
        if t > 0:
            xz *= 1.0 - 0.30 * t * t             # 前側(胸との接続部)を細く
        band = 0.5 + 0.5 * math.cos(math.pi * 4.0 * (t + 1.0) / 2.0 * 2.0)
        xz *= 1.0 - 0.025 * band * (1.0 - abs(t))  # 体節のゆるい凹凸(3〜4段)
        return Vector((p.x * xz, p.y, p.z * xz))

    abdomen = Part("abdomen", "thorax", (0, -0.13, 0.02))
    ellipsoid(abdomen.bm, ABD_C, ABD_R, ABDOMEN_SEG, ABDOMEN_RINGS,
              rot=Matrix.Rotation(math.radians(8), 3, 'X'), deform=abd_deform)
    parts.append(abdomen)

    # ---- 頭 ----
    head = Part("head", "thorax", (0, 0.13, 0.03))
    ellipsoid(head.bm, HEAD_C, HEAD_R, HEAD_SEG, HEAD_RINGS)
    parts.append(head)

    for side, s in (("L", -1), ("R", 1)):
        # ---- 目 ----
        eye_pos = ellipsoid_surface(HEAD_C, HEAD_R, (0.70 * s, 0.60, 0.20))
        eye_pos -= (eye_pos - HEAD_C).normalized() * 0.010   # 少し頭に埋める
        eye = Part(f"eye_{side}", "head", eye_pos)
        sphere(eye.bm, eye_pos, 0.030, EYE_SEG, EYE_RINGS)
        parts.append(eye)

        # ---- 触角(数珠状) ----
        base = ellipsoid_surface(HEAD_C, HEAD_R, (0.45 * s, 0.75, 0.49))
        ant = Part(f"antenna_{side}", "head", base)
        ant.exclude_from_length = True
        p1 = base + Vector((0.075 * s, 0.045, 0.105))
        p2 = base + Vector((0.16 * s, 0.18, 0.09))
        # 弧長で等間隔に並べ、玉どうしが軽く接するようにする
        samples = [bezier(base, p1, p2, i / 200) for i in range(201)]
        acc = [0.0]
        for a, b in zip(samples, samples[1:]):
            acc.append(acc[-1] + (b - a).length)
        for i in range(ANTENNA_BEADS):
            target = ANTENNA_START + ANTENNA_SPACING * i
            k = min(range(len(acc)), key=lambda j: abs(acc[j] - target))
            r = lerp(0.027, 0.021, i / (ANTENNA_BEADS - 1))
            if i == ANTENNA_BEADS - 1:
                r = 0.025                            # 先端の玉を少しだけ大きく
            sphere(ant.bm, samples[k], r, BEAD_SEG, BEAD_RINGS)
        parts.append(ant)

        # ---- 顎(左右で先端を交差。重ならないよう上下に少しずらす) ----
        m_base = ellipsoid_surface(HEAD_C, HEAD_R, (0.35 * s, 0.85, -0.40))
        m_tip = Vector((-0.018 * s, 0.525, -0.034 if s < 0 else -0.048))
        m_ctrl = Vector((0.08 * s, 0.49, -0.03))
        mand = Part(f"mandible_{side}", "head", m_base)
        centers, radii = [], []
        for i in range(MANDIBLE_RINGS):
            t = i / MANDIBLE_RINGS * 0.95
            centers.append(bezier(m_base, m_ctrl, m_tip, t))
            radii.append(lerp(0.034, 0.007, t ** 0.8))
        tube(mand.bm, centers, radii, MANDIBLE_SIDES, tip_point=m_tip.copy())
        mand.axis = (m_base.copy(), m_tip.copy())
        parts.append(mand)

        # ---- 脚 3本(前・中・後) ----
        for idx, (yaw_deg, attach_y) in enumerate(((35, 0.08), (0, -0.02), (-35, -0.10)), start=1):
            th = math.radians(yaw_deg)
            h = Vector((s * math.cos(th), math.sin(th), 0))
            A = Vector((0.06 * s, attach_y, -0.05))          # 付け根(胸の下側)
            B = A + h * 0.16 + Vector((0, 0, 0.06))          # 膝(上へ持ち上げる)
            C = B + h * 0.10 + Vector((0, 0, -0.20))         # 足首
            D = C + h * 0.05 + Vector((0, 0, -0.02))         # 足先
            leg = Part(f"leg_{side}{idx}", "thorax", A)
            tube(leg.bm, [A, B, C, D], [0.031, 0.026, 0.020, 0.015], LEG_SIDES)
            sphere(leg.bm, B, 0.032, JOINT_SEG, JOINT_RINGS)
            sphere(leg.bm, C, 0.024, JOINT_SEG, JOINT_RINGS)
            sphere(leg.bm, D, 0.019, JOINT_SEG, JOINT_RINGS)
            parts.append(leg)

    return parts


# =====================================================================
# 縮尺・重心・UV
# =====================================================================


def normalize_parts(parts):
    for p in parts:
        bmesh.ops.recalc_face_normals(p.bm, faces=p.bm.faces[:])
        for f in p.bm.faces:
            f.smooth = True

    # 全長(顎の先〜腹の端 = Y方向)を BODY_LENGTH に合わせる
    ys = [v.co.y for p in parts if not p.exclude_from_length for v in p.bm.verts]
    scale = BODY_LENGTH / (max(ys) - min(ys))
    m = Matrix.Scale(scale, 4)
    for p in parts:
        bmesh.ops.transform(p.bm, matrix=m, verts=p.bm.verts[:])
        p.pivot = p.pivot * scale
        if p.axis:
            p.axis = (p.axis[0] * scale, p.axis[1] * scale)

    # 重心を求めて原点へ移動(各パーツの閉じた殻の体積から計算する近似値)
    total_v, moment = 0.0, Vector((0, 0, 0))
    for p in parts:
        for f in p.bm.faces:
            vs = [l.vert.co for l in f.loops]
            for i in range(1, len(vs) - 1):
                a, b, c = vs[0], vs[i], vs[i + 1]
                vol = a.dot(b.cross(c)) / 6.0
                total_v += vol
                moment += vol * (a + b + c) / 4.0
    com = moment / total_v
    t = Matrix.Translation(-com)
    for p in parts:
        bmesh.ops.transform(p.bm, matrix=t, verts=p.bm.verts[:])
        p.pivot = p.pivot - com
        if p.axis:
            p.axis = (p.axis[0] - com, p.axis[1] - com)
    return scale, com, total_v


def cell_center_uv(cell):
    """マス番号 -> OBJ/BlenderのUV(左下原点)でのマス中央。"""
    col, row = cell % PALETTE_GRID, cell // PALETTE_GRID
    cw = 1.0 / PALETTE_GRID
    return (col + 0.5) * cw, 1.0 - (row + 0.5) * cw


def assign_uvs(parts):
    cw = 1.0 / PALETTE_GRID
    for p in parts:
        uv_layer = p.bm.loops.layers.uv.verify()
        cu, cv = cell_center_uv(p.cell)
        if p.axis:   # 顎:根元→先端をマス内の左→右に対応させ、グラデーションを拾う
            a, b = p.axis
            ab = b - a
            u0 = (cu - 0.5 * cw) + MANDIBLE_UV_MARGIN / PALETTE_SIZE
            u1 = (cu + 0.5 * cw) - MANDIBLE_UV_MARGIN / PALETTE_SIZE
        for f in p.bm.faces:
            for l in f.loops:
                d = l.vert.co - p.pivot
                n = d.normalized() if d.length > 1e-8 else Vector((0, 0, 0))
                if p.axis:
                    t = max(0.0, min(1.0, (l.vert.co - a).dot(ab) / ab.length_squared))
                    l[uv_layer].uv = (lerp(u0, u1, t), cv + n.z * UV_SPREAD * 0.3 * cw)
                else:
                    l[uv_layer].uv = (cu + n.x * UV_SPREAD * cw, cv + n.z * UV_SPREAD * cw)


# =====================================================================
# パレット画像(PNGを直接書き出す。色空間の変換を挟まず、指定した色がそのまま入る)
# =====================================================================


def palette_pixels():
    size, cpx = PALETTE_SIZE, PALETTE_SIZE // PALETTE_GRID
    colors = [COL_SPARE] * (PALETTE_GRID * PALETTE_GRID)
    colors[CELL["head"]] = COL_HEAD
    colors[CELL["thorax"]] = COL_BODY
    colors[CELL["abdomen"]] = COL_BODY
    for k in CELL:
        if k.startswith("leg_"):
            colors[CELL[k]] = COL_LEG
        elif k.startswith("antenna_"):
            colors[CELL[k]] = COL_ANTENNA
        elif k.startswith("eye_"):
            colors[CELL[k]] = COL_EYE
    mandible_cells = {CELL["mandible_L"], CELL["mandible_R"]}

    rows = []   # 上の行から(PNGの並び)
    for y in range(size):
        row = bytearray()
        for x in range(size):
            cell = (y // cpx) * PALETTE_GRID + (x // cpx)
            if cell in mandible_cells:
                t = (x % cpx) / (cpx - 1)
                t = max(0.0, min(1.0, (t - 0.45) / 0.40))
                t = t * t * (3 - 2 * t)                  # smoothstep
                c = tuple(round(lerp(a, b, t)) for a, b in zip(COL_MANDIBLE_BASE, COL_MANDIBLE_TIP))
            else:
                c = colors[cell]
            row += bytes(c)
        rows.append(bytes(row))
    return rows


def write_png(path, rows, width, height):
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


# =====================================================================
# Blenderのシーン構築
# =====================================================================


def make_material(texture_path):
    mat = bpy.data.materials.new(f"{MODEL_NAME}_mat")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    img = bpy.data.images.load(str(texture_path))
    tex = nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    tex.interpolation = 'Closest'
    tex.location = (-400, 300)
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.8
    return mat


def new_collection(name):
    col = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(col)
    return col


def build_part_objects(parts, mat):
    col = new_collection(f"{MODEL_NAME}_parts")
    objs = {}
    for p in parts:  # 親が先に来る順でパーツを作っている
        bm = p.bm.copy()
        bmesh.ops.transform(bm, matrix=Matrix.Translation(-p.pivot), verts=bm.verts[:])
        me = bpy.data.meshes.new(p.name)
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new(p.name, me)
        col.objects.link(ob)
        if p.parent:
            ob.parent = objs[p.parent]
            ob.location = p.pivot - next(q.pivot for q in parts if q.name == p.parent)
        else:
            ob.location = p.pivot
        objs[p.name] = ob
    return col, objs


def build_export_object(parts, mat):
    """全パーツを1メッシュに結合し、三角形化した書き出し用オブジェクトを作る。"""
    col = new_collection(f"{MODEL_NAME}_export")
    out = bmesh.new()
    out_uv = out.loops.layers.uv.new("UVMap")
    for p in parts:
        src_uv = p.bm.loops.layers.uv.active
        vmap = {v: out.verts.new(v.co) for v in p.bm.verts}
        for f in p.bm.faces:
            nf = out.faces.new([vmap[l.vert] for l in f.loops])
            nf.smooth = True
            for nl, sl in zip(nf.loops, f.loops):
                nl[out_uv].uv = sl[src_uv].uv
    bmesh.ops.triangulate(out, faces=out.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
    me = bpy.data.meshes.new(MODEL_NAME)
    out.to_mesh(me)
    out.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new(MODEL_NAME, me)
    col.objects.link(ob)
    return ob


def export_obj(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob

    wanted = dict(
        filepath=str(path), check_existing=False,
        forward_axis='NEGATIVE_Z', up_axis='Y', global_scale=1.0,
        apply_modifiers=True, export_selected_objects=True,
        export_uv=True, export_normals=True, export_colors=False,
        export_materials=True, export_pbr_extensions=False, path_mode='STRIP',
        export_triangulated_mesh=True, export_curves_as_nurbs=False,
        export_object_groups=False, export_material_groups=False,
        export_vertex_groups=False, export_smooth_groups=False,
    )
    # Blenderのバージョンによって存在しない引数は除く
    props = {p.identifier for p in bpy.ops.wm.obj_export.get_rna_type().properties}
    bpy.ops.wm.obj_export(**{k: v for k, v in wanted.items() if k in props})


# =====================================================================
# ピボットJSON・検証
# =====================================================================


def to_engine(v):
    """Blender座標 -> エンジン座標(左手系・Y-up・頭が-Z)。"""
    return [round(-v.x, 6), round(v.z, 6), round(-v.y, 6)]


def write_pivots(parts, path):
    cw = 1.0 / PALETTE_GRID
    data = {
        "model": MODEL_NAME,
        "coordinate_system": "engine (left-handed, +Y up, head = -Z, origin = center of mass)",
        "note": "uv_rect は左上原点(DirectX)のUV。V反転して読み込む前提。"
                "頂点のUVがこの範囲に入っていれば、そのパーツの頂点と判別できる。",
        "parts": [],
    }
    for p in parts:
        col, row = p.cell % PALETTE_GRID, p.cell // PALETTE_GRID
        data["parts"].append({
            "name": p.name,
            "parent": p.parent,
            "pivot": to_engine(p.pivot),
            "palette_cell": p.cell,
            "uv_rect": [col * cw, row * cw, (col + 1) * cw, (row + 1) * cw],
        })
    Path(path).write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def validate_obj(obj_path, mtl_path):
    """書き出したOBJを読み直して、エンジンのローダー制約を満たすか確認する。"""
    v, vt, vn, faces, objs, usemtl = [], 0, 0, 0, 0, set()
    bad_faces = 0
    for line in Path(obj_path).read_text().splitlines():
        sp = line.split()
        if not sp:
            continue
        if sp[0] == "v":
            v.append(tuple(map(float, sp[1:4])))
        elif sp[0] == "vt":
            vt += 1
        elif sp[0] == "vn":
            vn += 1
        elif sp[0] == "f":
            faces += 1
            corners = sp[1:]
            if len(corners) != 3 or any(len(c.split("/")) != 3 or "" in c.split("/") for c in corners):
                bad_faces += 1
        elif sp[0] in ("o", "g"):
            objs += 1
        elif sp[0] == "usemtl":
            usemtl.add(sp[1])
    mtl = Path(mtl_path).read_text()
    newmtl = mtl.count("newmtl ")
    map_kd = [l.strip() for l in mtl.splitlines() if l.strip().startswith("map_Kd")]
    return dict(v=v, vt=vt, vn=vn, faces=faces, objs=objs, usemtl=usemtl,
                bad_faces=bad_faces, newmtl=newmtl, map_kd=map_kd)


def bbox(points):
    mn = [min(p[i] for p in points) for i in range(3)]
    mx = [max(p[i] for p in points) for i in range(3)]
    return mn, mx


def build_report(parts, scale, com, check):
    lines = []
    add = lines.append
    add(f"=== {MODEL_NAME} 生成レポート (Blender {bpy.app.version_string}) ===")
    add("")
    add("[OBJ / ローダー制約]")
    add(f"  三角形数           : {check['faces']}")
    add(f"  頂点 v / vt / vn   : {len(check['v'])} / {check['vt']} / {check['vn']}")
    add(f"  三角形以外・UV/法線欠けの面 : {check['bad_faces']}  (0であること)")
    add(f"  o/g の数           : {check['objs']}  (1であること)")
    add(f"  usemtl の種類      : {len(check['usemtl'])}  (1であること)")
    add(f"  MTLのマテリアル数  : {check['newmtl']}  (1であること)")
    add(f"  map_Kd             : {check['map_kd']}")

    # OBJ座標(右手系) -> エンジン座標(X反転)
    eng = [(-x, y, z) for (x, y, z) in check["v"]]
    mn, mx = bbox(eng)
    size = [mx[i] - mn[i] for i in range(3)]
    radius = max(math.sqrt(x * x + y * y + z * z) for (x, y, z) in eng)

    body_pts = []
    for p in parts:
        if not p.exclude_from_length:
            body_pts += [to_engine(v.co) for v in p.bm.verts]
    bmn, bmx = bbox(body_pts)
    no_ant_r = max(math.sqrt(sum(c * c for c in q)) for q in body_pts)

    add("")
    add("[寸法(エンジン座標・縮尺は全長1.0基準)]")
    add(f"  全長(顎の先〜腹の端, Z): {bmx[2] - bmn[2]:.4f}")
    add(f"  横幅 X (脚を含む)       : {bmx[0] - bmn[0]:.4f}   触角も含む: {size[0]:.4f}")
    add(f"  高さ Y (脚を含む)       : {bmx[1] - bmn[1]:.4f}   触角も含む: {size[1]:.4f}")
    add(f"  奥行 Z (触角も含む)     : {size[2]:.4f}")
    add(f"  バウンディングボックス(触角を含む全体)")
    add(f"    min = ({mn[0]:+.4f}, {mn[1]:+.4f}, {mn[2]:+.4f})")
    add(f"    max = ({mx[0]:+.4f}, {mx[1]:+.4f}, {mx[2]:+.4f})")
    add(f"  バウンディングボックス(触角を除く)")
    add(f"    min = ({bmn[0]:+.4f}, {bmn[1]:+.4f}, {bmn[2]:+.4f})")
    add(f"    max = ({bmx[0]:+.4f}, {bmx[1]:+.4f}, {bmx[2]:+.4f})")
    add(f"  原点から最も遠い頂点までの距離 : 全体 {radius:.4f} / 触角を除く {no_ant_r:.4f}")
    add(f"  頭の向き確認: 顎の先 Z = {bmn[2]:+.4f} (負であること), 腹の端 Z = {bmx[2]:+.4f}")
    add("")
    add("[重心]")
    add("  原点 = 体積から求めた重心(各パーツの殻の重なりは二重に数える近似)")
    add(f"  設計単位からの縮尺 : {scale:.5f}")
    add("")
    add("[パーツ / ピボット(エンジン座標)]")
    for p in parts:
        tris = sum(len(f.verts) - 2 for f in p.bm.faces)
        pv = to_engine(p.pivot)
        add(f"  {p.name:<12} cell {p.cell:>2}  tris {tris:>4}  parent {str(p.parent):<8}"
            f" pivot ({pv[0]:+.4f}, {pv[1]:+.4f}, {pv[2]:+.4f})")
    return "\n".join(lines)


# =====================================================================
# メイン
# =====================================================================


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    root = None
    if "--project-root" in argv:
        root = Path(argv[argv.index("--project-root") + 1]).resolve()
    if root is None:
        # このファイル: project/tools/models/termite_worker/generate_termite_worker.py
        root = Path(__file__).resolve().parents[3]
    return root


def main():
    project_root = parse_args()
    res_dir = project_root / "resources" / MODEL_NAME
    tool_dir = project_root / "tools" / "models" / MODEL_NAME
    res_dir.mkdir(parents=True, exist_ok=True)
    tool_dir.mkdir(parents=True, exist_ok=True)

    obj_path = res_dir / f"{MODEL_NAME}.obj"
    mtl_path = res_dir / f"{MODEL_NAME}.mtl"
    png_path = res_dir / f"{MODEL_NAME}_palette.png"
    json_path = res_dir / f"{MODEL_NAME}_pivots.json"
    blend_path = tool_dir / f"{MODEL_NAME}.blend"
    report_path = tool_dir / f"{MODEL_NAME}_report.txt"

    bpy.ops.wm.read_factory_settings(use_empty=True)

    write_png(png_path, palette_pixels(), PALETTE_SIZE, PALETTE_SIZE)

    parts = build_parts()
    scale, com, _ = normalize_parts(parts)
    assign_uvs(parts)

    mat = make_material(png_path)
    build_part_objects(parts, mat)
    export_ob = build_export_object(parts, mat)
    export_obj(export_ob, obj_path)
    export_ob.hide_set(True)   # .blendを開いたときはパーツ側を見せる

    write_pivots(parts, json_path)

    check = validate_obj(obj_path, mtl_path)
    report = build_report(parts, scale, com, check)
    print(report)
    report_path.write_text(report, encoding="utf-8")

    bpy.data.images[png_path.name].filepath = bpy.path.relpath(str(png_path), start=str(tool_dir))
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), relative_remap=True)

    for p in parts:
        p.bm.free()

    ok = (check["bad_faces"] == 0 and check["objs"] == 1 and len(check["usemtl"]) == 1
          and check["newmtl"] == 1 and check["vt"] > 0 and check["vn"] > 0)
    print("\n[RESULT] " + ("OK: ローダー制約を満たしています" if ok else "NG: レポートを確認してください"))


if __name__ == "__main__":
    main()
