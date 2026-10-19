// UNavigatorComponent — планшет навигаторы: A* бойынша «ең қысқа», «ең қауіпсіз», «жолсыз тура» маршруттар
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KAP/AI/KAPAI.h"
#include "NavigatorComponent.generated.h"

UENUM()
enum class ENavMode : uint8 { Short, Safe, Direct };

UCLASS(ClassGroup = (KAP), meta = (BlueprintSpawnableComponent))
class KAPSIMULATOR_API UNavigatorComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNavigatorComponent();
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetTarget(FVector2D TargetM, const FString& Name);
	void Clear();
	void Recompute();
	const KAPAI::FRoute* Active() const;

	bool bHasTarget = false;
	FVector2D Target = FVector2D::ZeroVector;
	FString TargetName;
	ENavMode Mode = ENavMode::Short;
	KAPAI::FRoute Routes[3];
	FVector2D Next = FVector2D::ZeroVector;
	bool bHasNext = false;

	/** Тор графы (жолсыз тура жол + NPC жол табуы) */
	static KAPAI::FGrid& Grid(UWorld* World);
private:
	double LastCalc = 0;
};
