import unreal
# Ландшафтты KAPTerrain бойынша қайта құру — тек -AllowCommandletRendering (edit layer GPU-да біріктіріледі)
ok = unreal.KAPEditorLibrary.rebuild_terrain('/Game/KAP/Maps/KAP_Steppe')
unreal.log('KAPTERRAIN landscape result: ' + str(ok))
ok2 = unreal.KAPEditorLibrary.place_player_start('/Game/KAP/Maps/KAP_Steppe')
unreal.log('KAPTERRAIN playerstart result: ' + str(ok2))
