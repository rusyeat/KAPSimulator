import unreal
L = unreal.KAPEditorLibrary
unreal.log("KAPUP industrial=%s" % L.create_industrial_material())
unreal.log("KAPUP invisible=%s" % L.create_invisible_material())
unreal.log("KAPUP land=%s" % L.create_landscape_material2())
unreal.log("KAPUP land4=%s" % L.create_landscape_material4())
unreal.log("KAPUP sandtex=%s" % L.generate_sand_textures())
unreal.log("KAPUP land5=%s" % L.create_landscape_material5())
unreal.log("KAPUP apply=%s" % L.apply_landscape_material("/Game/KAP/Maps/KAP_Steppe"))
unreal.log("KAPUP bevel=%s" % L.create_bevel_meshes())
