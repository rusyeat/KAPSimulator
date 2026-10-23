"""SourceArt/AnimalsV1 (Blender, build_animals_v1.py) → /Game/KAP/Animals/V1: қаңқалы мештер, физика (рагдолл), қашықтық LOD-тары, анимациялар.
Материал — M_KAP_Animal (KAPEditorLibrary.create_animal_material)."""
import unreal, json, traceback
from pathlib import Path
SRC = Path('D:/KAPSimulator/SourceArt/AnimalsV1')
DEST = '/Game/KAP/Animals/V1'
REPORT = Path('D:/KAPSimulator/Saved/Tmp/animals_import.json')
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ROWS = []


def task(path, name, skeleton=None, anim=False):
    t = unreal.AssetImportTask()
    for p, v in dict(filename=str(path), destination_path=DEST, destination_name=name, automated=True, replace_existing=True, save=False, factory=unreal.FbxFactory()).items():
        t.set_editor_property(p, v)
    ui = unreal.FbxImportUI()
    for p, v in dict(import_as_skeletal=True, mesh_type_to_import=unreal.FBXImportType.FBXIT_ANIMATION if anim else unreal.FBXImportType.FBXIT_SKELETAL_MESH,
                     automated_import_should_detect_type=False, import_mesh=not anim, import_animations=anim, import_materials=False, import_textures=False,
                     create_physics_asset=not anim, skeleton=skeleton).items():
        ui.set_editor_property(p, v)
    sk = ui.get_editor_property('skeletal_mesh_import_data')
    sk.set_editor_property('update_skeleton_reference_pose', not anim)
    sk.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    sk.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    if anim:
        ui.set_editor_property('override_animation_name', name)
        ui.get_editor_property('anim_sequence_import_data').set_editor_property('import_meshes_in_bone_hierarchy', False)
    t.set_editor_property('options', ui)
    TOOLS.import_asset_tasks([t])
    return t


try:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
    mat = unreal.load_asset('/Game/KAP/Materials/M_KAP_Animal')
    assert mat, 'M_KAP_Animal жоқ'
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    for nm in ('Horse', 'Cow', 'Goat', 'Sheep', 'Fox'):
        name = 'SKM_KAP_' + nm
        task(SRC / (name + '.fbx'), name)
        mesh = unreal.load_asset(DEST + '/' + name)
        assert isinstance(mesh, unreal.SkeletalMesh), name
        slots = mesh.get_editor_property('materials')
        for i, s in enumerate(slots):
            s.set_editor_property('material_interface', mat)
            slots[i] = s
        mesh.set_editor_property('materials', slots)
        try:
            sub.regenerate_lod(mesh, 3, False, False)
        except Exception as e:
            unreal.log_warning('KAPANIMAL LOD: %s' % e)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        b = mesh.get_bounds()
        skel = mesh.get_editor_property('skeleton')
        anims = []
        for an in ('Idle', 'Walk', 'Trot', 'Gallop', 'Graze'):
            f = SRC / ('AN_KAP_%s_%s.fbx' % (nm, an))
            if not f.exists():
                continue
            aname = 'AN_KAP_%s_%s' % (nm, an)
            t = task(f, aname, skel, True)
            seqs = [unreal.load_asset(p) for p in t.get_editor_property('imported_object_paths')]
            seq = next((s for s in seqs if isinstance(s, unreal.AnimSequence)), None)
            assert seq, (nm, an)
            target = DEST + '/' + aname
            if seq.get_path_name().split('.')[0] != target:
                if unreal.EditorAssetLibrary.does_asset_exist(target):
                    unreal.EditorAssetLibrary.delete_asset(target)
                unreal.EditorAssetLibrary.rename_loaded_asset(seq, target)
            unreal.EditorAssetLibrary.save_loaded_asset(seq)
            anims.append(an)
        lods = sub.get_lod_count(mesh)
        ROWS.append(dict(animal=nm, mesh=mesh.get_path_name(), lods=lods, anims=anims, origin=[b.origin.x, b.origin.y, b.origin.z], extent=[b.box_extent.x, b.box_extent.y, b.box_extent.z],
                         physics=str(mesh.get_editor_property('physics_asset'))))
        unreal.log('KAPANIMAL imported %s lods=%d anims=%s extent=(%.0f %.0f %.0f)' % (nm, lods, anims, b.box_extent.x, b.box_extent.y, b.box_extent.z))
    unreal.EditorAssetLibrary.save_directory(DEST, only_if_is_dirty=True, recursive=True)
    REPORT.write_text(json.dumps(dict(status='done', rows=ROWS), indent=2))
    unreal.log('KAPANIMAL IMPORT DONE')
except Exception:
    REPORT.write_text(json.dumps(dict(status='failed', error=traceback.format_exc(), rows=ROWS), indent=2))
    unreal.log_error(traceback.format_exc())
    raise
