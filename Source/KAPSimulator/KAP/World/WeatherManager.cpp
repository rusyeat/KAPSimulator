#include "WeatherManager.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "KAPVisuals.h"
#include "KAPWeatherFX.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

AWeatherManager::AWeatherManager()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>("Sun");
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetAtmosphereSunLightIndex(0);
	Sun->SetIntensity(9.f);
	Sun->SetLightColor(FLinearColor(1.f, 0.95f, 0.86f));
	Sun->SetDynamicShadowCascades(4);
	Sun->bUseTemperature = true;
	Sun->Temperature = 5900.f;
	Sun->ForwardShadingPriority = 1;
	// таң/кеште шаң арқылы күн сәулесінің «бағаналары» (экрандық, арзан)
	Sun->bEnableLightShaftBloom = true; Sun->BloomScale = 0.12f; Sun->BloomThreshold = 6.f;
	Sun->bEnableLightShaftOcclusion = true; Sun->OcclusionMaskDarkness = 0.35f;
	// бұлт көлеңкелері далада жылжиды (бұлтты күн)
	Sun->bCastCloudShadows = true; Sun->CloudShadowStrength = 0.85f; Sun->CloudShadowOnSurfaceStrength = 1.f;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>("Moon");
	Moon->SetupAttachment(RootComponent);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetAtmosphereSunLight(true);
	Moon->SetAtmosphereSunLightIndex(1);
	Moon->SetIntensity(0.25f);
	Moon->SetLightColor(FLinearColor(0.55f, 0.65f, 1.f));
	Moon->SetCastShadows(false);
	// ай дискі: күңгірт, кішкентай (жарқырауы «көпіршік» болып жайылмасын)
	Moon->SetAtmosphereSunDiskColorScale(FLinearColor(0.12f, 0.13f, 0.16f));
	Moon->ForwardShadingPriority = 0;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>("Atmosphere");
	Atmosphere->SetupAttachment(RootComponent);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>("SkyLight");
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SetIntensity(1.f);

	HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>("HeightFog");
	HeightFog->SetupAttachment(RootComponent);
	HeightFog->SetVolumetricFog(true);
	HeightFog->SetFogHeightFalloff(0.08f);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>("Clouds");
	Clouds->SetupAttachment(RootComponent);

	Post = CreateDefaultSubobject<UPostProcessComponent>("Post");
	Post->SetupAttachment(RootComponent);
	Post->bUnbound = true;
}

void AWeatherManager::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(FPlatformTime::Cycles());
	if (UMaterialInterface* CM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
	{
		CloudMID = UMaterialInstanceDynamic::Create(CM, this);
		Clouds->SetMaterial(CloudMID);
		TArray<FMaterialParameterInfo> Inf; TArray<FGuid> Ids;
		CM->GetAllScalarParameterInfo(Inf, Ids); for (const FMaterialParameterInfo& I : Inf) CloudScalars.Add(I.Name);
		Inf.Reset(); Ids.Reset(); CM->GetAllVectorParameterInfo(Inf, Ids); for (const FMaterialParameterInfo& I : Inf) CloudVectors.Add(I.Name);
		FString Sc, Vc; for (const FName& N : CloudScalars) Sc += N.ToString() + TEXT(" "); for (const FName& N : CloudVectors) Vc += N.ToString() + TEXT(" ");
		UE_LOG(LogTemp, Display, TEXT("KAPWEATHER cloud scalars: %s | vectors: %s"), *Sc, *Vc);
	}
	EnvMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/KAP/Materials/MPC_KAPEnv.MPC_KAPEnv"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	// жаңбыр тамшылары, найзағай сызығы (ойында ғана)
	if (GetWorld()->IsGameWorld()) GetWorld()->SpawnActor<AKAPWeatherFX>();
	// Сыртқы ашылу (auto exposure) шамалы — дала жарық, түн қараңғы болуы үшін
	FPostProcessSettings& P = Post->Settings;
	P.bOverride_AutoExposureMinBrightness = true; P.AutoExposureMinBrightness = 0.5f;
	P.bOverride_AutoExposureMaxBrightness = true; P.AutoExposureMaxBrightness = 12.f;
	P.bOverride_BloomIntensity = true; P.BloomIntensity = 0.6f;
	P.bOverride_VignetteIntensity = true; P.VignetteIntensity = 0.35f;
	P.bOverride_FilmGrainIntensity = true; P.FilmGrainIntensity = 0.08f;
}

AWeatherManager* AWeatherManager::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AWeatherManager> It(W); It; ++It) return *It;
	return nullptr;
}

const FKAPWeatherSeg* AWeatherManager::SegAt(const FKAPSession& S, double Hour) const
{
	for (const FKAPWeatherSeg& Seg : S.Weather) if (Hour >= Seg.Start && Hour < Seg.Start + Seg.Dur) return &Seg;
	return nullptr;
}

/** Ауа райының табиғи тізбегі: бұлт → жаңбыр → найзағай, ыстық → гроза/шаң дауылы, қатты жел → құм дауылы */
void AWeatherManager::GenSchedule(FKAPSession& S, double Until)
{
	double T = S.Weather.Num() ? S.Weather.Last().Start + S.Weather.Last().Dur : S.Time;
	while (T < Until)
	{
		const EWeatherType Prev = S.Weather.Num() ? S.Weather.Last().Type : EWeatherType::Clear;
		const double H = FMath::Fmod(T, 24.0); const float R = Rng.FRand();
		const bool bDay = H > 9 && H < 19, bAfternoon = H > 12 && H < 19, bNightMorning = H > 22 || H < 8;
		FKAPWeatherSeg Seg; Seg.Start = T; Seg.Type = EWeatherType::Clear; Seg.Dur = 2.5 + Rng.FRand() * 5;
		auto Set = [&](EWeatherType W, double D0, double D1) { Seg.Type = W; Seg.Dur = D0 + Rng.FRand() * (D1 - D0); };
		switch (Prev)
		{
		case EWeatherType::Clear:
			if (bDay && H > 10 && H < 16 && R < 0.22f) Set(EWeatherType::Heat, 3, 6);
			else if (R < 0.48f) Set(EWeatherType::Cloudy, 2, 5);
			else if (R < 0.6f) Set(EWeatherType::Windy, 1.5, 3.5);
			else if (R < 0.68f) Set(EWeatherType::Storm, 0.8, 2);
			else if (bNightMorning && R < 0.82f) Set(EWeatherType::Fog, 1.5, 3.5);
			break;
		case EWeatherType::Cloudy:
			if (R < 0.38f) Set(EWeatherType::Rain, 1, 3);
			else if (R < (bAfternoon ? 0.62f : 0.5f)) Set(EWeatherType::Thunder, 0.8, 2);
			else if (R < 0.7f) Set(EWeatherType::Windy, 1.5, 3);
			break;
		case EWeatherType::Rain:
			if (R < 0.2f) Set(EWeatherType::Thunder, 0.7, 1.6);
			else if (R < 0.65f) Set(EWeatherType::Cloudy, 1.5, 3.5);
			break;
		case EWeatherType::Thunder:
			if (R < 0.45f) Set(EWeatherType::Rain, 1, 2.5);
			else if (R < 0.8f) Set(EWeatherType::Cloudy, 1.5, 3);
			break;
		case EWeatherType::Heat:
			if (bAfternoon && R < 0.28f) Set(EWeatherType::Thunder, 0.8, 1.8); // ыстықтан кейінгі гроза
			else if (R < 0.42f) Set(EWeatherType::Storm, 0.8, 1.6);           // құрғақ шаң дауылы
			else if (R < 0.55f) Set(EWeatherType::Windy, 1.5, 3);
			break;
		case EWeatherType::Windy:
			if (R < 0.35f) Set(EWeatherType::Storm, 0.8, 2);                  // жел күшейіп, құм көтереді
			else if (R < 0.6f) Set(EWeatherType::Cloudy, 2, 4);
			break;
		case EWeatherType::Storm:
			if (R < 0.4f) Set(EWeatherType::Windy, 1, 2.5);
			break;
		case EWeatherType::Fog:
			if (R < 0.3f) Set(EWeatherType::Cloudy, 2, 4);
			break;
		}
		S.Weather.Add(Seg);
		T += Seg.Dur;
	}
}

AWeatherManager::FTarget AWeatherManager::TargetOf(EWeatherType T)
{
	switch (T)
	{
	//                                     Storm  Rain   Fog   Heat  Cloud  Thund  Wind м/с  Көріну м
	case EWeatherType::Heat:    return { 0.f,   0.f,   0.f,  1.f,  0.05f, 0.f,   2.f,  3200.f };
	case EWeatherType::Storm:   return { 1.f,   0.f,   0.f,  0.f,  0.55f, 0.f,   19.f, 36.f };
	case EWeatherType::Rain:    return { 0.f,   0.75f, 0.f,  0.f,  0.92f, 0.f,   6.f,  750.f };
	case EWeatherType::Fog:     return { 0.f,   0.f,   1.f,  0.f,  0.6f,  0.f,   1.f,  55.f };
	case EWeatherType::Cloudy:  return { 0.f,   0.f,   0.f,  0.f,  0.52f, 0.f,   5.f,  4500.f };
	case EWeatherType::Thunder: return { 0.f,   1.f,   0.f,  0.f,  1.f,   1.f,   11.f, 450.f };
	case EWeatherType::Windy:   return { 0.12f, 0.f,   0.f,  0.f,  0.35f, 0.f,   14.f, 1900.f };
	default:                    return { 0.f,   0.f,   0.f,  0.f,  0.18f, 0.f,   3.5f, 5500.f };
	}
}

void AWeatherManager::Strike(const FVector& Around)
{
	// жақын соққы сирек: 25% — 1.2 км ішінде, қалғаны 1.2–4.5 км
	const float D = Rng.FRand() < 0.25f ? Rng.FRandRange(150.f, 1200.f) : Rng.FRandRange(1200.f, 4500.f);
	const float A = Rng.FRandRange(0.f, UE_TWO_PI);
	FVector P = Around + FVector(FMath::Cos(A), FMath::Sin(A), 0) * (D * 100.f);
	P.Z = KAPVis::GroundZ(GetWorld(), P.X, P.Y, Around.Z);
	Env.StrikeAt = P;
	Env.StrikeSeq++;
	Env.StrikeDelay = D / 343.f;
	Env.Lightning = 1.f;
}

void AWeatherManager::UpdateEnv(FKAPSession& S, float Dt, double Dh)
{
	while (S.Weather.Num() && S.Weather[0].Start + S.Weather[0].Dur < S.Time - 2) S.Weather.RemoveAt(0);
	GenSchedule(S, S.Time + 36);
	EWeatherType Cur = EWeatherType::Clear;
	const FKAPWeatherSeg* Seg = SegAt(S, S.Time);
	if (bOverride) { Cur = OverrideType; }
	else if (Seg) Cur = Seg->Type;
	Env.Cur = Cur;
	float Ramp = bOverride ? FMath::Clamp(OverrideK, 0.f, 1.f) : 1.f;
	if (!bOverride && Seg) Ramp = FMath::Clamp((float)FMath::Min(S.Time - Seg->Start, Seg->Start + Seg->Dur - S.Time) / 0.2f, 0.f, 1.f);
	const FTarget Tc = TargetOf(EWeatherType::Clear), Tw = TargetOf(Cur);
	auto Mix = [Ramp](float A, float B) { return FMath::Lerp(A, B, Ramp); };
	const float K = bInstant ? 1.f : FMath::Min(1.f, Dt * 0.6f);
	Env.Storm = FMath::Lerp(Env.Storm, Mix(Tc.Storm, Tw.Storm), K); Env.Rain = FMath::Lerp(Env.Rain, Mix(Tc.Rain, Tw.Rain), K);
	Env.Fog = FMath::Lerp(Env.Fog, Mix(Tc.Fog, Tw.Fog), K); Env.Heat = FMath::Lerp(Env.Heat, Mix(Tc.Heat, Tw.Heat), K);
	Env.Cloud = FMath::Lerp(Env.Cloud, Mix(Tc.Cloud, Tw.Cloud), bInstant ? 1.f : FMath::Min(1.f, Dt * 0.25f));
	Env.Thunder = FMath::Lerp(Env.Thunder, Mix(Tc.Thunder, Tw.Thunder), K);
	// ---- жел: орташа жылдамдық ауа райына қарай, бағыты баяу бұрылады, екпіні — шу (гроза мен қатты желде күшті) ----
	WindT += Dt;
	WindBase = FMath::Lerp(WindBase, Mix(Tc.WindMs, Tw.WindMs), bInstant ? 1.f : FMath::Min(1.f, Dt * 0.15f));
	if (Seg) { const uint32 Hs = GetTypeHash(FMath::FloorToInt64(Seg->Start * 100.0)); YawTarget = 20.f + ((Hs % 1000) / 1000.f - 0.5f) * 140.f; }
	YawBase = FMath::FInterpTo(YawBase, YawTarget, Dt, 0.05f);
	const float Gusty = 0.25f + 0.45f * FMath::Max(Env.Thunder, FMath::Clamp((WindBase - 8.f) / 8.f, 0.f, 1.f));
	Env.Gust = FMath::Max(0.3f, 1.f + Gusty * FMath::PerlinNoise1D(WindT * 0.21f) + 0.12f * FMath::PerlinNoise1D(WindT * 1.3f + 3.7f));
	Env.WindMs = ForceWindMs >= 0.f ? ForceWindMs : WindBase * Env.Gust;
	Env.WindYaw = ForceWindYaw > -900.f ? ForceWindYaw : YawBase + 14.f * FMath::PerlinNoise1D(WindT * 0.05f + 11.f);
	Env.Wind = FRotator(0, Env.WindYaw, 0).Vector() * (Env.WindMs * 100.f);
	bInstant = false;
	// ---- жер ылғалы: жаңбыр сіңеді, күн мен жел кептіреді (батпақ 2–4 сағатта кебеді) ----
	const double H = FMath::Fmod(S.Time, 24.0);
	const float SunF = FMath::Sin((float)((H - 9.0) / 24.0 * UE_TWO_PI));
	const float Elev = FMath::Sin((float)((H - 6.0) / 12.0 * UE_PI));
	const float Dry = (0.16f + 0.3f * Env.Heat + 0.012f * Env.WindMs + (Elev > 0 ? 0.08f : 0.f)) * (1.f - Env.Cloud * 0.4f);
	Env.Wet = FMath::Clamp(Env.Wet + (Env.Rain > 0.15f ? Env.Rain * 0.9f : -Dry) * (float)Dh, 0.f, 1.f);
	Env.Temp = 21.f + 13.f * SunF * (1.f - Env.Cloud * 0.35f) + Env.Heat * 10.f - Env.Rain * 6.f - Env.Storm * 3.f - FMath::Max(0.f, Env.WindMs - 6.f) * 0.25f;
	Env.bNight = Elev < 0;
	// ---- көріну: ашық күні таулар көрінеді (5.5 км), түнде қараңғылық шектейді ----
	float VisT = FMath::Exp(FMath::Lerp(FMath::Loge(Tc.Vis), FMath::Loge(Tw.Vis), Ramp));
	if (Elev <= 0) VisT = FMath::Min(VisT, FMath::Lerp(5500.f, 1600.f, FMath::Clamp(-Elev * 4.f, 0.f, 1.f)));
	const float Vis0 = FMath::Max(10.f, Env.Visibility);
	const float Vis = FMath::Exp(FMath::Lerp(FMath::Loge(Vis0), FMath::Loge(VisT), K));
	Env.Visibility = VisOverride > 0 ? VisOverride : Vis;
	// ---- найзағай: грозада жиі, жай жаңбырда сирек ----
	Env.Lightning = FMath::Max(0.f, Env.Lightning - Dt * 4.f);
	const float Rate = Env.Thunder * 0.11f + (Env.Rain > 0.6f ? 0.008f : 0.f);
	if (Rate > 0.f && Rng.FRand() < Dt * Rate)
	{
		FVector At = GetActorLocation();
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (PC->PlayerCameraManager) At = PC->PlayerCameraManager->GetCameraLocation();
		Strike(At);
	}
	ApplyVisuals(S.Time, Dt);
}

void AWeatherManager::ApplyVisuals(double Hour, float Dt)
{
	const double H = FMath::Fmod(Hour, 24.0);
	const float A = (float)((H - 6.0) / 12.0 * UE_PI);
	const float ElevDeg = FMath::Sin(A) * 68.f;
	// Күн: шығыстан батысқа, биіктігі синусоида бойынша
	const float Yaw = FMath::RadiansToDegrees(A) - 90.f;
	Sun->SetWorldRotation(FRotator(-ElevDeg, Yaw, 0));
	Moon->SetWorldRotation(FRotator(-FMath::Max(10.f, -ElevDeg), Yaw + 180.f, 0));
	const float Day = FMath::Clamp(FMath::Sin(A) * 3.f + 0.2f, 0.f, 1.f);
	const float Dim = (1 - Env.Storm * 0.85f) * (1 - Env.Rain * 0.6f) * (1 - Env.Fog * 0.4f) * (1 - Env.Cloud * 0.22f);
	Sun->SetIntensity(FMath::Max(0.f, FMath::Sin(A)) > 0 ? 10.f * Dim + Env.Lightning * 15.f : Env.Lightning * 6.f);
	Sun->SetTemperature(FMath::Lerp(3200.f, 6000.f, FMath::Clamp(FMath::Sin(A) * 2.f, 0.f, 1.f)));
	Moon->SetIntensity((1 - Day) * 0.6f * Dim); // ай: суық, әлсіз, бірақ пішіндер көрінеді
	// Тұман: көріну қашықтығы → тығыздық
	const float Vis = FMath::Max(8.f, Env.Visibility);
	HeightFog->SetFogDensity(FMath::Clamp(28.f / Vis, 0.01f, 5.f));
	// күндізгі мұнар — жылы сұр-охра шаң (көк емес); жаңбыр — суық сұр; тұман — сүтті; дауыл — қою қоңыр
	const FLinearColor StormC(0.42f, 0.29f, 0.17f), RainC(0.3f, 0.33f, 0.36f), FogC(0.58f, 0.58f, 0.57f), ClearC(0.6f, 0.54f, 0.45f);
	FLinearColor C = ClearC;
	C = FMath::Lerp(C, RainC, Env.Rain); C = FMath::Lerp(C, FogC, Env.Fog); C = FMath::Lerp(C, StormC, Env.Storm);
	C *= 0.12f + 0.88f * Day;
	const float Night = 1.f - Day;
	C = FMath::Lerp(C, FLinearColor(0.03f, 0.045f, 0.085f), Night * (1.f - Env.Storm * 0.5f)); // суық көкшіл түн
	HeightFog->SetFogInscatteringColor(C);
	HeightFog->SetFogMaxOpacity(Env.Storm > 0.3f || Env.Fog > 0.3f ? 1.f : 0.97f);
	HeightFog->SetFogHeightFalloff(FMath::Lerp(FMath::Lerp(0.1f, 0.03f, Night), 0.004f, FMath::Max(Env.Storm, Env.Fog)));
	// күн жағына қарағанда мұнар жарқырайды (кинематографиялық «жарық ауа»)
	const FLinearColor SunGlow = FLinearColor(0.62f, 0.45f, 0.27f) * Day * Dim + FLinearColor(0.025f, 0.035f, 0.06f) * Night;
	HeightFog->SetDirectionalInscatteringColor(SunGlow);
	HeightFog->SetDirectionalInscatteringExponent(FMath::Lerp(7.f, 3.f, Env.Storm));
	HeightFog->SetDirectionalInscatteringStartDistance(1500.f);
	// жерге жабысқан шаң қабаты (~10 м): алыстағы аяқ-қол көмескі, қабат-қабат тереңдік
	HeightFog->SetSecondFogDensity(0.004f + 0.02f * (1.f - FMath::Clamp(Vis / 3000.f, 0.f, 1.f)) + Env.Storm * 0.22f + Env.Fog * 0.12f + Night * 0.01f);
	HeightFog->SetSecondFogHeightFalloff(0.9f);
	HeightFog->SetSecondFogHeightOffset(0.f);
	HeightFog->SetVolumetricFogDistance(7000.f);
	HeightFog->SetVolumetricFogScatteringDistribution(0.62f);
	HeightFog->SetVolumetricFogExtinctionScale(1.f + Night * 1.2f + Env.Storm * 4.f + Env.Fog * 2.5f);
	HeightFog->SetVolumetricFogAlbedo(FLinearColor(FMath::Lerp(FLinearColor(0.9f, 0.88f, 0.84f), FLinearColor(0.85f, 0.6f, 0.35f), Env.Storm)).ToFColor(true));
	// дауылда да жақын 1.5–3 м ашық: заттар қарамен көрінеді, ары қарай біртіндеп жоғалады (тегіс қабырға емес)
	HeightFog->SetStartDistance(FMath::Lerp(400.f, 150.f, FMath::Max(Env.Storm, Env.Fog)));
	// Бұлттар — жаңбыр мен дауылда қалыңырақ көрінуі үшін төмендейді
	Clouds->SetLayerBottomAltitude(FMath::Lerp(FMath::Lerp(5.f, 2.4f, Env.Cloud), 1.1f, FMath::Max(Env.Rain, Env.Storm)));
	Clouds->SetLayerHeight(FMath::Lerp(6.f, 9.f, Env.Thunder));
	if (CloudMID)
	{
		// бұлттылық: «күн бұлтты» — шашыраңқы бұлттар, жаңбыр/гроза — тұтас қара бұлт; жел бағытымен жылжиды
		if (CloudScalars.Contains("Cloud_GlobalCoverage")) CloudMID->SetScalarParameterValue("Cloud_GlobalCoverage", FMath::Lerp(-0.6f, 0.6f, Env.Cloud));
		if (CloudVectors.Contains("Cloud_GlobalCoverage")) CloudMID->SetVectorParameterValue("Cloud_GlobalCoverage", FLinearColor(FMath::Lerp(-0.6f, 0.6f, Env.Cloud), FMath::Lerp(-0.6f, 0.6f, Env.Cloud), FMath::Lerp(-0.6f, 0.6f, Env.Cloud), FMath::Lerp(-0.6f, 0.6f, Env.Cloud)));
		if (CloudScalars.Contains("Storm")) CloudMID->SetScalarParameterValue("Storm", FMath::Max(Env.Thunder, Env.Rain * 0.6f));
		const FVector Wv = Env.Wind / 100.f;
		if (CloudVectors.Contains("CloudWind")) CloudMID->SetVectorParameterValue("CloudWind", FLinearColor(Wv.X * 0.02f, Wv.Y * 0.02f, 0.f, 1.f));
		if (CloudScalars.Contains("WindMultiplier")) CloudMID->SetScalarParameterValue("WindMultiplier", 0.5f + Env.WindMs * 0.08f);
	}
	if (EnvMPC) if (UMaterialParameterCollectionInstance* MI = GetWorld()->GetParameterCollectionInstance(EnvMPC))
	{
		const FVector Wd = Env.Wind.GetSafeNormal2D();
		MI->SetScalarParameterValue("Wetness", Env.Wet);
		MI->SetScalarParameterValue("Rain", Env.Rain);
		MI->SetScalarParameterValue("WindX", Wd.X);
		MI->SetScalarParameterValue("WindY", Wd.Y);
		MI->SetScalarParameterValue("WindSpeed", Env.WindMs);
		MI->SetScalarParameterValue("Puddles", Env.Wet);
	}
	SkyLight->SetIntensity(0.16f + Day * 0.95f + Env.Lightning * 2.f);
	// Пост-процесс: дауылда қоңыр, ыстықта «жылу» реңкі
	FPostProcessSettings& P = Post->Settings;
	P.bOverride_ColorSaturation = true;
	const float Sat = 0.82f - Env.Storm * 0.3f - Night * 0.2f; // «сөнген» түстер (0.7–0.85)
	P.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	P.bOverride_ColorContrast = true; P.ColorContrast = FVector4(1.f + Night * 0.12f, 1.f + Night * 0.12f, 1.f + Night * 0.12f, 1.f);
	P.BloomIntensity = 0.65f - Night * 0.3f; P.VignetteIntensity = 0.4f + Night * 0.1f; P.FilmGrainIntensity = 0.15f + Night * 0.08f;
	P.bOverride_SceneFringeIntensity = true; P.SceneFringeIntensity = 0.25f + Env.Storm * 0.6f + Fear * 2.2f;
	// қорқыныш: экран шеттері қараяды, дән мен түссіздену күшейеді
	P.VignetteIntensity += Fear * 0.45f; P.FilmGrainIntensity += Fear * 0.3f;
	P.ColorSaturation = FVector4(Sat * (1.f - Fear * 0.35f), Sat * (1.f - Fear * 0.35f), Sat * (1.f - Fear * 0.35f), 1.f);
	P.bOverride_MotionBlurAmount = true; P.MotionBlurAmount = 0.25f;
	P.bOverride_BloomThreshold = true; P.BloomThreshold = FMath::Lerp(-1.f, 1.2f, Night); // түнде тек шамдар жарқырасын
	P.bOverride_ColorGain = true;
	// түс сценарийі (атмосфера құжаты): таң — қызғылт-күлгін, күндіз — ағарған охра, кеш — алтын-қызыл, түн — суық көк, дауыл — қоңыр-сары
	auto Bump = [](float X, float C, float W) { return FMath::Clamp(1.f - FMath::Abs(X - C) / W, 0.f, 1.f); };
	const float Dawn = Bump((float)H, 6.3f, 1.4f), Dusk = Bump((float)H, 17.4f, 1.3f);
	const float DayK = FMath::Max(0.f, Day - Dawn - Dusk);
	FVector G = FVector(1, 1, 1);
	G = FMath::Lerp(G, FVector(1.07f, 1.0f, 0.86f), DayK);
	G = FMath::Lerp(G, FVector(1.06f, 0.93f, 1.08f), Dawn);
	G = FMath::Lerp(G, FVector(1.18f, 0.92f, 0.74f), Dusk);
	G = FMath::Lerp(G, FVector(0.84f, 0.95f, 1.16f), Night);
	G = FMath::Lerp(G, FVector(1.06f, 1.0f, 0.86f), Env.Storm);
	G += FVector(0.05f, 0.02f, -0.04f) * Env.Heat;
	P.ColorGain = FVector4(G.X, G.Y, G.Z, 1.f);
	const float Ct = 1.f + DayK * 0.06f + Night * 0.12f - Env.Storm * 0.1f;
	P.ColorContrast = FVector4(Ct, Ct, Ct, 1.f);
	// фильм реңкі: көлеңкелер суық, жарық жерлер жылы, қара түс тереңірек
	P.bOverride_ColorGainShadows = true; P.ColorGainShadows = FVector4(0.93f, 0.97f, 1.07f, 1.f);
	P.bOverride_ColorGainHighlights = true; P.ColorGainHighlights = FVector4(1.04f, 1.f, 0.94f, 1.f);
	P.bOverride_FilmToe = true; P.FilmToe = 0.6f;
	P.bOverride_FilmSlope = true; P.FilmSlope = 0.86f;
	P.bOverride_WhiteTemp = true;
	P.WhiteTemp = 6500.f - Env.Heat * 600.f + (1 - Day) * 1200.f;
	// экспозиция (EV100): күндіз кең, түнде −1.5…1 — фара мен прожектор басым жарық, бірақ ай жарығында пішіндер көрінеді
	P.bOverride_AutoExposureBias = true; P.AutoExposureBias = -0.3f * Night - 0.1f * Indoor;
	// ішке кіргенде көз біртіндеп үйренеді (бұрын күндіз EV 4.5-тен төмен түспейтін — шам жанып тұрса да іші қап-қараңғы еді)
	P.AutoExposureMinBrightness = FMath::Lerp(FMath::Lerp(0.5f, 1.f, Indoor), FMath::Lerp(-1.5f, -0.8f, Indoor), Night);
	// түнде экспозиция ең жарық бөлікке қарай өлшенеді: фонарь/фара түскен жер ағарып кетпейді, айнала — қараңғы
	P.AutoExposureMaxBrightness = FMath::Lerp(12.f, 6.f, Night);
	P.bOverride_AutoExposureLowPercent = true; P.AutoExposureLowPercent = FMath::Lerp(10.f, 62.f, Night);
	P.bOverride_AutoExposureHighPercent = true; P.AutoExposureHighPercent = FMath::Lerp(90.f, 97.f, Night);
	// дауыл: біркелкі жарық «қабырға» емес, күңгірт, ауыр
	P.AutoExposureBias += -0.7f * Env.Storm;
	P.bOverride_AutoExposureSpeedUp = true; P.AutoExposureSpeedUp = bFastExposure ? 40.f : 2.f;
	P.bOverride_AutoExposureSpeedDown = true; P.AutoExposureSpeedDown = bFastExposure ? 40.f : 1.f;
	SkyRecaptureAcc += Dt;
	if (SkyRecaptureAcc > 0.5f) { SkyRecaptureAcc = 0; SkyLight->RecaptureSky(); }
}
