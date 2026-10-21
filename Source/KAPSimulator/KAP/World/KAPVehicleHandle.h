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
private:
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
