# Жүктелген тегін дыбыстарды (External/Audio, External/VO) /Game/KAP/Audio ішіне импорттау.
# Атаулары тұрақты: <Санат>_<индекс> — ойын оларды индекс бойынша жүктейді (UKAPAudio::Bank).
import unreal, os, glob

ROOT = "D:/KAPSimulator/External"
A = ROOT + "/Audio"
KI = A + "/kenney_impact/x_kenney_impact-sounds/Audio"
KR = A + "/kenney_rpg/x_kenney_rpg-audio/Audio"
YL = A + "/yelling/x_yelling/yelling sounds"

def files(pattern):
    return sorted(glob.glob(pattern))

BANKS = [
    ("SFX", "StepSand", files(KI + "/footstep_snow_*.ogg")),
    ("SFX", "StepConcrete", files(KI + "/footstep_concrete_*.ogg")),
    ("SFX", "StepWood", files(KI + "/footstep_wood_*.ogg")),
    ("SFX", "StepCarpet", files(KI + "/footstep_carpet_*.ogg")),
    ("SFX", "StepDirt", files(KR + "/footstep0*.ogg")),
    ("SFX", "StepMetal", files(KI + "/impactMetal_light_*.ogg")),
    ("SFX", "ImpMetal", files(KI + "/impactMetal_medium_*.ogg")),
    ("SFX", "ImpMetalHeavy", files(KI + "/impactMetal_heavy_*.ogg")),
    ("SFX", "ImpWood", files(KI + "/impactWood_medium_*.ogg")),
    ("SFX", "ImpPlank", files(KI + "/impactPlank_medium_*.ogg")),
    ("SFX", "ImpBody", files(KI + "/impactPunch_medium_*.ogg") + files(KI + "/impactPunch_heavy_*.ogg")),
    ("SFX", "ImpSand", files(KI + "/impactSoft_medium_*.ogg")),
    ("SFX", "ImpMining", files(KI + "/impactMining_*.ogg")),
    ("SFX", "DoorOpen", files(KR + "/doorOpen_*.ogg")),
    ("SFX", "DoorClose", files(KR + "/doorClose_*.ogg")),
    ("SFX", "Creak", files(KR + "/creak*.ogg")),
    ("SFX", "Cloth", files(KR + "/cloth*.ogg")),
    ("SFX", "MetalClick", files(KR + "/metalClick.ogg") + files(KR + "/metalLatch.ogg")),
    ("SFX", "DrawKnife", files(KR + "/drawKnife*.ogg")),
    ("SFX", "Engine", [A + "/engine/loop_%d.wav" % i for i in range(6)]),
    ("SFX", "Scream", files(A + "/screams/x_aargh/aargh*.ogg")),
    ("SFX", "Yell", files(YL + "/*yell*.wav")),
    ("SFX", "Grunt", files(YL + "/3grunt*.wav")),
    ("SFX", "Moan", [A + "/monster/zombie-moans.ogg", A + "/monster/hungry-ghoul.ogg"]),
    ("SFX", "GunPM", [A + "/guns/x_gunshots/sounds/cz.wav"]),
    ("SFX", "GunAK", [A + "/guns/x_gunshots/sounds/sks.wav"]),
    ("SFX", "GunSniper", [A + "/guns/x_gunshots/sounds/mosin.wav"]),
    ("SFX", "GunShotgun", [A + "/guns/x_gunshots/sounds/shotty.wav"]),
    ("Music", "Ambient", files(A + "/music/*.mp3")),
    ("Music", "Danger", [A + "/music/Insistent.ogg"]),
    ("Music", "Indoor", [A + "/music/freezer.ogg"]),
]
LOOPING = {"Engine", "Ambient", "Danger", "Indoor"}

tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks, meta = [], []
for folder, cat, lst in BANKS:
    for i, f in enumerate(lst):
        if not os.path.exists(f):
            unreal.log_warning("KAPIMP missing " + f); continue
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = "/Game/KAP/Audio/" + folder
        t.destination_name = "%s_%d" % (cat, i)
        t.replace_existing = True; t.automated = True; t.save = True
        tasks.append(t); meta.append(cat)
for f in sorted(glob.glob(ROOT + "/VO/VO_*.wav")):
    t = unreal.AssetImportTask()
    t.filename = f; t.destination_path = "/Game/KAP/Audio/VO"
    t.destination_name = os.path.splitext(os.path.basename(f))[0]
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t); meta.append("VO")
tools.import_asset_tasks(tasks)
ok = 0
for t, cat in zip(tasks, meta):
    paths = t.get_editor_property("imported_object_paths")
    if not paths: unreal.log_warning("KAPIMP failed " + t.filename); continue
    ok += 1
    if cat in LOOPING:
        w = unreal.load_asset(paths[0])
        w.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(w)
unreal.log("KAPIMP imported %d / %d" % (ok, len(tasks)))
