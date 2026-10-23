"""SourceArt/AnimalsV1/SKM_KAP_*.fbx модельдерін бүйірінен және 3/4 қырынан суретке түсіру (Workbench, вершина түсі)"""
import bpy, math
from pathlib import Path
from mathutils import Vector
SRC = Path('D:/KAPSimulator/SourceArt/AnimalsV1')
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete()
x = 0.0
for nm in ('Horse', 'Cow', 'Goat', 'Sheep', 'Fox'):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(SRC / ('SKM_KAP_%s.fbx' % nm)))
    new = [o for o in bpy.data.objects if o not in before]
    for o in new:
        if o.parent is None:
            o.location.y += x
    x += 2.6 if nm in ('Horse', 'Cow') else 1.5
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sc.display.shading.light = 'STUDIO'
sc.display.shading.color_type = 'VERTEX'
sc.display.shading.show_shadows = True
sc.render.resolution_x = 1800; sc.render.resolution_y = 700
cam = bpy.data.objects.new('Cam', bpy.data.cameras.new('Cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = 10.5
for name, loc, rot in (('side', (4.6, -12, 1.0), (math.radians(88), 0, 0)), ('quarter', (8, -3.5, 3.0), (math.radians(72), 0, math.radians(60)))):
    cam.location = loc; cam.rotation_euler = rot
    sc.render.filepath = str(SRC / ('preview_%s.png' % name))
    bpy.ops.render.render(write_still=True)
print('KAPANIMAL PREVIEW DONE')
