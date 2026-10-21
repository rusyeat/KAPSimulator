"""M_KAP_Human3 материалын жаңарту (параметр атаулары өзгермейді — бар MID-тер жұмыс істей береді).
Киім: қатпарлар мен мата тоқымасы (pre-skinned кеңістікте — анимацияда «жүзбейді»), қатпар көлеңкесі (AO),
шалбар/етік етегінде шаң, жоғарғы беттер күнге оңған, аймаққа қарай кедір-бұдырлық (тері, мата, былғары, шаш, көз).
Нормаль — биіктіктің экран туындыларынан (Mikkelsen bump), әлем кеңістігінде."""
import unreal, traceback
from pathlib import Path
REPORT = Path(unreal.Paths.project_dir()) / 'Saved/HumanMaterialV4.txt'
DEST = '/Game/KAP/Characters/V3'

HLSL = r'''
struct FKH {
  float h3(float3 p){ p=frac(p*0.3183099+0.1); p*=17.0; return frac(p.x*p.y*p.z*(p.x+p.y+p.z)); }
  float vn(float3 x){ float3 i=floor(x), f=frac(x); f=f*f*(3.0-2.0*f);
    return lerp(lerp(lerp(h3(i),h3(i+float3(1,0,0)),f.x), lerp(h3(i+float3(0,1,0)),h3(i+float3(1,1,0)),f.x),f.y),
                lerp(lerp(h3(i+float3(0,0,1)),h3(i+float3(1,0,1)),f.x), lerp(h3(i+float3(0,1,1)),h3(i+float3(1,1,1)),f.x),f.y),f.z); }
};
FKH K;
int r=(int)floor(RegionUV.x*16.0);
float3 P=LP;
float dist=length(RP);
float2 uv=SurfaceUV;
float3 c=Skin;
float rough=0.52, ao=1.0, h=0.0;
bool fabric=(r==1||r==2||r==10);
bool leather=(r==3||r==11||(r==13&&Gloves>0.5));
// ---- негізгі түстер (бұрынғы логика) ----
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
if(r==1||r==2){
  float camoNoise=K.vn(P*float3(0.09,0.09,0.07));
  float3 cam=camoNoise>.62?CamoB:(camoNoise<.38?CamoA:c);
  c=lerp(c,cam,Camo*.85);
  if(r==1){
    float vestMask=step(abs(uv.x-.5),.29)*step(.29,uv.y)*step(uv.y,.46);
    c=lerp(c,Vest,vestMask*VestAmt);
    float band=(1-step(.006,abs(uv.y-.355)))*(1-step(.34,abs(uv.x-.5)));
    c=lerp(c,float3(.58,.60,.53),saturate(band*StripeAmt));
  }
}
float nearK=saturate(1.0-dist/900.0);   // жақын деталь (тоқыма) — алыста сөнеді
float midK=saturate(1.0-dist/4000.0);
if(fabric){
  // қатпарлар: көлденең (аяқ-қол бойымен), ирелеңдеген, ~7 см қадам; кей жерде қалың
  // ирелең қатпарлар (ridged noise): көлденеңінен созылған, тек буындарда (тізе, тобық, бел, жең) қалыңырақ
  float rn=K.vn(P*float3(0.045,0.045,0.16)+float3(0,0,K.vn(P*0.03)*4.0));
  float fold=pow(1.0-abs(2.0*rn-1.0),3.0);
  float joint=(r==2)?(exp(-pow((P.z-50.0)/9.0,2.0))+0.8*exp(-pow((P.z-17.0)/7.0,2.0))+0.5*exp(-pow((P.z-88.0)/7.0,2.0))):
              (0.7*exp(-pow((P.z-100.0)/7.0,2.0))+0.9*saturate((abs(P.x)-24.0)/10.0));
  float amp=(0.18+0.82*saturate(joint))*(0.55+0.45*K.vn(P*0.05+3.0));
  h+=fold*0.28*amp*midK;
  // тоқыма: диагональ (твил), 2.8 мм
  float tw=sin(6.2831*(P.x+P.y*0.7+P.z)/0.28)*0.5+0.5;
  h+=tw*0.018*saturate(1.0-dist/250.0);
  c*=lerp(0.92,1.02,1.0-fold*amp);
  ao=lerp(0.82,1.0,1.0-fold*amp);
  c*=0.93+0.12*K.vn(P*0.05+11.0);
  // матаның түсі: шалбар — джинса твилі (диагональ жіптер), күрте — меланж тоқыма; кейде дақтар
  float closeK=saturate(1.0-dist/250.0); // 2.8 мм жіп — тек жақын жерде (әйтпесе муар)
  if(r==2) c*=lerp(1.0,0.9+0.16*tw,closeK);
  else c*=lerp(1.0,0.93+0.12*K.vn(P*float3(2.6,2.6,3.4)),saturate(1.0-dist/350.0));
  float stain=saturate((K.vn(P*0.07+31.0)-0.72)*5.0);
  c*=1.0-0.16*stain*(0.5+Grime);
  c*=1.0+saturate(NW.z)*0.07;           // күнге оңған иық пен жамбас
  rough=(r==2)?0.86:((r==10)?0.62:0.8);
  // шаң: шалбар етегі мен тізе, күртенің етегі
  float dz=(r==2)?saturate((44.0-P.z)/38.0):saturate((92.0-P.z)/16.0)*0.35;
  float dust=dz*(0.5+0.5*K.vn(P*0.13))*(0.6+Grime);
  c=lerp(c,float3(.5,.41,.29),saturate(dust*0.75));
  rough=lerp(rough,0.95,saturate(dust));
}
else if(leather){
  float grain=K.vn(P*2.2);
  h+=grain*0.02*nearK;
  float scuff=saturate((K.vn(P*0.4+5.0)-0.62)*4.0);
  c=lerp(c,c*1.8+0.02,scuff*0.5);
  rough=lerp(0.48,0.72,scuff);
  if(r==3){ float dust=saturate((14.0-P.z)/12.0)*(0.5+0.5*K.vn(P*0.2)); c=lerp(c,float3(.45,.37,.27),dust*0.7); rough=lerp(rough,0.92,dust); }
}
else if(r==4){
  // шаш: тік талшықтар, түбі қоюырақ
  float strand=K.vn(float3(P.x*3.0,P.y*3.0,P.z*0.25));
  h+=strand*0.05*nearK;
  c*=0.8+0.35*strand;
  rough=0.45;
}
else if(r==5||r==6||r==7){ rough=0.12; }
else if(r==8){ rough=0.4; }
else {
  // тері: кеуек, сәл қызғылт/сарғыш дақтар
  float pore=K.vn(P*1.6);
  h+=pore*0.006*nearK;
  c*=0.95+0.08*K.vn(P*0.12+21.0);
  rough=0.5+0.08*pore;
}
if((r==12||r==4||r==8)&&Cover>.5){ c=Hair; rough=0.6; }
// ---- биіктіктен нормаль (экран туындылары, әлем кеңістігі) ----
float3 N=normalize(NW);
float3 dpdx=ddx(RP), dpdy=ddy(RP);
float dhdx=ddx(h), dhdy=ddy(h);
float3 r1=cross(dpdy,N), r2=cross(N,dpdx);
float det=dot(dpdx,r1);
float3 grad=sign(det)*(dhdx*r1+dhdy*r2);
float3 Nb=normalize(abs(det)*N-grad);
if(!(abs(det)>1e-12))Nb=N;
Rough=rough; AO=ao; Nrm=Nb;
return max(c,0);
'''


def run():
    lib = unreal.MaterialEditingLibrary
    mat = unreal.load_asset(DEST + '/M_KAP_Human3')
    if mat is None:
        raise RuntimeError('M_KAP_Human3 not found')
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('two_sided', False)
    mat.set_editor_property('tangent_space_normal', False)

    def expr(kind, x=0, y=0):
        return lib.create_material_expression(mat, kind, x, y)
    region = expr(unreal.MaterialExpressionTextureCoordinate, -1200, -400); region.set_editor_property('coordinate_index', 1)
    surface = expr(unreal.MaterialExpressionTextureCoordinate, -1200, -300)
    lp = expr(unreal.MaterialExpressionPreSkinnedPosition, -1200, -200)
    wp = expr(unreal.MaterialExpressionWorldPosition, -1200, -100)
    try:
        wp.set_editor_property('world_position_shader_offset', unreal.WorldPositionIncludedOffsets.WPT_CAMERA_RELATIVE)
    except Exception as e:
        REPORT.write_text('warn: camera relative failed ' + str(e))
    nw = expr(unreal.MaterialExpressionVertexNormalWS, -1200, 0)
    node = expr(unreal.MaterialExpressionCustom, -100, 0)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    defaults = {'Skin': (.48, .31, .21), 'Jacket': (.22, .30, .32), 'Pants': (.065, .09, .12), 'Boots': (.035, .029, .024), 'Hair': (.025, .018, .014), 'Glove': (.04, .04, .035), 'Vest': (.2, .25, .15), 'CamoA': (.16, .18, .1), 'CamoB': (.42, .37, .24)}
    scalars = {'HairAmt': 1., 'Gloves': 0., 'Cover': 0., 'VestAmt': 0., 'Camo': 0., 'StripeAmt': 0., 'Grime': .4, 'Beard': 0.}
    names = ['RegionUV', 'SurfaceUV', 'LP', 'RP', 'NW'] + list(defaults) + list(scalars)
    ins = []
    for n in names:
        ci = unreal.CustomInput(); ci.set_editor_property('input_name', n); ins.append(ci)
    node.set_editor_property('inputs', ins)
    outs = []
    for n, t in (('Rough', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('AO', unreal.CustomMaterialOutputType.CMOT_FLOAT1), ('Nrm', unreal.CustomMaterialOutputType.CMOT_FLOAT3)):
        co = unreal.CustomOutput(); co.set_editor_property('output_name', n); co.set_editor_property('output_type', t); outs.append(co)
    node.set_editor_property('additional_outputs', outs)
    node.set_editor_property('code', HLSL)
    lib.connect_material_expressions(region, '', node, 'RegionUV')
    lib.connect_material_expressions(surface, '', node, 'SurfaceUV')
    vi = expr(unreal.MaterialExpressionVertexInterpolator, -1000, -200)  # PreSkinnedPosition тек төбе шейдерінде бар
    lib.connect_material_expressions(lp, '', vi, '')
    lib.connect_material_expressions(vi, '', node, 'LP')
    lib.connect_material_expressions(wp, '', node, 'RP')
    lib.connect_material_expressions(nw, '', node, 'NW')
    for i, (n, col) in enumerate(defaults.items()):
        p = expr(unreal.MaterialExpressionVectorParameter, -850, 120 + i * 100)
        p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', unreal.LinearColor(*col, 1.))
        lib.connect_material_expressions(p, 'RGB', node, n)
    for i, (n, v) in enumerate(scalars.items()):
        p = expr(unreal.MaterialExpressionScalarParameter, -600, 120 + i * 100)
        p.set_editor_property('parameter_name', n); p.set_editor_property('default_value', v)
        lib.connect_material_expressions(p, '', node, n)
    ok = [lib.connect_material_property(node, '', unreal.MaterialProperty.MP_BASE_COLOR),
          lib.connect_material_property(node, 'Rough', unreal.MaterialProperty.MP_ROUGHNESS),
          lib.connect_material_property(node, 'AO', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
          lib.connect_material_property(node, 'Nrm', unreal.MaterialProperty.MP_NORMAL)]
    spec = expr(unreal.MaterialExpressionConstant, 0, 400); spec.set_editor_property('r', .3)
    lib.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    REPORT.write_text('done connections=' + str(ok))


try:
    run()
except Exception:
    REPORT.write_text('error ' + traceback.format_exc())
    raise
