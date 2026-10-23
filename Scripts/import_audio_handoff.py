"""AUDIO_DEPLOY_HANDOFF.md бойынша дыбыстарды сәйкестендіру және импорттау (UnrealEditor-Cmd ... -run=pythonscript).

1) SFX/музыка (283 жол, SFX_Music_file_mapping.csv): asset бар ма, SoundWave ме, ұзақтығы > 0, loop белгісі CSV-мен
   бірдей ме (бірдей болмаса — түзетіледі), банк индекстері 0-ден үзіліссіз бе, қай бастапқы файлдан импортталған.
   Бұлар бұрын өңделген нұсқадан (кесілген атыстар, т.б.) импортталған — қайта жазылмайды, тек тексеріледі.
2) VO (203 жол, VO_203_file_mapping.csv): ready_wav_exists=True 169 ElevenLabs WAV → /Game/KAP/Audio/VO/<аты>
   (ескі Windows TTS нұсқасын ауыстырады, дыбыс класы/топ/сығу баптаулары сақталады). Дайын емес 34 жол
   импортталмайды (бар TTS баламасы қалады). Импорттан кейін ұзақтығы all_203_files.csv-мен салыстырылады.
Есеп: Saved/Tmp/audio_handoff_report.json
"""
import unreal, csv, json, traceback
from pathlib import Path

HANDOFF = Path(r'C:\Users\rusya\Documents\Codex\2026-10-21\js\work\Audio_Claude_Handoff')
DUR_CSV = Path(r'C:\Users\rusya\Downloads\all_203_files.csv')
REPORT = Path(r'D:\KAPSimulator\Saved\Tmp\audio_handoff_report.json')
EAL = unreal.EditorAssetLibrary
KEEP_PROPS = ('sound_class_object', 'sound_group', 'compression_quality', 'attenuation_settings', 'volume', 'pitch')


def rows(p):
    with open(p, encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))


def norm(p):
    return str(p).replace('/', '\\').lower()


report = {'sfx': {}, 'banks': {}, 'vo': {}, 'errors': []}
try:
    sfx = rows(HANDOFF / 'SFX_Music_file_mapping.csv')
    vo = rows(HANDOFF / 'VO_203_file_mapping.csv')
    durs = {}
    if DUR_CSV.exists():
        for r in rows(DUR_CSV):
            try:
                durs[r['Файл']] = float(r['Ұзақтығы секунд'] or 0)
            except Exception:
                pass

    # ---------------- 1) SFX / музыка: тексеру ----------------
    banks = {}
    missing, bad_loop_fixed, zero_dur, src_same, src_other = [], [], [], 0, []
    for r in sfx:
        a = unreal.load_asset(r['unreal_asset'])
        idx = int(r['file'].rsplit('_', 1)[1])
        banks.setdefault(r['bank'], []).append(idx)
        if not isinstance(a, unreal.SoundWave):
            missing.append(r['file'])
            continue
        want_loop = r['loop'] == 'True'
        if bool(a.get_editor_property('looping')) != want_loop:
            a.set_editor_property('looping', want_loop)
            EAL.save_loaded_asset(a, only_if_is_dirty=False)
            bad_loop_fixed.append(r['file'])
        if a.get_editor_property('duration') <= 0.01:
            zero_dur.append(r['file'])
        aid = a.get_editor_property('asset_import_data')
        src = aid.get_first_filename() if aid else ''
        if src and norm(Path(src).stem) == norm(Path(r['source_file']).stem):
            src_same += 1
        else:
            src_other.append({'file': r['file'], 'imported_from': src, 'csv_source': r['source_file']})
    gaps = {b: sorted(v) for b, v in banks.items() if sorted(v) != list(range(len(v)))}
    report['sfx'] = {'rows': len(sfx), 'missing': missing, 'loop_fixed': bad_loop_fixed, 'zero_duration': zero_dur,
                     'source_name_matches': src_same, 'source_differs': src_other}
    report['banks'] = {'count': len(banks), 'sizes': {b: len(v) for b, v in sorted(banks.items())}, 'gaps': gaps}
    unreal.log('KAPAUDIO sfx rows=%d missing=%d loop_fixed=%d zero=%d src_same=%d src_other=%d banks=%d gaps=%d' % (
        len(sfx), len(missing), len(bad_loop_fixed), len(zero_dur), src_same, len(src_other), len(banks), len(gaps)))

    # ---------------- 2) VO: 169 ElevenLabs WAV импорты ----------------
    ready = [r for r in vo if r['ready_wav_exists'] == 'True']
    kept, tasks = {}, []
    for r in ready:
        name = r['unreal_asset'].rsplit('/', 1)[1]
        old = unreal.load_asset(r['unreal_asset'])
        if isinstance(old, unreal.SoundWave):
            kept[name] = {p: old.get_editor_property(p) for p in KEEP_PROPS}
        t = unreal.AssetImportTask()
        for p, v in dict(filename=r['source_wav'], destination_path='/Game/KAP/Audio/VO', destination_name=name,
                         replace_existing=True, automated=True, save=False).items():
            t.set_editor_property(p, v)
        tasks.append((r, name, t))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for _, _, t in tasks])
    imported, failed, dur_bad = 0, [], []
    for r, name, t in tasks:
        a = unreal.load_asset(r['unreal_asset'])
        if not isinstance(a, unreal.SoundWave):
            failed.append(name)
            continue
        for p, v in kept.get(name, {}).items():
            try:
                a.set_editor_property(p, v)
            except Exception:
                pass
        a.set_editor_property('looping', False)
        EAL.save_loaded_asset(a, only_if_is_dirty=False)
        imported += 1
        d = a.get_editor_property('duration')
        want = durs.get(r['file'], 0.0)
        if want > 0 and abs(d - want) > 0.15:
            dur_bad.append({'file': name, 'asset': round(d, 3), 'wav': want})
    not_ready = [r for r in vo if r['ready_wav_exists'] != 'True']
    fallback = [r['file'] for r in not_ready if isinstance(unreal.load_asset(r['unreal_asset']), unreal.SoundWave)]
    report['vo'] = {'rows': len(vo), 'ready': len(ready), 'imported': imported, 'replaced_existing': len(kept), 'failed': failed,
                    'duration_mismatch': dur_bad, 'not_ready': len(not_ready), 'not_ready_with_tts_fallback': len(fallback),
                    'not_ready_silent': [r['file'] for r in not_ready if r['file'] not in fallback]}
    unreal.log('KAPAUDIO vo ready=%d imported=%d replaced=%d failed=%d dur_mismatch=%d not_ready=%d tts_fallback=%d' % (
        len(ready), imported, len(kept), len(failed), len(dur_bad), len(not_ready), len(fallback)))
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log('KAPAUDIO DONE')
except Exception:
    report['errors'].append(traceback.format_exc())
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    unreal.log_error(traceback.format_exc())
    raise
