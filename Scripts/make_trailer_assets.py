# Тизер-трейлер рендеріне керек активтер (Movie Render Queue):
#   /Game/KAP/Cinematics/LS_KAPTrailer  — бос Level Sequence, 15 с @ 30 к/с (MRQ уақытты осыдан жүргізеді, ойын коды KAPTrailer.cpp осы уақытқа сүйенеді)
#   /Game/KAP/Cinematics/MPC_KAPTrailer — рендер баптауы: 1920×1080, ProRes 422 (MOV) + MP4 (H.264) + WAV + тексеруге JPG кадрлар,
#                                          ойынның өз GameMode-ы, Slate жазуларын (UI) кадрға қосу
# Іске қосу: UnrealEditor-Cmd.exe KAPSimulator.uproject -run=pythonscript -script=Scripts/make_trailer_assets.py
import unreal

FOLDER = "/Game/KAP/Cinematics"
OUT_DIR = "D:/KAPSimulator/Saved/Trailer"
FPS, SECONDS = 30, 15

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
for name in ("LS_KAPTrailer", "MPC_KAPTrailer"):
    if eal.does_asset_exist(FOLDER + "/" + name):
        eal.delete_asset(FOLDER + "/" + name)

seq = tools.create_asset("LS_KAPTrailer", FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(0)
seq.set_playback_end(FPS * SECONDS)
eal.save_loaded_asset(seq)

cfg = tools.create_asset("MPC_KAPTrailer", FOLDER, unreal.MoviePipelinePrimaryConfig, unreal.MoviePipelinePrimaryConfigFactory())

out = cfg.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
out.output_resolution = unreal.IntPoint(1920, 1080)
out.output_directory = unreal.DirectoryPath(OUT_DIR)
out.file_name_format = "KAP_Trailer_15s"
out.override_existing_output = True
out.use_custom_frame_rate = True
out.output_frame_rate = unreal.FrameRate(FPS, 1)

cfg.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
ui = cfg.find_or_add_setting_by_class(unreal.MoviePipelineWidgetRenderer)
ui.composite_onto_final_image = True

aa = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAntiAliasingSetting)
aa.spatial_sample_count = 1
aa.temporal_sample_count = 1
aa.engine_warm_up_count = 60   # ~2 с: Lumen мен экспозиция орнығады (трейлер уақыты 0-де күтеді)
aa.render_warm_up_count = 32

go = cfg.find_or_add_setting_by_class(unreal.MoviePipelineGameOverrideSetting)
go.soft_game_mode_override = unreal.load_class(None, "/Script/KAPSimulator.KAPGameMode")
go.cinematic_quality_settings = True

codec = None
for nm in ("PRO_RES_422", "PRORES_422", "PRO_RES422"):
    codec = getattr(unreal.AppleProResEncoderCodec, nm, None)
    if codec is not None:
        break
pr = cfg.find_or_add_setting_by_class(unreal.MoviePipelineAppleProResOutput)
if codec is not None:
    pr.codec = codec
pr.include_audio = True

mp4 = cfg.find_or_add_setting_by_class(unreal.MoviePipelineMP4EncoderOutput)
mp4.include_audio = True

cfg.find_or_add_setting_by_class(unreal.MoviePipelineWaveOutput)
cfg.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_JPG)

eal.save_loaded_asset(cfg)
unreal.log("KAPTRAILERASSETS ok codec=%s settings=%s" % (codec, [s.get_class().get_name() for s in cfg.get_all_settings()]))
