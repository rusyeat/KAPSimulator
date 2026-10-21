# OGG → WAV: Unreal импорттап, SoundWave-ті WAV қылып қайта экспорттайды (Windows OGG декодері жоқ болғанда).
import unreal, os

SRC = "C:/Users/rusya/Downloads/soundef/660978__mchrpt__shooting_1.ogg"
OUT = "D:/KAPSimulator/External/Audio/freesound/shooting_1_decoded.wav"
TMP = "/Game/KAP/Audio/_Tmp"

t = unreal.AssetImportTask()
t.filename = SRC
t.destination_path = TMP
t.destination_name = "Shoot1"
t.replace_existing = True
t.automated = True
t.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
paths = t.get_editor_property("imported_object_paths")
if not paths:
    unreal.log_error("KAPOGG import failed")
else:
    w = unreal.load_asset(paths[0])
    unreal.log("KAPOGG imported %s duration=%.2f" % (paths[0], w.get_editor_property("duration")))
    e = unreal.AssetExportTask()
    e.object = w
    e.filename = OUT
    e.automated = True
    e.replace_identical = True
    e.prompt = False
    ok = unreal.Exporter.run_asset_export_task(e)
    unreal.log("KAPOGG export ok=%s size=%d" % (ok, os.path.getsize(OUT) if os.path.exists(OUT) else -1))
    unreal.EditorAssetLibrary.delete_directory(TMP)
