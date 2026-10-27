"""KAP: дала жануарлары — жылқы (қазақы жылқы), сиыр (қазақтың ақбас сиыры), ешкі, қазақтың құйрықты қойы, түлкі.
Blender 5.2, толық процедуралық (сырттан ештеңе жүктелмейді). Әр жануар:
  • анатомиялық «лофт» бөліктерінен жиналып, воксельмен біртұтас бетке айналады (тігіссіз), тегістеледі, ойын тығыздығына дейін жеңілдетіледі;
  • түс маскасы вершина түсінде: R — екінші түс (сиырдың ақ басы, қойдың қара басы, түлкінің ақ кеудесі), G — тұяқ/мүйіз/танау, B — көз;
  • қаңқа: түбір, жамбас, омыртқа, кеуде, мойын×2, бас, құйрық×2, әр аяқта 4 сүйек; салмақтар — Blender «heat» (сәтсіз болса — қашықтық);
  • анимациялар (30 к/с, орнында): Idle (тыныс, құйрық), Walk (4 тактілі адым), Trot (диагональ желіс), Gallop (шабыс), Graze (шөп жеу).
Шығыс: D:/KAPSimulator/SourceArt/AnimalsV1/SKM_KAP_<A>.fbx, AN_KAP_<A>_<Anim>.fbx, animals.json (адым ұзындығы — ойындағы ойнату жылдамдығы үшін)
Іске қосу: blender.exe -b --factory-startup -P build_animals_v1.py"""
import bpy, math, json
from pathlib import Path
from mathutils import Vector, Matrix, noise

OUT = Path('D:/KAPSimulator/SourceArt/AnimalsV1')
OUT.mkdir(parents=True, exist_ok=True)
FPS = 30
REPORT = {}


def reset():
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete()
    for coll in (bpy.data.meshes, bpy.data.armatures, bpy.data.actions, bpy.data.materials):
        for b in list(coll):
            coll.remove(b)


def mat(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return m


def loft(name, stations, n=18):
    """stations: (x, y, z, rv, rl) — орта нүкте және тік/көлденең радиустар"""
    verts, faces = [], []
    pts = [Vector(s[:3]) for s in stations]
    for j, s in enumerate(stations):
        t = pts[min(j + 1, len(pts) - 1)] - pts[max(j - 1, 0)]
        t.normalize()
        side = Vector((0, 1, 0))
        up = t.cross(side)
        if up.length < 0.1:
            up = t.cross(Vector((1, 0, 0)))
        up.normalize()
        side = up.cross(t).normalized()
        for i in range(n):
            a = 2 * math.pi * i / n
            verts.append(pts[j] + up * (math.cos(a) * s[3]) + side * (math.sin(a) * s[4]))
    for j in range(len(stations) - 1):
        for i in range(n):
            faces.append((j * n + i, j * n + (i + 1) % n, (j + 1) * n + (i + 1) % n, (j + 1) * n + i))
    faces.append(tuple(reversed(range(n))))
    faces.append(tuple((len(stations) - 1) * n + i for i in range(n)))
    d = bpy.data.meshes.new(name)
    d.from_pydata([tuple(v) for v in verts], [], faces)
    d.update()
    ob = bpy.data.objects.new(name, d)
    bpy.context.collection.objects.link(ob)
    return ob


def ellipsoid(name, loc, scale, seg=16, rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=rings, location=loc)
    ob = bpy.context.object
    ob.name = name
    ob.scale = scale
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return ob


def mirror_y(st):
    return [(x, -y, z, rv, rl) for (x, y, z, rv, rl) in st]


def join(objs, name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.object
    ob.name = name
    return ob


def weld(ob, voxel, tris):
    bpy.context.view_layer.objects.active = ob
    r = ob.modifiers.new('Weld', 'REMESH'); r.mode = 'VOXEL'; r.voxel_size = voxel; r.use_smooth_shade = True
    bpy.ops.object.modifier_apply(modifier=r.name)
    s = ob.modifiers.new('Relax', 'SMOOTH'); s.factor = 0.6; s.iterations = 4
    bpy.ops.object.modifier_apply(modifier=s.name)
    d = ob.modifiers.new('Game', 'DECIMATE'); d.ratio = min(1.0, tris / max(1, len(ob.data.polygons)))
    bpy.ops.object.modifier_apply(modifier=d.name)
    for p in ob.data.polygons:
        p.use_smooth = True


def paint(ob, fn):
    me = ob.data
    if 'Col' in me.color_attributes:
        me.color_attributes.remove(me.color_attributes['Col'])
    ca = me.color_attributes.new(name='Col', type='BYTE_COLOR', domain='CORNER')
    me.color_attributes.active_color = ca
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            r, g, b = fn(v)
            ca.data[li].color = (r, g, b, 1.0)
    # UV: цилиндрлік проекция (материалдағы жүн шуы әлем/дене кеңістігінде, UV — қосымша)
    uv = me.uv_layers.new(name='UVMap')
    for poly in me.polygons:
        for li in poly.loop_indices:
            v = me.vertices[me.loops[li].vertex_index].co
            uv.data[li].uv = (v.x * 0.5 + 0.5, math.atan2(v.y, v.z - 0.5) / (2 * math.pi) + 0.5)


def smooth01(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def build_rig(name, bones):
    d = bpy.data.armatures.new(name + '_Skeleton')
    arm = bpy.data.objects.new(name + 'Rig', d)
    bpy.context.collection.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for bn, (a, b, parent) in bones.items():
        e = d.edit_bones.new(bn)
        e.head = a; e.tail = b
        e.align_roll(Vector((0, 1, 0)))
        if parent:
            e.parent = d.edit_bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


def skin(ob, arm, bones):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True); arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    ok = True
    try:
        bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    except Exception:
        ok = False
    # heat сәтсіз болса (бос топтар), қашықтыққа негізделген салмақ
    empty = sum(1 for v in ob.data.vertices if len(v.groups) == 0)
    if not ok or empty > len(ob.data.vertices) * 0.02:
        for vg in list(ob.vertex_groups):
            ob.vertex_groups.remove(vg)
        groups = {n: ob.vertex_groups.new(name=n) for n in bones if n != 'root'}

        def segd(p, a, b):
            a = Vector(a); b = Vector(b); ab = b - a
            t = max(0, min(1, (p - a).dot(ab) / ab.length_squared))
            return (p - a - ab * t).length
        for v in ob.data.vertices:
            c = sorted(((1 / max(0.01, segd(v.co, a, b)) ** 5, n) for n, (a, b, _) in bones.items() if n != 'root'), reverse=True)[:3]
            tot = sum(w for w, _ in c)
            for w, n in c:
                groups[n].add([v.index], w / tot, 'REPLACE')
        if not ob.parent:
            ob.parent = arm
        if not any(m.type == 'ARMATURE' for m in ob.modifiers):
            m = ob.modifiers.new('Skeleton', 'ARMATURE'); m.object = arm
    return ok and empty <= len(ob.data.vertices) * 0.02


def export(arm, objs, name, anim):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs + [arm]:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '.fbx')), use_selection=True, object_types={'ARMATURE', 'MESH'},
                             add_leaf_bones=False, bake_anim=anim, bake_anim_use_all_actions=False, bake_anim_use_nla_strips=False,
                             bake_anim_simplify_factor=0, use_armature_deform_only=False, axis_forward='-Y', axis_up='Z',
                             path_mode='COPY', embed_textures=False, mesh_smooth_type='FACE', colors_type='LINEAR')


CM = 100.0


def to_cm(arm, objs):
    """Экспорт алдында бәрін 100 есе үлкейтіп, масштабты қолданамыз, сахна бірлігі = 1 см.
    Әйтпесе FBX-те арматура түйініне ×100 масштаб жазылады, UE оны түбір сүйекке (…Rig) қояды, ал UE рагдоллы
    масштабы бар түбір сүйекті дұрыс есептемейді: сүйектер физика денелерінен ығысып, жануар ауада ілінеді/жерге батады."""
    bpy.context.scene.unit_settings.scale_length = 1.0 / CM
    bpy.ops.object.select_all(action='DESELECT')
    for o in [arm] + objs:
        if o.parent is None:
            o.scale = (CM, CM, CM)
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    bpy.context.view_layer.update()
    for o in [arm] + objs:
        assert max(abs(s - 1.0) for s in o.matrix_world.to_scale()) < 1e-4, (o.name, tuple(o.matrix_world.to_scale()))


def leg_bones(bones, side, part, pts, parent):
    for j in range(4):
        bones['%s%d_%s' % (part, j, side)] = (pts[j], pts[j + 1], parent if j == 0 else '%s%d_%s' % (part, j - 1, side))


# ---------------------------------------------------------------- анимация
def animate(arm, name, a):
    """a: жануар сипаттамасы (аяқ ұзындығы, жүріс түрлері)"""
    sc = bpy.context.scene
    sc.render.fps = FPS
    out = {}
    PB = arm.pose.bones

    def clear():
        for pb in PB:
            pb.rotation_mode = 'XYZ'; pb.rotation_euler = (0, 0, 0); pb.location = (0, 0, 0); pb.scale = (1, 1, 1)

    def key(f):
        for pb in PB:
            pb.location = pb.location * CM   # сүйек кеңістігі енді сантиметрде (to_cm)
            pb.keyframe_insert('rotation_euler', frame=f); pb.keyframe_insert('location', frame=f); pb.keyframe_insert('scale', frame=f)

    def gait(kind, frames, amp, duty, phases, lift, bob, nod, spine=0.0):
        act = bpy.data.actions.new('%s_%s' % (name, kind))
        arm.animation_data_create(); arm.animation_data.action = act
        for f in range(frames + 1):
            u = f / frames
            clear()
            for leg, ph in phases.items():
                part, side = leg[0], leg[1]
                p = (u + ph) % 1.0
                if p < duty:
                    ang = -amp + 2 * amp * (p / duty)          # тірек: аяқ алдынан артқа (дене алға жылжиды)
                    flex = 0.0
                else:
                    s = (p - duty) / (1 - duty)
                    ang = amp - 2 * amp * (s * s * (3 - 2 * s))   # көтеріліп, алға шығады
                    flex = math.sin(math.pi * s) * lift
                pre = 'fore' if part == 'f' else 'hind'
                b0, b1, b2, b3 = (PB['%s%d_%s' % (pre, j, side)] for j in range(4))
                b0.rotation_euler.z = ang
                if pre == 'fore':
                    b1.rotation_euler.z = flex * 0.25
                    b2.rotation_euler.z = flex * 1.25
                    b3.rotation_euler.z = flex * 0.6 + (0.15 if p < duty and p > duty * 0.7 else 0.0)
                else:
                    b1.rotation_euler.z = -flex * 0.55
                    b2.rotation_euler.z = flex * 1.05
                    b3.rotation_euler.z = flex * 0.5
            w = 2 * math.pi * u
            PB['pelvis'].location.z = -abs(math.sin(w * (1 if kind == 'Gallop' else 2))) * bob
            PB['neck1'].rotation_euler.z = math.sin(w * 2) * nod
            PB['head'].rotation_euler.z = -math.sin(w * 2) * nod * 0.6
            if spine:
                PB['spine'].rotation_euler.z = math.sin(w) * spine
                PB['chest'].rotation_euler.z = -math.sin(w) * spine * 0.8
                PB['neck1'].rotation_euler.z += math.sin(w + 0.6) * spine * 1.2
            PB['tail1'].rotation_euler.x = math.sin(w) * 0.12
            PB['tail1'].rotation_euler.z = -0.15 - (0.25 if kind == 'Gallop' else 0.0)
            key(f + 1)
        sc.frame_start = 1; sc.frame_end = frames + 1; sc.frame_set(1)
        # адым ұзындығы: тірек фазасында аяқ ұшы жүретін қашықтық = 2·L·sin(amp) → жылдамдық = адым / (duty·T)
        stride = 2 * a['leg'] * math.sin(amp)
        out[kind] = {'frames': frames, 'seconds': frames / FPS, 'speed': stride / (duty * frames / FPS)}
        export(arm, a['objs'], 'AN_KAP_%s_%s' % (name, kind), True)

    def idle():
        act = bpy.data.actions.new('%s_Idle' % name)
        arm.animation_data_create(); arm.animation_data.action = act
        frames = 120
        for f in range(frames + 1):
            u = f / frames; w = 2 * math.pi * u
            clear()
            PB['chest'].scale = (1, 1 + 0.015 * math.sin(w * 3), 1 + 0.02 * math.sin(w * 3))
            PB['neck1'].rotation_euler.z = 0.04 * math.sin(w)
            PB['head'].rotation_euler.x = 0.18 * math.sin(w) * smooth01(0.2, 0.5, u) * (1 - smooth01(0.6, 0.9, u))
            PB['tail1'].rotation_euler.x = 0.35 * math.sin(w * 2) * smooth01(0.5, 0.6, u) * (1 - smooth01(0.8, 0.95, u))
            PB['tail1'].rotation_euler.z = -0.05
            PB['pelvis'].location.z = 0.004 * math.sin(w * 3)
            key(f + 1)
        sc.frame_start = 1; sc.frame_end = frames + 1; sc.frame_set(1)
        out['Idle'] = {'frames': frames, 'seconds': frames / FPS, 'speed': 0.0}
        export(arm, a['objs'], 'AN_KAP_%s_Idle' % name, True)

    def graze():
        act = bpy.data.actions.new('%s_Graze' % name)
        arm.animation_data_create(); arm.animation_data.action = act
        frames = 90
        down = a['graze']
        for f in range(frames + 1):
            u = f / frames; w = 2 * math.pi * u
            clear()
            PB['neck1'].rotation_euler.z = -down[0]
            PB['neck2'].rotation_euler.z = -down[1]
            PB['head'].rotation_euler.z = -down[2] + 0.06 * math.sin(w * 6)   # шайнау
            PB['head'].rotation_euler.x = 0.12 * math.sin(w)
            PB['fore0_l'].rotation_euler.z = -0.06; PB['fore0_r'].rotation_euler.z = -0.02
            PB['tail1'].rotation_euler.x = 0.3 * math.sin(w * 2)
            key(f + 1)
        sc.frame_start = 1; sc.frame_end = frames + 1; sc.frame_set(1)
        out['Graze'] = {'frames': frames, 'seconds': frames / FPS, 'speed': 0.0}
        export(arm, a['objs'], 'AN_KAP_%s_Graze' % name, True)

    idle()
    walk = {'hl': 0.0, 'fl': 0.25, 'hr': 0.5, 'fr': 0.75}       # латерал 4 такт
    trot = {'fl': 0.0, 'hr': 0.0, 'fr': 0.5, 'hl': 0.5}         # диагональ
    gal = {'hl': 0.0, 'hr': 0.1, 'fl': 0.45, 'fr': 0.56}        # айналмалы шабыс
    g = a['gaits']
    gait('Walk', g['walk'], a['amp'] * 0.75, 0.66, walk, a['lift'] * 0.8, 0.008 * a['scale'], 0.06)
    gait('Trot', g['trot'], a['amp'], 0.45, trot, a['lift'], 0.02 * a['scale'], 0.03)
    gait('Gallop', g['gallop'], a['amp'] * 1.35, 0.36, gal, a['lift'] * 1.25, 0.05 * a['scale'], 0.05, spine=0.09)
    if a.get('graze'):
        graze()
    return out


# ---------------------------------------------------------------- жануарлар
def quadruped(name, spec):
    reset()
    m = mat('M_KAP_Animal')
    pieces = []
    pieces.append(loft('Trunk', spec['trunk'], 26))
    pieces.append(loft('Neck', spec['neck'], 20))
    pieces.append(loft('Head', spec['head'], 18))
    for extra in spec.get('extras', []):
        kind, data = extra
        if kind == 'loft':
            pieces.append(loft('X', data, 12))
            if data[0][1] != 0:
                pieces.append(loft('X', mirror_y(data), 12))
        else:
            loc, sc = data
            pieces.append(ellipsoid('E', loc, sc, 14, 9))
            if loc[1] != 0:
                pieces.append(ellipsoid('E', (loc[0], -loc[1], loc[2]), sc, 14, 9))
    bones = {'root': ((0, 0, 0), (0.2 * spec['scale'], 0, 0), None)}
    for bn, (a, b, p) in spec['bones'].items():
        bones[bn] = (a, b, p)
    for side, sg in (('l', 1), ('r', -1)):
        for part, key, parent in (('fore', 'fore', 'chest'), ('hind', 'hind', 'pelvis')):
            pts = [(x, y * sg, z) for (x, y, z) in spec[key]]
            radii = spec[key + "_r"]
            st = [(p[0], p[1], p[2], radii[i] * 1.05, radii[i]) for i, p in enumerate(pts)]
            # аяқтың үсті денеге кіріп тұрсын (тегіс «қақпақ» сыртқа шықпасын)
            x0, y0, z0 = pts[0]
            st = [(x0, y0 * 0.45, z0 + radii[0] * 1.6, radii[0] * 0.55, radii[0] * 0.5)] + st
            pieces.append(loft(part + side, st, 14))
            hx, hy, hz = pts[-1]
            pieces.append(ellipsoid('Hoof', (hx + spec['hoof'][0] * 0.3, hy, hz + spec['hoof'][2] * 0.4), spec['hoof'], 12, 8))
            leg_bones(bones, side, part, pts, parent)
    pieces.append(loft('Tail', spec['tail'], 12))
    if spec.get('wool'):
        # қой жүні: денені бұйра-бұйра «бүршіктер» жабады (тегіс шар емес)
        import random
        rnd = random.Random(7)
        tr = spec['trunk']
        for k in range(spec['wool']):
            j = rnd.uniform(0.6, len(tr) - 1.6)
            a0, b0 = tr[int(j)], tr[int(j) + 1]
            t = j - int(j)
            cx, cz = a0[0] + (b0[0] - a0[0]) * t, a0[2] + (b0[2] - a0[2]) * t
            rv, rl = a0[3] + (b0[3] - a0[3]) * t, a0[4] + (b0[4] - a0[4]) * t
            ang = rnd.uniform(-1.9, 1.9)
            r = rnd.uniform(0.055, 0.085)
            pieces.append(ellipsoid('Wool', (cx, math.sin(ang) * rl * 0.95, cz + math.cos(ang) * rv * 0.95), (r, r * 0.9, r * 0.8), 10, 6))
    body = join(pieces, name + '_Body')
    weld(body, spec['voxel'], spec['tris'])
    paint(body, spec['paint'])
    body.data.materials.clear(); body.data.materials.append(m)
    # көз: жеке кішкентай шарлар (тегістелмейді)
    eyes = []
    ex, ey, ez = spec['eye']
    for s in (1, -1):
        e = ellipsoid('Eye', (ex, ey * s, ez), (spec['eye_r'], spec['eye_r'] * 0.8, spec['eye_r'] * 0.9), 10, 7)
        me = e.data
        ca = me.color_attributes.new(name='Col', type='BYTE_COLOR', domain='CORNER')
        for li in range(len(me.loops)):
            ca.data[li].color = (0, 0, 1, 1)
        uv = me.uv_layers.new(name='UVMap')
        e.data.materials.append(m)
        eyes.append(e)
    eye = join(eyes, name + '_Eyes')
    arm = build_rig(name, bones)
    ok1 = skin(body, arm, bones)
    # көз — тек басқа
    for vg in list(eye.vertex_groups):
        eye.vertex_groups.remove(vg)
    g = eye.vertex_groups.new(name='head'); g.add(list(range(len(eye.data.vertices))), 1.0, 'REPLACE')
    eye.parent = arm
    md = eye.modifiers.new('Skeleton', 'ARMATURE'); md.object = arm
    objs = [body, eye]
    zmax0 = max((body.matrix_world @ v.co).z for v in body.data.vertices)
    pz0 = (arm.matrix_world @ arm.data.bones['pelvis'].head_local).z
    to_cm(arm, objs)
    zmax1 = max((body.matrix_world @ v.co).z for v in body.data.vertices)
    pz1 = (arm.matrix_world @ arm.data.bones['pelvis'].head_local).z
    assert abs(zmax1 - zmax0 * CM) < 0.5 and abs(pz1 - pz0 * CM) < 0.5, (name, zmax0, zmax1, pz0, pz1)
    print('KAPANIMAL %s to_cm: top %.3f m -> %.1f cm, pelvis %.3f m -> %.1f cm' % (name, zmax0, zmax1, pz0, pz1))
    export(arm, objs, 'SKM_KAP_' + name, False)
    tri = sum(len(p.vertices) - 2 for o in objs for p in o.data.polygons)
    anims = animate(arm, name, {'leg': spec['leg'], 'amp': spec['amp'], 'lift': spec['lift'], 'scale': spec['scale'], 'gaits': spec['gaits'], 'graze': spec.get('graze'), 'objs': objs})
    REPORT[name] = {'triangles': tri, 'bones': len(bones), 'heat_weights': ok1, 'anims': anims, 'height': spec['height'], 'length': spec['length']}
    print('KAPANIMAL', name, tri, 'tris', len(bones), 'bones heat=', ok1)


def mkbones(tx):
    """tx: негізгі нүктелер (x, z): pelvis0, spine0, chest0, chest1, neck mid, head0, head tip, tail0, tail1, tail2"""
    P = {k: (v[0], 0, v[1]) for k, v in tx.items()}
    return {
        'pelvis': (P['pelvis'], P['spine'], 'root'),
        'spine': (P['spine'], P['chest'], 'pelvis'),
        'chest': (P['chest'], P['withers'], 'spine'),
        'neck1': (P['withers'], P['neckmid'], 'chest'),
        'neck2': (P['neckmid'], P['poll'], 'neck1'),
        'head': (P['poll'], P['muzzle'], 'neck2'),
        'tail1': (P['tail0'], P['tail1'], 'pelvis'),
        'tail2': (P['tail1'], P['tail2'], 'tail1'),
    }


def hsv_noise(v, s=6.0):
    return noise.noise(Vector((v.x * s, v.y * s, v.z * s))) * 0.5 + 0.5


# ---- жылқы (қазақы жылқы: шомбал, шоқтығы 1.42 м) ----
def horse_paint(v):
    hoof = 1.0 if v.z < 0.07 else 0.0
    nose = smooth01(1.28, 1.36, v.x) if v.z > 1.2 else 0.0
    mane = 1.0 if (v.z > 1.36 and abs(v.y) < 0.05 and 0.42 < v.x < 0.98 and v.z > 1.2 + (v.x - 0.4) * 1.1) else 0.0
    legs = smooth01(0.5, 0.35, v.z)                              # аяқтың төменгі жағы қарадан (қоңыр жылқы)
    tail = 1.0 if v.x < -0.8 and v.z < 1.2 else 0.0
    return (max(legs, mane, tail), max(hoof, nose * 0.8), 0.0)


HORSE = dict(
    height=1.42, length=2.35, scale=1.0, voxel=0.018, tris=7000, leg=1.0, amp=0.52, lift=0.95,
    gaits=dict(walk=40, trot=26, gallop=20), graze=(0.75, 0.55, 0.25),
    trunk=[(-0.80, 0, 1.10, 0.14, 0.12), (-0.66, 0, 1.08, 0.30, 0.22), (-0.40, 0, 1.07, 0.33, 0.26), (-0.10, 0, 1.06, 0.34, 0.28),
           (0.22, 0, 1.08, 0.34, 0.26), (0.46, 0, 1.13, 0.29, 0.21), (0.62, 0, 1.12, 0.18, 0.15)],
    neck=[(0.45, 0, 1.24, 0.26, 0.17), (0.64, 0, 1.44, 0.20, 0.12), (0.80, 0, 1.62, 0.16, 0.10), (0.91, 0, 1.70, 0.13, 0.09)],
    head=[(0.89, 0, 1.71, 0.13, 0.095), (1.04, 0, 1.62, 0.12, 0.09), (1.22, 0, 1.46, 0.085, 0.072), (1.34, 0, 1.35, 0.07, 0.064), (1.38, 0, 1.31, 0.04, 0.05)],
    extras=[('loft', [(0.93, 0.055, 1.79, 0.03, 0.02), (0.92, 0.065, 1.88, 0.018, 0.012), (0.91, 0.07, 1.95, 0.003, 0.003)]),   # құлақ
            ('loft', [(0.46, 0, 1.47, 0.09, 0.04), (0.64, 0, 1.66, 0.1, 0.042), (0.84, 0, 1.81, 0.07, 0.035), (0.95, 0, 1.84, 0.025, 0.02)]),  # жал
            ('ell', ((0.98, 0.0, 1.60), (0.13, 0.095, 0.12))),
            ('ell', ((0.42, 0.12, 1.17), (0.17, 0.09, 0.2))), ('ell', ((-0.58, 0.11, 1.12), (0.22, 0.12, 0.2))),   # жауырын, сауыр бұлшықеттері
            ('ell', ((0.55, 0.08, 0.98), (0.12, 0.09, 0.14))), ('ell', ((0.45, 0.155, 0.68), (0.07, 0.055, 0.13))),  # төс, білек
            ('ell', ((-0.57, 0.165, 0.68), (0.085, 0.06, 0.14))), ('ell', ((0.99, 0.06, 1.6), (0.1, 0.045, 0.085)))],  # сирақ, жақ
    fore=[(0.48, 0.17, 1.05), (0.44, 0.16, 0.78), (0.46, 0.15, 0.46), (0.47, 0.15, 0.15), (0.52, 0.15, 0.045)],
    fore_r=[0.11, 0.075, 0.048, 0.042, 0.045],
    hind=[(-0.62, 0.18, 1.08), (-0.47, 0.19, 0.80), (-0.72, 0.16, 0.50), (-0.68, 0.15, 0.15), (-0.63, 0.15, 0.045)],
    hind_r=[0.16, 0.095, 0.05, 0.043, 0.045],
    hoof=(0.075, 0.062, 0.05),
    tail=[(-0.82, 0, 1.24, 0.06, 0.05), (-0.95, 0, 1.02, 0.085, 0.06), (-1.0, 0, 0.72, 0.075, 0.05), (-0.98, 0, 0.46, 0.04, 0.03), (-0.96, 0, 0.40, 0.01, 0.01)],
    eye=(1.0, 0.105, 1.70), eye_r=0.022,
    paint=horse_paint,
    bones=mkbones(dict(pelvis=(-0.62, 1.10), spine=(-0.15, 1.08), chest=(0.30, 1.12), withers=(0.48, 1.22), neckmid=(0.70, 1.50), poll=(0.91, 1.72), muzzle=(1.36, 1.34),
                       tail0=(-0.82, 1.22), tail1=(-0.97, 0.90), tail2=(-0.97, 0.45))))


# ---- сиыр (қазақтың ақбас сиыры: қызыл-қоңыр, басы мен құрсағы ақ) ----
def cow_paint(v):
    head = smooth01(0.82, 0.92, v.x)
    belly = smooth01(0.66, 0.58, v.z) * smooth01(-0.7, -0.5, v.x) * smooth01(0.55, 0.4, v.x)
    legs = smooth01(0.32, 0.2, v.z)
    tuft = 1.0 if v.x < -0.85 and v.z < 0.45 else 0.0
    horn = 1.0 if v.z > 1.2 and abs(v.y) > 0.1 and v.x > 0.88 else 0.0
    hoof = 1.0 if v.z < 0.055 else 0.0
    nose = smooth01(1.17, 1.22, v.x) * 0.9
    return (max(head, belly, legs * 0.8, tuft * 0.0), max(horn, hoof, nose), 0.0)


COW = dict(
    height=1.30, length=2.3, scale=1.0, voxel=0.018, tris=7000, leg=0.85, amp=0.45, lift=0.7,
    gaits=dict(walk=44, trot=28, gallop=22), graze=(0.45, 0.35, 0.3),
    trunk=[(-0.85, 0, 0.98, 0.18, 0.16), (-0.70, 0, 0.96, 0.34, 0.27), (-0.40, 0, 0.92, 0.39, 0.33), (-0.05, 0, 0.91, 0.40, 0.34),
           (0.30, 0, 0.94, 0.38, 0.31), (0.55, 0, 1.0, 0.29, 0.24), (0.68, 0, 0.98, 0.2, 0.17)],
    neck=[(0.55, 0, 1.04, 0.27, 0.2), (0.75, 0, 1.08, 0.22, 0.16), (0.88, 0, 1.11, 0.19, 0.14)],
    head=[(0.87, 0, 1.12, 0.17, 0.13), (1.0, 0, 1.06, 0.15, 0.125), (1.13, 0, 0.92, 0.12, 0.11), (1.21, 0, 0.84, 0.10, 0.10), (1.24, 0, 0.81, 0.06, 0.07)],
    extras=[('loft', [(0.93, 0.10, 1.24, 0.03, 0.03), (0.95, 0.19, 1.29, 0.022, 0.022), (0.99, 0.24, 1.37, 0.008, 0.008)]),   # мүйіз
            ('loft', [(0.91, 0.12, 1.17, 0.045, 0.02), (0.89, 0.25, 1.14, 0.03, 0.012), (0.88, 0.29, 1.13, 0.005, 0.005)]),   # құлақ
            ('ell', ((0.72, 0.0, 0.84), (0.2, 0.07, 0.17))),                                                                # салпаң қабақ (dewlap)
            ('ell', ((-0.45, 0.0, 0.56), (0.15, 0.13, 0.09))),
            ('ell', ((0.45, 0.17, 1.0), (0.2, 0.1, 0.22))), ('ell', ((-0.64, 0.19, 1.16), (0.07, 0.07, 0.06))),   # жауырын, жамбас сүйегі
            ('ell', ((-0.6, 0.12, 1.05), (0.22, 0.13, 0.2)))],                                                              # желін
    fore=[(0.55, 0.2, 0.85), (0.52, 0.19, 0.58), (0.54, 0.18, 0.33), (0.55, 0.18, 0.12), (0.58, 0.18, 0.035)],
    fore_r=[0.12, 0.08, 0.055, 0.05, 0.05],
    hind=[(-0.68, 0.21, 0.95), (-0.55, 0.22, 0.64), (-0.76, 0.19, 0.40), (-0.72, 0.18, 0.12), (-0.69, 0.18, 0.035)],
    hind_r=[0.17, 0.10, 0.055, 0.05, 0.05],
    hoof=(0.075, 0.07, 0.045),
    tail=[(-0.86, 0, 1.04, 0.04, 0.035), (-0.92, 0, 0.76, 0.03, 0.028), (-0.92, 0, 0.46, 0.025, 0.022), (-0.91, 0, 0.32, 0.055, 0.045), (-0.9, 0, 0.26, 0.01, 0.01)],
    eye=(1.0, 0.13, 1.1), eye_r=0.024,
    paint=cow_paint,
    bones=mkbones(dict(pelvis=(-0.68, 0.98), spine=(-0.15, 0.94), chest=(0.32, 0.96), withers=(0.55, 1.05), neckmid=(0.74, 1.09), poll=(0.88, 1.13), muzzle=(1.23, 0.82),
                       tail0=(-0.86, 1.02), tail1=(-0.92, 0.70), tail2=(-0.91, 0.30))))


# ---- ешкі ----
def goat_paint(v):
    horn = 1.0 if v.z > 0.9 and v.x < 0.45 else 0.0
    hoof = 1.0 if v.z < 0.03 else 0.0
    beard = 1.0 if v.z < 0.72 and v.x > 0.5 else 0.0
    nose = smooth01(0.57, 0.61, v.x) * 0.8
    dark = smooth01(0.3, 0.15, v.z) * 0.7
    return (max(beard, dark), max(horn, hoof, nose), 0.0)


GOAT = dict(
    height=0.70, length=1.15, scale=0.5, voxel=0.0095, tris=5000, leg=0.48, amp=0.5, lift=0.9,
    gaits=dict(walk=30, trot=20, gallop=15), graze=(0.85, 0.55, 0.25),
    trunk=[(-0.40, 0, 0.58, 0.09, 0.08), (-0.32, 0, 0.56, 0.17, 0.13), (-0.15, 0, 0.54, 0.19, 0.15), (0.05, 0, 0.54, 0.2, 0.15), (0.22, 0, 0.56, 0.18, 0.13), (0.32, 0, 0.58, 0.12, 0.1)],
    neck=[(0.24, 0, 0.62, 0.12, 0.08), (0.34, 0, 0.75, 0.09, 0.065), (0.40, 0, 0.84, 0.08, 0.06)],
    head=[(0.40, 0, 0.86, 0.08, 0.06), (0.47, 0, 0.83, 0.07, 0.055), (0.56, 0, 0.76, 0.05, 0.045), (0.60, 0, 0.73, 0.035, 0.035), (0.62, 0, 0.71, 0.015, 0.02)],
    extras=[('loft', [(0.43, 0.03, 0.91, 0.02, 0.02), (0.40, 0.05, 0.99, 0.016, 0.016), (0.34, 0.07, 1.04, 0.011, 0.011), (0.28, 0.08, 1.03, 0.004, 0.004)]),  # мүйіз
            ('loft', [(0.42, 0.05, 0.87, 0.025, 0.012), (0.42, 0.13, 0.85, 0.018, 0.008), (0.42, 0.155, 0.845, 0.003, 0.003)]),  # құлақ
            ('loft', [(0.55, 0, 0.72, 0.02, 0.018), (0.54, 0, 0.66, 0.015, 0.012), (0.53, 0, 0.62, 0.004, 0.004)])],              # сақал
    fore=[(0.24, 0.09, 0.45), (0.22, 0.09, 0.30), (0.23, 0.08, 0.17), (0.24, 0.08, 0.06), (0.26, 0.08, 0.02)],
    fore_r=[0.05, 0.035, 0.025, 0.022, 0.022],
    hind=[(-0.32, 0.10, 0.52), (-0.25, 0.11, 0.36), (-0.36, 0.09, 0.22), (-0.34, 0.08, 0.06), (-0.32, 0.08, 0.02)],
    hind_r=[0.07, 0.045, 0.025, 0.022, 0.022],
    hoof=(0.032, 0.026, 0.02),
    tail=[(-0.40, 0, 0.6, 0.03, 0.025), (-0.43, 0, 0.66, 0.025, 0.02), (-0.44, 0, 0.69, 0.005, 0.005)],
    eye=(0.47, 0.055, 0.87), eye_r=0.012,
    paint=goat_paint,
    bones=mkbones(dict(pelvis=(-0.32, 0.58), spine=(-0.05, 0.55), chest=(0.2, 0.57), withers=(0.26, 0.63), neckmid=(0.34, 0.76), poll=(0.41, 0.87), muzzle=(0.61, 0.72),
                       tail0=(-0.40, 0.6), tail1=(-0.43, 0.66), tail2=(-0.44, 0.7))))


# ---- қазақтың құйрықты қойы (еділбай): жүні жуан, басы мен аяғы қара-қоңыр, құйрығы майлы ----
def sheep_paint(v):
    head = smooth01(0.42, 0.48, v.x) if v.z > 0.68 else 0.0
    legs = smooth01(0.36, 0.28, v.z)
    hoof = 1.0 if v.z < 0.03 else 0.0
    nose = smooth01(0.6, 0.63, v.x) * 0.6
    return (max(head, legs), max(hoof, nose), 0.0)


SHEEP = dict(
    height=0.80, length=1.2, scale=0.55, voxel=0.01, tris=5000, leg=0.42, amp=0.48, wool=46, lift=0.75,
    gaits=dict(walk=30, trot=20, gallop=16), graze=(0.8, 0.5, 0.3),
    trunk=[(-0.45, 0, 0.62, 0.15, 0.15), (-0.36, 0, 0.62, 0.25, 0.24), (-0.15, 0, 0.60, 0.28, 0.27), (0.08, 0, 0.61, 0.28, 0.26), (0.26, 0, 0.63, 0.24, 0.22), (0.36, 0, 0.66, 0.15, 0.15)],
    neck=[(0.30, 0, 0.70, 0.14, 0.12), (0.40, 0, 0.80, 0.10, 0.08), (0.45, 0, 0.85, 0.08, 0.07)],
    head=[(0.44, 0, 0.86, 0.08, 0.065), (0.52, 0, 0.84, 0.07, 0.055), (0.60, 0, 0.77, 0.05, 0.045), (0.63, 0, 0.74, 0.035, 0.035), (0.645, 0, 0.72, 0.015, 0.02)],
    extras=[('ell', ((-0.47, 0.085, 0.50), (0.10, 0.09, 0.14))),                                                           # майлы құйрық (екі бөлік)
            ('loft', [(0.47, 0.05, 0.88, 0.025, 0.012), (0.46, 0.12, 0.83, 0.02, 0.008), (0.455, 0.14, 0.81, 0.004, 0.004)])],   # салбыраған құлақ
    fore=[(0.26, 0.10, 0.45), (0.25, 0.10, 0.32), (0.26, 0.09, 0.18), (0.27, 0.09, 0.06), (0.29, 0.09, 0.02)],
    fore_r=[0.045, 0.03, 0.022, 0.02, 0.02],
    hind=[(-0.34, 0.11, 0.50), (-0.27, 0.12, 0.36), (-0.38, 0.10, 0.22), (-0.36, 0.09, 0.06), (-0.34, 0.09, 0.02)],
    hind_r=[0.06, 0.04, 0.022, 0.02, 0.02],
    hoof=(0.03, 0.025, 0.02),
    tail=[(-0.46, 0, 0.56, 0.05, 0.04), (-0.5, 0, 0.44, 0.04, 0.035), (-0.5, 0, 0.36, 0.01, 0.01)],
    eye=(0.5, 0.055, 0.87), eye_r=0.012,
    paint=sheep_paint,
    bones=mkbones(dict(pelvis=(-0.34, 0.62), spine=(-0.05, 0.6), chest=(0.22, 0.62), withers=(0.3, 0.7), neckmid=(0.4, 0.8), poll=(0.45, 0.87), muzzle=(0.64, 0.73),
                       tail0=(-0.46, 0.58), tail1=(-0.5, 0.44), tail2=(-0.5, 0.34))))


# ---- түлкі ----
def fox_paint(v):
    white = 0.0
    if v.z < 0.27 and 0.0 < v.x < 0.3:
        white = smooth01(0.28, 0.2, v.z)
    if v.x > 0.36 and v.z < 0.4:
        white = max(white, 0.9)
    if v.x < -0.62:
        white = 1.0                                           # құйрық ұшы — ақ
    legs = smooth01(0.16, 0.08, v.z)
    ears = 1.0 if v.z > 0.52 else 0.0
    nose = 1.0 if v.x > 0.44 else 0.0
    return (white, max(legs * 0.85, ears * 0.7, nose), 0.0)


FOX = dict(
    height=0.38, length=1.15, scale=0.3, voxel=0.0065, tris=4000, leg=0.27, amp=0.55, lift=0.85,
    gaits=dict(walk=22, trot=14, gallop=11), graze=None,
    trunk=[(-0.30, 0, 0.31, 0.06, 0.055), (-0.22, 0, 0.30, 0.09, 0.08), (-0.05, 0, 0.30, 0.1, 0.085), (0.12, 0, 0.31, 0.1, 0.08), (0.22, 0, 0.33, 0.08, 0.07)],
    neck=[(0.18, 0, 0.34, 0.08, 0.06), (0.27, 0, 0.40, 0.065, 0.05)],
    head=[(0.27, 0, 0.42, 0.07, 0.055), (0.33, 0, 0.42, 0.055, 0.05), (0.40, 0, 0.395, 0.03, 0.03), (0.45, 0, 0.38, 0.012, 0.012)],
    extras=[('loft', [(0.29, 0.035, 0.47, 0.03, 0.012), (0.285, 0.045, 0.52, 0.018, 0.007), (0.28, 0.05, 0.56, 0.002, 0.002)])],  # құлақ
    fore=[(0.15, 0.05, 0.27), (0.15, 0.05, 0.16), (0.16, 0.045, 0.06), (0.17, 0.045, 0.02), (0.185, 0.045, 0.008)],
    fore_r=[0.03, 0.02, 0.014, 0.013, 0.014],
    hind=[(-0.22, 0.055, 0.30), (-0.16, 0.06, 0.18), (-0.25, 0.05, 0.08), (-0.23, 0.045, 0.02), (-0.21, 0.045, 0.008)],
    hind_r=[0.045, 0.028, 0.015, 0.013, 0.014],
    hoof=(0.02, 0.015, 0.009),
    tail=[(-0.30, 0, 0.33, 0.04, 0.035), (-0.42, 0, 0.27, 0.07, 0.065), (-0.58, 0, 0.20, 0.065, 0.06), (-0.70, 0, 0.17, 0.03, 0.03), (-0.73, 0, 0.165, 0.005, 0.005)],
    eye=(0.33, 0.04, 0.445), eye_r=0.008,
    paint=fox_paint,
    bones=mkbones(dict(pelvis=(-0.22, 0.31), spine=(-0.02, 0.3), chest=(0.14, 0.32), withers=(0.18, 0.35), neckmid=(0.24, 0.39), poll=(0.28, 0.43), muzzle=(0.45, 0.38),
                       tail0=(-0.3, 0.33), tail1=(-0.5, 0.24), tail2=(-0.72, 0.17))))


for nm, sp in (('Horse', HORSE), ('Cow', COW), ('Goat', GOAT), ('Sheep', SHEEP), ('Fox', FOX)):
    quadruped(nm, sp)

(OUT / 'animals.json').write_text(json.dumps(REPORT, indent=2, ensure_ascii=False), encoding='utf-8')
print('KAPANIMAL DONE', list(REPORT.keys()))
