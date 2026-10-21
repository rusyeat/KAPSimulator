"""Run with Unreal Editor's Python console. Imports new assets only."""
import unreal,json,traceback
from pathlib import Path
BASE=Path(unreal.Paths.project_dir())
OUT=BASE/'SourceArt/CharactersV3'
REPORT=BASE/'Saved/CharacterV3ImportReport.json'
DEST='/Game/KAP/Characters/V3'
records=[]
def report(status,**kw):REPORT.write_text(json.dumps(dict(status=status,models=records,**kw),indent=2))
def run():
    assets=unreal.AssetToolsHelpers.get_asset_tools()
    lib=unreal.MaterialEditingLibrary
    unreal.EditorAssetLibrary.make_directory(DEST)
    mat=unreal.load_asset(DEST+'/M_KAP_Human3')
    if mat is None:mat=assets.create_asset('M_KAP_Human3',DEST,unreal.Material,unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided',False)
    def expr(kind,x=0,y=0):return lib.create_material_expression(mat,kind,x,y)
    region=expr(unreal.MaterialExpressionTextureCoordinate,-1100,-300);region.set_editor_property('coordinate_index',1)
    surface=expr(unreal.MaterialExpressionTextureCoordinate,-1100,-200)
    node=expr(unreal.MaterialExpressionCustom,-100,0)
    node.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    defaults={'Skin':(.48,.31,.21),'Jacket':(.22,.30,.32),'Pants':(.065,.09,.12),'Boots':(.035,.029,.024),'Hair':(.025,.018,.014),'Glove':(.04,.04,.035),'Vest':(.2,.25,.15),'CamoA':(.16,.18,.1),'CamoB':(.42,.37,.24)}
    scalars={'HairAmt':1.,'Gloves':0.,'Cover':0.,'VestAmt':0.,'Camo':0.,'StripeAmt':0.,'Grime':.4,'Beard':0.}
    names=['RegionUV','SurfaceUV']+list(defaults)+list(scalars)
    custom_inputs=[]
    for name in names:
        ci=unreal.CustomInput();ci.set_editor_property('input_name',name);custom_inputs.append(ci)
    node.set_editor_property('inputs',custom_inputs)
    lib.connect_material_expressions(region,'',node,'RegionUV');lib.connect_material_expressions(surface,'',node,'SurfaceUV')
    for i,(name,col) in enumerate(defaults.items()):
        p=expr(unreal.MaterialExpressionVectorParameter,-850,i*100)
        p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',unreal.LinearColor(*col,1.))
        lib.connect_material_expressions(p,'RGB',node,name)
    for i,(name,value) in enumerate(scalars.items()):
        p=expr(unreal.MaterialExpressionScalarParameter,-600,i*100)
        p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value)
        lib.connect_material_expressions(p,'',node,name)
    node.set_editor_property('code',r'''
int r=(int)floor(RegionUV.x*16.0);
float3 c=Skin;
float2 uv=SurfaceUV;
float n=frac(sin(dot(floor(uv*float2(210,330)),float2(12.9898,78.233)))*43758.5453);
if(r==1)c=Jacket;
else if(r==2)c=Pants;
else if(r==3)c=Boots;
else if(r==4)c=lerp(Skin,Hair,HairAmt);
else if(r==5)c=float3(.55,.52,.46);
else if(r==6)c=float3(.045,.026,.012);
else if(r==7)c=float3(.004,.004,.003);
else if(r==8)c=Skin*float3(.70,.46,.43);
else if(r==9)c=Skin*float3(.64,.53,.52);
else if(r==10)c=Vest;
else if(r==11)c=float3(.026,.029,.026);
else if(r==13)c=lerp(Skin,Glove,Gloves);
if(r==1 || r==2){
 float camoNoise=frac(sin(dot(floor(uv*float2(22,27)),float2(31.23,15.87)))*17437.3);
 float3 cam=camoNoise>.65?CamoB:(camoNoise<.3?CamoA:c);
 c=lerp(c,cam,Camo*.8);
 if(r==1){
  float vestMask=step(abs(uv.x-.5),.29)*step(.29,uv.y)*step(uv.y,.46);
  c=lerp(c,Vest,vestMask*VestAmt);
  float band=(1-step(.006,abs(uv.y-.355)))*(1-step(.34,abs(uv.x-.5)));
  c=lerp(c,float3(.58,.60,.53),saturate(band*StripeAmt));
 }
 c*=.95+.05*n;
 c*=1-Grime*.06;
}
if((r==12 || r==4 || r==8) && Cover>.5)c=Hair;
return max(c,0);
''')
    lib.connect_material_property(node,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=expr(unreal.MaterialExpressionConstant,0,200);rough.set_editor_property('r',.88)
    lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    spec=expr(unreal.MaterialExpressionConstant,0,300);spec.set_editor_property('r',.22)
    lib.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    mat.set_editor_property('used_with_skeletal_mesh',True)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    old=unreal.load_asset('/Game/KAP/Characters/SKM_KAP_BodyM')
    skeleton=old.get_editor_property('skeleton')
    physics=old.get_editor_property('physics_asset')
    subsystem=unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    for variant in ['AdultMale','AdultFemale','Boy','Girl','SickBoy']:
        name='SKM_KAP_V3_'+variant
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(OUT/(name+'.fbx')))
        task.set_editor_property('destination_path',DEST);task.set_editor_property('destination_name',name)
        task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',False)
        task.set_editor_property('factory',unreal.FbxFactory())
        opts=unreal.FbxImportUI();opts.set_editor_property('import_as_skeletal',True)
        opts.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        opts.set_editor_property('automated_import_should_detect_type',False)
        opts.set_editor_property('import_mesh',True);opts.set_editor_property('import_animations',False)
        opts.set_editor_property('import_materials',False);opts.set_editor_property('import_textures',False)
        opts.set_editor_property('create_physics_asset',False);opts.set_editor_property('skeleton',skeleton)
        data=opts.get_editor_property('skeletal_mesh_import_data')
        data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
        data.set_editor_property('import_morph_targets',False)
        data.set_editor_property('update_skeleton_reference_pose',False)
        task.set_editor_property('options',opts)
        assets.import_asset_tasks([task])
        mesh=unreal.load_asset(DEST+'/'+name)
        if not isinstance(mesh,unreal.SkeletalMesh):raise RuntimeError('No skeletal mesh imported: '+name)
        if mesh.get_editor_property('skeleton')!=skeleton:raise RuntimeError('Skeleton mismatch: '+name)
        slots=mesh.get_editor_property('materials')
        for slot in slots:slot.set_editor_property('material_interface',mat)
        mesh.set_editor_property('materials',slots);mesh.set_editor_property('physics_asset',physics)
        lod_ok=subsystem.regenerate_lod(mesh,3,False,False)
        count=subsystem.get_lod_count(mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)
        records.append({'name':name,'path':mesh.get_path_name(),'lods':count,'lod_generated':lod_ok,'vertices':[subsystem.get_num_verts(mesh,i) for i in range(count)],'sections':[subsystem.get_num_sections(mesh,i) for i in range(count)],'skeleton':skeleton.get_path_name()})
        report('importing')
    report('done',material=mat.get_path_name())
try:run()
except Exception:report('error',traceback=traceback.format_exc());raise
