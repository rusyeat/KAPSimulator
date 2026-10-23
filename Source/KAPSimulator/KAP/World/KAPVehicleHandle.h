// Көлік күйі: жанармай, двигатель, дөңгелек, құлып — Chaos Vehicles павнымен байланысты
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/KAPTypes.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAPVehicleHandle.generated.h"

class AKAPSimulatorPawn;

UCLASS()
class KAPSIMULATOR_API AKAPVehicleHandle : public AActor, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AKAPVehicleHandle();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual FString GetInteractText(class AKAPCharacter* By) const override;
	virtual void Interact(class AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override;
	virtual float GetInteractRadius() const override { return 380.f; }

	FName Id;
	FKAPVehicleRow Def;
	UPROPERTY() TObjectPtr<AKAPSimulatorPawn> Pawn;
	float Fuel = 20, Hp = 100;
	int32 Flats = 0;
	FName State = "ok"; // ok, radiator, parts, locked, broken
	bool bDriven = false;
	bool CanDrive() const { return State == "ok" && Fuel > 0 && Hp > 0; }
	float SpeedKmh() const;
	/** Қозғалтқыш: айналым (0..1), температура (°C шартты 0..110) */
	float Rpm01 = 0.f, Heat = 60.f;
	/** Автопилот (кіріспе, NPC жүргізуші): нүктеге физикамен жүру, дөңгелектер шын айналады */
	bool bAutoPilot = false;
	FVector AutoTarget = FVector::ZeroVector;
	float AutoMaxKmh = 45.f;
	/** Фаралар мәжбүрлі (кіріспе) */
	bool bLightsForced = false;
	/** Көлікті сенімді түрде жаңа орынға қою: физика өшіріліп-қосылады (Chaos денесі ескі орнына қайтпауы үшін), жылдамдық нөл */
	void PlaceAt(FVector At, float Yaw);
	/** Жел күші (Н) — HUD/тест үшін */
	float WindForceN = 0.f;
	/** Дөңгелек астындағы батпақ (0..1) */
	float MudK = 0.f;
	// ---- есіктер мен бұзылу көрінісі (KAPVehicleDoor.cpp) ----
	/** Есікті ашу (Side: 0 — сол/жүргізуші, 1 — оң); HoldSec өткен соң өзі жабылады, HoldSec < 0 — ашық тұрады */
	void SwingDoor(int32 Side = 0, float HoldSec = 1.1f);
	void CloseDoor(int32 Side = 0);
	bool HasDoor(int32 Side = 0) const { return Doors.IsValidIndex(Side); }
	/** Есіктің ашылу дәрежесі (0..1) */
	float DoorOpen01(int32 Side = 0) const { return Doors.IsValidIndex(Side) ? Doors[Side].Ang / FMath::Max(1.f, Doors[Side].MaxAng) : 0.f; }
	/** Корпус мешінің жүйесіндегі нүкте (X=0 — база ортасы, Z=0 — жер) → әлем */
	FVector ShellToWorld(const FVector& L) const;
	FVector WorldToShell(const FVector& W) const;
	/** Радиатордан бу (0..1), астынан антифриз ағады; апат шамдары жыпылықтайды */
	float SteamK = 0.f, SteamFade = 0.f; // SteamFade > 0: бу осынша секундта сөнеді
	bool bHazard = false;
private:
	struct FDoorRt { USceneComponent* Hinge = nullptr; TArray<UPrimitiveComponent*> Show; float Ang = 0.f, Target = 0.f, MaxAng = 65.f, Hold = 0.f, Sign = 1.f; bool bStay = false; };
	TArray<FDoorRt> Doors;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> DoorParts;
	void BuildDoors(USkeletalMeshComponent* M, const FVector& O);
	void DoorTick(float Dt);
	FVector ShellO = FVector::ZeroVector, GrilleLocal = FVector::ZeroVector;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Coolant;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HazardLens;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> HazardL;
	float HazardT = 0.f, SteamAcc = 0.f, CoolantK = 0.f, HissCd = 0.f;
	/** Жел, батпақ, су, шашырау (KAPVehicleEnv.cpp) */
	void EnvTick(float Dt);
	TArray<float> BaseFriction;
	float GripApplied = 1.f, SplashAcc = 0.f, SplashSndCd = 0.f;
	/** Қауіпсіздік: аударылса — дөңгелегіне, жер астына түссе — соңғы қауіпсіз орнына (KeepSafe) */
	void KeepSafe(float Dt);
	FTransform LastGoodXf = FTransform(FVector::ZeroVector);
	float FlipT = 0.f, SafeAcc = 0.f;
	/** Адамды қағу: көлік мешінің жергілікті шекарасы (бір рет есептеледі) */
	void CheckPedestrianHits();
	FVector HitExt = FVector::ZeroVector, HitCtr = FVector::ZeroVector;
	/** Шаблон көлігінің рамасын жасырып, нақты дөңгелек орындарына сай корпус құрастыру (KAPVehicleBody.cpp) */
	void BuildBody();
	/** Тегіс корпус мештері (SM_KAP_Veh_*) болса — солармен құрастыру (KAPVehicleShell.cpp); болмаса false */
	bool BuildShellBody(USkeletalMeshComponent* M, float Xf, float Xr, float Yw, float Zc, float R);
	/** Тегіс корпустың қосымша дөңгелектері (тұтас бөлшекті пивоттар) */
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> FakePivots;
	/** Көрінетін база: дөңгелектер X бойынша VisXc айналасында VisKX есе жақындатылады */
	float VisKX = 1.f, VisXc = 0.f;
	bool bBodyBuilt = false;
	FVector HeadLocal[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector TailLocal[2] = { FVector::ZeroVector, FVector::ZeroVector };
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeadLens;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> TailLens;
	/** Қосымша (декоративті) дөңгелектер — жылдамдыққа қарай айналады (КамАЗ, БТР) */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FakeWheels;
	float FakeR = 45.f, FakeRoll = 0.f;
	/** Көрінетін дөңгелектер: физикалық сүйектің айналуы/серпілуін қайталайды, ені VisK есе тар */
	TArray<FName> WheelBones;
	TArray<FQuat> WheelRest;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> WheelPivots;
	float VisK = 1.f, VisDrop = 0.f;
	void UpdateVisualWheels();
	void EnsureLights();
	void UpdateLights(float Dt);
	UPROPERTY() TArray<TObjectPtr<class USpotLightComponent>> Heads;
	UPROPERTY() TObjectPtr<class UPointLightComponent> TailL;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Lenses;
	float LightK = 0.f, HeatWarnCd = 0.f;
};
