# KAP Simulator — деңгейді автоматты құру.
# Іске қосу (редакторсыз терминалдан):
#   UnrealEditor-Cmd.exe KAPSimulator.uproject -ExecutePythonScript="Scripts/build_world.py" -unattended -nosplash
# немесе редактор ішінде: Tools → Execute Python Script.
import unreal

MAP = "/Game/KAP/Maps/KAP_Steppe"
ok = unreal.KAPEditorLibrary.build_everything(MAP)
unreal.log("KAP build_everything: " + str(ok))
