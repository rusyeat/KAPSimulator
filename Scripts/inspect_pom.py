import unreal
f = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/ParallaxOcclusionMapping")
for e in unreal.MaterialEditingLibrary.get_material_function_input_names(f) if hasattr(unreal.MaterialEditingLibrary, "get_material_function_input_names") else []:
    unreal.log("KAPPOM in " + str(e))
inps = unreal.MaterialEditingLibrary.get_inputs_for_material_expression if hasattr(unreal.MaterialEditingLibrary,"get_inputs_for_material_expression") else None
exprs = f.get_editor_property("function_expressions") if False else []
unreal.log("KAPPOM ok " + str(f))
