// AKAPCharacter — ойыншы: бірінші жақтан көрініс, survival (GAS), инвентарь, қару, әрекет
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "KAPCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UAbilitySystemComponent;
class UKAPSurvivalAttributeSet;
class UNavigatorComponent;
class IKAPInteractable;

UCLASS()
class KAPSIMULATOR_API AKAPCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	AKAPCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return ASC; }
	virtual float TakeDamage(float Damage, struct FDamageEvent const& Event, AController* Instigator, AActor* Causer) override;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USpotLightComponent> Flashlight;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent> ASC;
	UPROPERTY() TObjectPtr<UKAPSurvivalAttributeSet> Attr;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UNavigatorComponent> Navigator;
	UPROPERTY() TObjectPtr<USceneComponent> HeldRoot;

	// ---- енгізу ----
	void MoveInput(FVector2D V);
	void LookInput(FVector2D V);
	void SetSprint(bool b) { bSprint = b; }
	void DoJump();
	void Interact();
	void FireStart();
	void FireStop() { bFiring = false; }
	void SetAim(bool b);
	void Reload();
	void SelectSlot(int32 I);
	void CycleSlot(int32 Dir);
	void ToggleFlashlight();
	void UseItem(FName Id);

	void Equip(FName Id);
	void DamagePlayer(float Amount, float DoseAdd = 0.f, bool bSilent = false);
	void Respawn(FVector At);

	// ---- күй (HUD оқиды) ----
	float GetHp() const; float GetMaxHp() const; float GetWater() const; float GetTemp() const; float GetDose() const; float GetStamina() const;
	void SetAttr(const struct FGameplayAttribute& A, float V);
	float DoseRate = 0.f;
	bool bIndoor = false, bMoving = false, bDead = false, bScoped = false;
	float Noise = 0.f, HurtFlash = 0.f, WarnT = 0.f;
	float ReloadT = 0.f, Cooldown = 0.f, SwingT = 0.f;
	FName ReloadId;
	FString InteractText;
	TWeakObjectPtr<UObject> InteractTarget;
	bool bFrozen = false; // кинематика/планшет кезінде

	static AKAPCharacter* Get(const UObject* WorldContext);
private:
	void UpdateSurvival(float Dt, double Dh);
	void UpdateInteraction();
	void UpdateHeld(float Dt);
	void DoFire();
	void BuildHeld(FName Id);
	bool bSprint = false, bFiring = false;
	FVector2D MoveVec = FVector2D::ZeroVector;
	float BobT = 0.f, Recoil = 0.f, InteractAcc = 0.f, CamBob = 0.f;
	FVector HeldBase = FVector(40, 22, -24);
	FVector MuzzleLocal = FVector::ZeroVector;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> MuzzleFx;
	UPROPERTY() TObjectPtr<class UPointLightComponent> MuzzleLight;
	float FlashT = 0.f, ReloadDur = 0.f, RunPose = 0.f;
	int32 ReloadStage = 0;
};
