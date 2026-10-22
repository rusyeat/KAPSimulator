// Ашылатын есік: топсада айналатын полотно. E — ашу/жабу (ойыншыдан әрі қарай ашылады),
// адамдар жақындаса өзі ашылып, өтіп кеткен соң жабылады. Құбыжықтар мен қасқырлар аша алмайды:
// жабық есік оларды өткізбейді, тек тарсылдатады. Дыбыс есіктің өз қозғалысымен бірге шығады.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAPDoor.generated.h"

struct FKAPMat;

UCLASS()
class KAPSIMULATOR_API AKAPDoor : public AActor, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AKAPDoor();
	/** Актер = топса (ойықтың төменгі бұрышы), полотно актердің +X осі бойымен жатады, жазықтығы — X/Z.
	 *  Width/Height — см; OpenYaw — толық ашылу бұрышы; Sound: 0 металл, 1 ағаш */
	void Build(float Width, float Height, float InOpenYaw, int32 Sound, const FKAPMat& Panel, const FKAPMat& Handle);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float Dt) override;

	virtual FString GetInteractText(class AKAPCharacter* By) const override;
	virtual void Interact(class AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override { return Mid(); }
	virtual float GetInteractRadius() const override { return 200.f; }

	/** Ашу/жабу. From — кім ашып жатыр (есік содан әрі қарай ашылады) */
	void SetOpen(bool bOpen, const FVector& From, const FString& Who = FString());
	bool IsOpen() const { return bWantOpen; }
	/** 0 — жабық, 1 — толық ашық */
	float OpenAmount() const { return Ang; }
	/** Ойықтың ортасы (әлем, еденнен 1 м) */
	FVector Mid() const;

	UPROPERTY() TObjectPtr<USceneComponent> Hinge;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Leaf;
	int32 Knocks = 0; // тест: құбыжық неше рет тарсылдатты
private:
	float W = 110.f, H = 215.f, OpenYaw = 100.f, Dir = 1.f, Ang = 0.f;
	float Check = 0.f, NpcAway = 0.f, KnockCd = 1.f, ShakeT = 0.f;
	bool bWantOpen = false, bNpcOpened = false;
	int32 SoundType = 0;
	/** Полотно A күйінде (0..1) ойыншыға не NPC-ге тиіп тұр ма */
	bool Blocked(float A) const;
	void ApplyAngle();
	void CheckNPCs();
};
