// AWellActor — ISR ұңғымасы: күйі, ақауы, визуалы, басқару пульті
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/KAPTypes.h"
#include "KAP/Core/KAPInteractable.h"
#include "WellActor.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UAudioComponent;

UCLASS()
class KAPSIMULATOR_API AWellActor : public AActor, public IKAPInteractable
{
	GENERATED_BODY()
public:
	AWellActor();
	virtual void Tick(float Dt) override;

	/** Индекс (0..20) және кесте жолы бойынша құрастыру */
	void Build(int32 InIndex, const FKAPWellRow& Row);
	void SetVisualState(EWellState State, EFaultType Fault);

	virtual FString GetInteractText(AKAPCharacter* By) const override;
	virtual void Interact(AKAPCharacter* By) override;
	virtual FVector GetInteractLocation() const override;
	virtual float GetInteractRadius() const override { return 280.f; }

	int32 Index = 0;
	EWellState VisState = EWellState::Ok;
	EFaultType VisFault = EFaultType::None;
private:
	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Lamp;
	UPROPERTY() TObjectPtr<UPointLightComponent> LampLight;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Puddle;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Scorch;
	UPROPERTY() TObjectPtr<USceneComponent> FireGroup;
	UPROPERTY() TObjectPtr<UPointLightComponent> FireLight;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Flames;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Valves;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Sign;
	FVector PanelLocal;
	UPROPERTY() TObjectPtr<UAudioComponent> Hum;
	float FxAcc = 0.f;
};
