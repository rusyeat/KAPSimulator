# Freesound (CC0) өңделген дыбыстарын импорттау: Scripts/process_audio.ps1 → External/Audio/freesound/*.wav
# Банктер: StepSand (дымқыл құм, жүру), StepSandRun, LandSand, Generator (цикл), GenStart, GenStop, GunRifle
import unreal, os, glob, re

SRC = "D:/KAPSimulator/External/Audio/freesound"
LOOPING = {"Generator"}
tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks, cats = [], []
for f in sorted(glob.glob(SRC + "/*_*.wav")):
    name = os.path.splitext(os.path.basename(f))[0]
    m = re.match(r"^([A-Za-z]+)_(\d+)$", name)
    if not m:
        continue  # shooting_1_decoded сияқты аралық файлдар
    t = unreal.AssetImportTask()
    t.filename = f
    t.destination_path = "/Game/KAP/Audio/SFX"
    t.destination_name = name
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t); cats.append(m.group(1))
tools.import_asset_tasks(tasks)
ok = 0
for t, cat in zip(tasks, cats):
    paths = t.get_editor_property("imported_object_paths")
    if not paths:
        unreal.log_warning("KAPFS failed " + t.filename); continue
    ok += 1
    if cat in LOOPING:
        w = unreal.load_asset(paths[0])
        w.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(w)
unreal.log("KAPFS imported %d / %d" % (ok, len(tasks)))
