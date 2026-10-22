// AKAPGameMode — ойын циклі: уақыт, ауа райы, ұңғымалар, сюжет/квесттер, диалогтар, монстр режиссері,
// навигатор қауіп аймақтары, HMM жылу картасы, CSP бригадалары, концовкалар, кинематика, сақтау
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "KAP/KAPTypes.h"
#include "KAP/AI/KAPAI.h"
#include "KAPGameMode.generated.h"

class AKAPNPC;
class AKAPCharacter;
class AKAPInteractSpot;
class AKAPVehicleHandle;
class AKAPWorldBuilder;
class AWeatherManager;
class ACameraActor;
struct FKAPSession;
struct FKAPContract;

struct FKAPDialogOption
{
	FString Label;
	TFunction<void()> Fn;
	bool bDisabled = false;
};

struct FKAPDangerZone { FVector2D P; double R; double Level; };

UCLASS()
class KAPSIMULATOR_API AKAPGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AKAPGameMode();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	static AKAPGameMode* Get(const UObject* Ctx);

	FKAPSession& S() const;
	AKAPCharacter* Player() const;

	// ---- меню ----
	void MenuNewGame();
	void MenuContinue();
	bool bInMenu = true;
	bool bPaused = false;
	bool bEnded = false;
	float TimeScale = 1.f;
	double LastDh = 0;

	// ---- хабарламалар ----
	/** VoChain — динамикалық хабар үшін дауыс бөліктері (ұңғыма нөмірі + тұрақты сөз тіркесі); бос болса — бүкіл мәтін бір файлмен */
	void Radio(const FString& Msg, FName Kind, const TArray<FString>& VoChain = TArray<FString>());
	void Toast(const FString& Msg, FName Kind);
	void Subtitle(const FString& Kk, const FString& Ru, const FString& Who, float Dur);
	void Dialog(const FString& Name, const FString& Text, TArray<FKAPDialogOption> Opts);

	// ---- квесттер ----
	EQuestState QuestState(FName Id) const;
	void StartQuest(FName Id, bool bAnnounce = true);
	void CompleteQuest(FName Id);
	void CompleteSide(FName Id);
	FString ObjectiveText() const;
	FString QuestProgress(FName Id) const;
	FString GiveItems(const TMap<FName, int32>& Items);
	void TakeItem(FName Id, int32 N);
	int32 Count(FName Id) const;

	// ---- әрекеттер ----
	FString SpotText(const AKAPInteractSpot* Spot) const;
	void SpotUse(AKAPInteractSpot* Spot);
	void VehicleInteract(AKAPVehicleHandle* V);
	void EnterVehicle(AKAPVehicleHandle* V);
	void ExitVehicle();
	TWeakObjectPtr<AKAPVehicleHandle> CurrentVehicle;
	void Talk(AKAPNPC* N);
	void StartWellRepair(int32 Index);
	void OnWellRepaired(int32 Index, bool bByBrigade);
	void UseWearToggle(FName Id);
	void Sleep();

	// ---- ұрыс, фракциялар ----
	void OnActorKilled(AKAPNPC* N, bool bByPlayer);
	void OnNPCAttackedByPlayer(AKAPNPC* N);
	void Provoke(const FString& Msg);
	void SpawnReinforcement(AKAPNPC* Caller);
	float Provoked = 0.f;
	float IndoorAu = 0.f, StingerCd = 0.f, DirRest = 0.f, FearSm = 0.f, StalkerCd = 60.f;
	bool bWasDanger = false;
	float LurkK = 0.f, DombraCd = 20.f;
	void OnPlayerDied();
	void RespawnPlayer();
	bool bPlayerDead = false;

	// ---- навигатор, қауіп ----
	void RefreshDangerZones();
	double DangerAt(double Xm, double Ym) const;
	double DistToRoad(FVector2D Pm) const;
	float RadiationZoneAt(FVector2D Pm) const;
	TArray<FKAPDangerZone> Zones;

	// ---- HMM жылу картасы ----
	TArray<float> Heat; // 64×64
	bool bHeatActive = false;
	int32 HeatCount = 0;

	// ---- CSP ----
	void RunCSP(bool bBacktracking);
	void DispatchCSP();
	TOptional<KAPAI::FCSPResult> CSPResult;
	TArray<FName> CSPBrigadeIds;

	// ---- сақтау ----
	void Save(bool bAuto);
	void SetCheckpoint();

	// ---- концовкалар ----
	void CheckEndings();
	void SendReport();
	void Ending(bool bSuccess, const FString& Why);

	// ---- кинематика ----
	bool bCinematic = false;
	bool bCineSkippable = true;
	void SkipCinematic();
	bool bCredits = false;
	FString CreditsText;
	float CreditsT = 0;

	UPROPERTY() TObjectPtr<AKAPWorldBuilder> Builder;
	UPROPERTY() TObjectPtr<AWeatherManager> Weather;
	TMap<FName, TWeakObjectPtr<AKAPNPC>> NamedNPC;
	/** Соңғы сөйлескен NPC (диалог дауысының жынысы үшін) */
	TWeakObjectPtr<AKAPNPC> TalkNPC;
	// ---- экономика: ақша, келісімшарттар, дүкен, полиция, әскери аймақ (KAPEconomy.cpp) ----
	void AddMoney(int32 Amount, const FString& Why);
	void BuildEconomyPlaces();
	void SpawnEconomyNPCs();
	void RefreshContracts(bool bForce = false);
	TArray<FKAPContract*> ContractsOf(FName Board, int32 State);
	void AcceptContract(int32 Uid);
	void AbandonContract(int32 Uid);
	void TrackContract(int32 Uid);
	FKAPContract* FindContract(int32 Uid);
	FString ContractProgress(const FKAPContract& C) const;
	void ContractTick(float Dt);
	void ContractKill(AKAPNPC* N, bool bByPlayer);
	bool EconomyTalk(AKAPNPC* N);
	FString EconomySpotText(const AKAPInteractSpot* Sp) const;
	bool EconomySpotUse(AKAPInteractSpot* Sp);
	struct FShopItem { FName Id; int32 Price; int32 Qty; int32 Cat; bool bPermit; };
	static const TArray<FShopItem>& ShopItems();
	int32 ShopPrice(const FShopItem& It, FName Kind) const;
	bool CanBuy(const FShopItem& It, FName Kind, FString& Why) const;
	void Buy(const FShopItem& It, FName Kind);
	void SellScrap();
	bool InArmyZone(FVector2D Pm) const;
	FVector2D ArmyC = FVector2D::ZeroVector, ArmyHalf = FVector2D(62, 47);
	float ArmyHostile = 0.f, ArmyInT = 0.f, ContractT = 0.f;
	int32 ArmyWarnStage = 0;
	bool bArmyBuilt = false;
	TArray<FString> RadioLog;
private:
	void StartSession(bool bLoad);
	void SpawnPersistentNPCs();
	void SyncPlayerToSession();
	void SyncSessionToWorld();
	void Simulate(float Dt);
	void Director(float Dt);
	void UpdateTrackers();
	void StoryTick(float Dt);
	void ActivateTower(FName Id, int32 Region);
	void OpenCrate(FName Id);
	AKAPNPC* SpawnNPC(int32 Kind, FVector2D PosM, const FString& Name, bool bPersistent = false, float Scale = 1.f, bool bBoss = false);
	// кинематика қозғалтқышы
	void StartCine(float Dur, TFunction<void(float, float)> Update, TFunction<void()> OnEnd);
	void EndCine();
	void CineCamera(FVector Pos, FVector LookAt, float Fov = 70.f);
	void Intro();
	void EndingFail(const FString& Why, bool bErlanWasAlive);
	void EndingSuccess(bool bErlanWasAlive);
	void MotherScene(bool bErlanWasAlive, bool bSuccess);
	void Credits(bool bSuccess);
	struct FCine { float T = 0, Dur = 0; TFunction<void(float, float)> Update; TFunction<void()> OnEnd; };
	TOptional<FCine> Cine;
	UPROPERTY() TObjectPtr<ACameraActor> CineCam;
	TArray<TWeakObjectPtr<AActor>> CineActors;
	// ---- 15 с тизер-трейлер (KAPTrailer.cpp): -kaptrailer — MRQ рендері, -kaptrailershots — алдын ала кадрлар ----
	void StartTrailer(bool bShots);
	void TrailerBegin();
	void TrailerTick(float CineT, float Dt);
	void TrailerEnterShot(int32 Shot);
	bool bTrailerMRQ = false, bTrailerShots = false;
	float TrailerPrevT = -1.f;
	int32 TrailerShotI = -1;
	TSet<int32> TrailerFired;
	TArray<TWeakObjectPtr<AKAPNPC>> TrailerNPCs;
	TWeakObjectPtr<AKAPNPC> TrailerWalker, TrailerMonster;
	FVector2D TrailerWalkTo = FVector2D::ZeroVector;
	FVector TrA = FVector::ZeroVector, TrB = FVector::ZeroVector, TrLook = FVector::ZeroVector, TrLookB = FVector::ZeroVector;
	float TrFov = 50.f, TrShake = 0.f;
	UPROPERTY() TObjectPtr<class USpotLightComponent> TrailerTorch;
	UPROPERTY() TObjectPtr<class UAudioComponent> TrailerMusic;
	UPROPERTY() TObjectPtr<class UAudioComponent> TrailerMusic2;
	UPROPERTY() TArray<TObjectPtr<class UAudioComponent>> TrailerSfx;
	TSharedPtr<class SWidget> TrailerUI;
	float TrBlack = 1.f, TrCardA = 0.f, TrTitleA = 0.f, TrSubA = 0.f, TrSmallA = 0.f;
	FString TrCard, TrSmall;
	/** Басты мәзір сахнасы (KAPMenuScene.cpp): түнгі ұңғыма, маңдай шамы бар инженер, апат шамы, прожектор */
	void SetupMenuScene();
	void ClearMenuScene();
	bool bMenuScene = false;
	FVector MenuCamBase = FVector::ZeroVector, MenuLook = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<class UPointLightComponent> MenuBeacon;
	UPROPERTY() TObjectPtr<class USpotLightComponent> MenuHeadlamp;
	TArray<TWeakObjectPtr<AActor>> MenuActors;
	float WaitT = 0, WaterRadioT = 0, DirT = 0, Acc1 = 0, Acc2 = 0, Acc15 = 0;
	TMap<int32, TWeakObjectPtr<AKAPNPC>> Bosses;
	bool bRadioPulse = false;
	FRandomStream Rng;
};
