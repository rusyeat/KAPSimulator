# KAP Simulator — жобаның толық құжаты (басқа ЖИ мен әзірлеушіге арналған)

Жаңартылған күні: 2026-10-27. Соңғы код коммиті: `2f7445c` (`main`, https://github.com/rusyeat/KAPSimulator).
Құжатты Claude Code мен OpenAI Codex бірге жүргізеді. Бұл файл жобаның не екенін, не жасалғанын, кодтың қай жерде
жатқанын, қай құралдар мен дереккөздер қолданылғанын, қалай құрастырылатынын және нені абайлау керегін бір жерде түсіндіреді.

> **Маңызды:** 2026-10-23-тен кейінгі Codex жұмысының бәрі (V5 кейіпкерлер, тіршілік иелері, Stalker саудагері, ақылды NPC,
> қару V2/V3, тірі кеніш, инвентарь, баптаулар, нұсқаулық) дискіде бар, ойнауға болатын EXE-ге кірген, бірақ **git-ке әлі коммиттелмеген**.
> GitHub-тағы `main` тек §3-тегі коммиттерді қамтиды. Толық тізім — §14.

---

## 0. English TL;DR (for another AI)

- **What:** KAP Simulator — a first-person open-world survival-horror / industrial simulator in **Unreal Engine 5.8, C++**.
  A lone repair engineer arrives at an abandoned **ISR uranium mine** ("QazUran", fictional) in the Kazakh steppe:
  21 wells keep breaking, workers get irradiated and may turn into mutants, the management fled and hid the cause.
- **Where:** project `D:\KAPSimulator`, engine `D:\ue5\UE_5.8`, map `/Game/KAP/Maps/KAP_Steppe`.
  Game module `Source/KAPSimulator/KAP/*`, editor generator module `Source/KAPSimulatorEditor/*`.
- **Almost everything is procedural/code-built:** world layout from CSV tables, terrain from `KAPTerrain.h`,
  buildings/vehicles/props from code or SDF mesh generators, UI on Canvas, cinematics in C++ (no Sequencer),
  FX without Niagara, procedural audio synth + recorded banks. Characters, animals, creatures and weapons are Blender-built.
- **Latest playable build:** `D:\KAPSimulator\Saved\Releases\AnimalRagdoll_2026-10-27\Windows\KAPSimulator.exe`
  (2026-10-27; `Play.bat` and `Ойынды ашу.lnk` in the project root open it). Same tree as `PlayerExperience_2026-10-24`
  (packaged checks 86/86) plus the animal ragdoll fix; `-kapanimaltest` passed on the package. It includes all committed work
  **and** Codex's uncommitted work.
- **Git state:** `main` = commit `2f7445c` (animal ragdoll fix) + this doc. Large uncommitted Codex work is on disk (see §14). Never commit someone else's
  uncommitted hunks; stage only your own.
- **Build editor:** `D:\ue5\UE_5.8\Engine\Build\BatchFiles\Build.bat KAPSimulatorEditor Win64 Development -Project="D:\KAPSimulator\KAPSimulator.uproject" -WaitMutex`
  (Codex adds `-NoUBA -UBARootDir=D:\KAPSimulator\Saved\BuildCache\UBA -MaxParallelActions=2` because UBA can hang at the end).
- **Test modes:** command-line flags such as `-kapexperiencetest`, `-kaplivingtest`, `-kapweaponstest`, `-kapaitest`,
  `-kapfixshots`, `-kapanimaltest`, `-kapnatureshots`, `-kapaudiocheck` (full table in §11), each with a `Scripts/verify_*.ps1` checker.
- **Animal skeletons must have no bone scale.** Unreal's ragdoll mishandles a scaled root bone (Blender's default FBX export puts
  x100 there). `build_animals_v1.py` now exports in centimetres (`to_cm`); physics assets come from `BuildAnimalPhysics` (§10.7).
- **Story expansion «Кезек аяқталған жоқ» (2026-10-27, §5.1):** Codex's scenario pack is wired into the game in
  `Core/KAPStoryExpansion.cpp` — scenes S01–S12, villains (director Serik Sarsenov, Sapar Amanov), new obstacles on the main path
  (Bekzat's permit, Sapar's roadblock at the HQ gate, the director's call and blocked report channel, hired guards at the repeater),
  slower well decay. Test: `-kapstorytest` (34 checks, `KAPS_*.png`). Story flags are `st_*` in `FKAPSession::Flags`.

---

## 1. Ойын туралы

**Жанр:** бірінші жақтан көрінетін ашық әлем, survival, хоррор, өндірістік симулятор. Тіл — қазақша (баптауда орысша субтитр бар).

**Логлайн:** басшылығы қашып кеткен уран кенішінде инженер ұңғымаларды тоқтатуға, жұмысшыларды сәуледен құтқаруға
және апатты кім жасырғанын ашуға тырысады. Басты сұрақ: жоспар адам өмірінен жоғары қойылған жерде кімді құтқара аласың?

**Әлем:** 2.4 × 2.4 км дала (QazUran кеніші). 21 ISR ұңғымасы, вагон-үйлер, цех, күзет бекеті, кеніш басқармасы,
екі ауыл (Ақсай, Жосалы), қираған ауыл Көкжиек, су қоймасы, әскери аймақ, бандит лагері (Қаражар), 4 мутант ұясы.
Шығыстағы шатқалдан өзен ағып, батыста атырапқа (3 тармақ) бөлініп көлге құяды; оңтүстікте оазис; ауыл маңында егістік.
Шекарада 3 км-лік орманды таулар (шығуға болмайды — көрінбейтін қабырға). Карта 4 аймаққа бөлінген, сигнал мұнараларымен ашылады.
Жолдарда 8 қызметтік көлік жүреді (4 концентрат КамАЗы, су тасушы, 2 УАЗ, Буханка), 4 КПП, ауылдарда қосымша 14 тұрғын (Codex, §3).

**Сюжет (толығы: `C:\Users\rusya\Downloads\KAP Simulator — ойын сюжеті.docx`):**
- Апаттың шын себебі: басқарма шығым үшін қышқыл мөлшерін нормадан 40%-ға асырған, датчик ескертулерін елемеген,
  қаптама жарылғанда хабарды жасырып, өзі эвакуацияланған. Ойыншы мұны соңында басқармадағы апат журналынан біледі.
- **Кіріспе:** инженер ескі УАЗ-452 «Буханкамен» келеді, рацияда үзік хабар. Радиатор қайнап, көлік тоқтайды (бу, апат шамы),
  инженер есікті ашып түседі, құм дауылы келе жатыр → «Баспана табыңыз». Enter — кіріспені өткізу.
- **I акт — Дауыл:** вагон-үй → планшет → дауылдың басылуын күту.
- **II акт — Кеніш:** ескі көлік қаңқасынан радиатор → Буханканы жөндеу → №1 ұңғыма → күзет бастығы Бекзат (қару рұқсаты, ПМ)
  → 21 ұңғыманы қорғау (3 жөнделгенде бригадалар ашылады) → сигнал мұнаралары.
- **III акт — Шындық:** 5 ұңғымадан кейін басқармаға рұқсат пен КамАЗ → бос кеңседегі апат журналы → басқарма радиосынан есеп.
- **Финалдар:** «Құтқару» (≥ 11 жұмыс істейтін ұңғыма, ағу/өрт жоқ, мутация < 70%) немесе «Қашу» (8-ден көп ұңғыма жанды /
  жұмысшылардың 60%+ қаза тапты / мутация 100% / есеп ерте жіберілді). Екеуі де бір сахнамен бітеді: жас жұмысшы
  **Ерлан Сейітов (19 жас)** әрқашан қаза табады, анасы Ақсайда баласының денесінің жанында. Сосын титрлар.
- **Кейіпкерлер:** Ерлан Сейітов мен анасы, күзет бастығы Бекзат, диспетчер (рация), Төлеген ақсақал (Ақсай),
  Сағат ақсақал (Жосалы), жоғалған Нұрлан Есенов, полиция капитаны Әділет, дүкенші Ерболат (Fab Stalker киімінде),
  әскери аймақ бастығы **Майор Ғабит Құрманов** (бұрын Ерланмен аттас еді — `327973f`),
  жұмысшылар, мутанттар мен 4 босс (Ақ жал, Құм айдаһары, Көкжиек алыбы, Қаражар алыбы), бандиттер, тірі қалғандар.
  Codex қосқандар: дәрігер Айгерім, науқас бала, жүргізуші Данияр, зертханашы Майра, механик Рүстем, тілші Әлия.
- **Тапсырмалар:** 13 негізгі + 12 қосымша (`Content/KAP/Data/Quests.csv`). Қосымшалар: ауыл жасағы (6 мутант), жоғалған жұмысшы,
  Ниваның 4 бөлшегі, бандит лагері, су қоймасы, Көкжиектегі тірілер, полиция жұмыстары, әскери аймақ, босстар,
  және тірі кеніштің 6 байланысқан тапсырмасы (`l_clinic`, `l_care`, `l_power`, `l_manifest`, `l_samples`, `l_archive`):
  емхана, науқас бала, жарық, пломба сәйкессіздігі, су үлгілері, куәгерді қорғау.

**Негізгі механика:**
- **Ұңғыма құлдырауы** (ойын сағатымен): ақау (сорғы/құбыр/электр/қышқыл/өрт) → 6 сағ кейін ағу (жұмысшыға 7 мЗв/сағ)
  → тағы 5 сағ кейін өрт (+260 мЗв, мутация +7%) → тағы 2 сағ кейін ұңғыма жоғалады.
- **Жөндеу мини-ойыны:** ақау түріне қарай қадамдар (вентиль, бөлшек салу, сымдар, қысым, өрт сөндіру). `KAPTypes.cpp → KAP::RepairSteps`.
- **Жұмысшы дозасы:** 100 мЗв — уланған, 250 — ауру, 450 — өлім алдында, 650 — өлім; өлгендердің 30%-ы мутантқа айналады.
- **Мутация таралуы** (бүкіл аймақтың «ауру деңгейі»): өрт +7%, өлім +1.2%, босс −10%.
- **Ойыншы (GAS):** HP, су, дене температурасы, доза, төзімділік. Ыстық, суық, шөл, сәуле. Отыру (Left Ctrl).
- **Уақыт:** 1 ойын тәулігі = 48 минут, ойын 12:30-да басталады (`Config.csv`).
- **Ауа райы:** ашық, ыстық, құм дауылы, жаңбыр, тұман, бұлтты, найзағай, қатты жел; бір-біріне табиғи ауысады.
- **Экономика:** теңге, бүйір жұмыстар (полиция, саудагер, әскер), қару дүкені, сынық металл, сексеуіл отыны.
- **Көліктер:** УАЗ-452 «Буханка», УАЗ-469, Нива, КамАЗ, су тасушы, БТР (Chaos Vehicles, жанармай, қызу, жел, батпақ, дөңгелек жарылуы).
- **Қару (13):** кілт, пышақ, лом, балта, сигнал шамы/тапаншасы, ПМ, қос ұңғылы мылтық, АК, снайпер, молотов, Кедр (SMG), K-12 карабині.
  Оң тышқан — көздеу (ADS), кері тебу, оқтау кезеңдері, гильза, трассер, тигізу белгісі. Рұқсатсыз қару — ауыл жасағы оқ атады.
- **NPC (Codex):** көру/есту, соңғы көрген нүктені іздеу, оқтау, таса, қауіптен қашу, жергілікті A* бағыты (капсула енімен).

**Басқару (баптауда ауыстыруға болады):** WASD, Shift (жүгіру), Space (секіру), Left Ctrl (отыру), тышқан, оң тышқан (көздеу),
E — әрекет, I — жеке сөмке (инвентарь), Tab — планшет, M — карта, Q — көрсеткіштер, 1–9 — бекітілген қару/құрал ұяшықтары,
R — оқтау, F — фонарь, H/J/K — аптечка/су/тамақ; көлікте W/S/A/D, Space (қол тежегіш), V (камера), E (түсу);
F1 — нұсқаулық, Esc — үзіліс/баптаулар, F9 — уақыт жылдамдығы, Enter — кинематиканы өткізу.
Ойыншыға арналған нұсқаулық: `Saved/PlayerExperience_20261024/PLAYER_GUIDE_KK.md`.

---

## 2. Қазіргі күй (2026-10-27)

**Ойнау үшін ең соңғы нұсқа:** `D:\KAPSimulator\Saved\Releases\AnimalRagdoll_2026-10-27\Windows\KAPSimulator.exe`
(Win64 Development, 2026-10-27). `Play.bat` пен `Ойынды ашу.lnk` осыны ашады (ескі launcher — `Play_before_update.bat`;
алдыңғы нұсқа `PlayerExperience_2026-10-24` орнында қалды). Бұл — `PlayerExperience` ағашы + жануар рагдоллын түзету:
`-kapanimaltest` пакетте өтті (`-userdir=Saved\Tmp\AnimalTestUserDir`), кук 0 қате / 9 ескі ескерту,
`Binaries\Win64\KAPSimulator.exe` SHA256 `91E8E2FC…B086`. 86/86 тексеру алдыңғы нұсқада жасалған
(`Saved/PlayerExperience_20261024/validation.json`); жаңа пакетте олар қайта іске қосылған жоқ. Ішінде барлық коммиттелген және коммиттелмеген жұмыс бар.

Барлық релиздер (`Saved\Releases\`, ескілерінен жаңасына қарай; әрқайсысы алдыңғысының бәрін қамтиды):

| Релиз | Кім | Не қосты | Есеп |
|---|---|---|---|
| `Models_2026-10-23` | Codex | V5 кейіпкерлер мен киім | `Saved/ModelDeploy_20261023`, `Saved/Clothing_20261023` |
| `Audio_2026-10-23` | Claude | 169 ElevenLabs дауысы, `-kapaudiocheck` | коммит `b82ce10` |
| `Fixes_2026-10-23` | Claude | Кіріспе, жүру, костюм, жәшіктер, таулар, есіктер | коммит `96f5b03` |
| `Stalker_2026-10-23` | Codex | Ерболатқа Fab Stalker киімі (риг, 3 анимация, 3 LOD) | `Saved/Stalker_20261023/Stalker_Report.md` |
| `SmarterNPC_2026-10-23` | Codex | NPC көру/есту/есте сақтау, A*, таса, оқтау | `Saved/AI_20261023/AI_CHANGES.md` |
| `VehicleGround_2026-10-24` | Codex | Кіріспені өткізгенде автобустың ауада қалуы түзетілді | `Saved/VehicleGround_20261024/VEHICLE_GROUND_FIX.md` |
| `Weapons_2026-10-24` | Codex | Қару V2 (11 модель), ADS, оқтау, гильза, трассер, Кедр мен K-12 | `Saved/Weapons_20261024/WEAPONS_UPDATE.md` |
| `LivingWorld_2026-10-24` | Codex | Жол қозғалысы, 4 КПП, 6 кейіпкер мен 6 тапсырма, `I` инвентарь, отыру, перне ауыстыру | `Saved/LivingWorld_20261024/LIVING_WORLD_UPDATE.md` |
| `PlayerExperience_2026-10-24` | Codex | Қару V3, жеке сөмке, 5 бөлімді баптаулар, экранды растау/қайтару, F1 нұсқаулық | `Saved/PlayerExperience_20261024/PLAYER_EXPERIENCE_UPDATE.md` |
| `AnimalRagdoll_2026-10-27` | Claude | Өлген жануарлар ауада ілінбей, жерге табиғи жатады (сүйек масштабы, жаңа физика ассеті) | коммит `2f7445c`, `-kapanimaltest` |

Таныстыру материалдары: `Saved\Presentation\` — 10 скриншот (`01_dala_kun_batuy.png` … `10_vagon_tun.png`) және
15 секундтық тизер `KAP_Trailer_15s.mov` (ProRes) / `.mp4`.
Басқа құжаттар: сюжет `C:\Users\rusya\Downloads\KAP Simulator — ойын сюжеті.docx`,
QAIRU тапсырмасы `C:\Users\rusya\Downloads\qairu\QAIRU_AI_Storytelling_Endterm_KZ_FINAL.docx`,
дыбыс деплойы `C:\Users\rusya\Documents\Codex\2026-10-21\js\work\Audio_Claude_Handoff\AUDIO_DEPLOY_HANDOFF.md`,
дауыс есебі `External\VO_ElevenLabs\Documentation\README.md`.
`Saved/` пен `External/` git-ке кірмейді (`.gitignore`): релиздер, есептер, тизер, шикі дыбыстар тек осы компьютерде.

---

## 3. Хронология — не, қашан қосылды

**Коммиттелген (GitHub `main`):**

| Күні | Коммит | Не жасалды |
|---|---|---|
| 10-19 | `27ab301` | UE 5.8 C++ жобасы: GameMode, кейіпкер (GAS), NPC мен тапсырмалар, көліктер, ауа райы, атмосфера, процедуралық дыбыс, HUD; редактор модулі; KAP_Steppe картасы, Data Table-дар |
| 10-21 | `98f8201` | Жол үстіндегі ғимараттарды алып тастау/жылжыту |
| 10-21 | `8d2d398` | Жазылған дыбыс банктері (CC0/CC-BY), Windows TTS дауыстары (FNV-1a хэш), SDF-тен тегіс көлік корпустары |
| 10-21 | `83b8fb1` | Экономика, бүйір жұмыстар, дүкен, полиция, әскери аймақ, басты мәзір, тегіс киінген денелер, жабдық мештері, кирилл қаріпі |
| 10-21 | `0d77e07` | Нақты карта, өнімділік (game thread 23–30 → 7–12 мс), кинематографиялық атмосфера, мәзір сахнасы, freesound дыбыстары |
| 10-22 | `028877c` | Кіріспедегі автобустың аударылуы мен радиатор крашы түзетілді, көлікті қайта тұрғызу |
| 10-22 | `10a2cdf` | Жеке атыс дыбыстары, мәзір музыкасы «Бос үміт», V3 кейіпкерлер, V4 (MetaHuman басы — тек жергілікті) |
| 10-22 | `64165f2` | NPC зақымы, көлік қағуы, асфальт/топырақ жолдар, ауыл қайта жасалды, GPS навигатор, көлік камерасы |
| 10-22 | `7d22e83` | Карта қайта жүктелгенде дыбыс крашы түзетілді |
| 10-22 | `b953a0c` | Динамикалық сөйлемдерге дауыс тізбегі, VO сценарий құралдары |
| 10-22 | `382dae1` | Ашылатын есіктер, ішкі жарық, асфальт қадамдары |
| 10-23 | `97646ba` | 15 с тизер-трейлер (Movie Render Queue, ProRes + MP4) |
| 10-23 | `327973f` | Майордың аты → Ғабит Құрманов |
| 10-23 | `98aa978` | Жаңа дала әлемі: өзен, атырап, көл, оазис, егістік, шекара таулары, ауа райының жаңа түрлері, жел/батпақ көлікке әсері, сексеуіл, жануарлар, белсенді рагдолл, тұрғындардың күн тәртібі |
| 10-23 | `c22b1fb` | Сексеуіл тәжі жеңілдетілді, unity-құрастыру атау қақтығыстары |
| 10-23 | `b82ce10` | 169 ElevenLabs дауысы импортталды, `-kapaudiocheck` |
| 10-23 | `96f5b03` | Кіріспе: көлік бұзылуы (бу, апат шамы, антифриз), есік ашылып кейіпкер түседі, шын жүру анимациясы; NPC LOD камераға қарай; көлік есіктері; «қорап дене» → жиырылған костюм; жәшіктер жерге түсті; эрозиялы орманды таулар (75 мың шырша); бу/түтін материалы; `-kapfixshots` |
| 10-27 | (осы коммит) | Кеңейтілген сюжет «Кезек аяқталған жоқ» (Codex сценарийі бойынша): S01–S12 сахналары, Мәрия апа, директор Серік пен Сапар Аманов, Бекзаттың рұқсаты, Сапардың жол бөгеті, директордың қоңырауы мен бұғатталған арна, ретранслятор, есеп алдындағы шолу, жаңа финал мәтіндері; ұңғымалар баяу бұзылады; диалог оқылғанда уақыт тоқтайды; еркін мәтінді мини-ойын қадамы түзетілді; `-kapstorytest` |
| 10-27 | `2f7445c` | Өлген жануарлар ауада ілінбей, жерге табиғи жатады: FBX-тегі ×100 сүйек масштабы жойылды (Blender `to_cm`, бес жануар қайта экспортталып импортталды), әр сүйекке капсула мен буын шегі бар жаңа физика ассеті (`BuildAnimalPhysics`), өлгенде анимация тоқтайды, 6 с кейін денелер ұйықтайды; `-kapanimaltest` |

**Коммиттелмеген (Codex, дискіде, соңғы EXE-де бар):**

| Күні | Не жасалды |
|---|---|
| 10-23 | V5 кейіпкерлер (MakeHuman CC0), V5 киім (Worker, Civilian, FemaleCasual), 3 LOD |
| 10-23 | Мутант пен қасқырдың Blender модельдері, қасқыр анимациясы, мутанттың секіру шабуылы, 21 creature FX дыбысы, `-kapcreaturetest` |
| 10-23 | Fab Stalker саудагер киімі Ерболатқа (24 сүйек риг, idle/walk/run, 3 LOD), `-kapstalkertest` |
| 10-23 | Ақылды NPC: жергілікті A* (2.5 м тор, тар өткелге 0.5 м), көру/есту, соңғы белгілі орынды іздеу, таса, оқтау 2.6 с, қасқыр тобы, бейбіт адамдардың қауіптен қашуы, `-kapaitest` (25) |
| 10-24 | Кіріспені өткізгенде автобус ауада қалатын — `PlaceAt` Chaos күйін тазалайды, тұрақта қол тежегіш, `-kapvehiclegroundtest` |
| 10-24 | Қару V2: 11 Blender модель, 22 меш, ADS, кері тебу, оқтау, гильза/трассер (96 компоненттен аспайды), Кедр мен K-12 дүкенде, NPC АК-сы, `-kapweaponstest` (28) |
| 10-24 | Тірі кеніш: 8 қызметтік көлік (кинематикалық, Chaos емес), 4 КПП, әскери аймаққа жол, басқарма қақпасы, 14 тұрғын, 6 кейіпкер мен 6 тапсырма, `I` инвентарь, 9 ұяшық, отыру, сақталатын перне ауыстыру, `-kaplivingtest` (37) |
| 10-25 | Қару V3 (геометрия қайта жасалды, 98 880 үшбұрыш LOD0, 512² BaseColor/ORM), балтаны ұстау қалпы, 4 бөлімді жеке сөмке, 5 бөлімді баптаулар (ойын, тышқан, пернетақта, экран/графика, дыбыс), экранды 15 с растау/қайтару, тұрақты тапсырма мен «келесі әрекет», F1 бес бетті нұсқаулық, `-kapexperiencetest` (21) |

---

## 4. Жоба құрылымы

```
D:\KAPSimulator
├─ KAPSimulator.uproject      UE 5.8; плагиндер: ChaosVehicles, ProceduralMeshComponent, MovieRenderPipeline,
│                             AppleProResMedia, GameplayAbilities, PythonScriptPlugin, EditorScriptingUtilities,
│                             ModelingToolsEditorMode, GameInputWindows
├─ Play.bat, Ойынды ашу.lnk   соңғы релизді ашады (жоқ болса — редактор арқылы -game)
├─ README.md, CREDITS.md, PROJECT_HANDOFF.md (осы файл)
├─ Config/                    DefaultEngine.ini (Lumen, VSM, Nanite, DX12, ray tracing, runtime grass, DDC, шейдер жұмысшылары),
│                             DefaultGame.ini (пакеттеу: KAP/Data NonUFS, DirectoriesToAlwaysCook)
├─ Source/KAPSimulator/       ойын модулі (Runtime)
│   └─ KAP/{AI,Audio,Core,NPC,Player,UI,World}, KAPTypes.*
│   (+ UE Vehicle Template қалдықтары: KAPSimulatorPawn = Chaos көлік павны, OffroadCar, SportsCar, Variant_*)
├─ Source/KAPSimulatorEditor/ редактор модулі: процедуралық активтер генераторлары (Python командлет шақырады)
├─ Content/KAP/               ойын активтері (Git LFS): Maps, Data, Materials, Meshes, Textures, Audio, Characters (V3, V4*, V5, FabStalker*),
│                             Animals, Creatures, Weapons (V2, V3), Foliage, Fonts, Cinematics
├─ Content/Characters/Mannequins, Content/Vehicles, Content/VehicleTemplate — UE шаблон контенті
├─ Scripts/                   Python (UE ішінде), PowerShell (verify_*.ps1 тексерушілер), C#, Blender скрипттері (Scripts/blender)
├─ SourceArt/                 Blender/FBX бастапқылары: CharactersV3, CharactersV4*, CharactersV5, ClothingV1, AnimalsV1,
│                             CreaturesV1, FabStalker*, WeaponsV2, WeaponsV2Refined, WeaponsV3
├─ External/                  шикі дыбыстар мен дауыстар (git-те жоқ)
└─ Saved/                     логтар, релиздер, есептер, скриншоттар, тизер (git-те жоқ)
```
`*` — лицензия бойынша жария репоға жүктелмейді.

### 4.1 Ойын коды (`Source/KAPSimulator/KAP`)

«(Codex)» — коммиттелмеген файл немесе бөлік.

| Файл | Жауапкершілігі |
|---|---|
| `KAPTypes.h/.cpp` | Data Table жолдары, ортақ типтер, `RepairSteps` |
| `AI/KAPAI.*` | Силлабустағы AI тақырыптары таза C++: A* (жол графы, навигатор), Байес желісі (ұңғыма бұзылуы, жұмысшы ауруы), MDP + Q-learning (жасақ/бандит/монстр), HMM (дауылдағы монстр дыбысы, жұмысшы күйі), CSP (бригадаларды бөлу) |
| `Core/KAPGameMode.*` | Ойын циклі: уақыт, ауа райы, ұңғымалар, сюжет/квесттер, диалогтар, монстр режиссері, навигатор қауіп аймақтары, HMM жылу картасы, CSP бригадалары, концовкалар, **кинематика қозғалтқышы** (`StartCine`, `CineCamera`, `Intro`, `EndingFail/Success`, `MotherScene`, `Credits`), көлікке мінү/түсу, тест режимдерін іске қосу |
| `Core/KAPGameInstance.*` | Сессия күйі (`FKAPSession`: Quests, QuestData, Flags, QuickSlots, StoryTracked…), SaveGame |
| `Core/KAPDataSubsystem.*` | `Content/KAP/Data/*.csv` → DataTable, жол графы |
| `Core/KAPEconomy.cpp` | Теңге, келісімшарттар (полиция/саудагер/әскер), дүкен (Кедр, K-12 қосылды), полиция бөлімі, әскери аймақ |
| `Core/KAPLivingStory.cpp` (Codex) | Тірі кеніштің кейіпкерлері, диалог тармақтары, тапсырма шарттары, жергілікті әңгімелер |
| `Core/KAPStoryExpansion.cpp` | Кеңейтілген сюжет (§5.1): S01–S12, Мәрия апа, Ерланның сахналары, Бекзат S06, Сапардың бөгеті мен лагері, апат журналы, директордың қоңырауы, ретранслятор, есеп шолуы, Ана сахнасының мәтіндері, титрдегі «ОҚИҒА» |
| `Core/KAPStoryTest.cpp` | `-kapstorytest`: сюжетті басынан титрге дейін өтеді (34 тексеріс) |
| `Core/KAPSettings.*` (Codex) | Ойыншы баптаулары (`GameUserSettings.ini` → `KAPSettings` секциясы), перне бекіту |
| `Core/KAPMenuScene.cpp` | Басты мәзір сахнасы (түн, дауыл, ұңғыма, инженер) |
| `Core/KAPTrailer.cpp` | 15 с тизер (`-kaptrailer`, `-kaptrailershots`) |
| `Core/KAPRegistry.*` | Актерлердің жылдам тізімдері (TActorIterator орнына) |
| `Core/KAP*Test.cpp` (Codex) | `KAPExperienceTest`, `KAPLivingWorldTest`, `KAPWeaponsTest`, `KAPSmarterNPCTest`, `KAPStalkerTest`, `KAPVehicleGroundTest`, `KAPCreatureTest` — §11 |
| `Player/KAPCharacter.*` | Ойыншы: бірінші жақ, GAS survival, инвентарь, әрекет, батпақ/су, дене температурасы, отыру |
| `Player/KAPCharacterCombat.cpp` (Codex) | Қолдағы қару, көздеу, кері тебу, оқтау кезеңдері, атыс |
| `Player/KAPSurvivalAttributeSet.*` | GAS атрибуттары: HP, Су, Температура, Доза, Төзімділік |
| `Player/KAPPlayerController.*` | Enhanced Input (кодта құрылады), жаяу/көлік басқару, қайта бекітілетін пернелер (Codex) |
| `Player/NavigatorComponent.*` | Планшет навигаторы: «ең қысқа», «ең қауіпсіз», «жолсыз тура» (A*) |
| `NPC/KAPNPC.*` | Барлық адамдар мен монстрлар (17 түрі: Worker, Villager, Elder, Militia, Guard, Bandit, Survivor, Irradiated, Wolf, Worm, Mother, Police, Soldier, Trader, Boy, Girl, SickChild), Q-кесте ұрыс AI, таса, шегіну, LOD, рагдолл, көлік қағуы |
| `NPC/KAPNPCNavigation.cpp` (Codex) | Жергілікті A* бағыты, кедергі тексеру, топтағы арақашықтық |
| `NPC/KAPNPCPerception.cpp` (Codex) | Көру, есту, соңғы белгілі орын, іздеу |
| `NPC/KAPNPCLook.cpp` | NPC сыртқы түрі: V5 киім → V5 → V4 → V3 → ескі дене тізбегі, материал, каска/дулыға/респиратор/рюкзак |
| `NPC/KAPNPCCreatures.cpp` (Codex) | Мутант/қасқыр мештері мен анимациясы, мутанттың секіру шабуылы |
| `NPC/KAPNPCStalker.cpp` (Codex) | Саудагердің Fab Stalker меші, риг анимациясы, қалпына келу |
| `NPC/KAPNPCTasks.cpp` | Жөндеу, жүк тасу, күзет, от басы, қашу; бейбіт адамдардың қауіпке жауабы (Codex) |
| `NPC/KAPLife.*` | Белсенді рагдолл (PhysicalAnimation) + тұрғындардың күн тәртібі, шопан |
| `NPC/KAPAnimal.*` | Дала жануарлары, табындар, үрей, жылқы шабысы, қасқыр үйірлері |
| `UI/KAPHUD.*` | Барлық UI Canvas арқылы: HUD, компас, миникарта, субтитр, нысана/тигізу белгісі, планшет, диалогтар, жөндеу мини-ойындары, мәзірлер, титр |
| `UI/KAPHUDInventory.cpp` (Codex) | `I` жеке сөмке: Қорлар, Жабдық, Бөлшек/құжат, Қару/құрал; 1–9 ұяшықтар |
| `UI/KAPHUDPreferences.cpp` (Codex) | Esc → Баптаулар (5 бөлім), экранды растау/қайтару |
| `UI/KAPHUDGuide.cpp` (Codex) | F1 нұсқаулық (5 бет), «келесі әрекет» кеңесі |
| `UI/KAPHUDMap.cpp` | Нақты карта: әлемді жоғарыдан түсіріп 2048² текстура, масштаб, белгі қою |
| `UI/KAPHUDShop.cpp` | Дүкен, жұмыс тақтасы, басты мәзір |
| `World/KAPTerrain.h` | **Жер бедері мен биомдардың бір ғана сипаттамасы** (редактор мен ойын бірдей оқиды): өзен, атырап, көл, оазис, құм, бөктер, егістік, биіктік; шекара тауларының торы `KAPTerrain::Mtn` |
| `World/KAPWorldBuilder.*` | CSV-ден әлемді құрастырады: ұңғымалар, ауылдар, басқарма, мұнаралар, жолдар, көліктер, жәшіктер, әрекет нүктелері |
| `World/KAPLivingWorld.*` (Codex) | Жол қозғалысы (8 кинематикалық көлік), КПП шлагбаумдары, емхана, тірі кеніштің әрекет нүктелері |
| `World/KAPNature.*` | Су, көпірлер, ағаштар, тастар, сексеуіл (сынады), тау меші, **тау орманы** (`BuildMountainForest`), шекара қабырғасы |
| `World/KAPStructure.*` | Ғимараттар (вагон, үй, киіз үй, мұнара, басқарма, т.б.) және `AKAPInteractSpot` |
| `World/WellActor.*`, `WellManagerSubsystem.*` | Ұңғыма визуалы мен күйі; 21 ұңғыманы басқару, Байес болжамы, доза, мутация, бригадалар |
| `World/KAPVehicleHandle.*` | Көлік күйі: жанармай, қызу, автопилот (кіріспе), фаралар, `PlaceAt` (Chaos күйін тазалап қою), аударылса тұрғызу, жаяуды қағу |
| `World/KAPVehicleShell.cpp`, `KAPVehicleBody.cpp`, `KAPVehicleSpec.h` | Көрінетін корпус (SDF мештері) пен дөңгелектер; координаталар корпус жүйесінде (X=0 база ортасы, Z=0 жер) |
| `World/KAPVehicleDoor.cpp` | Жүргізуші есігі (Буханка, УАЗ-469, Нива), бу, антифриз, апат шамдары |
| `World/KAPVehicleEnv.cpp` | Жел аэродинамикасы, батпақ, су, тайғақ асфальт |
| `World/KAPWeaponVisuals.*` (Codex) | Қару модельдері (V2/V3), құрал ұстау қалыптары, NPC қаруы |
| `World/KAPCombatFX.*` (Codex) | Гильза, трассер, тигізу әсерлері (ортақ қор) |
| `World/WeatherManager.*`, `KAPWeatherFX.*`, `KAPAtmosphere.*` | Тәулік пен ауа райы; жаңбыр, найзағай; шаң, іздер, прожекторлар |
| `World/KAPFXManager.*` | Niagara-сыз бөлшектер (түтін, бу, шаң, от, қан, лай…); газ түрлері `M_KAP_Puff` материалымен |
| `World/KAPDoor.*` | Ғимарат есіктері (адам ашады, монстр тарсылдатады) |
| `World/KAPWorkshop.cpp` | Ұңғыма цехы (ангар, сирена, маяк) |
| `World/KAPVisuals.*` | `FKAPMat` (PBR параметрлері) және пішіндерден құрастыру `KAPVis::Add`, `GroundZ` |
| `World/KAPProjectile.*` | Молотов, сигнал шамы |
| `World/KAPNatureTest.cpp`, `KAPFixTest.cpp` | Тест режимдері (скриншот + журнал): `-kapnatureshots`, `-kapfixshots`, `-kapanimaltest` |
| `Audio/KAPAudio.*`, `KAPAudioBanks.cpp` | Процедуралық синтез (жел, жаңбыр, от, радио шуы, двигатель, жүрек, Гейгер…) + жазылған банктер + VO; дыбыс деңгейлері баптаудан (Codex) |
| `Audio/KAPAudioCheck.cpp`, `KAPAudioCheckData.inl` | `-kapaudiocheck` (деректер `Scripts/gen_audio_check.ps1`-тен) |

### 4.2 Редактор генераторлары (`Source/KAPSimulatorEditor`)

`UKAPEditorLibrary` (Python-нан `unreal.KAPEditorLibrary.<функция>()` деп шақырылады). `KAPEditorLibrary.cpp` ішіне .inl файлдары қосылады:

| Файл | Не жасайды |
|---|---|
| `KAPEditorLibrary.cpp` | Ортақ көмекшілер (`NewMaterial`, `SaveMesh`, `SaveTex`, `FMeshBuild`), негізгі материалдар (`M_KAP_Surface`, `M_KAP_Translucent`…), құм, қырланған текше/цилиндр, DataTable импорты, ескі деңгей құру |
| `KAPVehicleGen.inl` | Көлік корпустары: SDF → surface nets → аймақ бойынша кесу → Nanite (`SM_KAP_Veh_<id>_<0..4>`), шина мен диск; `BuildShell` басқа модельдерге де қолданылады |
| `KAPHumanGen.inl` | Ескі тегіс денелер (`SKM_KAP_BodyM/F`), каска, дулыға, респиратор, рюкзак, қалпақ |
| `KAPRoadGen.inl` | Асфальт пен топырақ жол текстуралары, `M_KAP_Road` |
| `KAPTerrainGen.inl` | Биом текстурасы, `MPC_KAPEnv`, `M_KAP_Landscape8`, `M_KAP_Water`, `M_KAP_Mountain` (+ `T_KAP_CanopyN/H`, `T_KAP_RockN`), `SM_KAP_Mountains`, ландшафтты қайта құру, `PlacePlayerStart`, `BuildMountainAssets` |
| `KAPNatureGen.inl` | Өсімдік материалы (жел), шөп/бидай/қамыс, сексеуіл, терек/тал/қарағаш, ландшафт шөп түрлері `LGT_KAP_*` |
| `KAPPropGen.inl` | `SM_KAP_Spruce_*` (шырша) + `M_KAP_Spruce`, `SM_KAP_SuitPile_*` (тасталған костюм), `M_KAP_Puff` (бу/түтін) |
| `KAPAnimalPhysGen.inl` | `BuildAnimalPhysics`: жануарлардың `SKM_KAP_*_PhysicsAsset` — әр сүйекке төбелерге өлшенген капсула, аяқ/мойын/омыртқа буын шектері, қиылысқан денелердің соқтығысы өшірулі |

### 4.3 Деректер (`Content/KAP/Data/*.csv` → `DT_*`)

Баланс пен әлемнің орналасуы кодсыз өзгереді. CSV ойында `KAP/Data` арқылы да оқылады (`DirectoriesToAlwaysStageAsNonUFS`).
CSV өзгерсе, DataTable-ды қайта импорттау керек (тірі кеніш үшін — `Scripts/import_living_world.py`).
Codex `Locations`, `Nodes`, `Roads`, `Quests`, `Weapons` кестелерін өзгертті (коммиттелмеген).

| Файл | Мазмұны |
|---|---|
| `Locations.csv` | 23 орын: вагон-үйлер, ауылдар, басқарма, мұнаралар, ұялар, полиция, дүкен, әскери аймақ (метр координаттары) |
| `Wells.csv` | 21 ұңғыма |
| `Quests.csv` | 13 негізгі + 12 қосымша тапсырма |
| `Vehicles.csv` | 6 ойыншы көлігі, бастапқы күйі (radiator / ok / parts / locked) |
| `Weapons.csv`, `Items.csv` | 13 қару, 25 зат |
| `Config.csv` | Тәулік ұзақтығы, құлдырау уақыттары, доза шектері, мутация, финал шарттары |
| `Brigades.csv` | 3 жөндеу бригадасы |
| `Names.csv`, `Nodes.csv`, `Roads.csv` | Жұмысшы аттары, жол графы (44 түйін, 65 жол) |

---

## 5. Негізгі жүйелер қалай жұмыс істейді

- **Әлем қалай құрылады.** Картада тек ландшафт, жарық, атмосфера тұр. `AKAPGameMode::BeginPlay` → `AKAPWorldBuilder::BuildAll`
  CSV бойынша бәрін кодпен салады (ұңғымалар, ғимараттар, жолдар, көліктер), сосын `AKAPNature::Build` (су, көпір, ағаш, сексеуіл,
  таулар, орман), жануарлар (`AKAPFauna`), тұрғындар (`AKAPLife`), тірі кеніш (`KAPLivingWorld`). Ғимараттар мен ұңғымалар —
  `KAPVis::Add` пішіндері (қырланған текше/цилиндр + `FKAPMat` процедуралық PBR: тат, шаң, гофр, жіктер).
- **Жер бедері.** `KAPTerrain.h` — аналитикалық функциялар (метрмен). Редактор ландшафтты осыдан салады (`RebuildTerrain`,
  2017² нүкте, 1.2 м), ойын су, көпір, батпақ, өсімдікті осыдан алады. Таулар — бөлек Nanite меш (`SM_KAP_Mountains`,
  2800 × 420 полярлық тор, эрозиялы fbm, ~2.3 млн үшбұрыш); орман ойында сол тордың үшбұрыштарына дәл отырғызылады.
- **Кейіпкерлер.** Адамдар UE `SK_Mannequin` қаңқасы мен `ABP_Unarmed` анимациясын қолданады (саудагер мен қасқырдың өз риги бар).
  Түр/киім `KAPNPCLook.cpp`-та таңдалады. NPC-де өз AI бар (контроллерсіз); алыстағылардың tick жиілігі азаяды
  (`UpdateLod`, ойыншыға да, камераға да қашықтық ескеріледі). Жол табу — жол графы (A*) + жергілікті A* (кедергі айналу).
- **Кинематика.** Sequencer емес: `StartCine(Dur, Update(T, Dt), OnEnd)` әр кадрда камера мен актерлерді кодпен қозғайды.
  Кіріспе (`Intro`) — 62 с: автопилотпен жүретін Буханка, бұзылу, есік, кейіпкер шын CharacterMovement-пен жүреді
  (капсула тек WorldStatic-ке тіреледі), құм дауылы. Соңында/өткізгенде көлік `PlaceAt` арқылы тұраққа қойылып, қол тежегіш басылады.
- **Ұрыс.** Ойыншы атысы — камера сәулесі, қабырға мен ұңғы алдындағы кедергі тексеріледі; оқ саны оқтау аяқталғанда өзгереді.
  NPC атысы — ықтималдық + көріну тексеруі. Тигізу белгісі HUD-та.
- **Дыбыс.** `UKAPAudio`: банк `/Game/KAP/Audio/{SFX,Music}/<Банк>_<i>` (`Pick` бірінші жоқ индексте тоқтайды), банк жоқ болса — синтез.
  Дауыс файлы — `VO_<мәтіннің UTF-16 FNV-1a хэші, 8 hex>_<m|f>`; мәтінді өзгертсең, файл аты да өзгереді.
  Рацияда дауыс жынысы `мәтін ұзындығы % 3 == 0` → әйел. Динамикалық сөйлемдер `SpeakChain` тізбегімен (бөліктер арасы 0.06 с).
- **UI.** UMG жоқ, бәрі `AKAPHUD` Canvas-та (сөмке, баптаулар, нұсқаулық та). Карта ойын ішінде жоғарыдан түсіріліп жасалады.
- **Көлік.** Chaos шаблон павнының рамасы көрінбейді, оның орнына SDF корпус пен шын айналатын дөңгелектер. Жел мен батпақ
  физикаға күш ретінде қосылады. Аударылса 3 с кейін тұрғызылады. Жолдағы 8 қызметтік көлік — Chaos емес, жеңіл кинематика.
- **Сақтау.** `UKAPGameInstance` ішіндегі `FKAPSession` (`UPROPERTY(SaveGame)`) → SaveGame. Баптаулар бөлек:
  `GameUserSettings.ini` (`KAPSettings` секциясы + `UGameUserSettings`), жаңа ойын оларды өшірмейді.

### 5.1 Кеңейтілген сюжет «Кезек аяқталған жоқ» (2026-10-27)

Дереккөзі: Codex-тың сценарий пакеті `C:\Users\rusya\Documents\Codex\2026-10-27\j\outputs\KAP_Simulator_Story`
(оқиға, техникалық сценарий, сахна манифесі, 100 реплика KK/RU). Код — `Core/KAPStoryExpansion.cpp`; GameMode-та тек бір жолдық
ілгектер (`Talk`, `SpotText/SpotUse`, `ObjectiveText`, `QuestProgress`, `Tick`, `OnWellRepaired`, `OnActorKilled`, `Ending`,
`MotherScene`, `Credits`). Жаңа тапсырмалар `Quests.csv`-та `m_report`-тан кейін: `st_erlan`, `story_erlan_family`,
`story_gate_testimony`, `story_roadblock`, `story_director_call`, `story_repeater`, `story_bags`.

Негізгі жол енді: … күзет (Бекзат, қару) → диспетчер №3-ке жібереді → **S02 Ерлан** (сөмке, гараж «Ертең дайын») →
**S03 Мәрия апа** Ақсайда (екі кесе, жейде) → жейде Ерланға → 5 ұңғыма → **S06 Бекзат** рұқсатты өз қолымен береді (бұйрықтағы
өз қолы; куәлік / жұртқа айту / жол ашу) → **Сапардың бөгеті** басқарма қақпасының алдында (1500 ₸ төлеу, Бекзаттың жігіттерін
атау — S06-да «жол ашыңыз» таңдалса, не ұрыс; шешілмейінше қақпа ашылмайды) → **S09 апат журналы** (3 бет, Майраның сұрауы,
Серіктің «Ауысым тоқтатылмасын», кету тізімі 03:40, «Ескерту жойылғанша уақытша қабылдансын») → **S10 директордың қоңырауы**
(пара: «тек жабдық ақауы»; бас тартса не кейінге қалдырса — арна бұғатталады, «жұмсартуға» келіссе — арна ашық) →
**ретранслятор** 3-аймақ мұнарасында (кабель ×1, директор жалдаған 3 күзетші) → **есеп алдындағы шолу** (дәлелдер тізімі,
түпнұсқа/жұмсартылған нұсқа) → бұрынғы техникалық шарттар → финал → **S12 Ана сахнасы** (жейде, екі кесе, Ерланның хабары,
эпилог) → титрде «ОҚИҒА» бөлімі. Қосымша: S01 (рация, дауылда), S04 (Айгерім), S05 (Данияр жүк тізімін түзетеді), S07 (Майра),
S08 (Нұрлан куәгер), S08B (Сапар Қаражарда алыстан сөйлеседі; лагерь тазартылса — жұмысшылардың 3 сөмкесі Төлеген ақсақалға,
Сапардың есеп дәптері — кету уақыты 04:10). Ескі сейвтер бұзылмайды: рұқсат бұрын берілген болса, жаңа кедергілер іске қосылмайды.

**Ұңғымалар баяу:** `Config.csv` — `hazardScale` 0.06→0.03, ақау→ағу 6→10 сағ, ағу→өрт 5→8, өрт→жоғалу 2→3;
`maxOpenFaultsEarly` 2 / `maxOpenFaults` 3 (бір мезгілде ашық ақау шегі; бұдан көп болса кездейсоқ ақау шықпайды),
`faultGraceHours` 10 (жөнделген ұңғыма 10 сағ тыныш). Бастапқы ақаулар: №1, №3 (Ерланның), №7, №13. Найзағай ақауы 50%.
Диалог ашық тұрғанда ойын уақыты, ұңғымалар мен доза тоқтайды (`StoryReading`).

---

## 6. Ассеттер: не процедуралық, не импортталған

| Ассет | Қайдан | Қалай қайта жасалады |
|---|---|---|
| Ландшафт, биом текстурасы, су/ландшафт/тау материалдары | Код (`KAPTerrain.h`, `KAPTerrainGen.inl`) | `Scripts/build_terrain_assets.py` (NullRHI) → `Scripts/rebuild_landscape.py` (`-AllowCommandletRendering`) |
| Шекара таулары, шырша, тасталған костюм, бу материалы | Код (`KAPTerrain::Mtn`, `KAPPropGen.inl`) | `Scripts/build_mountain_assets.py` (NullRHI; ландшафтқа тимейді) |
| Көлік корпустары, шина, диск | Код (`KAPVehicleGen.inl`) | `Scripts/vehicle_meshes.py` |
| Өсімдіктер, сексеуіл, ағаштар | Код (`KAPNatureGen.inl`) | `build_terrain_assets.py` |
| Жол текстуралары | Код (`KAPRoadGen.inl`) | `Scripts/road_assets.py`, `road_material.py` |
| Ғимараттар, ұңғымалар, жабдық, КПП | Ойын коды (`KAPVis`, `KAPLivingWorld`) | Жасалмайды — ойын кезінде салынады |
| V3 кейіпкерлер | Blender (`SourceArt/CharactersV3`) | `Scripts/import_character_assets_v3.py` |
| V4 кейіпкерлер (MetaHuman басы) | Blender + MetaHuman (**тек жергілікті**) | `Scripts/blender/build_v4_heads.py` + `import_character_assets_v4.py` |
| V5 кейіпкерлер мен киім (Codex) | MakeHuman/MPFB CC0 (`SourceArt/CharactersV5`, `ClothingV1`) | `import_character_assets_v5.py`, `import_clothing_v1.py`, `fix_character_materials_v5.py` |
| Саудагер (Codex) | Fab Stalker (`SourceArt/FabStalker/Rigged`) | `Scripts/blender/build_stalker_trader.py` → `import_stalker_trader.py` |
| Жануарлар | Blender процедуралық (`Scripts/blender/build_animals_v1.py`) | `Scripts/import_animals_v1.py` |
| Мутант, қасқыр (Codex) | Blender + MakeHuman CC0 (`SourceArt/CreaturesV1`) | `blender/build_creatures_v1.py` → `import_creatures_v1.py` |
| Қару V2 / V3 (Codex) | Blender (`SourceArt/WeaponsV2Refined`, `WeaponsV3`) | `blender/build_weapons_v2.py` / `build_weapons_v3.py` → `import_weapons_v2.py` / `import_weapons_v3.py`, `refine_weapons_v3_materials.py` |
| Дыбыс банктері | `External/Audio` (CC0/CC-BY) | `import_audio.py`, `import_freesound.py`, `import_guns_menu.py`; тексеру `import_audio_handoff.py` |
| Дауыстар | ElevenLabs (`External/VO_ElevenLabs`), орынбасар Windows TTS (`External/VO`) | `import_vo_elevenlabs.py` немесе `import_audio_handoff.py` |
| Тизер активтері | `LS_KAPTrailer`, `MPC_KAPTrailer` | `make_trailer_assets.py`, рендер `render_trailer.ps1` |

---

## 7. Дереккөздер мен лицензиялар

Толығы — `CREDITS.md`. Қысқаша:

- **Дыбыстар, CC-BY 3.0 (авторды көрсету міндетті):** creativeheroes (Helmut Scream), thanvannispen (Male_Thijs_loud_scream),
  Vincent Sevedge (Gunshot sounds — ПМ, АК, снайпер, мылтық).
- **Дыбыстар, CC0:** Kenney (Impact Sounds, RPG Audio), JohnsonBrandEditing (scream), domasx2 (двигатель), Darsycho (zombie moans),
  OpenGameArt Hungry ghoul, HaelDB (айқай, ыңырсу; тіршілік иелерінің 21 дыбысы осыдан өңделген), Joth (Ambience Pack 1),
  yd (Insistent), congusbongus (Abandoned passages), Nox_Sound (құмдағы қадам), jameswrowles (дизель генератор),
  mchrpt (винтовка), cookies+policy (Гейгер).
- **Музыка:** «Бос үміт» (мәзір) — жоба авторы, ChatGPT + FL Studio көмегімен.
- **Дауыстар:** ElevenLabs Eleven v3 (Stability 50%), алты дауыс: Nurlan, Bolat, Aizhan, Serik, Bekzat, Yerlan; бір тыныс
  алу — ElevenLabs Sound Effects v3. Жазылмағандары — Windows TTS (Microsoft Pavel / Irina, ru-RU; қазақ әріптері орысшаға жуықталады).
- **Кейіпкерлер:** UE Mannequin (Epic, UE лицензиясы); V5 мен киім — MakeHuman Community / MPFB ассеттері (**CC0**; MPFB аддон коды
  орнатылмаған); V4 — MetaHuman басы (MetaHuman лицензиясы: тек UE ішінде, **репоға жүктелмейді**, `.gitignore`).
- **Fab:** «Free sample Squad Of Surviving Stalkers» (Oleg Ushenok), Fab Standard License (иесі мақұлдаған) — саудагер Ерболатқа қосылды
  (`/Game/KAP/Characters/FabStalker`, бастапқысы `SourceArt/FabStalker`). **Бөлек таратуға және жария репоға жүктеуге болмайды.**
- **Қару модельдері** (V2, V3) — осы жоба үшін Blender-де жасалған; басқа ойындардан ештеңе алынған жоқ.
- **S.T.A.L.K.E.R.**, Call of Duty — тек көңіл-күй үлгісі ретінде айтылған; олардан ешбір файл алынған жоқ.
- **Қалғаны** (көліктер, ғимараттар, таулар, ағаштар, материалдар, процедуралық дыбыстар) — жобаның өз коды.
- QazUran, кейіпкерлер мен апат — ойдан шығарылған.

---

## 8. Құралдар мен орта

| Құрал | Қайда / не үшін |
|---|---|
| Unreal Engine 5.8 (бастапқы кодтан құрастырылған нұсқа) | `D:\ue5\UE_5.8` |
| Visual Studio 2022 Community (MSBuild, MSVC) | C++ құрастыру |
| Git + Git LFS, GitHub | `https://github.com/rusyeat/KAPSimulator`, тармақ `main`; LFS: `*.uasset *.umap *.png *.wav *.fbx` |
| Blender 5.2.2 LTS (+ Blender MCP) | Жануарлар, V3/V4/V5 кейіпкерлер, тіршілік иелері, саудагер риги, қару |
| UE-дің ішкі Python-ы | Барлық `Scripts/*.py` (жүйеде бөлек Python жоқ) |
| PowerShell 5.1, C# (`KAPAudioTool.cs`) | Дыбыс өңдеу, TTS, VO сценарийі, `verify_*.ps1` тексерушілер |
| Movie Render Queue + Apple ProRes | Тизер рендері |
| ElevenLabs, Windows TTS | Дауыстар |
| FL Studio + ChatGPT | Мәзір музыкасы |
| Claude Code (Anthropic) және OpenAI Codex | Әзірлеу (бір мезгілде, бір репода) |

Компьютер: Windows 11 Pro, бір монитор. C: дискі толуға жақын — уақытша файлдар мен DDC D:-да ұсталады
(`TMP/TEMP=D:\KAPSimulator\Saved\Tmp`, `UE-LocalDataCachePath=D:\KAPSimulator\Saved\BuildCache\DDC`,
`DefaultEngine.ini` → `[DerivedDataCacheStores]`).

---

## 9. Құрастыру, іске қосу, пакеттеу

```powershell
# Редактор + ойын кодын құрастыру (редактор ашық тұрмауы керек — DLL құлыпталады)
& "D:\ue5\UE_5.8\Engine\Build\BatchFiles\Build.bat" KAPSimulatorEditor Win64 Development -Project="D:\KAPSimulator\KAPSimulator.uproject" -WaitMutex
# UBA соңында тоқтап қалса (Codex ортасында болған) — мына қосымшалармен:
#   -NoUBA -UBARootDir=D:\KAPSimulator\Saved\BuildCache\UBA -MaxParallelActions=2

# Ойынды редактор арқылы іске қосу (тест)
& "D:\ue5\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\KAPSimulator\KAPSimulator.uproject" -game -windowed -resx=1600 -resy=900

# Python генераторын командлетпен іске қосу (мысалы, таулар)
& "D:\ue5\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "D:\KAPSimulator\KAPSimulator.uproject" -run=pythonscript -script="D:\KAPSimulator\Scripts\build_mountain_assets.py" -NullRHI -unattended -nop4 -nosplash

# Win64 пакеті (≈ 5–10 мин; аргументтерді тырнақшаға ал, әйтпесе PowerShell "UnrealEditor-Cmd.exe"-ні бөліп жібереді)
$env:TMP='D:\KAPSimulator\Saved\Tmp'; $env:TEMP=$env:TMP; $env:UE_LocalDataCachePath='D:\KAPSimulator\Saved\BuildCache\DDC'
& "D:\ue5\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=D:\KAPSimulator\KAPSimulator.uproject" -target=KAPSimulator -platform=Win64 -clientconfig=Development -nop4 -utf8output -nocompileeditor -skipbuildeditor "-unrealexe=UnrealEditor-Cmd.exe" -build -cook -stage -pak -iostore -compressed -prereqs -package -archive "-archivedirectory=D:\KAPSimulator\Saved\Releases\<Атау>"
```
- Codex ортасында shell-ден кук үнсіз іске қоспай қалатын; ол `CookWeapons.lnk` арқылы UnrealEditor-Cmd кукын нативті іске қосып,
  сосын UAT `-skipbuild -skipcook -stage -pak -iostore -compressed -package -archive` орындаған. Claude ортасында жоғарғы BuildCookRun жұмыс істеді.
- Скриншоттар: пакетте `<релиз>\Windows\KAPSimulator\Saved\Screenshots\Windows\`; редактор `-game` режимінде `Saved\Screenshots\WindowsEditor\`;
  Codex тесттері жеке `UserDir` қолданады (ойыншы сейві мен баптауы өзгермейді), суреттері сол `UserDir` ішінде.
- Кук ескертулері: тек ескі `M_KAP_Landscape5` материалынікі (қолданылмайды, зиянсыз). Соңғы кук: 1858 пакет, 0 қате, 9 ескерту.

---

## 10. Пайплайндар (рет маңызды)

1. **Жер бедерін өзгерту** (`KAPTerrain.h`): `build_terrain_assets.py` (NullRHI) → `rebuild_landscape.py` **міндетті түрде**
   `-AllowCommandletRendering`-пен (UE 5.8 ландшафты әрқашан edit-layer, биіктік GPU-да біріктіріледі; NullRHI-да ландшафт тегіс
   және соқтығыссыз сақталады) → скрипт `PlacePlayerStart` шақырады (PlayerStart болмаса ойыншы (0,0,0)-де туып, жер астында қалады).
2. **Тауларды өзгерту:** тек `KAPTerrain::Mtn` → `build_mountain_assets.py`. Меш пен орман бір торды оқиды; мешті қайта жасамасаң, ағаштар ауада қалады.
3. **Ландшафт шөбі** ойын кезінде жасалады: `DefaultEngine.ini [SystemSettings]` → `grass.GrassMap.UseRuntimeGeneration=1`,
   `grass.GrassMap.AlwaysBuildRuntimeGenerationResources=1`.
4. **Дыбыс:** импорт реті — жалпы импорт → өңделген құм/генератор/винтовка → кесілген атыстар/мәзір → creature FX → ең соңында ElevenLabs VO
   (жалпы `import_audio.py` кесілмеген атыстар мен TTS-ті қайта жазып жібере алады). Тексеру: пакетте `-kapaudiocheck`.
   Кестелер өзгерсе, алдымен `Scripts/gen_audio_check.ps1`.
5. **Жаңа сөйлем қосу:** мәтінді кодқа жаз → `Scripts/vo_lines.tsv` жаңарт → `vo_script.ps1` (файл аттары) → жазба/TTS → импорт.
   Тірі кеніштің жаңа диалогтарына әлі жеке жазба жоқ.
6. **Қару:** Blender генераторы → FBX/PNG → `import_weapons_v3.py` (3 LOD, материалдар) → `refine_weapons_v3_materials.py` → `-kapweaponstest`.
7. **Жануарлар:** `blender.exe -b --factory-startup -P Scripts/blender/build_animals_v1.py` (`D:\steam\steamapps\common\Blender`;
   `to_cm` бәрін сантиметрге айналдырып масштабты қолданады — FBX-те ешбір түйінде масштаб болмауы керек) →
   `import_animals_v1.py` командлетпен (мештер, 3 LOD, анимациялар; соңында `build_animal_physics` — әр сүйекке капсула, 24 дене,
   23 буын) → `-kapanimaltest`. Тек физиканы қайта құру: `Scripts/build_animal_physics.py` (NullRHI).

---

## 11. Тест режимдері (командалық жол жалаулары)

Әр Codex тестінің `Scripts/verify_*.ps1` тексерушісі бар: журналды, скриншоттарды және EXE хэшін салыстырып, `validation.json` жазады.

| Жалауша | Тексерулер | Не тексереді | Тексеруші |
|---|---:|---|---|
| `-kapexperiencetest` | 21 | Сөмке батырмалары, V3 балта, сезімталдық, профиль, экран растауы/қайтуы, перне бекіту, F1 | `verify_player_experience.ps1` (86 бірге) |
| `-kaplivingtest` | 37 | Жол қозғалысы, КПП, басқарма қақпасы, 6 тапсырма, ресурстар, сейв, инвентарь, пернелер, отыру | `verify_living_world.ps1` |
| `-kapweaponstest` | 28 | 11 модель, 3 LOD, ADS, атыс, оқтау, оқ есебі, кедергі, тигізу, дүкен, компонент шегі | `verify_weapons_v2.ps1` |
| `-kapaitest` | 25 | NPC көру, есту, есте сақтау, оқтау, қауіп, кедергі/тар қақпа арқылы бағыт | `verify_smarter_npcs.ps1` |
| `-kapstalkertest` | 15 | Саудагер меші, LOD, материал, риг, анимация, нокаут/қалпына келу | `verify_stalker_trader.ps1` |
| `-kapvehiclegroundtest` | — | Кіріспені өткізгенде/толық кіріспеде автобус жерде, тұрақта қозғалмайды | `verify_vehicle_ground.ps1` |
| `-kapcreaturetest` | — | Мутант пен қасқыр | `verify_creatures.ps1` |
| `-kapfixshots` | — | Кіріспе (бұзылу, есік, шығу, жүру — аяқ бұрышы журналда), NPC жүрісі, костюм, жәшіктер, көліктен түсу, таулар (`KAPF_*.png`, `KAPFIX`/`KAPINTRO`) | журнал + скриншот |
| `-kapstorytest` | 34 | Кеңейтілген сюжет басынан титрге дейін: баланс (Config, 4 бастапқы ақау, ақау шегі), S01, Ерлан, Мәрия, жейде, Айгерім, Бекзат S06, бөгет (қақпа ашылмайды → Бекзаттың атымен шешіледі), журнал, директор (бас тарту → арна бұғатталды), жалдамалы күзет, ретранслятор, S11 хабар, шолу, «Құтқару», Ана сахнасы, титр (`KAPSTORY CHECK/RESULT`, `KAPS_*.png`) | журнал + скриншот |
| `-kapanimaltest` | — | Түлкі, қой, ешкі, сиыр, жылқы пышақпен, қой көлік күшімен өлтіріледі; әр дененің жерден биіктігі, дене↔сүйек арасы (0 болуы керек), ұйқы (`KAPANIM`, `KAPA_*_alive/_dead/_dead_low.png`) | журнал + скриншот |
| `-kapnatureshots`, `-kapdriveonly` | — | Өзен, көпір, атырап, оазис, таулар, сексеуіл, ауа райы, жел/батпақ көлік тесті, жануарлар (`KAPN_*.png`) | журнал + скриншот |
| `-kapaudiocheck` | — | Пакеттегі дыбыс: 47 банк, 203 VO, 182 сөйлем, ойнату | журнал |
| `-kapautotest`, `-kapperf` | — | Жалпы автотест скриншоттары, өнімділік | журнал |
| `-kapnpcshots`, `-kapvehshots`, `-kaproadshots`, `-kapmapshots` | — | NPC, көлік, жол, карта суреттері | скриншот |
| `-kapecontest`, `-kapdoortest`, `-kaphittest`, `-kapvehflip`, `-kapvochain`, `-kapaudiostress`, `-kapsounddump`, `-kapmodeltest` | — | Экономика, есіктер, зақым, аударылу, дауыс тізбегі, дыбыс стресі, дыбыс дампы, модельдер | журнал |
| `-kaptrailer`, `-kaptrailershots` | — | Тизер рендері / алдын ала кадрлар | — |
| `-kaplanddiag`, `-kapdebug` | — | Ландшафт диагностикасы; экрандағы дебаг хабарларын қалдыру | — |

---

## 12. Маңызды ережелер мен тұзақтар

- **Unity build:** әр `.cpp`-дағы файл ішілік көмекшілердің аты бірегей болсын (мыс. `NatLoadSM`, `AnimalDefs`, `IndicatorLensMaterial`) —
  әйтпесе құрастыру сынады. Ортақ UI түстері `KAPUIColors.h`-та.
- **PowerShell 5.1:** кириллицасы бар `.ps1` UTF-8 **BOM**-мен сақталсын; `H` — `Get-History` бүркеншік аты (функция аты бола алмайды);
  BuildCookRun аргументтерін тырнақшаға ал.
- **Файл кодировкасы:** бар файлдардың BOM-ын өзгертпе (бөлек git hunk болып шығады). `*.cpp *.h` — CRLF (`.gitattributes`).
- **VO атаулары** мәтін хэшінен шығады — сөйлемнің бір әрпін өзгертсең, дауыс файлы жоғалады.
- **Банк индекстері** 0-ден үзіліссіз болуы керек.
- **Кинематика актерлері:** телепорт емес, CharacterMovement-пен жүргіз; әйтпесе аяқ қозғалмайды. Көлік ішінен өтетін капсула
  көлік физикасын итереді — капсуланы тек WorldStatic-ке тірет.
- **Chaos көлігін** жылжытқанда тек `AKAPVehicleHandle::PlaceAt` қолдан (ескі суспензия контактілері қалмасын). Тежегіш кірісі кері
  газ болып та жұмыс істейді (`bReverseAsBrake`) — иесіз көлікке ұзақ тежегіш бермей, қол тежегішті қос.
- **Жәшік/нүкте** қоярда жерді трассировкамен тап (`SpawnSpot` енді өзі жасайды).
- **`SpawnStruct` жолдың үстіндегі әшекейді өшіреді** (Barrier, Crate, Campfire…): жолға әдейі қойылатын бөгетті тікелей
  `SpawnActor<AKAPStructure>` + `Build` арқылы қой (`StorySpawnRoadblock`).
- **Мини-ойын қадамдары:** `valve`, `wires`, `part[:зат]`, `pressure`, `fire` — арнайы ойындар; кез келген басқа мәтін енді
  «басып ұстау» қадамы (бұрын мұндай қадам ешқашан аяқталмайтын — емхана генераторы осыдан тұрып қалатын).
- **Жаңа тапсырма жолын `Quests.csv`-тың соңына емес, ортасына қос** (Codex соңына жазады — git hunk-тары қосылып кетеді),
  сосын `Scripts/import_story_data.py` (DT_Quests, DT_Config). Ойын алдымен DT активін оқиды, CSV — тек актив жоқ болса.
- **Қаңқалы мештерде сүйек масштабы болмасын.** Blender-дің әдепкі FBX экспорты арматура түйініне ×100 жазады, UE оны түбір
  сүйекке (`FoxRig` т.б.) қояды; UE рагдоллы (`PerformBlendPhysicsBones`) масштабы бар түбірде сүйекті денеден ығыстырады — жануар
  ауада ілінеді не жерге батады. UE импортындағы `convert_scene_unit`/`import_uniform_scale` көмектеспейді (масштабты қайтадан
  түбірге қояды); түзету тек Blender жағында (`to_cm`). FBX импортының автоматты физика ассеті де бұл мештерге жарамсыз (2–5 алып дене).
- **Өлген жануар:** меш актерде қалады (`ComponentTransformIsKinematic`), тек сүйектер денеге ереді; көлік соққан мәйіт актерден
  9 м-ге дейін ұшуы мүмкін — мәйіттің орны керек болса, `Mesh->GetBoneLocation("pelvis")` қолдан, актер орнын емес.
- **Экран баптауы:** ажыратымдылық өзгерісі HUD Canvas-ын жарамсыз етеді — қайтаруды келесі tick-ке қалдыр.
- **Codex шейдер жұмысшыларын шектеген** (`DefaultEngine.ini [DevOptions.Shaders]`) — шейдері көп командлеттер баяу.
- Ұзын жолдар: скретч папкасының жолы 260 таңбадан асып кетеді — уақытша файлдарды `D:\KAPSimulator\Saved\Tmp`-ға жаз.
- V4 (MetaHuman) мен FabStalker активтерін жария репоға **жүктеме**.

---

## 13. Белгілі мәселелер және келесі қадамдар

- **Git:** Codex-тың үлкен жұмысы коммиттелмеген (§14). Оны коммиттеу керек, бірақ FabStalker пен V4 жария репоға кірмеуі тиіс
  (`Content/KAP/Characters/FabStalker/` мен `SourceArt/FabStalker/`-ді `.gitignore`-ға қосу керек болады).
- Тек коммиттелген күй (Codex өзгерістерінсіз) бөлек құрастырылып тексерілген жоқ.
- **Дауыс:** 34 сөйлем жазылмаған (21-інде TTS орынбасары, 13-і тек субтитр); балаларға дауыс таңдалмаған
  (`External\VO_ElevenLabs\Documentation\missing_34_files.csv`). Тірі кеніштің жаңа диалогтарына жазба жоқ; бет mocap жоқ.
- **Кеңейтілген сюжет:** жаңа репликаларға дауыс жоқ (тек субтитр; `04_Dialogue_KK_RU.csv`-та voice_profile дайын).
  `-kapstorytest` бір жолды ғана өтеді (Бекзаттың атымен бөгет, директорға бас тарту, «Құтқару»); ұрыс жолы, жұмсартылған есеп,
  «Қашу», Ерланның ерте қазасы, Сапармен сөйлесу автоматты тексерілмеген. `KAPHUDGuide.cpp`-тағы `StoryGuidance` ілгегі Codex-тың
  коммиттелмеген файлында тұр; `DT_Quests.uasset` те (Codex өзгерткен бинар) коммитке кірмеді — CSV-ден `import_story_data.py`
  қайта жасайды. Кодтың тек коммиттелген күйі (Codex-сіз) бөлек құрастырылған жоқ.
- Импортталған, бірақ шақырылмайтын банктер: `ImpMetalHeavy`, `ImpMining`, `ImpPlank`, `StepCarpet`, `Indoor`.
- Ескі `M_KAP_Landscape5` материалы компиляцияланбайды (кукта ескерту) — өшіруге болады.
- Есік анимациясы тек Буханка, УАЗ-469, Нива үшін; КамАЗ, су тасушы, БТР-де жоқ.
- Бу артқы жарықта сәл сұрғылт; кіріспеде кейіпкер радиаторға тез жетеді.
- Карта ішіндегі бөктер биіктігі өзгерген жоқ (тек түсі жасылданды); тау меші карта шетінен ары басталады.
- V5 қол/саусақ пішіндері мен балалардың беті/киімі әлі өңдеуді қажет етеді. Бірінші жақтағы қол — бұрынғы қолғап пен жең (жаңа қол ригі жоқ).
- Кейбір ескі диалогтар мен көлік кеңестерінде әдепкі пернелер жазулы қалды (қайта бекітілген перне көрсетілмейді).
- Экран баптауы бір мониторда ғана тексерілді; екінші мониторға көшуді бөлек тексеру керек.
- NPC жол табуы — жергілікті әрі шектеулі; толық NavMesh, қабатаралық бағыт немесе NPC-дің есік ашуы жоқ.
- Жолдағы көліктерді айдап кетуге болмайды.
- ТТ-дан айырмашылықтар: карта 8×8 емес 2.4×2.4 км; Megascans/MetaHuman орнына процедуралық/CC0 модельдер; Sequencer, MetaSounds, Niagara орнына код.
- Барлық тексеру автоматты (журнал + скриншот); ойынды басынан аяғына дейін адам қолмен ойнап шыққан жоқ.

---

## 14. Codex-пен бірге жұмыс

Codex (OpenAI) осы репода қатар жұмыс істейді. 2026-10-27 күйі (`git status`):

- **Өзгертілген, коммиттелмеген:** `CREDITS.md`, `Play.bat`, `Config/DefaultEngine.ini`, `Config/DefaultGame.ini`,
  `Content/KAP/Data/{Locations,Nodes,Quests,Roads,Weapons}.csv` және сәйкес `DT_*.uasset`,
  `KAPAudio.cpp/.h`, `KAPAudioBanks.cpp`, `KAPEconomy.cpp`, `KAPGameInstance.h`, `KAPGameMode.cpp/.h`, `KAPNPC.cpp/.h`,
  `KAPNPCLook.cpp`, `KAPNPCTasks.cpp`, `KAPCharacter.cpp/.h`, `KAPPlayerController.cpp/.h`, `KAPHUD.cpp/.h`, `KAPHUDShop.cpp`,
  `KAPStructure.cpp`, `KAPVehicleHandle.cpp`, `KAPVehicleShell.cpp`, `KAPWorldBuilder.cpp`, `KAPSimulator.Build.cs`.
- **Жаңа, git-те жоқ:**
  - код: `Core/KAPLivingStory.cpp`, `KAPSettings.*`, `KAPExperienceTest.cpp`, `KAPLivingWorldTest.cpp`, `KAPWeaponsTest.cpp`,
    `KAPSmarterNPCTest.cpp`, `KAPStalkerTest.cpp`, `KAPVehicleGroundTest.cpp`, `KAPCreatureTest.cpp`;
    `NPC/KAPNPCNavigation.cpp`, `KAPNPCPerception.cpp`, `KAPNPCCreatures.cpp`, `KAPNPCStalker.cpp`;
    `Player/KAPCharacterCombat.cpp`; `UI/KAPHUDInventory.cpp`, `KAPHUDGuide.cpp`, `KAPHUDPreferences.cpp`;
    `World/KAPLivingWorld.*`, `KAPWeaponVisuals.*`, `KAPCombatFX.*`;
  - контент: `Content/KAP/Characters/V5/`, `Content/KAP/Characters/FabStalker/`*, `Content/KAP/Creatures/`, `Content/KAP/Weapons/`,
    `Content/KAP/Audio/SFX/Mutant*`, `Wolf*`;
  - бастапқылар: `SourceArt/CharactersV5`, `ClothingV1`, `CreaturesV1`, `FabStalker`*, `WeaponsV2`, `WeaponsV2Refined`, `WeaponsV3`;
  - скрипттер: V5, киім, тіршілік иелері, саудагер, қару V2/V3, `import_living_world.py`, `configure_packaging_session.py`,
    `configure_stalker_pack.py`, барлық `verify_*.ps1`;
  - түбірде `Ойынды ашу.lnk`; `Build/` папкасы.
- Codex-тың жұмыс папкасы: `C:\Users\rusya\Documents\Codex\2026-10-21\js\work\`; есептері — `Saved/*_202610*` (§2 кестесі).

**Ереже:** біреудің коммиттелмеген жұмысын өз коммитіңе қоспа. Ортақ файлда тек өз өзгерісіңді stage жаса:
`git diff` шығар, біреудің hunk-тарын бөлек патчқа алып, файлдың көшірмесіне `git apply -R` жаса, нәтижені
`git hash-object -w --path=<файл> <көшірме>` + `git update-index --cacheinfo 100644,<хэш>,<файл>` арқылы индекске сал.
Нөлдік контекстті (`-U0`) патч ауысқан жолдарда қате жерге түсуі мүмкін — абайла. Коммиттер соңында `Co-Authored-By:` жолы тұрады.
Бұл құжатты өзгерткенде тиісті бөлімді жаңарт (соңына бөлек жазба қоспа), жоғарыдағы күнді ауыстыр.

---

## 15. Тағы қайда не бар

- `README.md` — қысқа ТТ сәйкестігі мен іске қосу.
- `CREDITS.md` — дыбыс пен модель авторлары.
- `SourceArt/*/README.md` — әр модель жинағының шығу тегі мен лицензиясы.
- `Saved/*_202610*/` — әр жаңартудың есебі мен `validation.json` (§2 кестесі); `Saved/PlayerExperience_20261024/PLAYER_GUIDE_KK.md` — ойыншы нұсқаулығы.
- `External/VO_ElevenLabs/Documentation/` — дауыс есебі, `all_203_files.csv`, `missing_34_files.csv`.
- `Scripts/vo_lines.tsv` — барлық 182 сөйлем (кім, эмоция, тегтер).
- `Saved/Tmp/*.log`, `Saved/Logs/*` — тесттер мен пакеттеу журналдары.
