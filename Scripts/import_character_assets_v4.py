"""V4 кейіпкерлерін (V3 киімді дене + MetaHuman басы, Scripts/blender/build_v4_heads.py) импорттау.
Тек /Game/KAP/Characters/V4 жазылады; M_KAP_Human3 материалы (V3) қайта қолданылады.
UnrealEditor-Cmd ... -run=pythonscript -script=Scripts/import_character_assets_v4.py"""
import unreal, json, traceback
from pathlib import Path
BASE = Path(unreal.Paths.project_dir())
SRC = BASE / 'SourceArt/CharactersV4'
DEST = '/Game/KAP/Characters/V4'
REPORT = BASE / 'Saved/CharacterV4ImportReport.json'
records = []
def report(status, **kw): REPORT.write_text(json.dumps(dict(status=status, models=records, **kw), indent=2))
def run():
    assets = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(DEST)
    mat = unreal.load_asset('/Game/KAP/Characters/V3/M_KAP_Human3')
    if mat is None: raise RuntimeError('M_KAP_Human3 missing (run V3 importer first)')
    ref = unreal.load_asset('/Game/KAP/Characters/V3/SKM_KAP_V3_AdultMale')
    skeleton = ref.get_editor_property('skeleton'); physics = ref.get_editor_property('physics_asset')
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
    for variant in ['AdultMale', 'AdultFemale']:
        name = 'SKM_KAP_V4_' + variant
        task = unreal.AssetImportTask(); task.set_editor_property('filename', str(SRC / (name + '.fbx')))
        task.set_editor_property('destination_path', DEST); task.set_editor_property('destination_name', name)
        task.set_editor_property('automated', True); task.set_editor_property('replace_existing', True); task.set_editor_property('save', False)
        task.set_editor_property('factory', unreal.FbxFactory())
        opts = unreal.FbxImportUI(); opts.set_editor_property('import_as_skeletal', True)
        opts.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        opts.set_editor_property('automated_import_should_detect_type', False)
        opts.set_editor_property('import_mesh', True); opts.set_editor_property('import_animations', False)
        opts.set_editor_property('import_materials', False); opts.set_editor_property('import_textures', False)
        opts.set_editor_property('create_physics_asset', False); opts.set_editor_property('skeleton', skeleton)
        data = opts.get_editor_property('skeletal_mesh_import_data')
        data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        data.set_editor_property('import_morph_targets', False)
        data.set_editor_property('update_skeleton_reference_pose', False)
        task.set_editor_property('options', opts)
        assets.import_asset_tasks([task])
        mesh = unreal.load_asset(DEST + '/' + name)
        if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError('No skeletal mesh imported: ' + name)
        if mesh.get_editor_property('skeleton') != skeleton: raise RuntimeError('Skeleton mismatch: ' + name)
        slots = mesh.get_editor_property('materials')
        for slot in slots: slot.set_editor_property('material_interface', mat)
        mesh.set_editor_property('materials', slots); mesh.set_editor_property('physics_asset', physics)
        lod_ok = subsystem.regenerate_lod(mesh, 3, False, False)
        count = subsystem.get_lod_count(mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        b = mesh.get_bounds(); rb = ref.get_bounds()
        records.append({'name': name, 'lods': count, 'lod_generated': lod_ok,
                        'vertices': [subsystem.get_num_verts(mesh, i) for i in range(count)],
                        'bounds_extent': [b.box_extent.x, b.box_extent.y, b.box_extent.z], 'bounds_origin': [b.origin.x, b.origin.y, b.origin.z],
                        'v3_extent': [rb.box_extent.x, rb.box_extent.y, rb.box_extent.z], 'v3_origin': [rb.origin.x, rb.origin.y, rb.origin.z]})
        unreal.log('KAPV4 ' + json.dumps(records[-1]))
        report('importing')
    report('done')
try: run()
except Exception:
    report('error', traceback=traceback.format_exc()); raise
