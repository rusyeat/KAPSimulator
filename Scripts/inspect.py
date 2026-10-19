import unreal
for p in ["/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New", "/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01", "/Game/Characters/Mannequins/Materials/M_Mannequin"]:
    m = unreal.load_asset(p)
    if not m: continue
    names = unreal.MaterialEditingLibrary.get_vector_parameter_names(m) if hasattr(unreal.MaterialEditingLibrary, "get_vector_parameter_names") else []
    sc = unreal.MaterialEditingLibrary.get_scalar_parameter_names(m) if hasattr(unreal.MaterialEditingLibrary, "get_scalar_parameter_names") else []
    unreal.log("KAPMAT " + p + " V=" + str(list(names)) + " S=" + str(list(sc)))
sk = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
unreal.log("KAPMAT slots=" + str([m.material_slot_name for m in sk.materials]))
