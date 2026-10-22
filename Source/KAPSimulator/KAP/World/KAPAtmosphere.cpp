#include "KAPAtmosphere.h"
#include "KAPVisuals.h"
#include "KAPFXManager.h"
#include "KAP/Audio/KAPAudio.h"
#include "WeatherManager.h"
#include "KAPVehicleHandle.h"
#include "KAPWorldBuilder.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAPSimulatorPawn.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

static constexpr int32 NumDust = 2200;
static constexpr float DustBox = 1800.f; // см, камера айналасындағы куб
static constexpr int32 NumPuffs = 40;
static constexpr float PuffBox = 6000.f;

AKAPAtmosphere::AKAPAtmosphere()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

AKAPAtmosphere* AKAPAtmosphere::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AKAPAtmosphere> It(W); It; ++It) return *It;
	return W->SpawnActor<AKAPAtmosphere>();
}

/** Жұмсақ шаң материалы (M_KAP_Dust2: шеті біртіндеп мөлдір, көлемдік жарық); жоқ болса — ескісі */
static UMaterialInterface* DustMaterial()
{
	if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Dust2.M_KAP_Dust2"), nullptr, LOAD_NoWarn | LOAD_Quiet)) return M;
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Dust.M_KAP_Dust"), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

void AKAPAtmosphere::BeginPlay()
{
	Super::BeginPlay();
	// ---- шаң бөлшектері ----
	Dust = NewObject<UInstancedStaticMeshComponent>(this);
	Dust->SetStaticMesh(KAPVis::Mesh(EKAPShape::Sphere));
	Dust->SetupAttachment(RootComponent);
	Dust->SetAbsolute(true, true, true);
	Dust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dust->SetCastShadow(false);
	Dust->bAffectDistanceFieldLighting = false;
	if (UMaterialInterface* M = DustMaterial())
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, Dust);
		MID->SetVectorParameterValue("Color", FLinearColor(0.85f, 0.78f, 0.66f));
		Dust->SetMaterial(0, MID);
	}
	else Dust->SetMaterial(0, UMaterialInstanceDynamic::Create(KAPVis::TranslucentMaterial(), Dust));
	Dust->RegisterComponent();
	FRandomStream R(31);
	for (int32 i = 0; i < NumDust; i++)
	{
		DustPos.Add(FVector(R.FRandRange(-DustBox, DustBox), R.FRandRange(-DustBox, DustBox), R.FRandRange(-DustBox * 0.4f, DustBox)));
		DustVel.Add(FVector(R.FRandRange(-20.f, 20.f), R.FRandRange(-20.f, 20.f), R.FRandRange(-40.f, -8.f)));
		DustXf.Add(FTransform(FQuat::Identity, DustPos.Last(), FVector(0.012f)));
	}
	Dust->AddInstances(DustXf, false, false);
	// ---- шаң бұлттары (500–1500 см) және жер бетімен жылжитын құм ағыны ----
	Puffs = NewObject<UInstancedStaticMeshComponent>(this);
	Puffs->SetStaticMesh(KAPVis::Mesh(EKAPShape::Sphere));
	Puffs->SetupAttachment(RootComponent);
	Puffs->SetAbsolute(true, true, true);
	Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puffs->SetCastShadow(false);
	Puffs->bAffectDistanceFieldLighting = false;
	Puffs->TranslucencySortPriority = -1;
	if (UMaterialInterface* M = DustMaterial())
	{
		PuffMID = UMaterialInstanceDynamic::Create(M, Puffs);
		PuffMID->SetVectorParameterValue("Color", FLinearColor(0.78f, 0.62f, 0.42f));
		PuffMID->SetScalarParameterValue("Opacity", 0.08f);
		Puffs->SetMaterial(0, PuffMID);
	}
	Puffs->RegisterComponent();
	for (int32 i = 0; i < NumPuffs; i++)
	{
		PuffPos.Add(FVector(R.FRandRange(-PuffBox, PuffBox), R.FRandRange(-PuffBox, PuffBox), 0));
		PuffXf.Add(FTransform(FQuat::Identity, PuffPos.Last(), FVector(0.f)));
	}
	Puffs->AddInstances(PuffXf, false, false);
	Devil = NewObject<UInstancedStaticMeshComponent>(this);
	Devil->SetStaticMesh(KAPVis::Mesh(EKAPShape::Sphere));
	Devil->SetupAttachment(RootComponent); Devil->SetAbsolute(true, true, true);
	Devil->SetCollisionEnabled(ECollisionEnabled::NoCollision); Devil->SetCastShadow(false);
	if (PuffMID) { UMaterialInstanceDynamic* DM = UMaterialInstanceDynamic::Create(PuffMID->Parent, Devil); DM->SetVectorParameterValue("Color", FLinearColor(0.5f, 0.38f, 0.25f)); DM->SetScalarParameterValue("Opacity", 0.38f); Devil->SetMaterial(0, DM); }
	Devil->RegisterComponent();
	for (int32 i = 0; i < 64; i++) DevilXf.Add(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.f)));
	Devil->AddInstances(DevilXf, false, false);
	// ---- із декалдары ----
	UMaterialInterface* TM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Track.M_KAP_Track"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (TM)
		for (int32 i = 0; i < 700; i++)
		{
			UDecalComponent* D = NewObject<UDecalComponent>(this);
			D->SetupAttachment(RootComponent);
			D->SetAbsolute(true, true, true);
			D->SetFadeScreenSize(0.002f);
			FTrack T; T.D = D; T.M = UMaterialInstanceDynamic::Create(TM, D);
			D->SetDecalMaterial(T.M);
			D->SetVisibility(false);
			D->RegisterComponent();
			Tracks.Add(T);
		}
	TuneAllLights();
}

void AKAPAtmosphere::AddLampPost(FVector Base, float Yaw, bool bBeacon)
{
	const FKAPMat Pole = FKAPMat::RustySteel(FLinearColor(0.32f, 0.33f, 0.34f), 0.5f);
	const FRotator R(0, Yaw, 0);
	const FVector Up(0, 0, 1), Fw = R.Vector();
	UStaticMeshComponent* P = KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Base + Up * 450, FVector(14, 14, 900), Pole, FRotator::ZeroRotator, true);
	P->SetAbsolute(true, true, true); P->SetWorldLocation(Base + Up * 450);
	UStaticMeshComponent* Arm = KAPVis::Add(this, RootComponent, EKAPShape::Cube, FVector::ZeroVector, FVector(160, 10, 10), Pole, FRotator::ZeroRotator, false);
	Arm->SetAbsolute(true, true, true); Arm->SetWorldLocationAndRotation(Base + Up * 895 + Fw * 70, R);
	FLamp L;
	L.Bulb = KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector::ZeroVector, FVector(40, 40, 12), FKAPMat::Glow(FLinearColor(1.f, 0.93f, 0.8f), 0), FRotator::ZeroRotator, false);
	L.Bulb->SetAbsolute(true, true, true); L.Bulb->SetWorldLocation(Base + Up * 884 + Fw * 140);
	L.L = NewObject<USpotLightComponent>(this);
	L.L->SetupAttachment(RootComponent);
	L.L->SetAbsolute(true, true, true);
	L.L->SetWorldLocationAndRotation(Base + Up * 878 + Fw * 140, FRotator(-90, 0, 0));
	L.L->SetIntensityUnits(ELightUnits::Candelas);
	L.L->SetIntensity(0.f);
	L.L->SetLightColor(FLinearColor(0.82f, 0.88f, 1.f));
	L.L->SetOuterConeAngle(42.f); L.L->SetInnerConeAngle(18.f);
	L.L->SetAttenuationRadius(2600.f);
	L.L->SetVolumetricScatteringIntensity(2.5f);
	L.L->bCastVolumetricShadow = true;
	L.L->SetCastShadows(false);
	L.L->bCastVolumetricShadow = false;
	L.L->MaxDrawDistance = 9000.f;
	L.L->MaxDistanceFadeRange = 1500.f;
	L.L->RegisterComponent();
	if (bBeacon)
	{
		L.Beacon = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector::ZeroVector, FVector(18, 18, 18), FKAPMat::Glow(FLinearColor(1, 0.05f, 0.03f), 60), FRotator::ZeroRotator, false);
		L.Beacon->SetAbsolute(true, true, true); L.Beacon->SetWorldLocation(Base + Up * 915);
	}
	Lamps.Add(L);
}

void AKAPAtmosphere::TuneAllLights()
{
	for (TObjectIterator<ULocalLightComponent> It; It; ++It)
	{
		ULocalLightComponent* L = *It;
		if (!L->GetWorld() || L->GetWorld() != GetWorld()) continue;
		if (L->VolumetricScatteringIntensity < 2.f) L->SetVolumetricScatteringIntensity(3.5f);
	}
}

void AKAPAtmosphere::StampTrack(FVector Pos, float Yaw, FVector Size, bool bTread)
{
	if (!Tracks.Num()) return;
	FTrack& T = Tracks[NextTrack];
	NextTrack = (NextTrack + 1) % Tracks.Num();
	T.Age = 0;
	T.D->DecalSize = Size;
	T.D->SetWorldLocationAndRotation(Pos, FRotator(-90, Yaw, 0));
	T.M->SetScalarParameterValue("Tread", bTread ? 1.f : 0.f);
	T.M->SetScalarParameterValue("Opacity", 0.85f);
	T.D->SetVisibility(true);
	T.D->MarkRenderStateDirty();
}

void AKAPAtmosphere::UpdateTracks(float Dt)
{
	// уақыт өте (және желде тезірек) іздер өшеді
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Fade = WX ? (1.f + WX->Env.Storm * 6.f) / 240.f : 1.f / 240.f;
	for (FTrack& T : Tracks)
	{
		if (!T.D->IsVisible()) continue;
		T.Age += Dt * Fade;
		if (T.Age >= 1.f) { T.D->SetVisibility(false); continue; }
		if (T.Age > 0.5f) T.M->SetScalarParameterValue("Opacity", 0.85f * (1.f - (T.Age - 0.5f) * 2.f));
	}
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPCharacter* P = AKAPCharacter::Get(this);
	// жерге түсу: ауада болып, енді жерде — құлау жылдамдығына қарай дыбыс
	if (P && !P->IsHidden())
	{
		const bool bAir = P->GetCharacterMovement()->IsFalling();
		if (bAir) AirVz = FMath::Min(AirVz, (float)P->GetVelocity().Z);
		else if (bWasAir && P->GetCharacterMovement()->IsMovingOnGround() && AirVz < -250.f)
		{
			const int32 Surf = UKAPAudio::SurfaceOf(P->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent());
			if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Land(P->GetActorLocation() - FVector(0, 0, 88), Surf, FMath::Clamp((-AirVz - 250.f) / 900.f, 0.f, 1.f));
		}
		if (!bAir) AirVz = 0.f;
		bWasAir = bAir;
	}
	// аяқ іздері
	if (P && !P->IsHidden() && P->GetCharacterMovement()->IsMovingOnGround() && P->GetVelocity().Size2D() > 60.f)
	{
		const FVector Pos = P->GetActorLocation() - FVector(0, 0, 88);
		if (FVector::Dist2D(Pos, LastFoot) > 70.f)
		{
			LastFoot = Pos;
			bLeftFoot = !bLeftFoot;
			const float Yaw = P->GetVelocity().Rotation().Yaw;
			const FVector Side = FRotator(0, Yaw + 90, 0).Vector() * (bLeftFoot ? -13.f : 13.f);
			// беткей: асфальт жолдың өз тегі бар (SurfAsphalt) — бұрын ол «металл» деп танылып, қадамның орнына сыңғыр шығатын
			int32 Surf = UKAPAudio::SurfaceOf(P->GetCharacterMovement()->CurrentFloor.HitResult.GetComponent());
			const bool bSand = Surf == 0;
			if (bSand && GM && GM->DistToRoad(FVector2D(Pos) / 100.0) < 3.5) Surf = 4; // топырақ жол (қабатының соқтығысы жоқ, астында ландшафт)
			{ static uint32 Seen = 0; if (!(Seen & (1u << Surf))) { Seen |= 1u << Surf; UE_LOG(LogTemp, Display, TEXT("KAPTEST step surface=%d"), Surf); } }
			if (bSand) StampTrack(FVector(Pos.X, Pos.Y, KAPVis::GroundZ(GetWorld(), Pos.X, Pos.Y, Pos.Z)) + Side, Yaw, FVector(30, 7, 15), false);
			const bool bRun = P->GetVelocity().Size2D() > 500;
			if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Footstep(Pos, Surf, bRun ? 0.6f : 0.42f, bRun);
		}
	}
	// дөңгелек іздері (барлық қозғалыстағы көліктер)
	if (GM && GM->Builder)
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : GM->Builder->Vehicles)
		{
			if (!V.IsValid() || !V->Pawn) continue;
			AActor* A = V->Pawn;
			if (A->GetVelocity().Size2D() < 150.f) continue;
			FVector& Last = LastWheel.FindOrAdd(A);
			const FVector Pos = A->GetActorLocation();
			if (FVector::Dist2D(Pos, Last) < 35.f) continue;
			Last = Pos;
			const float Yaw = A->GetActorRotation().Yaw;
			const FVector Rt = FRotator(0, Yaw + 90, 0).Vector(), Fw = FRotator(0, Yaw, 0).Vector();
			for (float S : { -85.f, 85.f })
			{
				const FVector W = Pos + Rt * S - Fw * 140.f;
				StampTrack(FVector(W.X, W.Y, KAPVis::GroundZ(GetWorld(), W.X, W.Y, W.Z)), Yaw, FVector(40, 15, 40), true);
				if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) if (FMath::FRand() < 0.35f) FX->Spawn(EKAPFX::Dust, FVector(W.X, W.Y, KAPVis::GroundZ(GetWorld(), W.X, W.Y, W.Z) + 20) - Fw * 120.f, 0.5f + A->GetVelocity().Size2D() / 4000.f, -Fw * 80.f);
			}
		}
}

void AKAPAtmosphere::UpdateParticles(float Dt, const FVector& Cam)
{
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Storm = WX ? WX->Env.Storm : 0.f, Fog = WX ? WX->Env.Fog : 0.f;
	const bool bNight = WX && WX->Env.bNight;
	const FVector Wind = WX ? WX->Env.Wind * (0.25f + Storm * 0.6f) : FVector(50, 10, 0);
	// тығыздық: түнде көп (жарықта көрінеді), дауылда — ең көп
	const float Density = FMath::Clamp(0.15f + (bNight ? 0.45f : 0.f) + Fog * 0.3f + Storm * 0.9f, 0.f, 1.f);
	const int32 Active = FMath::RoundToInt32(NumDust * Density);
	const float Size = 0.03f + Storm * 0.012f;
	const float T = GetWorld()->GetTimeSeconds();
	const FQuat WindQ = (Wind.SizeSquared() > 1.f ? Wind.GetSafeNormal() : FVector::ForwardVector).ToOrientationQuat();
	for (int32 i = 0; i < NumDust; i++)
	{
		FVector& P = DustPos[i];
		const FVector Turb(FMath::Sin(T * 0.7f + i) * 25.f, FMath::Cos(T * 0.9f + i * 1.3f) * 25.f, FMath::Sin(T * 1.1f + i * 0.7f) * 10.f);
		P += (DustVel[i] + Wind + Turb) * Dt;
		// камераға қатысты орау (wrap)
		FVector Rel = P - Cam;
		for (int32 a = 0; a < 3; a++)
		{
			const float Lo = a == 2 ? -DustBox * 0.4f : -DustBox, Hi = DustBox;
			if (Rel[a] < Lo) Rel[a] += Hi - Lo; else if (Rel[a] > Hi) Rel[a] -= Hi - Lo;
		}
		P = Cam + Rel;
		// көз алдында (≤90 см) тұрған бөлшек үлкен «көпіршік» болып көрінбесін
		const float NearK = FMath::Clamp((Rel.Size() - 90.f) / 120.f, 0.f, 1.f);
		const float S = i < Active ? Size * (0.6f + 0.8f * ((i * 7919) % 100) / 100.f) * NearK : 0.f;
		DustXf[i].SetLocation(P);
		// дауылда жел бойымен созылған құм «сызықтары»
		DustXf[i].SetRotation(WindQ);
		DustXf[i].SetScale3D(FVector(S * (1.f + Storm * 5.f), S * (1.f - Storm * 0.4f), S * (1.f - Storm * 0.4f)));
	}
	Dust->BatchUpdateInstancesTransforms(0, DustXf, true, true, true);
	// шаң бұлттары: күндіз аздап (сағым/шаң), дауылда — қалың; жартысы жерге жабысқан жалпақ ағын
	if (!Puffs) return;
	const float PuffD = FMath::Clamp(0.12f + Storm * 0.88f + (bNight ? 0.1f : 0.f), 0.f, 1.f);
	const int32 PActive = FMath::RoundToInt32(NumPuffs * PuffD);
	if (PuffMID) PuffMID->SetScalarParameterValue("Opacity", 0.02f + Storm * 0.15f); // ашық ауада әлсіз — фонарь сәулесінде «сүт» болмасын
	const FVector PWind = WX ? WX->Env.Wind * (0.6f + Storm) : FVector(150, 40, 0);
	for (int32 i = 0; i < NumPuffs; i++)
	{
		FVector& P = PuffPos[i];
		const bool bDrift = (i & 1) == 0;
		P += PWind * Dt * (bDrift ? 1.4f : 0.8f);
		FVector Rel = P - Cam;
		for (int32 a = 0; a < 2; a++) { if (Rel[a] < -PuffBox) Rel[a] += 2 * PuffBox; else if (Rel[a] > PuffBox) Rel[a] -= 2 * PuffBox; }
		P = Cam + FVector(Rel.X, Rel.Y, 0);
		const float H = ((i * 2654435761u) >> 8) % 1000 / 1000.f;
		const float G = KAPVis::GroundZ(GetWorld(), P.X, P.Y, Cam.Z - 300.f);
		const float Sz = i < PActive ? (bDrift ? 4.f + H * 6.f : 5.f + H * 10.f) : 0.f;
		// камераға тым жақын бұлт — мөлдір шар «ішінде» тұрмас үшін кішірейеді
		const float Near = FMath::Clamp((FVector::Dist(P + FVector(0, 0, G - P.Z), Cam) - 300.f) / 900.f, 0.f, 1.f);
		PuffXf[i].SetLocation(FVector(P.X, P.Y, G + (bDrift ? 20.f : 150.f + H * 400.f)));
		PuffXf[i].SetRotation(FQuat(FVector::UpVector, H * 6.28f));
		PuffXf[i].SetScale3D(bDrift ? FVector(Sz * 1.6f, Sz * 0.7f, 0.35f + Storm * 0.6f) * Near : FVector(Sz, Sz, Sz * 0.55f) * Near);
	}
	Puffs->BatchUpdateInstancesTransforms(0, PuffXf, true, true, true);
}

void AKAPAtmosphere::Tick(float Dt)
{
	Super::Tick(Dt);
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	UpdateParticles(Dt, Cam);
	if (Devil) UpdateDevil(Dt, Cam);
	UpdateLife(Dt, Cam);
	UpdateTracks(Dt);
	// прожекторлар түнде, тұманда және дауылда жанады
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Want = WX && (WX->Env.bNight || WX->Env.Storm > 0.3f || WX->Env.Fog > 0.4f) ? 1.f : 0.f;
	LampLevel = LampLevel < 0 ? Want : FMath::FInterpTo(LampLevel, Want, Dt, 1.5f);
	const float Tm = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Lamps.Num(); i++)
	{
		FLamp& L = Lamps[i];
		// кейбір шамдар жыпылықтайды (ескі кеніш жабдығы)
		const float Flick = (i % 5 == 0) ? (FMath::Fmod(Tm * 3.1f + i, 7.f) < 0.15f ? 0.2f : 1.f) : 1.f;
		L.L->SetIntensity(LampLevel * 2600.f * Flick);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(L.Bulb->GetMaterial(0))) M->SetVectorParameterValue("Emissive", FLinearColor(1.f, 0.93f, 0.8f) * 40.f * LampLevel * Flick);
		if (L.Beacon) L.Beacon->SetVisibility(FMath::Fmod(Tm + i * 0.3f, 1.6f) < 0.8f);
	}
	ShadowT += Dt;
	if (ShadowT > 0.5f)
	{
		ShadowT = 0;
		TArray<int32> Idx; for (int32 i = 0; i < Lamps.Num(); i++) Idx.Add(i);
		Idx.Sort([&](int32 A, int32 B) { return FVector::DistSquared(Lamps[A].L->GetComponentLocation(), Cam) < FVector::DistSquared(Lamps[B].L->GetComponentLocation(), Cam); });
		for (int32 k = 0; k < Idx.Num(); k++) { const bool bNear = k < 4; USpotLightComponent* L = Lamps[Idx[k]].L; if (L->CastShadows != bNear) { L->SetCastShadows(bNear); L->bCastVolumetricShadow = bNear; L->MarkRenderStateDirty(); } }
	}
	TuneT += Dt;
	if (TuneT > 5.f) { TuneT = 0; TuneAllLights(); } // жаңадан пайда болған жарықтар (снарядтар, өрт)
}

void AKAPAtmosphere::UpdateDevil(float Dt, const FVector& Cam)
{
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const bool bOk = WX && !WX->Env.bNight && WX->Env.Storm < 0.2f && WX->Env.Rain < 0.2f;
	if (DevilT < 0)
	{
		DevilCd -= Dt;
		if (DevilCd > 0 || !bOk) return;
		// жаңа құйын: ойыншыдан 60–140 м, желмен бірге баяу жылжиды
		const float A = FMath::FRandRange(0.f, UE_TWO_PI);
		DevilPos = Cam + FVector(FMath::Cos(A), FMath::Sin(A), 0) * FMath::FRandRange(6000.f, 14000.f);
		DevilVel = (WX->Env.Wind.GetSafeNormal2D() * 0.7f + FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), 0)) * FMath::FRandRange(250.f, 450.f);
		DevilT = 0; DevilLife = FMath::FRandRange(20.f, 35.f);
	}
	DevilT += Dt;
	DevilPos += DevilVel * Dt;
	const float G = KAPVis::GroundZ(GetWorld(), DevilPos.X, DevilPos.Y, Cam.Z - 300.f);
	// пайда болу/өшу
	const float Fade = FMath::Min(1.f, FMath::Min(DevilT / 4.f, (DevilLife - DevilT) / 4.f));
	const float H = 1800.f * Fade;
	for (int32 i = 0; i < DevilXf.Num(); i++)
	{
		const float U = i / (float)DevilXf.Num();
		const float Z = U * H;
		const float R = 60.f + U * U * 420.f;                            // жоғары қарай кеңейетін воронка
		const float Ang = DevilT * (5.5f - U * 2.f) + i * 2.4f;
		const float Sway = FMath::Sin(DevilT * 0.7f + U * 3.f) * 120.f * U;
		const FVector P(DevilPos.X + FMath::Cos(Ang) * R + Sway, DevilPos.Y + FMath::Sin(Ang) * R, G + Z);
		const float S = (2.4f + U * 4.5f) * Fade * (0.7f + 0.3f * FMath::Sin(i * 1.7f));
		DevilXf[i].SetLocation(P);
		DevilXf[i].SetScale3D(FVector(S, S, S * 0.8f));
	}
	Devil->BatchUpdateInstancesTransforms(0, DevilXf, true, true, true);
	if (DevilT >= DevilLife) { DevilT = -1.f; DevilCd = FMath::FRandRange(40.f, 120.f); for (FTransform& X : DevilXf) X.SetScale3D(FVector::ZeroVector); Devil->BatchUpdateInstancesTransforms(0, DevilXf, true, true, true); }
}
void AKAPAtmosphere::ForceDevil()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator R(0, PC->PlayerCameraManager->GetCameraRotation().Yaw, 0);
	DevilPos = Cam + R.Vector() * 7000.f + FRotationMatrix(R).GetUnitAxis(EAxis::Y) * 2500.f;
	DevilVel = FRotationMatrix(R).GetUnitAxis(EAxis::Y) * -300.f;
	DevilT = 5.f; DevilLife = 30.f;
}
void AKAPAtmosphere::UpdateLife(float Dt, const FVector& Cam)
{
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Storm = WX ? WX->Env.Storm : 0.f;
	const bool bDay = WX && !WX->Env.bNight;
	const float T = GetWorld()->GetTimeSeconds();
	auto Mk = [&](EKAPShape S, FVector Sz, const FKAPMat& M)
	{
		UStaticMeshComponent* C = KAPVis::Add(this, RootComponent, S, FVector::ZeroVector, Sz, M, FRotator::ZeroRotator, false);
		C->SetAbsolute(true, true, true); C->SetCachedMaxDrawDistance(0.f); C->SetCastShadow(S == EKAPShape::Bush);
		return C;
	};
	if (!Birds.Num())
	{
		const FKAPMat Feather = FKAPMat::Paint(FLinearColor(0.05f, 0.045f, 0.04f), 0.9f);
		for (int32 i = 0; i < 5; i++)
		{
			FBird B;
			B.Body = Mk(EKAPShape::Sphere, FVector(70, 26, 20), Feather);
			B.WL = Mk(EKAPShape::CubeLow, FVector(40, 110, 3), Feather);
			B.WR = Mk(EKAPShape::CubeLow, FVector(40, 110, 3), Feather);
			B.Ph = FMath::FRandRange(0.f, UE_TWO_PI); B.Rad = FMath::FRandRange(2500.f, 5500.f); B.Alt = FMath::FRandRange(5000.f, 9000.f); B.Spd = FMath::FRandRange(0.08f, 0.14f) * (i % 2 ? 1.f : -1.f);
			Birds.Add(B);
		}
		FKAPMat Dry = FKAPMat::Paint(FLinearColor(0.5f, 0.4f, 0.24f), 0.95f); Dry.DustTop = 0.3f;
		for (int32 i = 0; i < 8; i++)
		{
			FWeed W; W.S = FMath::FRandRange(0.5f, 0.9f);
			W.C = Mk(EKAPShape::Bush, FVector(W.S * 100.f), Dry);
			const float A = FMath::FRandRange(0.f, UE_TWO_PI);
			W.P = Cam + FVector(FMath::Cos(A), FMath::Sin(A), 0) * FMath::FRandRange(800.f, 4000.f); W.Bph = FMath::FRandRange(0.f, 6.f);
			Weeds.Add(W);
		}
	}
	// құзғындар: күндіз, бір нүкте (әлдебір өлекше) айналасында қалықтап, анда-санда қанат қағады
	if (FVector::Dist2D(BirdC, Cam) > 40000.f || BirdC.IsZero()) { const float A = FMath::FRandRange(0.f, UE_TWO_PI); BirdC = Cam + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 14000.f; BirdC.Z = KAPVis::GroundZ(GetWorld(), BirdC.X, BirdC.Y, Cam.Z); }
	const bool bBirds = bDay && Storm < 0.35f;
	for (FBird& B : Birds)
	{
		B.Body->SetVisibility(bBirds); B.WL->SetVisibility(bBirds); B.WR->SetVisibility(bBirds);
		if (!bBirds) continue;
		B.Ph += B.Spd * Dt;
		const FVector P = BirdC + FVector(FMath::Cos(B.Ph) * B.Rad, FMath::Sin(B.Ph) * B.Rad, B.Alt + FMath::Sin(B.Ph * 3.f) * 300.f);
		const float Yaw = FMath::RadiansToDegrees(B.Ph) + (B.Spd > 0 ? 90.f : -90.f);
		const float Bank = B.Spd > 0 ? -18.f : 18.f;
		const float Flap = FMath::Fmod(T * 0.3f + B.Rad, 6.f) < 1.2f ? FMath::Sin(T * 9.f) * 28.f : 4.f;
		const FRotator R(0, Yaw, Bank);
		B.Body->SetWorldLocationAndRotation(P, R);
		B.WL->SetWorldLocationAndRotation(P + R.RotateVector(FVector(0, -55, 0)), R + FRotator(0, 0, Flap));
		B.WR->SetWorldLocationAndRotation(P + R.RotateVector(FVector(0, 55, 0)), R + FRotator(0, 0, -Flap));
	}
	// қаңбақтар: желмен домалайды, секіреді, алыстаса — ойыншының желге қарсы жағынан қайта шығады
	const FVector Wd = WX ? WX->Env.Wind.GetSafeNormal2D() : FVector(1, 0, 0);
	const float Spd = 120.f + Storm * 700.f;
	for (FWeed& W : Weeds)
	{
		W.P += (Wd + FVector(FMath::Sin(T * 0.5f + W.Bph) * 0.3f, FMath::Cos(T * 0.4f + W.Bph) * 0.3f, 0)) * Spd * Dt;
		if (FVector::Dist2D(W.P, Cam) > 5000.f) W.P = Cam - Wd * FMath::FRandRange(3000.f, 4500.f) + FVector(-Wd.Y, Wd.X, 0) * FMath::FRandRange(-2500.f, 2500.f);
		W.Roll += Spd * Dt / FMath::Max(10.f, W.S * 40.f) * 57.3f;
		W.Bph += Dt * (2.f + Storm * 4.f);
		const float G = KAPVis::GroundZ(GetWorld(), W.P.X, W.P.Y, Cam.Z - 300.f);
		const FVector P(W.P.X, W.P.Y, G + W.S * 35.f + FMath::Abs(FMath::Sin(W.Bph)) * (25.f + Storm * 80.f));
		W.C->SetWorldLocationAndRotation(P, FRotator(-W.Roll, FMath::RadiansToDegrees(FMath::Atan2(Wd.Y, Wd.X)), 0));
	}
}