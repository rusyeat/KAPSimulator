// AWeatherManager — тәулік циклі (1 тәулік = 48 мин) және ауа райы: ашық, ыстық, құм дауылы, жаңбыр, тұман
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KAP/KAPTypes.h"
#include "WeatherManager.generated.h"

class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;
struct FKAPSession;
struct FKAPWeatherSeg;

USTRUCT()
struct FKAPEnv
{
	GENERATED_BODY()
	float Storm = 0, Rain = 0, Fog = 0, Heat = 0, Wet = 0, Temp = 30, Visibility = 1100, Lightning = 0;
	bool bNight = false;
	FVector Wind = FVector(200, 50, 0);
	EWeatherType Cur = EWeatherType::Clear;
};

UCLASS()
class KAPSIMULATOR_API AWeatherManager : public AActor
{
	GENERATED_BODY()
public:
	AWeatherManager();
	virtual void BeginPlay() override;

	/** Ойын уақыты мен ауа райы күйін жаңарту (GameMode шақырады) */
	void UpdateEnv(FKAPSession& S, float DtReal, double Dh);
	void ApplyVisuals(double Hour, float DtReal);
	void GenSchedule(FKAPSession& S, double Until);
	const FKAPWeatherSeg* SegAt(const FKAPSession& S, double Hour) const;

	FKAPEnv Env;
	/** Демо/сюжет үшін ауа райын мәжбүрлеу */
	bool bOverride = false;
	EWeatherType OverrideType = EWeatherType::Clear;
	bool bInstant = false;
	/** Кіріспе: көріну қашықтығын тікелей беру (м), <0 — өшірулі */
	float VisOverride = -1.f;
	/** Қорқыныш (0..1): винетка, дән, хроматикалық ауытқу, түссіздену */
	float Fear = 0.f;

	static AWeatherManager* Get(UWorld* W);
private:
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> HeightFog;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	float SkyRecaptureAcc = 0;
	FRandomStream Rng;
};
