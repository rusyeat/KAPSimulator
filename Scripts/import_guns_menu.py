# Жеке атыстарға кесілген мылтық дыбыстары (process_guns.ps1) және мәзір музыкасы (Menu_0, ChatGPT/FL Studio «Бос үміт»)
import unreal, os, glob, re

tools = unreal.AssetToolsHelpers.get_asset_tools()
jobs = []
for f in sorted(glob.glob("D:/KAPSimulator/External/Audio/guns_cut/*_*.wav")):
    jobs.append((f, "/Game/KAP/Audio/SFX", False))
for f in sorted(glob.glob("D:/KAPSimulator/External/Audio/menu/Menu_*.wav")):
    jobs.append((f, "/Game/KAP/Audio/Music", True))
tasks = []
for f, dest, loop in jobs:
    t = unreal.AssetImportTask()
    t.filename = f; t.destination_path = dest
    t.destination_name = os.path.splitext(os.path.basename(f))[0]
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append((t, loop))
tools.import_asset_tasks([t for t, _ in tasks])
ok = 0
for t, loop in tasks:
    p = t.get_editor_property("imported_object_paths")
    if not p:
        unreal.log_warning("KAPGUN failed " + t.filename); continue
    ok += 1
    if loop:
        w = unreal.load_asset(p[0]); w.set_editor_property("looping", True); unreal.EditorAssetLibrary.save_loaded_asset(w)
unreal.log("KAPGUN imported %d / %d" % (ok, len(tasks)))
