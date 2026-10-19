// Лақтырылатын заттар: молотов (өрт аймағы), сигнал шамы (жарық, монстрларды алдайды)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPProjectile.generated.h"

class UPointLightComponent;

UENUM()
enum class EKAPProjectile : uint8 { Molotov, Flare };

UCLASS()
class KAPSIMULATOR_API AKAPProjectile : public AActor
{
	GENERATED_BODY()
public:
	AKAPProjectile();
	virtual void Tick(float Dt) override;
	void Launch(EKAPProjectile InType, FVector Velocity, AActor* By);

	static bool NearestFlare(UWorld* W, FVector2D FromM, double MaxM, FVector2D& Out);
	static bool IsNearFire(UWorld* W, const FVector& P);
private:
	EKAPProjectile Type = EKAPProjectile::Flare;
	FVector Vel = FVector::ZeroVector;
	bool bLanded = false;
	float Life = 0.f;
	TWeakObjectPtr<AActor> Thrower;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
};
