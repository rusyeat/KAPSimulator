import unreal
for n in ["T_KAP_SandHeight","T_KAP_SandNormal","T_KAP_SandAlbedo"]:
    t = unreal.load_asset("/Game/KAP/Textures/%s.%s" % (n, n))
    unreal.log("KAPTEX %s loaded=%s size=%s x %s" % (n, t is not None, t.blueprint_get_size_x() if t else -1, t.blueprint_get_size_y() if t else -1))
