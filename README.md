# KAP Simulator — Unreal Engine 5.8 (C++)

Survival-экшн: шөлейттегі ISR уран кеніші, 21 ұңғыма. Техникалық тапсырма бойынша UE5 C++ жобасы.

## Іске қосу
- **Ойнау:** `Play.bat` (немесе `UnrealEditor.exe KAPSimulator.uproject -game`)
- **Редактор:** `KAPSimulator.uproject` файлын ашыңыз (карта: `/Game/KAP/Maps/KAP_Steppe`)
- **Кодты қайта құрастыру:** `Engine\Build\BatchFiles\Build.bat KAPSimulatorEditor Win64 Development -Project=D:\KAPSimulator\KAPSimulator.uproject`
- **Деңгейді қайта құру** (материалдар, мештер, Data Table, ландшафт): `UnrealEditor-Cmd.exe KAPSimulator.uproject -ExecutePythonScript=Scripts/build_world.py`
- **Автотест** (скриншоттар `Saved/Screenshots`): `UnrealEditor.exe KAPSimulator.uproject -game -kapautotest`

**Басқару:** WASD, Shift, Space · тышқан · E — әрекет · Tab — планшет · 1–9 — құрал/қару · R — оқтау · F — фонарь · H/J/K — аптечка/су/тамақ · көлікте W/S/A/D, Space, V, E · Esc — үзіліс · F9 — уақыт жылдамдығы · Enter — кинематиканы өткізу.

## ТТ-дағы C++ кластары
| ТТ | Файл |
|---|---|
| AKAPCharacter | `Source/KAPSimulator/KAP/Player/KAPCharacter.*` |
| UKAPSurvivalAttributeSet (GAS) | `KAP/Player/KAPSurvivalAttributeSet.*` |
| AWellActor | `KAP/World/WellActor.*` |
| UWellManagerSubsystem (+ Байес) | `KAP/World/WellManagerSubsystem.*` |
| AWorkerNPC (+ жасақ, бандит, монстрлар) | `KAP/NPC/KAPNPC.*` |
| UTabletWidget (планшет, HUD, мини-ойындар) | `KAP/UI/KAPHUD.*` |
| UNavigatorComponent (A*) | `KAP/Player/NavigatorComponent.*` |
| AWeatherManager | `KAP/World/WeatherManager.*` |
| UFactionSubsystem, UMutationSubsystem логикасы | `KAP/Core/KAPGameMode.*`, `KAP/World/WellManagerSubsystem.*` |
| UKAPGameInstance + SaveGame | `KAP/Core/KAPGameInstance.*` |
| AI тақырыптары (A*, Байес, Q-learning, HMM, CSP) | `KAP/AI/KAPAI.*` |
| Data Table (CSV) | `Content/KAP/Data/*.csv` → `DT_*` |
| Деңгей құрастыру (редактор) | `Source/KAPSimulatorEditor/KAPEditorLibrary.*` |

## Графика
Lumen GI мен шағылысулар, Virtual Shadow Maps, Sky Atmosphere, Volumetric Clouds, Exponential Height Fog (volumetric), пост-процесс; 2.4×2.4 км Landscape (процедуралық бедер мен материал); Nanite тастар; Chaos Vehicles; Manny/Quinn кейіпкерлері анимациясымен.

## ТТ-дан айырмашылықтар (ассеттер жоқтығынан)
- Megascans/Fab, MetaHuman, УАЗ/КамАЗ модельдері жоқ: барлық көлік — шаблондағы Chaos offroad көлігі; адамдар — UE манекендері; ғимараттар мен ұңғымалар — пішіндерден құрастырылған; тастар, сексеуіл, қасқыр — процедуралық мештер.
- Кинематика Sequencer-де емес, C++ камера кілттік кадрларымен; ана сахнасы — манекенмен.
- Дыбыс MetaSounds емес, процедуралық синтез (жел, жаңбыр, от, радио, двигатель, атыс, жоқтау әуені).
- Карта 8×8 км емес, 2.4×2.4 км.
- Тамаша графика үшін: Fab-тан шөлейт/Megascans пакеттерін қосып, `KAPVisuals.cpp`, `KAPStructure.cpp`, `WellActor.cpp` ішіндегі меш жолдарын ауыстыруға болады.

Оқиғалар ойдан шығарылған. «QazUran» — ойдан шығарылған компания.
