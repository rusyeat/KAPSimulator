#include "KAPWeatherFX.h"
#include "WeatherManager.h"
#include "KAPVisuals.h"
#include "KAPFXManager.h"
#include "KAPNature.h"
#include "KAP/Player/KAPCharacter.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace { constexpr int32 NumDrops = 1600; constexpr float Box = 2600.f, BoxH = 1500.f; }

AKAPWeatherFX::AKAPWeatherFX()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

void AKAPWeatherFX::BeginPlay()
{
	Super::BeginPlay();
	Drops = NewObject<UInstancedStaticMeshComponent>(this);
	Drops->SetStaticMesh(KAPVis::Mesh(EKAPShape::CubeLow));
	Drops->SetupAttachment(RootComponent);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetAbsolute(true, true, true);
	DropMID = UMaterialInstanceDynamic::Create(KAPVis::TranslucentMaterial(), this);
	DropMID->SetVectorParameterValue("Color", FLinearColor(0.62f, 0.66f, 0.7f));
	DropMID->SetVectorParameterValue("Emissive", FLinearColor(0.02f, 0.022f, 0.025f));
	DropMID->SetScalarParameterValue("Opacity", 0.22f);
	Drops->SetMaterial(0, DropMID);
	Drops->RegisterComponent();
	DropPos.SetNum(NumDrops); DropXf.SetNum(NumDrops);
	for (int32 i = 0; i < NumDrops; i++)
	{
		DropPos[i] = FVector(FMath::FRandRange(-Box, Box), FMath::FRandRange(-Box, Box), FMath::FRandRange(-BoxH * 0.3f, BoxH));
		DropXf[i] = FTransform(FQuat::Identity, DropPos[i], FVector(0.f));
	}
	Drops->AddInstances(DropXf, false);
	Bolt = NewObject<UInstancedStaticMeshComponent>(this);
	Bolt->SetStaticMesh(KAPVis::Mesh(EKAPShape::CylLow));
	Bolt->SetupAttachment(RootComponent);
	Bolt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bolt->SetCastShadow(false);
	Bolt->SetAbsolute(true, true, true);
	Bolt->SetMaterial(0, KAPVis::MakeMID(Bolt, FKAPMat::Glow(FLinearColor(0.75f, 0.82f, 1.f), 900.f)));
	Bolt->RegisterComponent();
	Bolt->SetVisibility(false);
	Flash = NewObject<UPointLightComponent>(this);
	Flash->SetupAttachment(RootComponent);
	Flash->SetAbsolute(true, true, true);
	Flash->SetIntensityUnits(ELightUnits::Candelas);
	Flash->SetIntensity(0.f);
	Flash->SetAttenuationRadius(60000.f);
	Flash->SetLightColor(FLinearColor(0.8f, 0.86f, 1.f));
	Flash->SetCastShadows(false);
	Flash->RegisterComponent();
	if (const AWeatherManager* WX = AWeatherManager::Get(GetWorld())) LastSeq = WX->Env.StrikeSeq;
}

void AKAPWeatherFX::MakeBolt(const FVector& G)
{
	Bolt->ClearInstances();
	TArray<FTransform> Xf;
	// бас арна: бұлттан (≈1.3 км) жерге, иректеліп; 2–3 тармақ
	auto Chain = [&](FVector P, FVector End, int32 Steps, float Jit, float R)
	{
		for (int32 s = 0; s < Steps; s++)
		{
			const float T = (s + 1) / (float)Steps;
			FVector Q = FMath::Lerp(P, End, 1.f / (Steps - s));
			if (s + 1 < Steps) Q += FVector(FMath::FRandRange(-Jit, Jit), FMath::FRandRange(-Jit, Jit), FMath::FRandRange(-Jit, Jit) * 0.3f);
			const FVector D = Q - P;
			Xf.Add(FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).ToQuat(), (P + Q) * 0.5f, FVector(R * (1.f - T * 0.4f), R * (1.f - T * 0.4f), D.Size() / 100.f)));
			if (Steps > 6 && FMath::FRand() < 0.14f)
			{
				FVector Bp = Q, Bd = (D.GetSafeNormal() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), -0.2f)).GetSafeNormal();
				for (int32 k = 0; k < 5; k++) { const FVector Bq = Bp + (Bd + FVector(FMath::FRandRange(-0.5f, 0.5f), FMath::FRandRange(-0.5f, 0.5f), -0.3f)).GetSafeNormal() * FMath::FRandRange(3000.f, 7000.f); const FVector Dd = Bq - Bp; Xf.Add(FTransform(FRotationMatrix::MakeFromZ(Dd.GetSafeNormal()).ToQuat(), (Bp + Bq) * 0.5f, FVector(R * 0.35f, R * 0.35f, Dd.Size() / 100.f))); Bp = Bq; }
			}
			P = Q;
		}
	};
	const FVector Top = G + FVector(FMath::FRandRange(-20000.f, 20000.f), FMath::FRandRange(-20000.f, 20000.f), 130000.f);
	Chain(Top, G, 22, 5500.f, 4.f); // 4 м — алыстан да анық көрінетін жарқыл арнасы
	Bolt->AddInstances(Xf, false);
	Bolt->SetVisibility(true);
	Flash->SetWorldLocation(G + FVector(0, 0, 3000.f));
	BoltT = 0.f;
	BoltsShown++;
}

void AKAPWeatherFX::Tick(float Dt)
{
	Super::Tick(Dt);
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!WX || !PC || !PC->PlayerCameraManager) return;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
	const FKAPEnv& E = WX->Env;
	// ---- жаңбыр тамшылары: құлау 9 м/с + жел ----
	const float Rain = FMath::Clamp(E.Rain * (1.f - WX->Indoor * 0.9f), 0.f, 1.f);
	const int32 Active = FMath::RoundToInt32(NumDrops * Rain);
	const FVector V = FVector(0, 0, -900.f - 300.f * E.Thunder) + E.Wind * 0.85f;
	const FQuat Q = FRotationMatrix::MakeFromZ(-V.GetSafeNormal()).ToQuat();
	const float Len = FMath::Clamp(V.Size() * 0.045f, 30.f, 80.f) / 100.f;
	for (int32 i = 0; i < NumDrops; i++)
	{
		FVector& P = DropPos[i];
		P += V * Dt;
		FVector Rel = P - Cam;
		for (int32 a = 0; a < 2; a++) { if (Rel[a] < -Box) Rel[a] += 2 * Box; else if (Rel[a] > Box) Rel[a] -= 2 * Box; }
		if (Rel.Z < -BoxH * 0.3f) Rel.Z += BoxH * 1.3f; else if (Rel.Z > BoxH) Rel.Z -= BoxH * 1.3f;
		P = Cam + Rel;
		const float Near = FMath::Clamp((Rel.Size() - 60.f) / 150.f, 0.f, 1.f);
		const float S = i < Active ? Near : 0.f;
		DropXf[i].SetLocation(P);
		DropXf[i].SetRotation(Q);
		DropXf[i].SetScale3D(FVector(0.008f * S, 0.008f * S, Len * 1.3f * S));
	}
	Drops->BatchUpdateInstancesTransforms(0, DropXf, true, true, true);
	DropMID->SetScalarParameterValue("Opacity", 0.16f + 0.22f * Rain + E.Lightning * 0.3f);
	// ---- жердегі тамшы шашырауы ----
	if (Rain > 0.05f && WX->Indoor < 0.5f)
		if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld()))
		{
			SplashAcc += Dt * 40.f * Rain;
			while (SplashAcc > 1.f)
			{
				SplashAcc -= 1.f;
				const float A = FMath::FRandRange(0.f, UE_TWO_PI), R = FMath::Sqrt(FMath::FRand()) * 1200.f;
				const FVector P = Cam + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
				float Wz, Wd;
				const float G = AKAPNature::WaterAt(GetWorld(), P, Wz, Wd) ? Wz : KAPVis::GroundZ(GetWorld(), P.X, P.Y, Cam.Z - 170.f);
				if (FMath::Abs(G - Cam.Z) < 1500.f) FX->Spawn(EKAPFX::Rain, FVector(P.X, P.Y, G + 2.f), FMath::FRandRange(0.6f, 1.2f), FVector(0, 0, 0));
			}
		}
	// ---- найзағай ----
	if (E.StrikeSeq != LastSeq)
	{
		LastSeq = E.StrikeSeq;
		MakeBolt(E.StrikeAt);
		const float D = FVector::Dist2D(E.StrikeAt, Cam);
		if (D < 2500.f)
		{
			// жақын соққы: жер сілкінгендей жарқыл, түтін; сексеуіл сынады; ойыншы жақын болса — жарақат
			if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) { FX->Spawn(EKAPFX::Flash, E.StrikeAt + FVector(0, 0, 50), 6.f); FX->Spawn(EKAPFX::Smoke, E.StrikeAt + FVector(0, 0, 60), 1.5f); }
		}
		if (AKAPNature* N = AKAPNature::Get(GetWorld())) N->VehicleSweep(E.StrikeAt, FVector(0, 0, -600.f) + FVector(600.f, 0, 0), 900.f);
		if (AKAPCharacter* P = AKAPCharacter::Get(this))
		{
			const float Dp = FVector::Dist2D(P->GetActorLocation(), E.StrikeAt);
			if (Dp < 1500.f && WX->Indoor < 0.5f) P->DamagePlayer(FMath::GetMappedRangeValueClamped(FVector2f(200.f, 1500.f), FVector2f(45.f, 5.f), Dp));
		}
	}
	if (BoltT >= 0.f)
	{
		BoltT += Dt;
		// жыпылықтау: 3 импульс, 0.4 с
		const bool bOn = BoltT < 0.07f || (BoltT > 0.13f && BoltT < 0.19f) || (BoltT > 0.27f && BoltT < 0.33f);
		Bolt->SetVisibility(bOn);
		const float D = FVector::Dist(Flash->GetComponentLocation(), Cam);
		Flash->SetIntensity(bOn ? FMath::Clamp(9.0e7f / FMath::Max(1.f, D / 100.f), 2.0e5f, 4.0e6f) : 0.f);
		if (BoltT > 0.4f) { BoltT = -1.f; Bolt->SetVisibility(false); Flash->SetIntensity(0.f); }
	}
}
