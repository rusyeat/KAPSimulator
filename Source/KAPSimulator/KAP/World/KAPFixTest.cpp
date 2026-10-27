// -kapfixshots: ойыншы шағымдарынан кейінгі түзетулерді тексеру суреттері (Saved/Screenshots/.../KAPF_*.png) және KAPFIX журналы:
//  • кіріспе: мотор қызып жұлқынуы, бу, есік ашылуы, кейіпкердің секіріп түсуі мен жүруі (аяқ бұрышы журналға)
//  • NPC жүрісі (жұмысшы, ауыл әйелі) — бүйірден, аяқтардың ашылуы
//  • ұңғыма жанындағы тасталған костюм, жерде жатқан жәшіктер, көліктен түсу (есік), таулар
#include "KAPNature.h"
#include "KAPTerrain.h"
#include "KAPVisuals.h"
#include "KAPWorldBuilder.h"
#include "KAPVehicleHandle.h"
#include "KAPStructure.h"
#include "WellActor.h"
#include "WellManagerSubsystem.h"
#include "WeatherManager.h"
#include "KAPSimulatorPawn.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/NPC/KAPAnimal.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/BodySetup.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UnrealClient.h"
#include "TimerManager.h"

namespace
{
	/** Екі санның (сан → тізе) арасындағы бұрыш (°) — жүргенде 25–45°, тұрғанда < 8° */
	float LegSpreadDeg(AKAPNPC* N)
	{
		USkeletalMeshComponent* M = N ? N->GetMesh() : nullptr;
		if (!M) return -1.f;
		const FVector L = (M->GetBoneLocation("calf_l", EBoneSpaces::ComponentSpace) - M->GetBoneLocation("thigh_l", EBoneSpaces::ComponentSpace)).GetSafeNormal();
		const FVector R = (M->GetBoneLocation("calf_r", EBoneSpaces::ComponentSpace) - M->GetBoneLocation("thigh_r", EBoneSpaces::ComponentSpace)).GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(L, R), -1.f, 1.f)));
	}
	AKAPNPC* FindHero(UWorld* W) { for (TActorIterator<AKAPNPC> It(W); It; ++It) if (It->DisplayName == TEXT("Сіз") && !It->bDead) return *It; return nullptr; }
}

void AKAPNature::RunFixShots()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return;
	auto Shot = [this](const FString& N)
	{
		FScreenshotRequest::RequestScreenshot(TEXT("KAPF_") + N + TEXT(".png"), true, false);
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector C = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		UE_LOG(LogTemp, Display, TEXT("KAPFIX shot %s cam=(%.0f %.0f %.0f)"), *N, C.X, C.Y, C.Z);
	};
	auto Go = [this, GM](FVector Cm, FVector LookCm)
	{
		AKAPCharacter* P = GM->Player(); APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!P || !PC) return;
		P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		P->GetCharacterMovement()->StopMovementImmediately();
		P->SetActorLocation(Cm, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation((LookCm - Cm).Rotation());
	};
	auto Ground = [this](float X, float Y) { return KAPVis::GroundZ(GetWorld(), X, Y, 0.f); };
	auto Clear = [GM](double Hour)
	{
		GM->Weather->bOverride = true; GM->Weather->OverrideType = EWeatherType::Clear; GM->Weather->OverrideK = 1.f; GM->Weather->bInstant = true; GM->Weather->VisOverride = -1.f;
		GM->S().Time = FMath::FloorToDouble(GM->S().Time / 24) * 24 + 24.0 + Hour;
	};
	FTimerHandle H0; GetWorldTimerManager().SetTimer(H0, FTimerDelegate::CreateWeakLambda(this, [GM]() { if (GM->bInMenu) GM->MenuNewGame(); }), 3.f, false);
	// ---- кіріспе: кинематика уақыты бойынша суреттер мен кейіпкер журналы ----
	struct FIntro { TArray<float> Times; int32 Next = 0; float LastLog = -1.f; float MinSpread = 999.f, MaxSpread = -1.f; bool bDone = false; float WasT = -1.f; };
	TSharedRef<FIntro> In = MakeShared<FIntro>();
	In->Times = { 31.5f, 34.6f, 35.5f, 35.9f, 36.2f, 36.6f, 37.3f, 38.2f, 39.3f, 40.8f, 42.6f, 44.0f, 45.5f, 48.0f };
	FTimerHandle HI; GetWorldTimerManager().SetTimer(HI, FTimerDelegate::CreateWeakLambda(this, [this, GM, In, Shot]()
	{
		const float T = GM->CineTime();
		if (In->bDone) return;
		if (T < 0.f) { if (In->WasT > 40.f) { In->bDone = true; UE_LOG(LogTemp, Display, TEXT("KAPFIX intro done: hero leg spread min=%.1f max=%.1f"), In->MinSpread, In->MaxSpread); } return; }
		In->WasT = T;
		if (In->Next < In->Times.Num() && T >= In->Times[In->Next]) { Shot(FString::Printf(TEXT("intro_%02d_%04.1f"), In->Next, In->Times[In->Next])); In->Next++; }
		if (AKAPNPC* H = FindHero(GetWorld())) if (!H->IsHidden() && T > 35.f && T < 46.f)
		{
			const float Sp = LegSpreadDeg(H);
			if (H->GetVelocity().Size2D() > 60.f && H->GetCharacterMovement()->MovementMode == MOVE_Walking) { In->MinSpread = FMath::Min(In->MinSpread, Sp); In->MaxSpread = FMath::Max(In->MaxSpread, Sp); }
			if (T - In->LastLog > 0.25f)
			{
				In->LastLog = T;
				const FVector L = H->GetActorLocation();
				const float G = KAPVis::GroundZ(GetWorld(), L.X, L.Y, L.Z);
				FVector Ls = FVector::ZeroVector; if (GM->Builder) for (auto& X : GM->Builder->Vehicles) if (X.IsValid() && X->Id == "bukhanka") Ls = X->WorldToShell(L);
				UE_LOG(LogTemp, Display, TEXT("KAPFIX hero t=%.2f shell=(%.0f %.0f) mode=%d speed=%.0f feet-ground=%.1f cm legs=%.1f deg lod=%d"), T, Ls.X, Ls.Y, (int32)H->GetCharacterMovement()->MovementMode, H->GetVelocity().Size2D(),
					L.Z - H->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - G, Sp, H->GetMesh()->IsComponentTickEnabled() ? 1 : 0);
			}
		}
	}), 0.03f, true);
	// ---- кіріспеден кейін: көлік, есік, NPC жүрісі, костюм, жәшік, таулар ----
	TSharedRef<TArray<TFunction<void()>>> Steps = MakeShared<TArray<TFunction<void()>>>();
	auto FindVan = [GM]() -> AKAPVehicleHandle* { if (GM->Builder) for (auto& X : GM->Builder->Vehicles) if (X.IsValid() && X->Id == "bukhanka") return X.Get(); return nullptr; };
	TSharedRef<TWeakObjectPtr<AKAPNPC>> Walker = MakeShared<TWeakObjectPtr<AKAPNPC>>();
	TSharedRef<float> SpMin = MakeShared<float>(999.f), SpMax = MakeShared<float>(-1.f);
	auto WalkNPC = [this, Go, Walker, SpMin, SpMax](bool bFemale)
	{
		*SpMin = 999.f; *SpMax = -1.f; *Walker = nullptr;
		AKAPCharacter* P = AKAPCharacter::Get(this);
		AKAPNPC* Best = nullptr; double Bd = 1e18;
		for (TActorIterator<AKAPNPC> It(GetWorld()); It; ++It)
		{
			if (It->bDead || It->bKnockedOut || !It->IsHuman() || It->IsHidden() || It->IsChild()) continue;
			const bool bOk = bFemale ? (It->IsFemale() && It->Kind == ENPCKind::Villager) : It->Kind == ENPCKind::Worker;
			if (!bOk) continue;
			const double D = P ? FVector::DistSquared(It->GetActorLocation(), P->GetActorLocation()) : 0.0;
			if (D < Bd) { Bd = D; Best = *It; }
		}
		if (!Best) { UE_LOG(LogTemp, Warning, TEXT("KAPFIX walker %s not found"), bFemale ? TEXT("female") : TEXT("worker")); return; }
		*Walker = Best;
		const FVector2D Pm = FVector2D(Best->GetActorLocation()) / 100.0;
		const FVector2D Dir(1, 0);
		Best->GoTo = Pm + Dir * 14.0;
		const FVector Mid((Pm.X + 3.5) * 100.0, (Pm.Y + 5.0) * 100.0, 0.0);
		Go(FVector(Mid.X, Mid.Y, KAPVis::GroundZ(GetWorld(), Mid.X, Mid.Y, 0.f) + 150.f), FVector((Pm.X + 3.5) * 100.0, Pm.Y * 100.0, Best->GetActorLocation().Z));
		UE_LOG(LogTemp, Display, TEXT("KAPFIX walker %s: %s mesh=%s"), bFemale ? TEXT("female") : TEXT("worker"), *Best->DisplayName, *GetNameSafe(Best->GetMesh()->GetSkeletalMeshAsset()));
	};
	FTimerHandle HW; GetWorldTimerManager().SetTimer(HW, FTimerDelegate::CreateWeakLambda(this, [Walker, SpMin, SpMax]()
	{
		if (AKAPNPC* N = Walker->Get()) if (N->GetVelocity().Size2D() > 60.f) { const float S = LegSpreadDeg(N); *SpMin = FMath::Min(*SpMin, S); *SpMax = FMath::Max(*SpMax, S); }
	}), 0.03f, true);
	auto LogWalk = [Walker, SpMin, SpMax](const TCHAR* Tag) { AKAPNPC* N = Walker->Get(); UE_LOG(LogTemp, Display, TEXT("KAPFIX walk %s: speed=%.0f legs min=%.1f max=%.1f deg"), Tag, N ? N->GetVelocity().Size2D() : -1.f, *SpMin, *SpMax); };
	// ұңғыма жанындағы тасталған костюм (ұңғыма индексі бойынша бірдей кездейсоқ сан — WellActor-дағыдай)
	auto SuitGo = [this, Go, Ground]()
	{
		UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>(); if (!WM) return;
		for (int32 i = 0; i < WM->Wells.Num(); i++)
		{
			FRandomStream Rg(i * 131 + 7);
			if (Rg.FRand() >= 0.6f || !WM->Wells[i].Actor.IsValid()) continue;
			const FVector C(Rg.FRandRange(-650.f, -560.f), Rg.FRandRange(-300.f, 300.f), 0.f);
			const FTransform Xf = WM->Wells[i].Actor->GetActorTransform();
			const FVector Pw = Xf.TransformPosition(C);
			const FVector Cam = Xf.TransformPosition(C + FVector(-190.f, 120.f, 0.f));
			Go(FVector(Cam.X, Cam.Y, Ground(Cam.X, Cam.Y) + 165.f), FVector(Pw.X, Pw.Y, Ground(Pw.X, Pw.Y)));
			UE_LOG(LogTemp, Display, TEXT("KAPFIX suit pile at well %d (%.0f %.0f)"), i + 1, Pw.X, Pw.Y);
			break;
		}
	};
	struct FStep { float T; TFunction<void()> F; };
	TArray<FStep> St = {
		// көлік кіріспеден кейін: бу, апат шамдары, антифриз
		{ 0.5f, [this, GM, Clear]() { Clear(12.5); GM->Weather->bOverride = true; } },
		{ 1.0f, [this, Go, FindVan]() { if (AKAPVehicleHandle* V = FindVan()) Go(V->ShellToWorld(FVector(520.f, -330.f, 170.f)), V->ShellToWorld(FVector(180.f, 0.f, 60.f))); } },
		{ 3.5f, [Shot]() { Shot(TEXT("van_after_intro")); } },
		// көлікке отыру/түсу: есік
		{ 4.0f, [this, GM, FindVan]() { if (AKAPVehicleHandle* V = FindVan()) { V->State = "ok"; V->Fuel = 30; V->SteamK = 0.f; V->SteamFade = 0.f; GM->EnterVehicle(V); } } },
		{ 6.0f, [GM]() { GM->ExitVehicle(); } },
		{ 6.35f, [Shot]() { Shot(TEXT("exit_door_open")); } },
		{ 6.6f, [this, Go, FindVan]() { if (AKAPVehicleHandle* V = FindVan()) { V->SwingDoor(0, 2.5f); Go(V->ShellToWorld(FVector(-60.f, -430.f, 160.f)), V->ShellToWorld(FVector(160.f, -110.f, 110.f))); } } },
		{ 7.4f, [Shot]() { Shot(TEXT("door_open_side")); } },
		// NPC жүрісі: жұмысшы (V5 Worker), ауыл әйелі (V5 FemaleCasual)
		{ 8.0f, [WalkNPC]() { WalkNPC(false); } },
		{ 9.3f, [Shot]() { Shot(TEXT("walk_worker_a")); } },
		{ 9.75f, [Shot]() { Shot(TEXT("walk_worker_b")); } },
		{ 11.0f, [LogWalk]() { LogWalk(TEXT("worker")); } },
		{ 11.2f, [WalkNPC]() { WalkNPC(true); } },
		{ 12.5f, [Shot]() { Shot(TEXT("walk_female_a")); } },
		{ 12.95f, [Shot]() { Shot(TEXT("walk_female_b")); } },
		{ 14.2f, [LogWalk]() { LogWalk(TEXT("female")); } },
		// ұңғыма жанындағы тасталған костюм (ұңғыма индексі бойынша бірдей кездейсоқ сан — WellActor-дағыдай)
		{ 14.5f, SuitGo },
		{ 16.2f, SuitGo },
		{ 16.5f, [Shot]() { Shot(TEXT("suit_pile")); } },
		// жәшік: ұңғыма маңындағы және ауылдағы (жерде жатуы керек)
		{ 17.0f, [this, Go, Ground]() {
			int32 N = 0;
			for (TActorIterator<AKAPInteractSpot> It(GetWorld()); It; ++It)
				if (It->Action.ToString().StartsWith(TEXT("crate:well")))
				{
					const FVector P = It->GetActorLocation();
					const float G = Ground(P.X, P.Y);
					UE_LOG(LogTemp, Display, TEXT("KAPFIX crate %s z=%.1f ground=%.1f (above %.1f cm)"), *It->Action.ToString(), P.Z, G, P.Z - G);
					if (N++ == 0) Go(FVector(P.X - 170.f, P.Y - 120.f, G + 150.f), P + FVector(0, 0, 15));
				}
			} },
		{ 19.0f, [Shot]() { Shot(TEXT("crate_well")); } },
		{ 19.5f, [this, Go, Ground]() {
			for (TActorIterator<AKAPInteractSpot> It(GetWorld()); It; ++It)
				if (It->Action.ToString().StartsWith(TEXT("crate:")) && !It->Action.ToString().StartsWith(TEXT("crate:well")) && !It->Action.ToString().StartsWith(TEXT("crate:wagon")))
				{
					const FVector P = It->GetActorLocation();
					UE_LOG(LogTemp, Display, TEXT("KAPFIX crate %s z=%.1f ground=%.1f"), *It->Action.ToString(), P.Z, Ground(P.X, P.Y));
					Go(FVector(P.X - 160.f, P.Y + 130.f, P.Z + 150.f), P + FVector(0, 0, 15));
					break;
				}
			} },
		{ 21.5f, [Shot]() { Shot(TEXT("crate_village")); } },
		// таулар: даладан, бөктерден, шатқалдан, әуеден
		{ 22.0f, [Go, Ground, Clear]() { Clear(11.0); Go(FVector(70000, 70000, Ground(70000, 70000) + 180.f), FVector(200000, 140000, 35000)); } },
		{ 26.0f, [Shot]() { Shot(TEXT("mtn_ne_plain")); } },
		{ 26.5f, [Go, Ground]() { Go(FVector(-30000, -70000, Ground(-30000, -70000) + 180.f), FVector(-30000, -200000, 30000)); } },
		{ 30.0f, [Shot]() { Shot(TEXT("mtn_south_plain")); } },
		{ 30.5f, [Go, Ground]() { Go(FVector(100000, 30000, Ground(100000, 30000) + 180.f), FVector(160000, 30000, 26000)); } },
		{ 34.0f, [Shot]() { Shot(TEXT("mtn_foothill_close")); } },
		{ 34.5f, [Go, Ground]() { Go(FVector(-60000, 60000, Ground(-60000, 60000) + 180.f), FVector(-200000, 200000, 60000)); } },
		{ 38.0f, [Shot]() { Shot(TEXT("mtn_nw_plain")); } },
		{ 38.5f, [Go]() { Go(FVector(60000, 20000, 250000), FVector(250000, 60000, 120000)); } },
		{ 42.5f, [Shot]() { Shot(TEXT("mtn_aerial_e")); } },
		{ 43.0f, [Go, Ground]() { Go(FVector(112000, -80000, Ground(112000, -80000) + 1500.f), FVector(150000, -100000, 45000)); } },
		{ 47.0f, [Shot]() { Shot(TEXT("mtn_forest_close")); } },
		{ 48.0f, []() { FGenericPlatformMisc::RequestExit(false); } },
	};
	// қадамдар кіріспе біткен соң басталады
	TSharedRef<bool> bStarted = MakeShared<bool>(false);
	TSharedRef<TArray<FStep>> Sh = MakeShared<TArray<FStep>>(MoveTemp(St));
	FTimerHandle HS; GetWorldTimerManager().SetTimer(HS, FTimerDelegate::CreateWeakLambda(this, [this, GM, In, bStarted, Sh]()
	{
		if (*bStarted || !(In->bDone || (GM->CineTime() < 0.f && GetWorld()->GetTimeSeconds() > 110.f))) return;
		*bStarted = true;
		if (AKAPCharacter* P = GM->Player()) if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { if (PC->GetPawn() != P) PC->Possess(P); PC->SetViewTarget(P); }
		for (const FStep& S : *Sh) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [F = S.F]() { F(); }), S.T, false); }
	}), 0.2f, true);
}

// -kapanimaltest: әр жануар түрін камера алдында өлтіріп, рагдоллдың жерге дұрыс түскенін тексеру (KAPA_*.png, KAPANIM журналы):
// физика денелерінің ең төменгі нүктесі мен жердің арасы, мештің көрінетін шекарасы, дене/буын саны
void AKAPNature::RunAnimalTest()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return;
	auto Shot = [](const FString& N) { FScreenshotRequest::RequestScreenshot(TEXT("KAPA_") + N + TEXT(".png"), true, false); };
	auto Ground = [this](float X, float Y) { return KAPVis::GroundZ(GetWorld(), X, Y, 0.f); };
	auto Go = [this, GM](FVector Cm, FVector LookCm)
	{
		AKAPCharacter* P = GM->Player(); APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!P || !PC) return;
		P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		P->GetCharacterMovement()->StopMovementImmediately();
		P->SetActorLocation(Cm, false, nullptr, ETeleportType::TeleportPhysics);
		PC->SetControlRotation((LookCm - Cm).Rotation());
	};
	// рагдолл есебі: әр дененің төменгі нүктесі жерден қанша жоғары, мештің шекарасы
	auto Report = [this, Ground](AKAPAnimal* A, const TCHAR* Tag)
	{
		if (!A || !A->Mesh) return;
		USkeletalMeshComponent* M = A->Mesh;
		float MinGap = 1e9f, MaxGap = -1e9f, Mis = 0.f; int32 Sim = 0, Awake = 0;
		for (FBodyInstance* B : M->Bodies)
		{
			if (!B || !B->IsValidBodyInstance()) continue;
			// дене мен сүйектің арасы (меш денеден ығыспауы керек)
			if (B->BodySetup.IsValid()) Mis = FMath::Max(Mis, (float)FVector::Dist(B->GetUnrealWorldTransform().GetLocation(), M->GetBoneLocation(B->BodySetup->BoneName)));
			const FBox Bx = B->GetBodyBounds();
			const FVector C = Bx.GetCenter();
			const float Gap = Bx.Min.Z - Ground(C.X, C.Y);
			MinGap = FMath::Min(MinGap, Gap); MaxGap = FMath::Max(MaxGap, Gap);
			Sim += B->IsInstanceSimulatingPhysics() ? 1 : 0; Awake += B->IsInstanceAwake() ? 1 : 0;
		}
		const FBoxSphereBounds Mb = M->CalcBounds(M->GetComponentTransform());
		auto BoneGap = [M, Ground](FName B) { const FVector P = M->GetBoneLocation(B); return P.Z - Ground(P.X, P.Y); };
		const FVector Pv = M->GetBoneLocation("pelvis");
		UE_LOG(LogTemp, Display, TEXT("KAPANIM %s %s: bodies=%d sim=%d awake=%d bodyGap min=%.1f max=%.1f cm body-bone=%.1f | pelvis-ground=%.1f head-ground=%.1f moved=%.0f cm extent=(%.0f %.0f %.0f) constraints=%d"),
			*UEnum::GetValueAsString(A->Kind), Tag, M->Bodies.Num(), Sim, Awake, MinGap, MaxGap, Mis, BoneGap("pelvis"), BoneGap("head"), FVector::Dist2D(Pv, A->GetActorLocation()),
			Mb.BoxExtent.X, Mb.BoxExtent.Y, Mb.BoxExtent.Z, M->Constraints.Num());
	};
	TSharedRef<TWeakObjectPtr<AKAPAnimal>> Cur = MakeShared<TWeakObjectPtr<AKAPAnimal>>();
	struct FStep { float T; TFunction<void()> F; };
	TArray<FStep> St = {
		{ 3.f, [GM]() { if (GM->bInMenu) GM->MenuNewGame(); } },
		{ 6.f, [GM]() { if (GM->bInMenu) GM->MenuNewGame(); } },
		{ 10.f, [GM]() { if (GM->bCinematic) GM->SkipCinematic(); } },
		{ 12.f, [this, GM]() {
			if (GM->bCinematic) GM->SkipCinematic();
			GM->Weather->bOverride = true; GM->Weather->OverrideType = EWeatherType::Clear; GM->Weather->bInstant = true;
			GM->S().Time = FMath::FloorToDouble(GM->S().Time / 24) * 24 + 24.0 + 11.0;
			if (AKAPCharacter* P = GM->Player()) if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { if (PC->GetPawn() != P) PC->Possess(P); PC->SetViewTarget(P); }
		} },
	};
	// соңғысы — көлік соққандай күшті итеру (≈9 м/с): рагдолл «жарылмай», жерге түсуі керек
	const EAnimalKind Kinds[6] = { EAnimalKind::Fox, EAnimalKind::Sheep, EAnimalKind::Goat, EAnimalKind::Cow, EAnimalKind::Horse, EAnimalKind::Sheep };
	for (int32 k = 0; k < 6; k++)
	{
		const float T0 = 14.f + k * 14.f;
		const EAnimalKind Kd = Kinds[k];
		const bool bCar = k == 5;
		const FString Nm = UEnum::GetValueAsString(Kd).RightChop(13) + (bCar ? TEXT("_car") : TEXT(""));
		// жақын тірі жануарды тауып, бүйірінен 5 м жерге тұру
		St.Add({ T0, [this, Kd, Cur, Go, Ground]() {
			*Cur = nullptr;
			AKAPFauna* F = AKAPFauna::Get(GetWorld()); if (!F) return;
			for (const auto& A : F->Animals) if (A.IsValid() && A->Kind == Kd && !A->IsDead()) { *Cur = A; break; }
			if (AKAPAnimal* A = Cur->Get())
			{
				const FVector P = A->GetActorLocation(), Rt = A->GetActorRightVector();
				const FVector Cam = P + Rt * 480.f;
				Go(FVector(Cam.X, Cam.Y, Ground(Cam.X, Cam.Y) + 165.f), P + FVector(0, 0, 40));
			}
			else UE_LOG(LogTemp, Warning, TEXT("KAPANIM no alive %s"), *UEnum::GetValueAsString(Kd));
		} });
		St.Add({ T0 + 1.5f, [Shot, Nm]() { Shot(Nm + TEXT("_alive")); } });
		// ойыншы пышақпен ұрғандай: ApplyDamage → TakeDamage → Die
		St.Add({ T0 + 2.f, [this, GM, Cur, Report, bCar]() {
			AKAPAnimal* A = Cur->Get(); if (!A) return;
			if (bCar) A->Die(A->GetActorForwardVector() * 900.f + FVector(0, 0, 300.f));   // KAPAnimal::Tick көлік соққысы: Vv·0.6 + 300
			else UGameplayStatics::ApplyDamage(A, 9999.f, nullptr, GM->Player(), nullptr);
			Report(A, TEXT("t+0"));
		} });
		St.Add({ T0 + 4.f, [Cur, Report]() { Report(Cur->Get(), TEXT("t+2")); } });
		St.Add({ T0 + 9.f, [Cur, Report, Shot, Nm, Go, Ground]() {
			AKAPAnimal* A = Cur->Get(); if (!A) return;
			Report(A, TEXT("t+7"));
			// мешке қарай қайта бағыттау (рагдолл орнынан ауысқан)
			const FVector P = A->Mesh->Bounds.Origin;
			Go(FVector(P.X + 330.f, P.Y + 260.f, Ground(P.X + 330.f, P.Y + 260.f) + 165.f), P);
		} });
		St.Add({ T0 + 10.5f, [Shot, Nm]() { Shot(Nm + TEXT("_dead")); } });
		// төмен, жақын бұрыш: жерге жатқаны көрінсін
		St.Add({ T0 + 11.f, [Cur, Go, Ground]() {
			AKAPAnimal* A = Cur->Get(); if (!A) return;
			const FVector P = A->Mesh->Bounds.Origin;
			const float Dd = FMath::Max(170.f, A->Mesh->Bounds.SphereRadius * 2.6f);
			const FVector Cm(P.X - Dd * 0.8f, P.Y + Dd * 0.6f, 0.f);
			Go(FVector(Cm.X, Cm.Y, Ground(Cm.X, Cm.Y) + 55.f), FVector(P.X, P.Y, Ground(P.X, P.Y) + 10.f));
		} });
		St.Add({ T0 + 12.3f, [Shot, Nm]() { Shot(Nm + TEXT("_dead_low")); } });
	}
	St.Add({ 14.f + 6 * 14.f + 1.f, []() { FGenericPlatformMisc::RequestExit(false); } });
	for (FStep& S : St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [F = S.F]() { F(); }), S.T, false); }
}
