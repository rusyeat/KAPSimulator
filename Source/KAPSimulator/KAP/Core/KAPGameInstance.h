// UKAPGameInstance — сессия күйі, концовка шарттары, сақтау (SaveGame)
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameFramework/SaveGame.h"
#include "KAP/KAPTypes.h"
#include "KAPGameInstance.generated.h"

USTRUCT(BlueprintType)
struct FKAPWellSave
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) EWellState State = EWellState::Ok;
	UPROPERTY(SaveGame) EFaultType Fault = EFaultType::None;
	UPROPERTY(SaveGame) float Timer = 0.f;
	UPROPERTY(SaveGame) float BurnTimer = 0.f;
	UPROPERTY(SaveGame) float Stress = 0.f;
	UPROPERTY(SaveGame) int32 Repaired = 0;
};

USTRUCT(BlueprintType)
struct FKAPWeatherSeg
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) EWeatherType Type = EWeatherType::Clear;
	UPROPERTY(SaveGame) double Start = 0;
	UPROPERTY(SaveGame) double Dur = 1;
};

USTRUCT(BlueprintType)
struct FKAPVehicleSave
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName Id;
	UPROPERTY(SaveGame) FTransform Xf;
	UPROPERTY(SaveGame) float Fuel = 20.f;
	UPROPERTY(SaveGame) float Hp = 100.f;
	UPROPERTY(SaveGame) int32 Flats = 0;
	UPROPERTY(SaveGame) FName State;
};

USTRUCT(BlueprintType)
struct FKAPBrigadeState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName Id;
	UPROPERTY(SaveGame) FVector2D Pos = FVector2D::ZeroVector;   // метр
	UPROPERTY(SaveGame) FVector2D Base = FVector2D::ZeroVector;
	UPROPERTY(SaveGame) FName State = "idle";                    // idle, travel, repair, return
	UPROPERTY(SaveGame) int32 Target = -1;
	UPROPERTY(SaveGame) TArray<FVector2D> Route;
	UPROPERTY(SaveGame) int32 Ri = 0;
	UPROPERTY(SaveGame) float Work = 0.f;
};

/** Ойын сессиясы — бәрі сақталады */
USTRUCT(BlueprintType)
struct FKAPSession
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) double Time = 12.5;         // ойын сағаты (1-күн 0:00-ден)
	UPROPERTY(SaveGame) bool bSimActive = false;
	UPROPERTY(SaveGame) bool bHasTablet = false;
	UPROPERTY(SaveGame) TMap<FName, int32> Inv;
	UPROPERTY(SaveGame) TMap<FName, int32> Ammo;
	UPROPERTY(SaveGame) TMap<FName, int32> Mag;
	UPROPERTY(SaveGame) TArray<FName> Weapons;
	UPROPERTY(SaveGame) TMap<FName, EQuestState> Quests;
	UPROPERTY(SaveGame) TMap<FName, int32> QuestData;
	UPROPERTY(SaveGame) TMap<FName, double> Flags;
	UPROPERTY(SaveGame) TArray<bool> Regions = { false, false, false, false };
	UPROPERTY(SaveGame) TArray<bool> Visited = { true, false, false, false };
	UPROPERTY(SaveGame) TSet<FName> Towers;
	UPROPERTY(SaveGame) bool bPermitWeapon = false;
	UPROPERTY(SaveGame) bool bPermitAccess = false;
	UPROPERTY(SaveGame) float RepMilitia = 0.f;
	UPROPERTY(SaveGame) float Mutation = 6.f;
	UPROPERTY(SaveGame) int32 WellsRepaired = 0;
	UPROPERTY(SaveGame) int32 Burned = 0;
	UPROPERTY(SaveGame) int32 Kills = 0;
	UPROPERTY(SaveGame) TArray<bool> BossAlive = { true, true, true, true };
	UPROPERTY(SaveGame) TArray<FKAPWellSave> Wells;
	UPROPERTY(SaveGame) TArray<FKAPWorker> Workers;
	UPROPERTY(SaveGame) TArray<FKAPWeatherSeg> Weather;
	UPROPERTY(SaveGame) TArray<FKAPVehicleSave> VehicleStates;
	UPROPERTY(SaveGame) TArray<FKAPBrigadeState> Brigades;
	UPROPERTY(SaveGame) bool bBrigadesUnlocked = false;
	UPROPERTY(SaveGame) TMap<FName, double> CrateRefill;
	UPROPERTY(SaveGame) TArray<int32> PendingMutants;  // ұңғыма id
	// ойыншы
	UPROPERTY(SaveGame) FVector PlayerPos = FVector::ZeroVector;
	UPROPERTY(SaveGame) FVector Checkpoint = FVector::ZeroVector;
	UPROPERTY(SaveGame) float Hp = 100, MaxHp = 100, Water = 80, Temp = 36.6f, Dose = 0, Stamina = 100;
	UPROPERTY(SaveGame) float IodineUntil = -1;
	UPROPERTY(SaveGame) TSet<FName> Wear;
	UPROPERTY(SaveGame) FName Equipped = "wrench";
	UPROPERTY(SaveGame) bool bRuSubtitles = false;
	UPROPERTY(SaveGame) float MouseSens = 1.f;
};

UCLASS()
class KAPSIMULATOR_API UKAPSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) FKAPSession Session;
};

UCLASS()
class KAPSIMULATOR_API UKAPGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	UPROPERTY() FKAPSession S;
	/** Мәзірден «Жалғастыру» басылса — карта жүктелгенде сақталғаннан бастау */
	bool bLoadOnStart = false;
	bool bSessionStarted = false;

	static const FString SlotName;
	bool SaveSession();
	bool LoadSession();
	bool HasSave() const;

	static UKAPGameInstance* Get(const UObject* WorldContext);
};
