# Тек жаңа TTS дауыстарын импорттау (бар ассеттер өткізіледі)
import unreal, os, glob
tasks = []
for f in sorted(glob.glob("D:/KAPSimulator/External/VO/VO_*.wav")):
    n = os.path.splitext(os.path.basename(f))[0]
    if unreal.EditorAssetLibrary.does_asset_exist("/Game/KAP/Audio/VO/" + n):
        continue
    t = unreal.AssetImportTask()
    t.filename = f; t.destination_path = "/Game/KAP/Audio/VO"; t.destination_name = n
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
unreal.log("KAPVOIMP imported %d" % len(tasks))
