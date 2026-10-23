import unreal
# Жаңа жер бедері: биом текстурасы, MPC_KAPEnv, ландшафт/су/тау материалдары, SM_KAP_Mountains, ландшафт (KAP_Steppe)
ok = unreal.KAPEditorLibrary.build_terrain_all('/Game/KAP/Maps/KAP_Steppe')
unreal.log('KAPTERRAIN result: ' + str(ok))
