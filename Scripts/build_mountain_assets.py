import unreal
# Шекара таулары (эрозиялы меш, орман/шалғын/жартас материалы), шырша, тасталған костюм, бу материалы (M_KAP_Puff)
# және биом текстурасы (бөктер шөбі) — NullRHI командлетте; ландшафт биіктігі өзгермейді
ok0 = unreal.KAPEditorLibrary.create_terrain_textures()
ok = unreal.KAPEditorLibrary.build_mountain_assets()
unreal.log('KAPTERRAIN mountain assets result: textures=%s assets=%s' % (ok0, ok))
