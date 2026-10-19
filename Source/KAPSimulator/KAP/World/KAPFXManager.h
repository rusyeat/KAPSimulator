// Жеңіл бөлшектер жүйесі: түтін, бу, улы бу, шаң, от, қан, жарқыл (Niagara активтерінсіз)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPFXManager.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

UENUM()
enum class EKAPFX : uint8 { Smoke, Toxic, Fire, Steam, Dust, Blood, Green, Flash, Flare };

UCLASS()
class KAPSIMULATOR_API AKAPFXManager : public AActor
{
	GENERATED_BODY()
public:
	AKAPFXManager();
	virtual void Tick(float Dt) override;
	void Spawn(EKAPFX Type, FVector Pos, float Size = 1.f, FVector Vel = FVector::ZeroVector);
	static AKAPFXManager* Get(UWorld* W);
	FVector Wind = FVector(200, 50, 0);
private:
	struct FP { UStaticMeshComponent* C = nullptr; UMaterialInstanceDynamic* M = nullptr; float T = 0, Life = 1, Size = 1, Grow = 0, Op = 1; FVector V; FLinearColor Col; bool bAlive = false; };
	TArray<FP> Pool;
	int32 Next = 0;
};
