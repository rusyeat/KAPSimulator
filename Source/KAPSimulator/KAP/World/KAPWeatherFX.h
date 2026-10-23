// AKAPWeatherFX — ауа райының көрінетін бөлігі: камера айналасындағы жаңбыр тамшылары (желмен қисаяды),
// жердегі тамшы шашырауы, найзағай сызығы мен жарқылы (түскен жерге жақын болса — сексеуіл сынады, адам жарақаттанады)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAPWeatherFX.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class KAPSIMULATOR_API AKAPWeatherFX : public AActor
{
	GENERATED_BODY()
public:
	AKAPWeatherFX();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	/** Тест: найзағайды көзге көрінетін жерге түсіру */
	int32 BoltsShown = 0;
private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Drops;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> DropMID;
	TArray<FVector> DropPos;
	TArray<FTransform> DropXf;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Bolt;
	UPROPERTY() TObjectPtr<UPointLightComponent> Flash;
	int32 LastSeq = 0;
	float BoltT = -1.f, SplashAcc = 0.f;
	void MakeBolt(const FVector& Ground);
};
