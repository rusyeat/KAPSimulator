// Процедуралық дыбыс (MetaSounds активтерінсіз): жел/дауыл, жаңбыр, от, радио шуы, двигатель,
// атыс, соққы, ұлу, жоқтау әуені
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sound/SoundWaveProcedural.h"
#include "KAPAudio.generated.h"

class UAudioComponent;

UENUM()
enum class EKAPSound : uint8 { Swing, Hit, Hurt, Pickup, Growl, Howl, Thunder, Steam, ShotFar, Beep, BeepBad, RepairDone, Footstep, Cough, Shout };

/** Үздіксіз генератор: сүзгіленген шу немесе ара тісті тон */
UCLASS()
class KAPSIMULATOR_API UKAPSynthWave : public USoundWaveProcedural
{
	GENERATED_BODY()
public:
	UKAPSynthWave(const FObjectInitializer& OI);
	virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
	volatile float Gain = 0.f, TargetGain = 0.f;
	volatile float Cutoff = 0.1f;   // 0..1 (бір полюсті LPF коэффициенті)
	volatile float HighPass = 0.f;  // 0..1
	volatile float ToneHz = 0.f;    // >0 — тон (двигатель)
	volatile float Crackle = 0.f;   // от шытырлауы
	volatile float Chirp = 0.f;     // шегіртке шырылы
	/** 0 — шу/тон, 1 — Гейгер (Пуассон шертпелері), 2 — жүрек соғуы, 3 — тыныс */
	volatile int32 Mode = 0;
	volatile float Rate = 0.f;      // Гейгер: шертпе/сек, жүрек: соққы/мин, тыныс: дем/мин
	volatile float Misfire = 0.f;   // двигатель: цилиндр жіберу ықтималдығы
	volatile float Gust = 1.f;      // жел екпіні көбейткіші
	void Int7Init(float V) { Int7 = V; }
	volatile float ResHz = 500.f;   // 4-режим: желдің резонанстық ысқырығы (Гц)
private:
	float Lp = 0, Lp2 = 0, Phase = 0, ChirpPh = 0, ModPh = 0, ClickEnv = 0, NextClick = 0, Sub = 0, SkipEnv = 1, SvL = 0, SvB = 0, ResCur = 500; int32 SampleN = 0;
	float Ph7[6] = { 0, 0, 0, 0, 0, 0 }; float Lp7 = 0, Lp7b = 0, Int7 = 0;
	/** Гейгер: нақты жазылған шертпелердің қатар ойнайтын 4 дауысы (KAPGeigerClicks.inl) */
	int32 ClkPos[4] = { -1, -1, -1, -1 }, ClkIdx[4] = { 0, 0, 0, 0 };
	uint32 Seed = 12345;
};

/** Жазылған дыбыстар банкі: /Game/KAP/Audio/<SFX|Music>/<Атау>_<i> */
USTRUCT()
struct FKAPSoundBank
{
	GENERATED_BODY()
	UPROPERTY() TArray<TObjectPtr<USoundWave>> W;
	int32 Last = -1;
	bool bLoaded = false;
};
UCLASS()
class KAPSIMULATOR_API UKAPAudio : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UKAPAudio* Get(const UObject* Ctx);
	virtual void Deinitialize() override;
	void EnsureInit();
	void SetWind(float L);
	void SetRain(float L);
	void SetFire(float L);
	void SetRadio(float L);
	void SetEngine(bool bOn, float SpeedMs);
	void Play(EKAPSound S, float Vol = 1.f);
	void Shot(FName Weapon);
	void Beep(bool bOk) { Play(bOk ? EKAPSound::Beep : EKAPSound::BeepBad, 0.4f); }
	/** Кеңістіктік дыбыс (3D, қашықтықпен әлсірейді) */
	void PlayAt(EKAPSound S, FVector Loc, float Vol = 1.f);
	/** Сөйлеуге ұқсас дауыс (формант синтезі): ұзақтығы сек, еркек/әйел, кеңістікте */
	void Voice(FVector Loc, float Seconds, bool bFemale, float Vol = 0.8f, int32 Seed = 0);
	void SetCrickets(float L);
	/** Гейгер санауышы: мкЗв/сағ → шертпелер жиілігі λ (NextClickIn = -ln(U)/λ) */
	void SetGeiger(float DoseRate, bool bOn);
	/** Ойыншы күйі: шаршау (0..1), қорқыныш/жарақат (0..1) → тыныс пен жүрек соғуы */
	void SetBody(float Exhaust, float Fear);
	/** Баспанада: жел мен жаңбыр бәсеңдейді, жоғары жиілік кесіледі */
	void SetIndoor(float K);
	/** Стингер кезінде фон дыбыстарын басу */
	void Duck(float Seconds, float Amount = 0.5f);
	/** Двигатель айналымы (0..1) және ақау (жіберу) */
	void SetEngineLoad(float Rpm01, float Misfire);
	/** Қауіп/жеңілдік стингері */
	void Stinger(bool bDanger);
	/** Жел екпіндері, алыстағы ит үруі, металл сықыры — әр кадр (GameMode) */
	void Update(float Dt, FVector Listener, bool bNight, float Storm);
	/** Ұңғыма сорғысының гулі — компонентке тіркелген цикл */
	UAudioComponent* AttachHum(USceneComponent* To, float Hz);
	/** Апат сиренасы (5-режим), дауысы қайтарылған толқынның TargetGain-і арқылы */
	UKAPSynthWave* AttachSiren(USceneComponent* To);
	/** Үрей фоны (түн/қауіп), Tension — дисонанс */
	void SetDread(float L, float Tension);
	/** Динамикалық музыка: 0 — тыныш фон, 0.3+ — ішекті тремоло, 0.6+ — барабандар */
	void SetMusic(float Intensity, float Volume);
	/** Есік: 0 — металл (сықыр + тарс), 1 — ағаш */
	void Door(FVector Loc, int32 Type);
	/** Рация арқылы сөйлеу (2D, жолақ сүзгісі, шытырлақ) */
	void RadioVoice(float Seconds, bool bFemale = false);
	/** Сирек домбыра күйі (таң/кеш, тыныш сәттер); ұзақтығын қайтарады */
	float Dombra();
	/** Оқтау кезеңі: 0 шығару, 1 салу, 2 затвор */
	void ReloadSfx(FName Weapon, int32 Stage);
	void Whoosh(bool bHeavy);
	/** Соққы: 0 дене, 1 металл, 2 ағаш, 3 құм, 4 рация */
	void Impact(FVector Loc, int32 Type, float Vol = 0.7f);
	/** 0 фонарь, 1 қару ауыстыру, 2 бос шүріппе */
	void Foley(int32 Type);
	// ---- жазылған дыбыстар (CC0/CC-BY, KAPAudioBanks.cpp) ----
	/** Банктен кездейсоқ дыбыс (қатарынан қайталамайды); жоқ болса nullptr */
	USoundWave* Pick(FName Bank);
	/** Банктен ойнату: b2D — құлақта, әйтпесе Loc нүктесінде (3D) */
	UAudioComponent* PlayBank(FName Bank, FVector Loc, float Vol = 1.f, float Pitch = 1.f, bool b2D = false, float StartTime = 0.f, bool bFar = false);
	/** Сөйлемді жазылған дауыспен айту: Mode 0 — 3D (NPC), 1 — 2D (ойыншы/диктор), 2 — рация. Дауыс табылмаса false */
	bool Speak(const FString& Text, FVector Loc, bool bFemale, int32 Mode = 0, float Vol = 1.f, float Pitch = 1.f);
	void StopSpeech();
	/** Бірнеше дауыс файлын қатар ойнату (динамикалық сөйлем: «бесінші» + «ұңғымада ерітінді ағып жатыр!»).
	 *  Бөліктердің бәрі табылса ғана ойнайды және true қайтарады */
	bool SpeakChain(const TArray<FString>& Parts, FVector Loc, bool bFemale, int32 Mode = 1, float Vol = 1.f);
	/** 1–21: «бірінші» … «жиырма бірінші» (рет сан есім, ұңғыма нөмірі үшін) */
	static FString OrdinalKk(int32 N);
	/** NPC атысы (алыста, жаңғырықпен) */
	void ShotAt(FName Weapon, FVector Loc);
	/** Адамның айқайы: қорқыныш/ауырсыну (bDeath — өлім айқайы) */
	void Yell(FVector Loc, bool bDeath);
	static uint32 VoHash(const FString& S);
	bool HasBank(FName Bank);
	bool MoanAt(FVector Loc, float Pitch, float Vol, float Len, bool bFar = false);
	/** Тест: барлық дыбысты Saved/SoundDump/*.wav етіп жазып, статистикасын логқа шығару (-kapsounddump) */
	void DumpAll();
	/** Тіршілік дауысы: 0 ыңырсу, 1 шыңғырық, 2 ырыл, 3 сыбыр */
	void Creature(FVector Loc, int32 Type, float Vol = 1.f, bool bFar = false);
	class USoundAttenuation* Atten();
	/** Алыс дыбыстар (90 м дейін), LPF-пен */
	class USoundAttenuation* AttenFar();
	void Footstep(FVector Loc, int32 Surface, float Vol = 0.6f, bool bRun = false);
	/** Секіріп/құлап жерге түсу (Strength 0..1) */
	void Land(FVector Loc, int32 Surface, float Strength);
	/** Дизель генераторының гулі (тұйық цикл, 3D) — нүктеге тіркеледі */
	UAudioComponent* AttachGenerator(USceneComponent* To, FVector Offset, float Vol = 1.f);
	/** Жоқтау әуені (финал); ұзақтығын (сек) қайтарады */
	float Lament();
	void StopLoops();
	/** Басты мәзір музыкасы (Music/Menu_0, «Бос үміт»): қосу — 2 с баяу кіреді, өшіру — 2.5 с сөнеді */
	void SetMenuMusic(bool bOn);
private:
	UPROPERTY() TObjectPtr<UAudioComponent> MusMenu;
	/** Мылтық дыбыстарының бір уақыттағы шегі (ұзын жаңғырықтар үйіліп қалмасын) */
	UPROPERTY() TObjectPtr<class USoundConcurrency> GunConc;
	void PlayBuffer(const TArray<int16>& Pcm, float Vol);
	UKAPSynthWave* MakeLoop(UAudioComponent*& OutComp, float Cutoff, float HighPass, float Tone);
	UPROPERTY() TObjectPtr<UKAPSynthWave> Wind;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Rain;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Fire;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Radio;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Engine;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Crickets;
	UPROPERTY() TObjectPtr<UKAPSynthWave> WindHi;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Geiger;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Heart;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Breath;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Dread;
	UPROPERTY() TObjectPtr<UKAPSynthWave> Music;
	UPROPERTY() TObjectPtr<class UReverbEffect> RevOut;
	UPROPERTY() TObjectPtr<class UReverbEffect> RevIn;
	int32 RevState = -1;
	float WindL = 0, RainL = 0, IndoorK = 0, DuckT = 0, DuckAmt = 0, GustT = 0, GustK = 1, AmbT = 8;
	void ApplyBeds();
	UPROPERTY() TObjectPtr<class USoundAttenuation> Attenuation;
	UPROPERTY() TObjectPtr<class USoundAttenuation> AttenuationFar;
	USoundWaveProcedural* MakeBuffer(const TArray<int16>& Pcm);
	/** Әр бір реттік дыбыс: DC жою, басы/соңы жұмсақ, жұмсақ шектегіш; capture режимінде WAV-қа жазу */
	void Finalize(TArray<int16>& Pcm);
	bool bCapture = false;
	FString CaptureName;
	void WriteWav(const FString& Name, const TArray<int16>& Pcm, bool bReport = true);
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Comps;
	// (бір реттік толқындар енді KAPWaveKeep-те — KAPAudio.cpp)
	bool bInit = false;
	UPROPERTY() TMap<FName, FKAPSoundBank> Banks;
	UPROPERTY() TObjectPtr<UAudioComponent> EngLo;
	UPROPERTY() TObjectPtr<UAudioComponent> EngHi;
	UPROPERTY() TObjectPtr<UAudioComponent> MusCalm;
	UPROPERTY() TObjectPtr<UAudioComponent> MusDanger;
	UPROPERTY() TObjectPtr<UAudioComponent> SpeechC;
	USoundWave* LoadVo(const FString& Text, bool bFemale) const;
	UAudioComponent* PlayVoWave(USoundWave* W, FVector Loc, int32 Mode, float Vol, float Pitch, bool bFirst);
	void ChainStep(int32 Id, int32 I, FVector Loc, int32 Mode, float Vol);
	UPROPERTY() TArray<TObjectPtr<USoundWave>> ChainWaves;
	int32 ChainId = 0;
	float EngOn = 0.f, EngRpm = 0.f, EngMis = 0.f, EngLoV = 0.f, EngHiV = 0.f, MusI = 0.f, MusV = 0.f, MusCalmV = 0.f, MusDangerV = 0.f;
	bool bRealEngine = false, bRealMusic = false;
	void UpdateReal(float Dt);
};
