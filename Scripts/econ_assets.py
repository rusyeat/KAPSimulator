import unreal
L = unreal.KAPEditorLibrary
unreal.log('KAPECON tables: ' + str(L.import_data_tables()))
unreal.log('KAPECON humans: ' + str(L.create_human_assets()))
