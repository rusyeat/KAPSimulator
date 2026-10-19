import unreal
bp = unreal.load_class(None, "/Game/VehicleTemplate/Blueprints/OffroadCar/BP_OffroadCar_Pawn.BP_OffroadCar_Pawn_C")
cdo = unreal.get_default_object(bp)
sk = cdo.get_editor_property("mesh")
m = sk.get_editor_property("skeletal_mesh_asset")
unreal.log("KAPCAR mesh=" + m.get_path_name())
for i, s in enumerate(m.materials):
    unreal.log("KAPCAR slot %d %s %s" % (i, s.material_slot_name, s.material_interface.get_path_name() if s.material_interface else "none"))
b = m.get_bounds()
unreal.log("KAPCAR bounds origin=%s ext=%s" % (b.origin, b.box_extent))
sk2 = m.skeleton
for n in ["PhysWheel_FL","PhysWheel_FR","PhysWheel_BL","PhysWheel_BR","Root"]:
    pass
names = [str(x) for x in unreal.SkeletalMeshEditorSubsystem and []]
