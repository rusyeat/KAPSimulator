# V4 кейіпкерлер: V3 киімді денесі (SK_Mannequin қаңқасы, аймақ UV) + MetaHuman басы (шынайы бет пішіні).
# Іске қосу (фонда):
#   blender -b SourceArt/CharactersV3/KAP_Characters_Rework.blend --python Scripts/blender/build_v4_heads.py -- <OUTDIR> <MHDIR>
# OUTDIR: SourceArt/CharactersV4 (FBX + алдын ала көріністер + .blend), MHDIR: MetaHuman bodyShape*_CombinedSkelMesh.FBX қалтасы
import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
OUTDIR, MHDIR = argv[0], argv[1]
os.makedirs(OUTDIR, exist_ok=True)
# (вариант, MetaHuman пішіні, басқа: әйелдің V3 шаш қабатын сақтау)
VARIANTS = [("AdultMale", "D", False), ("AdultFemale", "C", False)]
HEAD_FACES = 3800
HAND_RATIO = 0.25
LOG = []
def log(*a):
    s = " ".join(str(x) for x in a); LOG.append(s); print("KAPV4", s)

def reg(uv1, p): return int(math.floor(uv1[p.loop_start].uv.x * 16 + 1e-4))
def lerp(a, b, t): return a + (b - a) * t
def clamp(x, a, b): return max(a, min(b, x))

def islands(bm):
    seen = set(); out = []
    for v in bm.verts:
        if v.index in seen: continue
        st = [v]; seen.add(v.index); grp = []
        while st:
            x = st.pop(); grp.append(x)
            for e in x.link_edges:
                o = e.other_vert(x)
                if o.index not in seen: seen.add(o.index); st.append(o)
        out.append(grp)
    return out

def bbox(vs):
    mn = Vector((min(v.co.x for v in vs), min(v.co.y for v in vs), min(v.co.z for v in vs)))
    mx = Vector((max(v.co.x for v in vs), max(v.co.y for v in vs), max(v.co.z for v in vs)))
    return mn, mx

def ring_extent(points, ax, ay):
    """Қима: кеңдігі (x), алдыңғы (−Y) және артқы (+Y) қашықтығы — ось ортасынан"""
    if not points: return None
    rx = max(abs(p.x - ax) for p in points)
    rf = max((ay - p.y) for p in points)
    rb = max((p.y - ay) for p in points)
    return (max(rx, 1e-3), max(rf, 1e-3), max(rb, 1e-3))

def set_active(o):
    for x in bpy.context.view_layer.objects: x.select_set(False)
    o.select_set(True); bpy.context.view_layer.objects.active = o

PALETTE = {0: (0.62, 0.45, 0.35), 1: (0.25, 0.35, 0.4), 2: (0.12, 0.15, 0.2), 3: (0.06, 0.05, 0.04), 4: (0.08, 0.06, 0.05), 5: (0.9, 0.88, 0.85),
           6: (0.25, 0.15, 0.07), 7: (0.0, 0.0, 0.0), 8: (0.55, 0.3, 0.28), 9: (0.42, 0.3, 0.25), 10: (0.4, 0.45, 0.25), 11: (0.05, 0.05, 0.05), 12: (0.72, 0.52, 0.4), 13: (0.7, 0.5, 0.4)}

def preview(obj, rig, name):
    """Аймақтарды түспен көрсетіп, Workbench-те бет пен толық бойды түсіру"""
    me = obj.data
    uv1 = me.uv_layers["Regions"].data
    ca = me.color_attributes.get("RegionPreview") or me.color_attributes.new("RegionPreview", "BYTE_COLOR", "CORNER")
    for p in me.polygons:
        c = PALETTE.get(reg(uv1, p), (1, 0, 1))
        for li in p.loop_indices: ca.data[li].color = (c[0], c[1], c[2], 1.0)
    me.color_attributes.active_color = ca
    scn = bpy.context.scene
    scn.render.engine = "BLENDER_WORKBENCH"
    scn.display.shading.light = "STUDIO"; scn.display.shading.color_type = "VERTEX"
    scn.render.resolution_x = 900; scn.render.resolution_y = 1000
    cam = bpy.data.objects.get("V4Cam") or bpy.data.objects.new("V4Cam", bpy.data.cameras.new("V4Cam"))
    if cam.name not in scn.collection.objects: scn.collection.objects.link(cam)
    scn.camera = cam; cam.data.lens = 70
    hb = rig.data.bones["head"]; hw = rig.matrix_world @ hb.head_local
    for tag, tgt, dist, ang in (("face", hw + Vector((0, 0, 0.08)), 0.75, 20), ("full", Vector((hw.x, hw.y, 0.95)), 5.2, 25)):
        a = math.radians(ang)
        cam.location = tgt + Vector((math.sin(a) * dist, -math.cos(a) * dist, 0.02 if tag == "face" else 0.3))
        cam.rotation_euler = (tgt - cam.location).to_track_quat("-Z", "Y").to_euler()
        scn.render.filepath = os.path.join(OUTDIR, "preview_%s_%s.png" % (name, tag))
        bpy.ops.render.render(write_still=True)

for name, L, keep_hair in VARIANTS:
    rig = bpy.data.objects["Rig_" + name]
    body = bpy.data.objects["SKM_KAP_V3_" + name]
    mwb = body.matrix_world.copy()
    me = body.data
    uvR = me.uv_layers["Regions"].data
    rigx = rig.matrix_world.translation.x
    # ---- V3: көз орталары, бет пен мойынның биіктігі ----
    eyes = {1: [], -1: []}; z12 = 9.0; z0 = 9.0
    for p in me.polygons:
        r = reg(uvR, p); c = mwb @ p.center
        if r in (5, 6, 7): eyes[1 if c.x > rigx else -1].append(c)
        if r == 12: z12 = min(z12, c.z)
        if r == 0: z0 = min(z0, c.z)
    eL = sum(eyes[1], Vector()) / len(eyes[1]); eR = sum(eyes[-1], Vector()) / len(eyes[-1])
    eyeC = (eL + eR) / 2
    Zc = (z0 + z12) / 2
    neck2 = rig.matrix_world @ rig.data.bones["neck_02"].head_local
    ax, ay = neck2.x, neck2.y
    log(name, "V3 eyes", tuple(round(c, 4) for c in eyeC), "sep", round((eL - eR).length, 4), "Zc", round(Zc, 4), "faces", len(me.polygons))
    # салмақ көзі ретінде V3 денесінің толық көшірмесі
    src = body.copy(); src.data = body.data.copy(); src.name = "V3src_" + name
    bpy.context.scene.collection.objects.link(src)
    # ---- MetaHuman импорты ----
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=os.path.join(MHDIR, "bodyShape%sof4_CombinedSkelMesh.FBX" % L))
    new = [o for o in bpy.data.objects if o not in before]
    mh = [o for o in new if o.type == "MESH"][0]
    M = mh.matrix_world.copy()
    mh.parent = None; mh.matrix_world = Matrix.Identity(4)
    mh.data.transform(M)
    for m in list(mh.modifiers): mh.modifiers.remove(m)
    mh.vertex_groups.clear()
    for o in new:
        if o != mh: bpy.data.objects.remove(o, do_unlink=True)
    mh.name = "MHHead_" + name
    # ---- аралдар: бас терісі (ең үлкені, жоғарыда) + екі көз алмасы; қалғаны (кірпік, тіс, тіл, қабық) — өшіріледі ----
    bm = bmesh.new(); bm.from_mesh(mh.data); bm.verts.ensure_lookup_table()
    isl = islands(bm)
    hi = [g for g in isl if bbox(g)[1].z > 1.5]
    head = max(hi, key=len)
    hmn, hmx = bbox(head)
    hcx = (hmn.x + hmx.x) / 2
    eyes_mh = {1: None, -1: None}
    for g in isl:
        mn, mx = bbox(g); s = mx - mn; c = (mn + mx) / 2
        if all(0.019 < d < 0.032 for d in s) and hmn.y < c.y < (hmn.y + hmx.y) / 2 and abs(c.x - hcx) > 0.012 and c.z > hmn.z + 0.15:
            side = 1 if c.x > hcx else -1
            if eyes_mh[side] is None or len(g) > len(eyes_mh[side][0]): eyes_mh[side] = (g, c)
    if eyes_mh[1] is None or eyes_mh[-1] is None: raise RuntimeError("eyeballs not found for " + name)
    head_ids = set(v.index for v in head)
    eye_ids = set(v.index for v in eyes_mh[1][0]) | set(v.index for v in eyes_mh[-1][0])
    mhEye = (eyes_mh[1][1] + eyes_mh[-1][1]) / 2
    T = eyeC - mhEye
    eyeCtr = {1: eyes_mh[1][1] + T, -1: eyes_mh[-1][1] + T}
    log(name, "MH eye sep", round((eyes_mh[1][1] - eyes_mh[-1][1]).length, 4), "shift", tuple(round(c, 4) for c in T))
    Z0 = Zc - 0.015
    # көз алмалары — бөлек меш (бас терісімен байланысы жоқ аралдар)
    bme = bm.copy(); bme.verts.ensure_lookup_table()
    bmesh.ops.delete(bme, geom=[v for v in bme.verts if v.index not in eye_ids], context="VERTS")
    for v in bme.verts: v.co += T
    eyes_me = bpy.data.meshes.new("MHEyes_" + name); bme.to_mesh(eyes_me); bme.free()
    eyes_o = bpy.data.objects.new("MHEyes_" + name, eyes_me); bpy.context.scene.collection.objects.link(eyes_o)
    # бас терісі: тек ең үлкен арал, мойын қимасы — Zc-тен 1.5 см төмен
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.index not in head_ids], context="VERTS")
    for v in bm.verts: v.co += T
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < Z0], context="VERTS")
    bm.to_mesh(mh.data); bm.free()
    # ---- бас терісін жұқарту ----
    set_active(mh)
    nf = len(mh.data.polygons)
    # мойын жиегін жұқартпау (ширақ, тегіс шекара): «Keep» тобы — жиек маңында 0
    kg = mh.vertex_groups.new(name="Keep")
    for v in mh.data.vertices: kg.add([v.index], 0.0 if v.co.z < Zc + 0.035 else 1.0, "REPLACE")
    dm = mh.modifiers.new("Dec", "DECIMATE"); dm.ratio = clamp(HEAD_FACES / max(1, nf), 0.05, 1.0); dm.use_collapse_triangulate = False
    dm.vertex_group = "Keep"; dm.vertex_group_factor = 1.0
    bpy.ops.object.modifier_apply(modifier=dm.name)
    if mh.vertex_groups.get("Keep"): mh.vertex_groups.remove(mh.vertex_groups["Keep"])
    log(name, "head faces", nf, "->", len(mh.data.polygons))
    # ---- мойын адаптері: V3 жиегі (Zc) MH ішінде жасырынсын, MH жиегі (Z0) V3 ішіне кірсін ----
    def plane_ring(meobj, mat, h):
        # z = h жазықтығымен қиылысатын қабырғалар → қима нүктелері
        pts = []
        vs = [mat @ v.co for v in meobj.data.vertices]
        for e in meobj.data.edges:
            a, b = vs[e.vertices[0]], vs[e.vertices[1]]
            if (a.z - h) * (b.z - h) < 0:
                t = (h - a.z) / (b.z - a.z); p = a.lerp(b, t)
                if math.hypot(p.x - ax, p.y - ay) < 0.11: pts.append(p)
        return ring_extent(pts, ax, ay)
    def v3_ring(h): return plane_ring(src, mwb, h)
    def mh_ring(h): return plane_ring(mh, Matrix.Identity(4), h)
    log(name, "rings V3", v3_ring(Z0 + 0.003), v3_ring(Zc), "MH", mh_ring(Z0 + 0.003), mh_ring(Zc))
    # әр бағытта V3 мойын бетіне дейінгі қашықтық (сәуле) — MH мойны соған жапсырылады:
    # Z0-де 0.95× (V3 ішінде, MH жиегі көрінбейді) → Zc-те 1.035× (V3 жиегін жабады) → Zc+3 см-де өз пішіні
    from mathutils.bvhtree import BVHTree
    sv = [mwb @ v.co for v in src.data.vertices]
    bvh = BVHTree.FromPolygons(sv, [tuple(p.vertices) for p in src.data.polygons])
    moved = 0
    for v in mh.data.vertices:
        z = v.co.z
        if z > Zc + 0.06: continue
        d = Vector((v.co.x - ax, v.co.y - ay, 0.0))
        if d.length < 1e-4: continue
        dirn = d.normalized()
        zq = clamp(z, Z0, Zc)
        hit = bvh.ray_cast(Vector((ax, ay, zq)), dirn, 0.25)
        if hit[0] is None: continue
        rv = hit[3]
        if z <= Zc: target = rv * lerp(0.95, 1.035, clamp((z - Z0) / max(1e-4, Zc - Z0), 0, 1))
        else: tt = (z - Zc) / 0.06; tt = tt * tt * (3 - 2 * tt); target = lerp(rv * 1.035, d.length, tt)
        v.co.x = ax + dirn.x * target; v.co.y = ay + dirn.y * target
        moved += 1
    log(name, "neck verts fitted", moved)
    # ---- UV: SurfaceUV (бар) + Regions (аймақ кодтары) — бас пен көзге бөлек, сосын біріктіру ----
    counts = {}
    def region_uv(obj, is_eye):
        mme = obj.data
        if mme.uv_layers: mme.uv_layers[0].name = "SurfaceUV"
        else: mme.uv_layers.new(name="SurfaceUV")
        rl = mme.uv_layers.new(name="Regions")
        for p in mme.polygons:
            r = classify(p, is_eye)
            counts[r] = counts.get(r, 0) + 1
            u = (r + 0.5) / 16.0
            for li in p.loop_indices: rl.data[li].uv = (u, 0.5)
    def classify(p, is_eye):
        c = p.center; n = p.normal
        rel = c - eyeC
        r = 12
        if is_eye:
            # көз алмасы: ақ / нұрлы қабық / қарашық (алға −Y қарайды)
            s = 1 if c.x > eyeC.x else -1
            d = (c - eyeCtr[s]).normalized(); cs = d.dot(Vector((0, -1, 0)))
            return 7 if cs > 0.985 else (6 if cs > 0.925 else 5)
        if True:
            back = rel.y; axs = abs(rel.x)
            lips = (rel.x / 0.026) ** 2 + ((rel.z + 0.07) / 0.0135) ** 2 < 1.0 and n.y < -0.25 and rel.y < -0.010
            brow = 0.015 < rel.z < 0.029 and 0.014 < axs < 0.054 and n.y < -0.35 and rel.y < 0.0 and rel.z < 0.029 - (axs - 0.014) * 0.12
            t = clamp(back / 0.13, 0.0, 1.0)
            hl = lerp(0.066, -0.11, t ** 1.3)
            if axs > 0.056 and back > -0.01: hl = min(hl, 0.032)
            ear = axs > 0.055 and -0.06 < rel.z < 0.03 and -0.01 < back < 0.095
            if lips: r = 8
            elif brow: r = 9
            elif rel.z > hl and not ear: r = 4
            elif rel.z < -0.115: r = 0
        return r
    region_uv(mh, False)
    # ---- шаш қабаты: шаш аймағын (4) көшіріп, сыртқа ығыстыру (ер — қысқа 5 мм, әйел — 13 мм + желке түйіні) ----
    female = name.endswith("Female")
    hb = bmesh.new(); hb.from_mesh(mh.data); hb.faces.ensure_lookup_table()
    huv = hb.loops.layers.uv["Regions"]
    hfaces = [f for f in hb.faces if int(math.floor(f.loops[0][huv].uv.x * 16 + 1e-4)) == 4]
    if hfaces:
        dup = bmesh.ops.duplicate(hb, geom=hfaces)
        nv = [g for g in dup["geom"] if isinstance(g, bmesh.types.BMVert)]
        hb.normal_update()
        off = 0.013 if female else 0.005
        for v in nv: v.co += v.normal * off
        if female:
            # желкедегі түйін (бас артында, сәл төмен)
            bc = eyeC + Vector((0.0, 0.135, -0.035))
            sph = bmesh.ops.create_uvsphere(hb, u_segments=12, v_segments=8, radius=0.04)
            for v in sph["verts"]: v.co = bc + Vector((v.co.x * 1.0, v.co.y * 0.8, v.co.z * 0.85))
            for f in {f for v in sph["verts"] for f in v.link_faces}:
                for l in f.loops: l[huv].uv = ((4 + 0.5) / 16.0, 0.5)
    hb.to_mesh(mh.data); hb.free()
    region_uv(eyes_o, True)
    set_active(mh); eyes_o.select_set(True); bpy.ops.object.join()
    log(name, "MH regions", sorted(counts.items()))
    # ---- салмақтар: V3 денесінен (жақын беттің интерполяциясы) ----
    for g in src.vertex_groups: mh.vertex_groups.new(name=g.name)
    dt = mh.modifiers.new("DT", "DATA_TRANSFER"); dt.object = src
    dt.use_vert_data = True; dt.data_types_verts = {"VGROUP_WEIGHTS"}; dt.vert_mapping = "POLYINTERP_NEAREST"
    dt.layers_vgroup_select_src = "ALL"; dt.layers_vgroup_select_dst = "NAME"
    set_active(mh); bpy.ops.object.modifier_apply(modifier=dt.name)
    # ---- V3 денесі: бас пен мойынның жоғарғы бөлігін өшіру, қолдарды жұқарту ----
    set_active(body)
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="DESELECT")
    em = bmesh.from_edit_mesh(me); uvl = em.loops.layers.uv["Regions"]
    rm = []
    for f in em.faces:
        r = int(math.floor(f.loops[0][uvl].uv.x * 16 + 1e-4))
        if keep_hair and r == 4: continue
        if any((mwb @ v.co).z > Zc for v in f.verts): rm.append(f)
    bmesh.ops.delete(em, geom=rm, context="FACES")
    # жеке қалған төбелерді тазалау
    bmesh.ops.delete(em, geom=[v for v in em.verts if not v.link_faces], context="VERTS")
    for f in em.faces: f.select = int(math.floor(f.loops[0][uvl].uv.x * 16 + 1e-4)) == 13
    bmesh.update_edit_mesh(me)
    nh = sum(1 for f in em.faces if f.select)
    bpy.ops.mesh.decimate(ratio=HAND_RATIO)
    bpy.ops.object.mode_set(mode="OBJECT")
    log(name, "hand faces", nh, "-> ~", int(nh * HAND_RATIO))
    # ---- біріктіру ----
    set_active(body); mh.select_set(True); bpy.ops.object.join()
    bpy.ops.object.mode_set(mode="EDIT"); bpy.ops.mesh.select_all(action="SELECT"); bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode="OBJECT")
    bpy.data.objects.remove(src, do_unlink=True)
    body.name = "SKM_KAP_V4_" + name
    # бір материал ұясы (MH басы өз ұясын әкелген)
    for p in body.data.polygons: p.material_index = 0
    while len(body.data.materials) > 1: body.data.materials.pop(index=len(body.data.materials) - 1)
    # салмақ тексерісі: әр төбе қаңқаға байланған болуы керек
    unw = sum(1 for v in body.data.vertices if not any(g.weight > 0.01 for g in v.groups))
    arm_mods = [m for m in body.modifiers if m.type == "ARMATURE" and m.object == rig]
    log(name, "groups", len(body.vertex_groups), "unweighted", unw, "armature mod", len(arm_mods), "parent", body.parent.name if body.parent else None)
    if not arm_mods:
        am = body.modifiers.new("Armature", "ARMATURE"); am.object = rig
    log(name, "final faces", len(body.data.polygons), "verts", len(body.data.vertices))
    preview(body, rig, name)
    # ---- FBX: арматура «root» деп аталып, бас нүктеге қойылады (V3 экспорты сияқты) ----
    old_name, old_loc = rig.name, rig.location.copy()
    # «root» атауы бос болсын (MH импортынан қалған арматура деректері атау алып қоюы мүмкін)
    for o in bpy.data.objects:
        if o.name == "root" and o != rig: o.name = "root_mh_leftover"
    rig.name = "root"; rig.location = Vector((0, 0, 0))
    # V3 файлында арматура жасырын — экспорт тек көрінетін/таңдалатын объектілерді алады
    for o in (rig, body):
        o.hide_viewport = False; o.hide_select = False; o.hide_set(False)
        for c in o.users_collection: c.hide_viewport = False
    set_active(rig); body.select_set(True)
    log(name, "export selection", [o.name for o in bpy.context.selected_objects])
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUTDIR, "SKM_KAP_V4_%s.fbx" % name), use_selection=True, object_types={"ARMATURE", "MESH"},
                             add_leaf_bones=False, bake_anim=False, mesh_smooth_type="FACE", use_armature_deform_only=False)
    rig.name = old_name; rig.location = old_loc
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUTDIR, "KAP_Characters_V4.blend"), copy=True)
with open(os.path.join(OUTDIR, "build_log.txt"), "w", encoding="utf-8") as f: f.write("\n".join(LOG))
