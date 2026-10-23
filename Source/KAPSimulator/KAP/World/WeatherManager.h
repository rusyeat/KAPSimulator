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
	/** Бұлттылық (0..1), найзағай белсенділігі (0..1), жел (м/с) мен бағыты (градус, қайда соғады), екпін (1 — орташа) */
	float Cloud = 0.15f, Thunder = 0, WindMs = 3.f, WindYaw = 20.f, Gust = 1.f;
	bool bNight = false;
	/** Жел жылдамдығы (см/с, әлем бағытымен): көлік, жаяу жүру, бөлшектер, өсімдіктер осыны қолданады */
	FVector Wind = FVector(200, 50, 0);
	/** Соңғы найзағай: түскен нүкте (см), реті (өзгерсе — жаңа соққы), күннің күркіреуіне дейінгі кідіріс (с) */
	FVector StrikeAt = FVector::ZeroVector;
	int32 StrikeSeq = 0;
	float StrikeDelay = 1.f;
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
	/** Мәжбүрлі ауа райының күші (0..1): мәзірде — жеңіл дауыл */
	float OverrideK = 1.f;
	bool bInstant = false;
	/** Кіріспе: көріну қашықтығын тікелей беру (м), <0 — өшірулі */
	float VisOverride = -1.f;
	/** Қорқыныш (0..1): винетка, дән, хроматикалық ауытқу, түссіздену */
	float Fear = 0.f;
	/** Ойыншы ғимарат ішінде (0..1): экспозиция көтерілмейді — шам жарығы «дақ» болып көрінеді */
	float Indoor = 0.f;
	/** Экспозиция бірден бейімделеді (трейлердегі кесу кадрлары үшін) */
	bool bFastExposure = false;
	/** Тест: желді мәжбүрлеу (бағыт градус, жылдамдық м/с); теріс — өшірулі */
	float ForceWindYaw = -999.f, ForceWindMs = -1.f;

	static AWeatherManager* Get(UWorld* W);
private:
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY() TObjectPtr<UDirectionalLightComponent> Moon;
	UPROPERTY() TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> HeightFog;
	UPROPERTY() TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY() TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY() TObjectPtr<UPostProcessComponent> Post;
	UPROPERTY() TObjectPtr<class UMaterialParameterCollection> EnvMPC;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> CloudMID;
	TSet<FName> CloudScalars, CloudVectors;
	/** Ауа райы түрінің мақсатты мәндері */
	struct FTarget { float Storm, Rain, Fog, Heat, Cloud, Thunder, WindMs, Vis; };
	static FTarget TargetOf(EWeatherType T);
	void Strike(const FVector& Around);
	float WindT = 0.f, WindBase = 3.5f, YawBase = 20.f, YawTarget = 20.f;
	float SkyRecaptureAcc = 0;
	FRandomStream Rng;
};
