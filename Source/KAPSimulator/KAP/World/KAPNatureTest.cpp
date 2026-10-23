// -kapnatureshots: жаңа табиғатты тексеру суреттері (Saved/Screenshots/.../KAPN_*.png) және логтағы KAPNATURE жолдары
#include "KAPNature.h"
#include "KAPTerrain.h"
#include "KAPVisuals.h"
#include "KAPWorldBuilder.h"
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "WeatherManager.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/KAPPlayerController.h"
#include "KAP/NPC/KAPAnimal.h"
#include "KAP/NPC/KAPLife.h"
#include "KAP/NPC/KAPNPC.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "KAP/Audio/KAPAudio.h"
#include "LandscapeProxy.h"
#include "EngineUtils.h"

void AKAPNature::BeginPlay()
{
	Super::BeginPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("kapnatureshots"))) RunShots();
	// ландшафт диагностикасы: проксилер, компоненттер, соқтығыс, жер биіктігі
	{
		int32 NL = 0, NC = 0, NH = 0;
		for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It) { NL++; NC += It->LandscapeComponents.Num(); NH += It->CollisionComponents.Num(); }
		UE_LOG(LogTemp, Display, TEXT("KAPNATURE landscape proxies=%d comps=%d collision=%d ground(0,0)=%.1f ground(300,161)=%.1f ground(1150,0)=%.1f"), NL, NC, NH,
			KAPVis::GroundZ(GetWorld(), 0, 0, -999.f), KAPVis::GroundZ(GetWorld(), 30000, 16100, -999.f), KAPVis::GroundZ(GetWorld(), 115000, 0, -999.f));
		// -kapaudiocheck: пакетталған ойындағы дыбыс тексеруі (банктер, VO, ойнату), сосын шығу
		if (FParse::Param(FCommandLine::Get(), TEXT("kapaudiocheck")))
		{
			FTimerHandle H1; GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this]() { if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->AudioCheck(); }), 4.f, false);
			FTimerHandle H2; GetWorldTimerManager().SetTimer(H2, []() { FGenericPlatformMisc::RequestExit(false); }, 9.f, false);
		}
		if (FParse::Param(FCommandLine::Get(), TEXT("kaplanddiag"))) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, []() { FGenericPlatformMisc::RequestExit(false); }, 8.f, false); }
	}
}

void AKAPNature::RunShots()
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM) return;
	auto Shot = [this](const FString& N)
	{
		FScreenshotRequest::RequestScreenshot(TEXT("KAPN_") + N + TEXT(".png"), true, false);
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const FVector C = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		UE_LOG(LogTemp, Display, TEXT("KAPNATURE shot %s cam=(%.0f %.0f %.0f) ground=%.0f"), *N, C.X, C.Y, C.Z, KAPVis::GroundZ(GetWorld(), C.X, C.Y, -1.f));
	};
	auto Go = [this, GM](FVector2D Pm, FVector2D LookM, float Up, float Pitch)
	{
		AKAPCharacter* P = GM->Player(); APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!P || !PC) return;
		P->GetCharacterMovement()->SetMovementMode(Up > 150.f ? MOVE_Flying : MOVE_Walking);
		P->GetCharacterMovement()->StopMovementImmediately();
		P->SetActorLocation(FVector(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0) + 100 + Up), false, nullptr, ETeleportType::TeleportPhysics);
		const FVector2D D = LookM - Pm;
		PC->SetControlRotation(FRotator(Pitch, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), 0));
	};
	auto Weather = [GM](EWeatherType T, double Hour)
	{
		GM->Weather->bOverride = true; GM->Weather->OverrideType = T; GM->Weather->OverrideK = 1.f; GM->Weather->bInstant = true;
		GM->S().Time = FMath::FloorToDouble(GM->S().Time / 24) * 24 + 24.0 + Hour;
	};
	using namespace KAPTerrain;
	const FVector2D Riv(300.f, RiverY(300.f));
	const FVector2D Br = Bridges.Num() ? (Bridges[0].A + Bridges[0].B) * 0.5 : Riv;
	const FVector2D BrDir = Bridges.Num() ? (Bridges[0].B - Bridges[0].A).GetSafeNormal() : FVector2D(1, 0);
	struct FStep { float T; TFunction<void()> F; };
	TArray<FStep> St = {
		{ 3.f, [GM]() { GM->MenuNewGame(); } },
		{ 6.f, [GM]() { if (GM->bInMenu) GM->MenuNewGame(); } },
		{ 10.f, [GM]() { if (GM->bCinematic) GM->SkipCinematic(); } },
		{ 11.f, [GM]() { if (GM->bCinematic) GM->SkipCinematic(); } },
		{ 12.f, [this, GM, Weather]() {
			if (GM->bCinematic) GM->SkipCinematic();
			Weather(EWeatherType::Clear, 10.5);
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			AKAPCharacter* P = GM->Player();
			const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
			UE_LOG(LogTemp, Display, TEXT("KAPNATURE test start: cine=%d menu=%d pawn=%s view=%s cam=(%.0f %.0f %.0f) player=%s at (%.0f %.0f %.0f) ground=%.0f veh=%s"), GM->bCinematic ? 1 : 0, GM->bInMenu ? 1 : 0,
				PC ? *GetNameSafe(PC->GetPawn()) : TEXT("-"), PC ? *GetNameSafe(PC->GetViewTarget()) : TEXT("-"), Cam.X, Cam.Y, Cam.Z, *GetNameSafe(P), P ? P->GetActorLocation().X : 0.0, P ? P->GetActorLocation().Y : 0.0, P ? P->GetActorLocation().Z : 0.0,
				P ? KAPVis::GroundZ(GetWorld(), P->GetActorLocation().X, P->GetActorLocation().Y, -1.f) : 0.f, *GetNameSafe(GM->CurrentVehicle.Get()));
			if (GM->CurrentVehicle.IsValid()) GM->ExitVehicle();
			if (PC && P && PC->GetPawn() != P) PC->Possess(P);
			if (PC && P) PC->SetViewTarget(P);
		} },
		{ 13.f, [Go, Riv]() { Go(Riv + FVector2D(-20, 32), Riv + FVector2D(60, 0), 0.f, -6.f); } },
		{ 17.f, [Shot]() { Shot(TEXT("01_river_bank")); } },
		{ 17.5f, [Go, Br, BrDir]() { Go(Br - BrDir * 30.0 + FVector2D(-BrDir.Y, BrDir.X) * 3.0, Br, 0.f, -4.f); } },
		{ 21.f, [Shot]() { Shot(TEXT("02_bridge")); } },
		{ 21.5f, [Go, Br, BrDir]() { Go(Br + FVector2D(-BrDir.Y, BrDir.X) * 40.0, Br, 900.f, -18.f); } },
		{ 25.f, [Shot]() { Shot(TEXT("03_bridge_side")); } },
		{ 25.5f, [Go]() { Go(FVector2D(-600, BranchY(1, -600) + 90.f), FVector2D(-850, BranchY(1, -850)), 5500.f, -22.f); } },
		{ 29.f, [Shot]() { Shot(TEXT("04_delta_air")); } },
		{ 29.5f, [Go]() { Go(FVector2D(-760, BranchY(0, -760) - 25.f), FVector2D(-800, BranchY(1, -800)), 0.f, -3.f); } },
		{ 33.f, [Shot]() { Shot(TEXT("05_delta_reeds")); } },
		{ 33.5f, [Go]() { Go(FVector2D(OasisX + 95.f, OasisY + 30.f), FVector2D(OasisX, OasisY), 60.f, -6.f); } },
		{ 37.f, [Shot]() { Shot(TEXT("06_oasis")); } },
		{ 37.5f, [Go]() { Go(FVector2D(700, 700), FVector2D(1400, 1300), 0.f, 4.f); } },
		{ 41.f, [Shot]() { Shot(TEXT("07_mountains_ne")); } },
		{ 41.5f, [Go]() { Go(FVector2D(-300, -700), FVector2D(-300, -1400), 0.f, 5.f); } },
		{ 45.f, [Shot]() { Shot(TEXT("08_mountains_s")); } },
		{ 45.5f, [Go]() { Go(FVector2D(900, 0), FVector2D(1500, RiverY(1209.f)), 2500.f, -3.f); } },
		{ 49.f, [Shot]() { Shot(TEXT("09_gorge")); } },
		{ 49.5f, [this, Go]() { FVector S; if (NearestSaxaul(FVector(10000, -40000, 0), S)) Go(FVector2D(S) / 100.0 + FVector2D(-9, -5), FVector2D(S) / 100.0, 0.f, -4.f); } },
		{ 53.f, [Shot]() { Shot(TEXT("10_saxaul_dunes")); } },
		{ 53.5f, [this, Go]() { FVector S; if (NearestSaxaul(FVector(-20000, -20000, 0), S)) Go(FVector2D(S) / 100.0 + FVector2D(-7, 4), FVector2D(S) / 100.0, 0.f, -6.f); } },
		{ 57.f, [Shot]() { Shot(TEXT("11_saxaul_steppe")); } },
		// балтамен шабу: үш соққы → құлайды
		{ 57.5f, [this, GM]() {
			if (AKAPCharacter* P = GM->Player())
			{
				FVector S; if (!NearestSaxaul(P->GetActorLocation(), S)) return;
				const FVector From = P->GetActorLocation() + FVector(0, 0, 40);
				const FVector Dir = (S + FVector(0, 0, 120) - From).GetSafeNormal();
				int32 Hits = 0; for (int32 i = 0; i < 4; i++) Hits += MeleeHit(From, Dir, 900.f, 40.f, true) ? 1 : 0;
				UE_LOG(LogTemp, Display, TEXT("KAPNATURE chop hits=%d alive=%d felled=%d"), Hits, AliveSaxaul(), Felled.Num());
			} } },
		{ 59.5f, [Shot]() { Shot(TEXT("12_saxaul_felled")); } },
		{ 60.f, [Go]() { Go(FVector2D(-430, 740), FVector2D(-520, 900), 1800.f, -14.f); } },
		{ 64.f, [Shot]() { Shot(TEXT("13_fields_aksai")); } },
		{ 64.5f, [this, Go]() { if (KAPTerrain::FieldStore().Num()) { const auto& F = KAPTerrain::FieldStore()[0]; const FVector2D C((F.X0 + F.X1) * 0.5f, (F.Y0 + F.Y1) * 0.5f); Go(C + FVector2D(0, (F.Y1 - F.Y0) * 0.5f + 4.f), C, 0.f, -6.f); } } },
		{ 68.f, [Shot]() { Shot(TEXT("14_field_close")); } },
		{ 68.5f, [Go]() { Go(FVector2D(LakeX + 160.f, LakeY + 40.f), FVector2D(LakeX, LakeY), 300.f, -8.f); } },
		{ 72.f, [Shot]() { Shot(TEXT("15_lake")); } },
		{ 72.5f, [Go]() { Go(FVector2D(0, 0), FVector2D(800, 800), 40000.f, -35.f); } },
		{ 77.f, [Shot]() { Shot(TEXT("16_overview")); } },
		// ---- ауа райы: бұлтты күн, жаңбыр (батпақ), гроза (найзағай), қатты жел ----
		{ 77.5f, [Go, Weather]() { Weather(EWeatherType::Cloudy, 12.0); Go(FVector2D(100, -150), FVector2D(700, -900), 0.f, 2.f); } },
		{ 83.f, [Shot, GM]() { Shot(TEXT("17_cloudy")); UE_LOG(LogTemp, Display, TEXT("KAPNATURE weather cloudy: cloud=%.2f vis=%.0f wind=%.1f m/s"), GM->Weather->Env.Cloud, GM->Weather->Env.Visibility, GM->Weather->Env.WindMs); } },
		{ 83.5f, [Go, Weather, GM]() { Weather(EWeatherType::Rain, 12.5); GM->Weather->Env.Wet = 1.f; const FVector2D R(300.f, RiverY(300.f) + 45.f); Go(R, R + FVector2D(40, 10), 0.f, -8.f); } },
		{ 89.f, [Shot, GM]() { Shot(TEXT("18_rain_mud")); UE_LOG(LogTemp, Display, TEXT("KAPNATURE weather rain: rain=%.2f wet=%.2f vis=%.0f"), GM->Weather->Env.Rain, GM->Weather->Env.Wet, GM->Weather->Env.Visibility); } },
		{ 89.5f, [Weather, GM]() { Weather(EWeatherType::Thunder, 13.0); } },
		{ 94.f, [this, GM, Shot]() {
			AWeatherManager* WX = GM->Weather; const AKAPCharacter* P = GM->Player(); if (!P) return;
			const FVector Fw = GetWorld()->GetFirstPlayerController()->GetControlRotation().Vector().GetSafeNormal2D();
			FVector At = P->GetActorLocation() + Fw * 60000.f; At.Z = KAPVis::GroundZ(GetWorld(), At.X, At.Y, 0.f);
			WX->Env.StrikeAt = At; WX->Env.StrikeSeq++; WX->Env.StrikeDelay = 600.f / 343.f; WX->Env.Lightning = 1.f;
			Shot(TEXT("19_thunder_bolt")); UE_LOG(LogTemp, Display, TEXT("KAPNATURE weather thunder: thunder=%.2f cloud=%.2f wind=%.1f"), WX->Env.Thunder, WX->Env.Cloud, WX->Env.WindMs); } },
		{ 95.f, [Shot]() { Shot(TEXT("20_thunder_after")); } },
		// ---- көлік: қарсы жел / артқы жел (14 м/с) — 10 с толық газдан кейінгі жылдамдық ----
		{ 96.f, [this, GM]() { GM->Weather->bOverride = true; GM->Weather->OverrideType = EWeatherType::Windy; GM->Weather->Env.Wet = 0.f; } },
		{ 96.5f, [this, GM]() { DriveTest(GM, 0); } },
		{ 108.f, [this, GM]() { DriveTest(GM, 1); } },
		{ 119.5f, [this, GM]() { DriveTest(GM, 2); } },
		{ 131.f, [this, GM]() { DriveTest(GM, 3); } },
		// ---- жануарлар: жылқы табыны, шабыс, қой отары мен шопан, сиырлар, түлкі, үрей; адамның физикалық реакциясы ----
		{ 142.5f, [this, GM]() { if (AKAPPlayerController* KPC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController())) KPC->AutoMove = FVector2D::ZeroVector; if (AKAPVehicleHandle* Vh = GM->CurrentVehicle.Get()) { if (Vh->Pawn) Vh->Pawn->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); GM->ExitVehicle(); } GM->Weather->ForceWindMs = -1.f; GM->Weather->ForceWindYaw = -999.f; GM->Weather->bOverride = true; GM->Weather->OverrideType = EWeatherType::Clear; GM->Weather->Env.Wet = 0.f; } },
		{ 143.5f, [this, Go]() { AnimalShot(0, Go, 14.f, 2.f); } },
		{ 147.f, [Shot]() { Shot(TEXT("22_horses")); } },
		{ 147.5f, [this, Go]() { if (AKAPFauna* F = AKAPFauna::Get(GetWorld())) F->Stampede(0); AnimalShot(0, Go, 30.f, 6.f); } },
		{ 151.f, [Shot]() { Shot(TEXT("23_horses_gallop")); } },
		{ 151.5f, [this, Go]() { AnimalShot(4, Go, 12.f, 3.f); } },
		{ 155.f, [Shot]() { Shot(TEXT("24_sheep_shepherd")); } },
		{ 155.5f, [this, Go]() { AnimalShot(2, Go, 10.f, 2.f); } },
		{ 159.f, [Shot]() { Shot(TEXT("25_cows")); } },
		{ 159.5f, [this, Go]() { AnimalShot(6, Go, 8.f, 1.f); } },
		{ 163.f, [Shot]() { Shot(TEXT("26_goats_foothill")); } },
		{ 163.5f, [this, Go]() { AnimalShot(8, Go, 6.f, 1.f); } },
		{ 167.f, [Shot]() { Shot(TEXT("27_fox")); } },
		{ 167.5f, [this, Go]() { AnimalShot(5, Go, 20.f, 3.f); if (AKAPFauna* F = AKAPFauna::Get(GetWorld())) if (F->Herds.IsValidIndex(5)) F->Alarm(FVector(F->Herds[5].Home.X * 100, F->Herds[5].Home.Y * 100, 0), 8000.f, 1.f); } },
		{ 170.f, [Shot]() { Shot(TEXT("28_sheep_flee")); } },
		{ 170.5f, [this, GM, Go]() {
			if (AKAPLife* L = AKAPLife::Get(GetWorld())) L->Dump();
			for (TActorIterator<AKAPNPC> It(GetWorld()); It; ++It) if (UKAPHitReactComponent* C = It->FindComponentByClass<UKAPHitReactComponent>()) if (!It->bDead && It->IsHuman())
			{ const FVector2D P = FVector2D(It->GetActorLocation()) / 100.0; Go(P + FVector2D(3.5, 1.5), P, 0.f, -4.f); TestNPC = *It; break; } } },
		{ 172.5f, [this, Shot]() { if (AKAPNPC* N = TestNPC.Get()) if (UKAPHitReactComponent* C = N->FindComponentByClass<UKAPHitReactComponent>()) C->React(FName("spine_03"), FVector(-1, 0.4f, 0.1f).GetSafeNormal() * 1800.f, 1.f); } },
		{ 172.62f, [Shot]() { Shot(TEXT("29_npc_hitreact")); } },
		{ 174.f, [this]() { if (AKAPLife* L = AKAPLife::Get(GetWorld())) L->Dump(); int32 N = 0, Dead = 0; if (AKAPFauna* F = AKAPFauna::Get(GetWorld())) { N = F->Animals.Num(); Dead = F->Killed; } UE_LOG(LogTemp, Display, TEXT("KAPNATURE done saxaul=%d alive=%d bridges=%d water=%d animals=%d killed=%d"), Sax.Num(), AliveSaxaul(), Bridges.Num(), Water.Num(), N, Dead); FGenericPlatformMisc::RequestExit(false); } },
	};
	// -kapdriveonly: тек бастау мен көлік тесті (жел/батпақ), жылдам тексеру
	if (FParse::Param(FCommandLine::Get(), TEXT("kapdriveonly")))
	{
		TArray<FStep> Keep;
		for (FStep& Sx : St) { if (Sx.T <= 12.5f) Keep.Add(Sx); else if (Sx.T >= 95.5f && Sx.T <= 142.6f) { Sx.T -= 80.f; Keep.Add(Sx); } }
		Keep.Add({ 64.f, []() { FGenericPlatformMisc::RequestExit(false); } });
		St = MoveTemp(Keep);
	}
	for (FStep& Sx : St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [F = Sx.F]() { F(); }), Sx.T, false); }
}

/** Көлік тесті: 0 — тыныш, 1 — қарсы жел 14 м/с, 2 — артқы жел 14 м/с, 3 — батпақ (жел жоқ). Топырақ жолмен 9.5 с толық газ */
void AKAPNature::DriveTest(AKAPGameMode* GM, int32 Mode)
{
	AKAPVehicleHandle* V = nullptr;
	if (GM->Builder) for (auto& H : GM->Builder->Vehicles) if (H.IsValid() && H->Id == "uaz469") V = H.Get();
	if (!V || !V->Pawn) { UE_LOG(LogTemp, Warning, TEXT("KAPNATURE drive: no uaz469")); return; }
	const FVector2D A(-780, 640), B(-850, 350);
	const FVector2D Dir = (B - A).GetSafeNormal();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	AWeatherManager* WX = GM->Weather;
	WX->ForceWindMs = Mode == 1 || Mode == 2 ? 14.f : 0.f;
	WX->ForceWindYaw = Mode == 1 ? Yaw + 180.f : Yaw;
	WX->Env.Wet = Mode == 3 ? 1.f : 0.f;
	const FVector2D S = A + Dir * 8.0;
	V->State = "ok"; V->Fuel = 50; V->Hp = 100; V->Flats = 0;
	if (GM->CurrentVehicle.Get() != V) { if (AKAPCharacter* P = GM->Player()) P->SetActorLocation(FVector(S.X * 100, S.Y * 100, KAPVis::GroundZ(GetWorld(), S.X * 100, S.Y * 100, 0) + 300)); }
	V->PlaceAt(FVector(S.X * 100, S.Y * 100, KAPVis::GroundZ(GetWorld(), S.X * 100, S.Y * 100, 0) + 70), Yaw);
	if (GM->CurrentVehicle.Get() != V) GM->EnterVehicle(V);
	AKAPPlayerController* PC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController());
	FTimerHandle H0; GetWorldTimerManager().SetTimer(H0, FTimerDelegate::CreateWeakLambda(this, [PC]() { if (PC) PC->AutoMove = FVector2D(0, 1); }), 0.6f, false);
	const FVector Start(S.X * 100, S.Y * 100, 0);
	FTimerHandle H1; GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateWeakLambda(this, [this, V, PC, Mode, Start]()
	{
		const float Kmh = V->SpeedKmh();
		const float Dist = FVector::Dist2D(V->Pawn->GetActorLocation(), Start) / 100.f;
		static const TCHAR* Nm[4] = { TEXT("calm"), TEXT("headwind14"), TEXT("tailwind14"), TEXT("mud") };
		UE_LOG(LogTemp, Display, TEXT("KAPNATURE drive %s: speed=%.1f km/h dist=%.1f m windN=%.0f mud=%.2f"), Nm[Mode], Kmh, Dist, V->WindForceN, V->MudK);
		if (Mode == 3) FScreenshotRequest::RequestScreenshot(TEXT("KAPN_21_mud_drive.png"), true, false);
		if (PC) PC->AutoMove = FVector2D(0, -1);
	}), 10.1f, false);
	FTimerHandle H2; GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateWeakLambda(this, [PC]() { if (PC) PC->AutoMove = FVector2D::ZeroVector; }), 11.2f, false);
}

/** Табынның ортасын бүйірден көру */
void AKAPNature::AnimalShot(int32 Herd, TFunctionRef<void(FVector2D, FVector2D, float, float)> Go, float Dist, float Up)
{
	AKAPFauna* F = AKAPFauna::Get(GetWorld());
	if (!F) { UE_LOG(LogTemp, Warning, TEXT("KAPNATURE no fauna")); return; }
	FVector2D Sum(0, 0); int32 N = 0; float Spd = 0.f;
	for (const auto& A : F->Animals) if (A.IsValid() && A->Herd == Herd && !A->IsDead()) { Sum += FVector2D(A->GetActorLocation()) / 100.0; N++; Spd += A->Speed; }
	if (!N) { UE_LOG(LogTemp, Warning, TEXT("KAPNATURE herd %d empty"), Herd); return; }
	const FVector2D C = Sum / N;
	const FVector2D Cam = C + FVector2D(-0.6, -0.8) * Dist;
	Go(Cam, C, Up * 100.f, -FMath::RadiansToDegrees(FMath::Atan2(Up + 0.6f, Dist)));
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE herd %d (%s): n=%d at (%.0f, %.0f) avg speed=%.0f cm/s"), Herd, F->Herds.IsValidIndex(Herd) ? *UEnum::GetValueAsString(F->Herds[Herd].Kind) : TEXT("?"), N, C.X, C.Y, Spd / N);
}
