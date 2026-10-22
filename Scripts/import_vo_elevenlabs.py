# ElevenLabs дауыстарын импорттау: External/VO_ElevenLabs/VO_*.wav → /Game/KAP/Audio/VO (бар робот дауысты АУЫСТЫРАДЫ)
# Файл аты құжаттағыдай болуы керек (VO_<хэш>_m.wav / _f.wav); аты дұрыс емес файлдар өткізіледі
import unreal, os, glob, re
SRC = "D:/KAPSimulator/External/VO_ElevenLabs"
tasks, skipped = [], []
for f in sorted(glob.glob(SRC + "/*.wav")):
    n = os.path.splitext(os.path.basename(f))[0]
    if not re.fullmatch(r"VO_[0-9a-f]{8}_[mf]", n):
        skipped.append(n)
        continue
    t = unreal.AssetImportTask()
    t.filename = f; t.destination_path = "/Game/KAP/Audio/VO"; t.destination_name = n
    t.replace_existing = True; t.automated = True; t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
unreal.log("KAPVOEL imported %d, skipped %d: %s" % (len(tasks), len(skipped), ", ".join(skipped[:20])))
