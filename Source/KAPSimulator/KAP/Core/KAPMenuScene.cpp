// Басты мәзір сахнасы: түн, жеңіл құм дауылы, №1 ұңғыма. Алдыңғы планда — маңдай шамы бар инженер (респиратор),
// артта — қызыл апат шамы мен натрий прожекторы (көлемді тұманда сәуле бағаналары). Камера қолмен ұстағандай тербеледі.
#include "KAPGameMode.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPDataSubsystem.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/KAPVisuals.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	AActor* MenuProp(UWorld* W, FVector Pos, float Yaw)
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), Pos, FRotator(0, Yaw, 0), SP);
		if (!A) return nullptr;
		USceneComponent* R = NewObject<USceneComponent>(A, TEXT("Root"));
		A->SetRootComponent(R); R->RegisterComponent();
		A->SetActorLocationAndRotation(Pos, FRotator(0, Yaw, 0));
		return A;
	}
}

void AKAPGameMode::SetupMenuScene()
{
	ClearMenuScene();
	UWorld* W = GetWorld();
	UWellManagerSubsystem* WM = W ? W->GetSubsystem<UWellManagerSubsystem>() : nullptr;
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!WM || !WM->Wells.Num() || !D) return;
	const FVector2D Wp = WM->Wells[0].PosM();
	FVector2D Toward0(-1, 0);
	if (const FKAPLocationRow* L = D->Loc("wagon0")) Toward0 = (FVector2D(L->X, L->Y) - Wp).GetSafeNormal();
	auto G = [&](FVector2D P, float Up) { return FVector(P.X * 100, P.Y * 100, KAPVis::GroundZ(W, P.X * 100, P.Y * 100, 0) + Up); };
	// ұңғымаға ашық көрініс беретін бағытты іздеу (резервуар, қоршау, төбе алдын жаппасын)
	FVector2D Toward = Toward0;
	{
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPMenuCam), false);
		float Best = -1.f;
		for (int32 k = 0; k < 16; k++)
		{
			const float A = k * UE_TWO_PI / 16.f;
			const FVector2D Dk(Toward0.X * FMath::Cos(A) - Toward0.Y * FMath::Sin(A), Toward0.X * FMath::Sin(A) + Toward0.Y * FMath::Cos(A));
			const FVector From = G(Wp + Dk * 12.f, 150.f);
			float Clear = 1e9f;
			for (float Side : { -1.2f, 0.f, 1.2f })
			{
				const FVector2D Off(-Dk.Y * Side, Dk.X * Side);
				FHitResult H;
				if (W->LineTraceSingleByChannel(H, From, G(Wp + Dk * 1.8f + Off, 150.f), ECC_Visibility, Q)) Clear = FMath::Min(Clear, (float)H.Distance);
			}
			const float Score = FMath::Min(Clear, 1100.f) - k * 1.f; // тең болса — вагон жағына жақыны
			if (Score > Best) { Best = Score; Toward = Dk; }
		}
	}
	const FVector2D Fw2 = -Toward, Rt2(-Fw2.Y, Fw2.X);
	const FVector2D CamP = Wp + Toward * 12.f;
	const FVector2D NpcP = CamP + Fw2 * 3.6f + Rt2 * 0.65f;
	MenuCamBase = G(CamP, 158.f);
	MenuLook = G(CamP + Fw2 * 9.f - Rt2 * 0.6f, 140.f);
	// ---- инженер: тоқтап тұрады (тек тыныс анимациясы), камераға 3/4 бұрылған ----
	if (AKAPNPC* N = SpawnNPC((int32)ENPCKind::Worker, NpcP, TEXT("Инженер"), false))
	{
		N->SetActorTickEnabled(false);
		N->GetCharacterMovement()->DisableMovement();
		N->SetActorRotation(FRotator(0, (MenuCamBase - N->GetActorLocation()).Rotation().Yaw + 32.f, 0));
		// маңдай шамы: шаңда көрінетін сәуле, қолдан ұстағандай аздап дірілдейді
		USpotLightComponent* L = NewObject<USpotLightComponent>(N);
		L->SetupAttachment(N->GetRootComponent());
		L->SetRelativeLocationAndRotation(FVector(20, 0, 70), FRotator(-9, 0, 0));
		L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(1500.f);
		L->SetAttenuationRadius(3000.f); L->SetOuterConeAngle(24.f); L->SetInnerConeAngle(9.f);
		L->SetLightColor(FLinearColor(1.f, 0.94f, 0.82f)); L->SetVolumetricScatteringIntensity(3.f);
		L->SetCastShadows(true); L->bCastVolumetricShadow = true;
		L->RegisterComponent();
		MenuHeadlamp = L;
		KAPVis::Add(N, N->GetRootComponent(), EKAPShape::Sphere, FVector(17, 0, 70), FVector(5, 5, 5), FKAPMat::Glow(FLinearColor(1.f, 0.95f, 0.85f), 80), FRotator::ZeroRotator, false);
		MenuActors.Add(N);
	}
	// ---- апат шамы: ұңғыма жанындағы бағанада қызыл маяк ----
	if (AActor* B = MenuProp(W, G(Wp + Rt2 * 1.2f - Toward * 1.5f, 0.f), 0.f))
	{
		const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.25f, 0.25f, 0.24f), 0.5f);
		KAPVis::Add(B, B->GetRootComponent(), EKAPShape::Cylinder, FVector(0, 0, 180), FVector(10, 10, 360), Steel, FRotator::ZeroRotator, false);
		KAPVis::Add(B, B->GetRootComponent(), EKAPShape::Sphere, FVector(0, 0, 372), FVector(22, 22, 26), FKAPMat::Glow(FLinearColor(1.f, 0.06f, 0.03f), 40), FRotator::ZeroRotator, false);
		UPointLightComponent* P = NewObject<UPointLightComponent>(B);
		P->SetupAttachment(B->GetRootComponent()); P->SetRelativeLocation(FVector(0, 0, 372));
		P->SetIntensityUnits(ELightUnits::Candelas); P->SetIntensity(1000.f); P->SetAttenuationRadius(2200.f);
		P->SetLightColor(FLinearColor(1.f, 0.08f, 0.04f)); P->SetVolumetricScatteringIntensity(4.f); P->SetCastShadows(false);
		P->RegisterComponent();
		MenuBeacon = P;
		MenuActors.Add(B);
	}
	// ---- натрий прожекторы: артта, биік бағанда, сахнаға қиғаш түседі (көлеңкелі сәуле бағаналары) ----
	if (AActor* F = MenuProp(W, G(Wp - Rt2 * 5.5f - Toward * 3.f, 0.f), 0.f))
	{
		const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.2f, 0.2f, 0.2f), 0.55f);
		KAPVis::Add(F, F->GetRootComponent(), EKAPShape::Cylinder, FVector(0, 0, 400), FVector(14, 14, 800), Steel, FRotator::ZeroRotator, false);
		KAPVis::Add(F, F->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 805), FVector(50, 36, 22), Steel, FRotator::ZeroRotator, false);
		KAPVis::Add(F, F->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 793), FVector(44, 30, 4), FKAPMat::Glow(FLinearColor(1.f, 0.72f, 0.4f), 30), FRotator::ZeroRotator, false);
		USpotLightComponent* S = NewObject<USpotLightComponent>(F);
		S->SetupAttachment(F->GetRootComponent()); S->SetRelativeLocation(FVector(0, 0, 790));
		const FVector Aim = G(NpcP + Fw2 * 2.f - Rt2 * 1.f, 0.f) - (F->GetActorLocation() + FVector(0, 0, 790));
		S->SetWorldRotation(Aim.Rotation());
		S->SetIntensityUnits(ELightUnits::Candelas); S->SetIntensity(5200.f); S->SetAttenuationRadius(4500.f);
		S->SetOuterConeAngle(36.f); S->SetInnerConeAngle(18.f);
		S->SetLightColor(FLinearColor(1.f, 0.7f, 0.38f)); S->SetVolumetricScatteringIntensity(2.2f);
		S->SetCastShadows(true); S->bCastVolumetricShadow = true;
		S->RegisterComponent();
		MenuActors.Add(F);
	}
	// ---- алдыңғы план: сол жақта бөшкелер (қараңғы силуэт — тереңдік) ----
	if (AActor* Bz = MenuProp(W, G(CamP + Fw2 * 2.6f - Rt2 * 2.1f, 0.f), 20.f))
	{
		FKAPMat Drum = FKAPMat::Paint(FLinearColor(0.12f, 0.16f, 0.1f), 0.6f); Drum.Rust = 0.45f; Drum.Grime = 0.8f;
		FKAPMat Drum2 = FKAPMat::Paint(FLinearColor(0.38f, 0.06f, 0.04f), 0.6f); Drum2.Rust = 0.35f; Drum2.Grime = 0.8f;
		KAPVis::Add(Bz, Bz->GetRootComponent(), EKAPShape::Cylinder, FVector(0, 0, 45), FVector(58, 58, 90), Drum, FRotator::ZeroRotator, true);
		KAPVis::Add(Bz, Bz->GetRootComponent(), EKAPShape::Cylinder, FVector(-40, 55, 45), FVector(58, 58, 90), Drum2, FRotator::ZeroRotator, true);
		KAPVis::Add(Bz, Bz->GetRootComponent(), EKAPShape::Cylinder, FVector(30, 70, 29), FVector(58, 58, 90), Drum, FRotator(0, 0, 90), true);
		MenuActors.Add(Bz);
	}
	bMenuScene = true;
}

void AKAPGameMode::ClearMenuScene()
{
	for (TWeakObjectPtr<AActor>& A : MenuActors) if (A.IsValid()) A->Destroy();
	MenuActors.Reset();
	MenuBeacon = nullptr; MenuHeadlamp = nullptr;
	bMenuScene = false;
}
