// KAP Simulator — Data Table жолдары мен ортақ типтер
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "KAPTypes.generated.h"

UENUM(BlueprintType)
enum class EWellState : uint8 { Ok, Fault, Leak, Fire, Lost };

UENUM(BlueprintType)
enum class EFaultType : uint8 { None, Pump, Pipe, Electric, Acid, Fire };

UENUM(BlueprintType)
enum class EKAPFaction : uint8 { Civil, Worker, Militia, Guard, Bandit, Monster };

UENUM(BlueprintType)
enum class EMonsterKind : uint8 { Wolf, Irradiated, Worm };

UENUM(BlueprintType)
enum class EWeatherType : uint8 { Clear, Heat, Storm, Rain, Fog };

UENUM(BlueprintType)
enum class EQuestState : uint8 { None, Active, Done };

USTRUCT(BlueprintType)
struct FKAPConfigRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Value = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Note;
};

USTRUCT(BlueprintType)
struct FKAPWellRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Id = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Region = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;   // метр
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;   // метр
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Age = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BasePressure = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Workers = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool AcidTank = false;
};

USTRUCT(BlueprintType)
struct FKAPNodeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
};

USTRUCT(BlueprintType)
struct FKAPRoadRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName A;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName B;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Danger = 0.f;
};

USTRUCT(BlueprintType)
struct FKAPLocationRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Type;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float R = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Region = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
};

USTRUCT(BlueprintType)
struct FKAPVehicleRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Yaw = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName State;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::Gray;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FuelCap = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Armor = 1.f;
};

USTRUCT(BlueprintType)
struct FKAPWeaponRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Kind;   // melee, gun, throw
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 2.f;  // метр
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Rate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Mag = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Ammo;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Spread = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Pellets = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Permit = false;
};

USTRUCT(BlueprintType)
struct FKAPItemRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Desc;
};

USTRUCT(BlueprintType)
struct FKAPQuestRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Kind;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Desc;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Giver;
};

USTRUCT(BlueprintType)
struct FKAPNameRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Type; // m, f, s, fam
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Value;
};

USTRUCT(BlueprintType)
struct FKAPBrigadeRow : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Tools; // pump|pipe|...
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float X = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Y = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Range = 1500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 14.f;
};

/** Жұмысшы (AWorkerNPC деректері) */
USTRUCT(BlueprintType)
struct FKAPWorker
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) int32 Id = 0;
	UPROPERTY(SaveGame) int32 WellId = 0;
	UPROPERTY(SaveGame) FString Name;
	UPROPERTY(SaveGame) bool bFemale = false;
	UPROPERTY(SaveGame) int32 Age = 30;
	UPROPERTY(SaveGame) FString Family;
	UPROPERTY(SaveGame) float Dose = 0.f;
	UPROPERTY(SaveGame) bool bProtected = false;
	UPROPERTY(SaveGame) int32 State = 0;      // 0 сау,1 уланған,2 ауру,3 өлім алдында,4 қайтыс болды
	UPROPERTY(SaveGame) TArray<float> Belief = { 0.97f, 0.03f, 0.f, 0.f };
	UPROPERTY(SaveGame) int32 Symptom = 0;
	UPROPERTY(SaveGame) float IodineUntil = -1.f;
	UPROPERTY(SaveGame) FString EvacTo;
	UPROPERTY(SaveGame) bool bMutated = false;
	UPROPERTY(SaveGame) bool bSpecial = false;
};

namespace KAP
{
	/** метр (дерек кестесі) → см (Unreal) */
	inline FVector2D M2CM(FVector2D M) { return M * 100.0; }
	inline FVector2D CM2M(const FVector& V) { return FVector2D(V.X, V.Y) / 100.0; }
	inline int32 RegionOf(double Xm, double Ym)
	{
		if (Xm < 0 && Ym > 0) return 0;
		if (Xm >= 0 && Ym > 0) return 1;
		if (Xm >= 0 && Ym <= 0) return 2;
		return 3;
	}
	KAPSIMULATOR_API FText RegionName(int32 R);
	KAPSIMULATOR_API FText FaultName(EFaultType F);
	KAPSIMULATOR_API FText WellStateName(EWellState S);
	KAPSIMULATOR_API FText WeatherName(EWeatherType W);
	KAPSIMULATOR_API FName FaultTool(EFaultType F);
	KAPSIMULATOR_API EFaultType FaultFromName(FName N);
	KAPSIMULATOR_API FString FmtTime(double Hours);
	KAPSIMULATOR_API FString FmtDur(double Hours);
	/** Жөндеу қадамдары: "part:pump_part", "valve", ... */
	KAPSIMULATOR_API TArray<FString> RepairSteps(EFaultType F);
}
