import unreal
# Жер бедерінің активтері (ландшафттың өзінен басқасы) + жануар материалы — NullRHI командлетте
ok = unreal.KAPEditorLibrary.build_terrain_assets('/Game/KAP/Maps/KAP_Steppe')
ok2 = unreal.KAPEditorLibrary.create_animal_material()
unreal.log('KAPTERRAIN assets result: %s animal=%s' % (ok, ok2))
