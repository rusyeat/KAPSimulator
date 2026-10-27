"""Кеңейтілген сюжет үшін DT_Quests пен DT_Config кестелерін CSV-ден жаңарту (басқа активтерге тимейді).

Іске қосу: UnrealEditor-Cmd.exe KAPSimulator.uproject -run=pythonscript -script=Scripts/import_story_data.py -nullrhi -unattended
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
for name in ['Quests', 'Config']:
    table = unreal.load_asset('/Game/KAP/Data/DT_' + name)
    assert table, name
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, str(root / ('Content/KAP/Data/' + name + '.csv'))), name
    assert unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False), name
    rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    unreal.log('KAPSTORY IMPORT %s rows=%d' % (name, len(rows)))
unreal.log('KAPSTORY IMPORT DONE')
