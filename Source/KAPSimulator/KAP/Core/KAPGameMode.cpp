#include "KAPGameMode.h"
#include "KAP/Core/KAPRegistry.h"
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "KAPGameInstance.h"
#include "KAPDataSubsystem.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/KAPPlayerController.h"
#include "KAP/Player/KAPSurvivalAttributeSet.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/UI/KAPHUD.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/WellActor.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAP/World/KAPFXManager.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAPSimulatorPawn.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "UnrealClient.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
extern ENGINE_API float GAverageFPS;
extern ENGINE_API float GAverageMS;
#include "KAP/World/KAPAtmosphere.h"

// ---- әзірлеуші консолі: KAP.* командалары ----
static AKAPGameMode* CmdGM(UWorld* W) { return W ? Cast<AKAPGameMode>(W->GetAuthGameMode()) : nullptr; }
static FAutoConsoleCommandWithWorldAndArgs GCmdTime(TEXT("KAP.Time"), TEXT("Тәулік сағатын орнату: KAP.Time 22.5"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
	{ if (AKAPGameMode* GM = CmdGM(W)) if (A.Num()) { FKAPSession& S = GM->S(); S.Time = FMath::FloorToDouble(S.Time / 24.0) * 24.0 + FCString::Atof(*A[0]); } }));
static FAutoConsoleCommandWithWorldAndArgs GCmdWeather(TEXT("KAP.Weather"), TEXT("Ауа райы: KAP.Weather clear|storm|rain|fog|heat|auto"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W)
	{
		AKAPGameMode* GM = CmdGM(W); if (!GM || !GM->Weather || !A.Num()) return;
		const FString T = A[0].ToLower();
		GM->Weather->bOverride = T != TEXT("auto");
		GM->Weather->OverrideType = T == TEXT("storm") ? EWeatherType::Storm : T == TEXT("rain") ? EWeatherType::Rain : T == TEXT("fog") ? EWeatherType::Fog : T == TEXT("heat") ? EWeatherType::Heat : EWeatherType::Clear;
	}));
static FAutoConsoleCommandWithWorldAndArgs GCmdHeal(TEXT("KAP.Heal"), TEXT("Ойыншыны толық емдеу"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* W) { if (AKAPCharacter* P = AKAPCharacter::Get(W ? W->GetFirstPlayerController() : nullptr)) P->UseItem("medkit"); }));

static FString Fmt(const TCHAR* F) { return FString(F); }

AKAPGameMode::AKAPGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AKAPCharacter::StaticClass();
	PlayerControllerClass = AKAPPlayerController::StaticClass();
	HUDClass = AKAPHUD::StaticClass();
}

AKAPGameMode* AKAPGameMode::Get(const UObject* Ctx)
{
	const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? Cast<AKAPGameMode>(W->GetAuthGameMode()) : nullptr;
}
FKAPSession& AKAPGameMode::S() const { return UKAPGameInstance::Get(this)->S; }
AKAPCharacter* AKAPGameMode::Player() const { return AKAPCharacter::Get(this); }

// =====================================================================
// Бастау
// =====================================================================
void AKAPGameMode::BeginPlay()
{
	Super::BeginPlay();
	// қозғалтқыштың экрандағы дебаг хабарламалары («Preparing SoundWaves» т.б.) ойыншыға көрінбейді; -kapdebug — қалдырады
	if (!FParse::Param(FCommandLine::Get(), TEXT("kapdebug"))) { GAreScreenMessagesEnabled = false; if (GEngine) GEngine->bEnableOnScreenDebugMessages = false; }
#if WITH_EDITOR
	// редактор жинағында (-game) асинхронды компиляцияланатын текстуралар дайын болсын (құм, т.б.)
	FTextureCompilingManager::Get().FinishAllCompilation();
#endif
	Rng.Initialize(FPlatformTime::Cycles());
	AKAPNPC::TrainPolicies();
	Heat.Init(0.f, 64 * 64);
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	WM->InitFromData();
	WM->DangerFn = [this](double X, double Y) { return DangerAt(X, Y); };
	Weather = AWeatherManager::Get(GetWorld());
	if (!Weather) Weather = GetWorld()->SpawnActor<AWeatherManager>();
	AKAPFXManager::Get(GetWorld());
	Builder = GetWorld()->SpawnActor<AKAPWorldBuilder>();
	Builder->BuildAll();
	BuildEconomyPlaces();
	// ---- кинематографиялық атмосфера: прожектор бағандары, шаң, іздер ----
	if (AKAPAtmosphere* Atm = AKAPAtmosphere::Get(GetWorld()))
	{
		UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
		auto Post = [&](FVector2D Pm, float Yaw, bool bBeacon) { Atm->AddLampPost(FVector(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0)), Yaw, bBeacon); };
		for (const FKAPWellRuntime& W : WM->Wells) { Post(W.PosM() + FVector2D(7, 7), 225, true); Post(W.PosM() + FVector2D(-7, -7), 45, false); }
		for (const KAPAI::FRoadNode& N : D->Graph.Nodes)
		{
			if (N.Id.ToString().StartsWith(TEXT("w"))) continue; // ұңғымаларда бар
			Post(N.Pos + FVector2D(5, 4), 200, N.Id.ToString().StartsWith(TEXT("tower")) || N.Id == "hq");
		}
		for (FName V : { FName("aksai"), FName("josaly") })
			if (const FKAPLocationRow* L = D->Loc(V)) for (int32 i = 0; i < 4; i++) { const float A = i * UE_HALF_PI + 0.4f; Post(FVector2D(L->X, L->Y) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 20, FMath::RadiansToDegrees(A) + 180, false); }
		if (const FKAPLocationRow* L = D->Loc("hq")) for (int32 i = 0; i < 6; i++) { const float A = i / 6.f * UE_TWO_PI; Post(FVector2D(L->X, L->Y) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 40, FMath::RadiansToDegrees(A) + 180, i % 2 == 0); }
		if (const FKAPLocationRow* L = D->Loc("guard1")) Post(FVector2D(L->X + 4, L->Y - 3), 135, true);
		Atm->TuneAllLights();
	}
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (GI && GI->bLoadOnStart) { GI->bLoadOnStart = false; StartSession(true); }
	else { bInMenu = true; if (AKAPCharacter* P = Player()) P->bFrozen = true; }
	// мәзір фоны — түнгі ұңғыма, жеңіл құм дауылы, маңдай шамы бар инженер (SetupMenuScene)
	Weather->bOverride = true; Weather->OverrideType = EWeatherType::Storm; Weather->OverrideK = 0.42f; Weather->bInstant = true;
	FKAPSession& Ss = S();
	Ss.Time = 21.8;
	Weather->UpdateEnv(Ss, 0.016f, 0);
	if (bInMenu) SetupMenuScene();
	// Автоматты тексеру: -kapautotest (скриншоттар Saved/Screenshots ішіне)
	if (FParse::Param(FCommandLine::Get(), TEXT("kapsounddump"))) { FTimerHandle Hd; GetWorldTimerManager().SetTimer(Hd, FTimerDelegate::CreateLambda([this]() { UKAPAudio::Get(this)->DumpAll(); FGenericPlatformMisc::RequestExit(false); }), 2.f, false); }
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapvehshots")))
	{
		// Көлік корпустарын тексеру: әр көлік — алдыңғы және артқы 3/4 көрініс, күндізгі жарық
		auto Shot = [](const FString& N) { FScreenshotRequest::RequestScreenshot(TEXT("KAPV_") + N + TEXT(".png"), true, false); };
		FTimerHandle H0; GetWorldTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([this]() { MenuNewGame(); }), 3.f, false);
		FTimerHandle H1; GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([this]() { SkipCinematic(); }), 10.f, false);
		static const TCHAR* Ids[] = { TEXT("uaz469"), TEXT("bukhanka"), TEXT("niva"), TEXT("kamaz"), TEXT("water"), TEXT("btr") };
		for (int32 i = 0; i < 6; i++)
			for (int32 v = 0; v < 2; v++)
			{
				const float T = 14.f + i * 4.f + v * 2.f;
				const FName Vid(Ids[i]);
				FTimerHandle Ha; GetWorldTimerManager().SetTimer(Ha, FTimerDelegate::CreateLambda([this, Vid, v]()
				{
					Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true;
					S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 34.5;
					for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == Vid) if (AKAPCharacter* P = Player())
					{
						const FVector C = V->Pawn->GetActorLocation();
						const float D = (Vid == "kamaz" || Vid == "water" || Vid == "btr") ? 1.45f : 1.f;
						const FVector Sd = C + V->Pawn->GetActorRightVector() * (v ? -390.f : 430.f) * D + V->Pawn->GetActorForwardVector() * (v ? -560.f : 560.f) * D;
						P->SetActorLocation(FVector(Sd.X, Sd.Y, KAPVis::GroundZ(GetWorld(), Sd.X, Sd.Y, C.Z) + 110));
						if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-9, (C - Sd).Rotation().Yaw, 0));
					}
				}), T, false);
				FTimerHandle Hb; GetWorldTimerManager().SetTimer(Hb, FTimerDelegate::CreateLambda([Shot, Vid, v]() { Shot(Vid.ToString() + (v ? TEXT("_b") : TEXT("_a"))); }), T + 1.6f, false);
			}
		FTimerHandle He; GetWorldTimerManager().SetTimer(He, FTimerDelegate::CreateLambda([]() { FGenericPlatformMisc::RequestExit(false); }), 14.f + 6 * 4.f + 2.f, false);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapnpcshots")))
	{
		// Кейіпкерлерді тексеру: барлық түр қатарға тұрады (қозғалмайды), күндізгі жарық, жалпы және жақын кадрлар
		auto Shot = [](const FString& N) { FScreenshotRequest::RequestScreenshot(TEXT("KAPN_") + N + TEXT(".png"), true, false); };
		FTimerHandle H0; GetWorldTimerManager().SetTimer(H0, FTimerDelegate::CreateLambda([this]() { MenuNewGame(); }), 3.f, false);
		FTimerHandle H1; GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda([this]() { SkipCinematic(); }), 10.f, false);
		TSharedRef<TArray<TWeakObjectPtr<AKAPNPC>>> Row = MakeShared<TArray<TWeakObjectPtr<AKAPNPC>>>();
		FTimerHandle H2; GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda([this, Row]()
		{
			Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true;
			S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 34.5;
			AKAPCharacter* P = Player(); if (!P) return;
			const FVector Pl = P->GetActorLocation(); const FVector F = FVector(1, 0, 0), Rt = FVector(0, 1, 0);
			static const ENPCKind Kinds[] = { ENPCKind::Worker, ENPCKind::Police, ENPCKind::Soldier, ENPCKind::Bandit, ENPCKind::Trader, ENPCKind::Villager, ENPCKind::Villager, ENPCKind::Mother, ENPCKind::Elder, ENPCKind::Irradiated };
			for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(Kinds); i++)
			{
				const FVector W = Pl + F * 520.f + Rt * ((i - 4.5f) * 95.f);
				if (AKAPNPC* N = SpawnNPC((int32)Kinds[i], KAP::CM2M(W), FString::Printf(TEXT("T%d"), i), true))
				{
					N->SetActorTickEnabled(false); N->GetCharacterMovement()->DisableMovement();
					N->SetActorLocation(FVector(W.X, W.Y, KAPVis::GroundZ(GetWorld(), W.X, W.Y, Pl.Z) + 92.f));
					N->SetActorRotation(FRotator(0, 180, 0));
					Row->Add(N);
				}
			}
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-4, 0, 0));
		}), 13.f, false);
		FTimerHandle H3; GetWorldTimerManager().SetTimer(H3, FTimerDelegate::CreateLambda([Shot]() { Shot(TEXT("row")); }), 16.f, false);
		// жақын кадрлар: әр кейіпкердің алдынан
		for (int32 i = 0; i < 10; i++)
		{
			FTimerHandle Ha; GetWorldTimerManager().SetTimer(Ha, FTimerDelegate::CreateLambda([this, Row, i]()
			{
				if (!Row->IsValidIndex(i) || !(*Row)[i].IsValid()) return;
				AKAPNPC* N = (*Row)[i].Get(); AKAPCharacter* P = Player(); if (!P) return;
				const FVector C = N->GetActorLocation(); const FVector Sd = C + FVector(-210.f, 60.f, 0);
				P->SetActorLocation(FVector(Sd.X, Sd.Y, KAPVis::GroundZ(GetWorld(), Sd.X, Sd.Y, C.Z) + 100.f));
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-6, (C - Sd).Rotation().Yaw, 0));
			}), 17.f + i * 1.6f, false);
			FTimerHandle Hb; GetWorldTimerManager().SetTimer(Hb, FTimerDelegate::CreateLambda([Shot, i]() { Shot(FString::Printf(TEXT("%02d"), i)); }), 18.f + i * 1.6f, false);
		}
		FTimerHandle He; GetWorldTimerManager().SetTimer(He, FTimerDelegate::CreateLambda([]() { FGenericPlatformMisc::RequestExit(false); }), 36.f, false);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapperf")))
	{
		// Өнімділікті өлшеу: ойын ағынының қай бөлігі уақыт алатынын логқа жазу (stat dumpave)
		auto Cmd = [this](const TCHAR* C) { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(C); };
		auto Go = [this](FVector2D Pm, float Yaw)
		{
			if (AKAPCharacter* P = Player())
			{
				P->SetActorLocation(FVector(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0) + 110), false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-5, Yaw, 0));
			}
		};
		auto Perf = [](const TCHAR* N) { UE_LOG(LogTemp, Display, TEXT("KAPPERF %s: fps %.1f game %.1f render %.1f gpu %.1f ms"), N, GAverageFPS, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); };
		struct FStep { float T; TFunction<void()> F; };
		TArray<FStep> St = {
			{ 3.f, [this]() { MenuNewGame(); } },
			{ 10.f, [this]() { SkipCinematic(); } },
			{ 12.f, [this, Go]() { Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 34.5; if (const FKAPLocationRow* L = UKAPDataSubsystem::Get(this)->Loc("aksai")) Go(FVector2D(L->X - 30, L->Y), 0.f); } },
			{ 17.f, [this, Perf, Cmd]()
			{
				Perf(TEXT("village"));
				int32 Npc = 0, Ticking = 0, Comps = 0; for (TActorIterator<AActor> It(GetWorld()); It; ++It) { if (It->IsA<AKAPNPC>()) Npc++; if (It->IsActorTickEnabled() && It->PrimaryActorTick.bCanEverTick) Ticking++; Comps += It->GetComponents().Num(); }
				UE_LOG(LogTemp, Display, TEXT("KAPPERF counts: npc=%d tickingActors=%d components=%d"), Npc, Ticking, Comps);
				Cmd(TEXT("stat dumpave -num=60 -ms=0.15 -root=TG_EndPhysics -depth=40"));
			} },
			{ 21.f, [Cmd]() { Cmd(TEXT("stat dumpave -num=60 -ms=0.12 -root=RenderViewFamily -depth=40")); } },
			{ 24.f, [this, Go]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.5; if (const FKAPLocationRow* L = UKAPDataSubsystem::Get(this)->Loc("josaly")) Go(FVector2D(L->X - 40, L->Y + 10), 0.f); } },
			{ 29.f, [Perf, Cmd]() { Perf(TEXT("night_josaly")); Cmd(TEXT("stat dumpave -num=60 -ms=0.25 -root=RenderThread -depth=9")); } },
			{ 36.f, [Cmd]() { Cmd(TEXT("dumpticks")); } },
			{ 38.f, []() { FGenericPlatformMisc::RequestExit(false); } },
		};
		for (FStep& Sx : St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([F = Sx.F]() { F(); }), Sx.T, false); }
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapmapshots")))
	{
		// Картаны тексеру: толық экран (бастапқы / барлық аймақ ашық / жақын + кеңес / әскери аймақ), планшет картасы
		auto Shot = [](const FString& N) { UE_LOG(LogTemp, Display, TEXT("KAPMAP fps %s: %.1f"), *N, GAverageFPS); FScreenshotRequest::RequestScreenshot(TEXT("KAPM_") + N + TEXT(".png"), true, false); };
		auto LocP = [this](FName Id) { const FKAPLocationRow* R = UKAPDataSubsystem::Get(this)->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
		auto Hud = [this]() { return AKAPHUD::Get(this); };
		struct FStep { float T; TFunction<void()> F; };
		TArray<FStep> St = {
			{ 3.f, [this]() { MenuNewGame(); } },
			{ 10.f, [this]() { SkipCinematic(); } },
			{ 12.f, [this]() { Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 34.5; S().bHasTablet = true; } },
			{ 16.f, [Hud]() { if (AKAPHUD* H = Hud()) H->ToggleMap(); } },
			{ 17.5f, [Shot]() { Shot(TEXT("01_start")); } },
			{ 18.f, [Hud]() { if (AKAPHUD* H = Hud()) H->MapDebug(FVector2D::ZeroVector, 1.f); } },
			{ 19.f, [Shot]() { Shot(TEXT("02_overview_locked")); } },
			{ 19.5f, [this, Hud, LocP]()
			{
				S().Regions = { true, true, true, true }; S().Towers.Add("tower1");
				TArray<FKAPContract*> O = ContractsOf("police", 0); if (O.Num()) AcceptContract(O[0]->Uid);
				TArray<FKAPContract*> O2 = ContractsOf("trader", 0); if (O2.Num()) AcceptContract(O2[0]->Uid);
				if (AKAPCharacter* P = Player()) if (P->Navigator) P->Navigator->SetTarget(LocP("josaly"), TEXT("Жосалы ауылы"));
				if (AKAPHUD* H = Hud()) H->MapDebug(FVector2D::ZeroVector, 1.f);
			} },
			{ 21.f, [Shot]() { Shot(TEXT("03_overview_all")); } },
			{ 21.5f, [Hud, LocP]() { if (AKAPHUD* H = Hud()) H->MapDebug(LocP("aksai") + FVector2D(20, 0), 6.f, LocP("police")); } },
			{ 22.5f, [Shot]() { Shot(TEXT("04_zoom_aksai")); } },
			{ 23.f, [this, Hud]() { if (AKAPHUD* H = Hud()) H->MapDebug(ArmyC, 3.5f, ArmyC); } },
			{ 24.f, [Shot]() { Shot(TEXT("05_army")); } },
			{ 24.5f, [Hud]() { if (AKAPHUD* H = Hud()) { H->CloseAll(); H->OpenTab(2); H->MapDebug(FVector2D::ZeroVector, 1.f); } } },
			{ 25.5f, [Shot]() { Shot(TEXT("06_tablet_map")); } },
			{ 26.f, [Hud]() { if (AKAPHUD* H = Hud()) { H->CloseAll(); H->MapDebug(FVector2D::ZeroVector, 1.f); } } },
			{ 28.f, [Shot]() { Shot(TEXT("07_gameplay")); } },
			{ 29.f, []() { FGenericPlatformMisc::RequestExit(false); } },
		};
		for (FStep& Sx : St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([F = Sx.F]() { F(); }), Sx.T, false); }
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapecontest")))
	{
		// Экономиканы тексеру: мәзір (+параметрлер), полиция бөлімі, жұмыс тақтасы, планшет, дүкен, әскери аймақ (күндіз/түн)
		auto Shot = [](const FString& N) { FScreenshotRequest::RequestScreenshot(TEXT("KAPE_") + N + TEXT(".png"), true, false); };
		auto Go = [this](FVector2D Pm, float Yaw, float Pitch = -6.f)
		{
			if (AKAPCharacter* P = Player())
			{
				P->SetActorLocation(FVector(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0) + 110), false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(Pitch, Yaw, 0));
			}
		};
		auto LocP = [this](FName Id) { const FKAPLocationRow* R = UKAPDataSubsystem::Get(this)->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
		struct FStep { float T; TFunction<void()> F; };
		TArray<FStep> St = {
			{ 2.5f, [Shot]() { Shot(TEXT("01_menu")); } },
			{ 3.0f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->MenuSettings(true); } },
			{ 3.6f, [Shot]() { Shot(TEXT("02_menu_settings")); } },
			{ 4.0f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->MenuSettings(false); MenuNewGame(); } },
			{ 10.f, [this]() { SkipCinematic(); } },
			{ 13.f, [this, Go, LocP]() { Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 34.5; S().bPermitWeapon = true; S().bHasTablet = true; S().Money = 25000; Go(LocP("police") + FVector2D(-34, 6), -10.f); } },
			{ 15.5f, [Shot]() { Shot(TEXT("03_police")); } },
			{ 16.f, [this]() { if (TWeakObjectPtr<AKAPNPC>* N = NamedNPC.Find("police")) if (N->IsValid()) Talk(N->Get()); } },
			{ 16.8f, [Shot]() { Shot(TEXT("04_police_talk")); } },
			{ 17.2f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) { H->CloseAll(); H->ShowJobs("police"); } } },
			{ 18.f, [Shot]() { Shot(TEXT("05_jobs")); } },
			{ 18.4f, [this]() { TArray<FKAPContract*> O = ContractsOf("police", 0); if (O.Num()) AcceptContract(O[0]->Uid); TArray<FKAPContract*> O2 = ContractsOf("trader", 0); if (O2.Num()) AcceptContract(O2[0]->Uid); if (AKAPHUD* H = AKAPHUD::Get(this)) { H->CloseAll(); H->OpenTab(4); } } },
			{ 19.2f, [Shot]() { Shot(TEXT("06_tablet_jobs")); } },
			{ 19.6f, [this, Go, LocP]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->CloseAll(); Go(LocP("shop") + FVector2D(9, 3), 200.f, -8.f); } },
			{ 21.5f, [Shot]() { Shot(TEXT("07_shop_outside")); } },
			{ 22.f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->ShowShop("trader"); } },
			{ 22.8f, [Shot]() { Shot(TEXT("08_shop")); } },
			{ 23.2f, [this]() { for (const FShopItem& It : ShopItems()) if (It.Id == "ak" || It.Id == "545" || It.Id == "armor") Buy(It, "trader"); UE_LOG(LogTemp, Display, TEXT("KAPECON bought: ak=%d ammo545=%d armor=%d money=%d"), S().Weapons.Contains("ak") ? 1 : 0, S().Ammo.FindRef("545"), S().Inv.FindRef("armor"), S().Money); if (AKAPHUD* H = AKAPHUD::Get(this)) H->ShowShop("trader"); } },
			{ 24.f, [Shot]() { Shot(TEXT("09_shop_bought")); } },
			{ 24.5f, [this, Go]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->CloseAll(); Go(ArmyC + FVector2D(-ArmyHalf.X - 40, 22), -20.f, -4.f); } },
			{ 27.f, [Shot]() { Shot(TEXT("10_army_gate")); } },
			{ 27.5f, [this, Go]() { Go(ArmyC + FVector2D(-20, -10), 30.f, -6.f); } },
			{ 30.f, [Shot]() { Shot(TEXT("11_army_inside")); } },
			{ 36.f, [this, Shot]() { UE_LOG(LogTemp, Display, TEXT("KAPECON army: inZone=%d warn=%d hostile=%.1f"), InArmyZone(KAP::CM2M(Player()->GetActorLocation())) ? 1 : 0, ArmyWarnStage, ArmyHostile); Shot(TEXT("12_army_warn")); } },
			{ 37.f, [this, Go]() { ArmyHostile = 0.f; ArmyWarnStage = 0; ArmyInT = 0.f; S().Flags.Add("armyPass", 1); Go(ArmyC + FVector2D(-ArmyHalf.X - 9, -2), 0.f, -4.f); } },
			{ 46.5f, [Shot]() { Shot(TEXT("13_army_officer")); } },
			{ 47.f, [this, Go]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.2; Go(ArmyC + FVector2D(-ArmyHalf.X - 30, 30), -30.f, -5.f); } },
			{ 50.f, [Shot]() { Shot(TEXT("14_army_night")); } },
			{ 51.f, [this, Go]() { for (FKAPContract* C : ContractsOf(NAME_None, 1)) if (C->Type == "deliver") { UE_LOG(LogTemp, Display, TEXT("KAPECON deliver start: money=%d cargo=%d"), S().Money, Count("cargo")); Go(C->Points[0], 0.f); } } },
			{ 53.f, [this]() { int32 Nd = 0; for (FKAPContract& C : S().Contracts) if (C.Type == "deliver") Nd = C.State; UE_LOG(LogTemp, Display, TEXT("KAPECON deliver: state=%d money=%d cargo=%d"), Nd, S().Money, Count("cargo")); for (FKAPContract* C : ContractsOf(NAME_None, 1)) if (C->Type == "bounty") { int32 K = 0; for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->ContractUid == C->Uid && !It->bDead) { UGameplayStatics::ApplyDamage(*It, 9999.f, nullptr, Player(), nullptr); K++; } UE_LOG(LogTemp, Display, TEXT("KAPECON bounty: killed %d"), K); } } },
			{ 54.f, [this]() { for (FKAPContract& C : S().Contracts) if (C.Type == "bounty") UE_LOG(LogTemp, Display, TEXT("KAPECON bounty: state=%d have=%d/%d money=%d"), C.State, C.Have, C.Need, S().Money); } },
			{ 55.f, []() { FGenericPlatformMisc::RequestExit(false); } },
		};
		for (FStep& Sx : St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([F = Sx.F]() { F(); }), Sx.T, false); }
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("kapautotest")))
	{
		struct FStep { float T; TFunction<void()> F; };
		auto Shot = [](const TCHAR* N) { UE_LOG(LogTemp, Display, TEXT("KAPTEST fps %s: %.1f (%.1f ms) game %.1f render %.1f gpu %.1f ms"), N, GAverageFPS, GAverageMS, FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles())); FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("KAP_%s.png"), N), true, false); };
		TArray<FStep> Steps = {
			{ 3.f, [Shot]() { Shot(TEXT("01_menu")); } },
			{ 4.f, [this]() { MenuNewGame(); } },
			{ 12.f, [Shot]() { Shot(TEXT("02_intro_aerial")); } },
			{ 22.f, [this, Shot]() { Shot(TEXT("03_intro_drive")); for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "bukhanka") UE_LOG(LogTemp, Display, TEXT("KAPTEST intro: speed=%.1f km/h rpm=%.2f autopilot=%d physics=%d"), V->SpeedKmh(), V->Rpm01, V->bAutoPilot ? 1 : 0, V->Pawn->GetMesh()->IsSimulatingPhysics() ? 1 : 0); } },
			{ 26.f, [this]() { SkipCinematic(); } },
			{ 29.f, [Shot]() { Shot(TEXT("04_storm")); } },
			{ 30.f, [this]() { Weather->bOverride = false; S().Weather.Reset(); FKAPWeatherSeg G; G.Start = S().Time - 0.1; G.Dur = 10; S().Weather.Add(G); Weather->Env.Storm = 0; S().bHasTablet = true; S().Regions = { true, true, true, true }; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 15.5; } },
			{ 31.f, [this]() { if (AKAPCharacter* P = Player()) if (const FKAPLocationRow* Wg = UKAPDataSubsystem::Get(this)->Loc("wagon0")) { const FVector2D Q(Wg->X + 9, Wg->Y - 11); P->SetActorLocation(FVector(Q.X * 100, Q.Y * 100, KAPVis::GroundZ(GetWorld(), Q.X * 100, Q.Y * 100, 0) + 120)); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-6, FMath::RadiansToDegrees(FMath::Atan2(Wg->Y - Q.Y, Wg->X - Q.X)), 0)); } } },
			{ 32.5f, [Shot]() { Shot(TEXT("04b_wagon")); } },
			{ 31.6f, [this]() { if (AKAPAtmosphere* At = AKAPAtmosphere::Get(GetWorld())) At->ForceDevil(); } },
			{ 33.f, [Shot]() { Shot(TEXT("05_clear")); } },
			{ 34.f, [this]() { if (AKAPCharacter* P = Player()) { const FVector2D W = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0].PosM(); P->SetActorLocation(FVector((W.X + 14) * 100, (W.Y + 10) * 100, KAPVis::GroundZ(GetWorld(), (W.X + 14) * 100, (W.Y + 10) * 100, 0) + 120)); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-8, 215, 0)); } } },
			{ 38.f, [Shot]() { Shot(TEXT("06_well")); } },
			{ 39.f, [this]() { if (AKAPCharacter* P = Player()) { const FKAPLocationRow* L = UKAPDataSubsystem::Get(this)->Loc("aksai"); P->SetActorLocation(FVector((L->X + 70) * 100, (L->Y + 30) * 100, KAPVis::GroundZ(GetWorld(), (L->X + 70) * 100, (L->Y + 30) * 100, 0) + 120)); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-5, 200, 0)); } } },
			{ 41.f, [this]() { if (AKAPCharacter* P = Player()) { const FVector2D Pm = KAP::CM2M(P->GetActorLocation()); const FVector2D F(FMath::Cos(FMath::DegreesToRadians(200.f)), FMath::Sin(FMath::DegreesToRadians(200.f))); const FVector2D R(-F.Y, F.X); SpawnNPC((int32)ENPCKind::Worker, Pm + F * 3.5f + R * 0.9f, TEXT("Жұмысшы")); SpawnNPC((int32)ENPCKind::Militia, Pm + F * 4.f - R * 0.9f, TEXT("Жасақшы")); } } },
			{ 44.f, [Shot]() { Shot(TEXT("07_village")); } },
			{ 45.f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->ToggleTablet(); } },
			{ 47.f, [Shot]() { Shot(TEXT("08_tablet")); } },
			{ 47.5f, [this]() { int32 C[6] = { 0, 0, 0, 0, 0, 0 }; for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) C[(int32)It->Task]++; UE_LOG(LogTemp, Display, TEXT("KAPTEST tasks none=%d repair=%d carry=%d patrol=%d campfire=%d flee=%d"), C[0], C[1], C[2], C[3], C[4], C[5]); } },
			{ 49.f, [this]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->ToggleTablet(); if (AKAPCharacter* P = Player()) { const FVector2D Pm = KAP::CM2M(P->GetActorLocation()); const FVector2D F = FVector2D(FMath::Cos(FMath::DegreesToRadians(200.f)), FMath::Sin(FMath::DegreesToRadians(200.f))); for (int32 i = 0; i < 3; i++) SpawnNPC((int32)ENPCKind::Wolf, Pm + F * (9 + i * 2) + FVector2D(i * 2 - 2, 0), TEXT("Мутант қасқыр")); SpawnNPC((int32)ENPCKind::Irradiated, Pm + F * 14 + FVector2D(4, 2), TEXT("Сәулеленген адам")); SpawnNPC((int32)ENPCKind::Worm, Pm + F * 18 - FVector2D(4, 0), TEXT("Құм құрты")); } } },
			{ 51.f, [Shot]() { Shot(TEXT("09a_monsters")); } },
			{ 52.f, [this]() { for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (!It->bPersistent) It->Destroy(); S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.0; Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; if (AKAPCharacter* P = Player()) { P->SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), 100); const FVector2D W = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0].PosM(); P->SetActorLocation(FVector((W.X + 22) * 100, (W.Y + 4) * 100, KAPVis::GroundZ(GetWorld(), (W.X + 22) * 100, (W.Y + 4) * 100, 0) + 120)); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-6, 190, 0)); P->MoveInput(FVector2D(0, 1)); } } },
			{ 56.f, [this]() { if (AKAPCharacter* P = Player()) P->MoveInput(FVector2D::ZeroVector); } },
			{ 58.f, [Shot]() { Shot(TEXT("10_night_well")); } },
			{ 59.f, [this]() { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-28, 10, 0)); } },
			{ 61.f, [Shot]() { Shot(TEXT("11_tracks")); } },
			{ 62.f, [this]() { Weather->OverrideType = EWeatherType::Storm; if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-6, 190, 0)); } },
			{ 67.f, [Shot]() { Shot(TEXT("12_night_storm")); } },
			
			{ 67.6f, [this]() { Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 38.0; for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "uaz469") if (AKAPCharacter* P = Player()) { const FVector C = V->Pawn->GetActorLocation(); const FVector Sd = C + V->Pawn->GetActorRightVector() * 480.f + V->Pawn->GetActorForwardVector() * 520.f; P->SetActorLocation(FVector(Sd.X, Sd.Y, KAPVis::GroundZ(GetWorld(), Sd.X, Sd.Y, 0) + 110)); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-12, (C - Sd).Rotation().Yaw, 0)); } } },
			{ 68.8f, [Shot]() { Shot(TEXT("12a_uaz_side")); } },
			{ 69.f, [this]() { Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true; S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 38.0; for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "uaz469") { V->State = "ok"; V->Fuel = 50; if (AKAPCharacter* P = Player()) P->SetActorLocation(V->Pawn->GetActorLocation() + FVector(0, 300, 100)); EnterVehicle(V.Get()); } } },
			{ 69.6f, [Shot]() { Shot(TEXT("12b_uaz")); } },
			{ 70.f, [this]() { if (AKAPPlayerController* PC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController())) PC->AutoMove = FVector2D(0.25f, 1.f); } },
			{ 74.f, [Shot]() { Shot(TEXT("13_drive")); } },
			{ 77.f, [this]() { if (AKAPPlayerController* PC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController())) PC->AutoMove = FVector2D(-0.6f, 1.f); } },
			{ 79.f, [Shot]() { Shot(TEXT("14_drive_turn")); } },
			{ 80.f, [this]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.5; if (AKAPPlayerController* PC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController())) PC->AutoMove = FVector2D(0.f, 0.6f); } },
			{ 84.f, [Shot]() { Shot(TEXT("15_night_headlights")); } },
			{ 85.f, [this]() { if (AKAPPlayerController* PC = Cast<AKAPPlayerController>(GetWorld()->GetFirstPlayerController())) PC->AutoMove = FVector2D::ZeroVector; } },
			{ 86.f, [this]() { if (AKAPVehicleHandle* V = CurrentVehicle.Get()) UE_LOG(LogTemp, Display, TEXT("KAPTEST drive: speed=%.1f km/h pos=%s heat=%.0f"), V->SpeedKmh(), *V->Pawn->GetActorLocation().ToString(), V->Heat); } },
			{ 86.6f, [this]() { if (AKAPVehicleHandle* V = CurrentVehicle.Get()) { V->Pawn->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); V->Pawn->GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector); V->Pawn->DoHandbrakeStart(); } } },
			{ 87.f, [this]() { if (AKAPVehicleHandle* V = CurrentVehicle.Get()) V->Pawn->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector); ExitVehicle(); S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 23.2; Weather->OverrideType = EWeatherType::Clear; UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>(); WM->StartFault(S(), 0, EFaultType::Electric, TEXT("тест")); const FVector2D C = WM->Wells[0].PosM() + FVector2D(-38, 26); if (AKAPCharacter* P = Player()) { const FVector At(C.X * 100 - 600, C.Y * 100 + 250, 0); P->SetActorLocation(FVector(At.X, At.Y, KAPVis::GroundZ(GetWorld(), At.X, At.Y, 0) + 130), false, nullptr, ETeleportType::TeleportPhysics); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-4, -25, 0)); } } },
			{ 88.5f, [this]() { if (AKAPCharacter* P = Player()) P->MoveInput(FVector2D(0, 1)); } },
			{ 90.2f, [this]() { if (AKAPCharacter* P = Player()) P->MoveInput(FVector2D::ZeroVector); } },
			{ 91.f, [Shot]() { Shot(TEXT("16_workshop_alarm")); } },
			{ 91.5f, [this]() { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(8, 20, 0)); } },
			{ 93.f, [Shot]() { Shot(TEXT("17_workshop_inside")); } },
			{ 93.5f, [this]() { const FVector2D C = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0].PosM() + FVector2D(-38, 26); if (AKAPCharacter* P = Player()) { const FVector At(C.X * 100 + 300, C.Y * 100 + 1500, 0); P->SetActorLocation(FVector(At.X, At.Y, KAPVis::GroundZ(GetWorld(), At.X, At.Y, 0) + 130), false, nullptr, ETeleportType::TeleportPhysics); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-2, -100, 0)); } } },
			{ 95.5f, [Shot]() { Shot(TEXT("18_workshop_outside")); } },
			{ 96.f, [this]() { if (AKAPCharacter* P = Player()) { const FVector2D W = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0].PosM() + FVector2D(40, 40); P->SetActorLocation(FVector(W.X * 100, W.Y * 100, KAPVis::GroundZ(GetWorld(), W.X * 100, W.Y * 100, 0) + 120), false, nullptr, ETeleportType::TeleportPhysics); P->SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), 100); if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-3, 30, 0)); if (P->Flashlight->Intensity <= 0) P->ToggleFlashlight(); const FVector2D F(FMath::Cos(FMath::DegreesToRadians(30.f)), FMath::Sin(FMath::DegreesToRadians(30.f))); SpawnNPC((int32)ENPCKind::Irradiated, W + F * 4.5f, TEXT("Сәулеленген адам")); } } },
			{ 98.f, [this, Shot]() { Shot(TEXT("19_monster_close")); UE_LOG(LogTemp, Display, TEXT("KAPTEST music fear=%.2f"), FearSm); } },
			{ 98.5f, [this]() { for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->bStalker && FVector::Dist(It->GetActorLocation(), Player()->GetActorLocation()) < 1000.f) It->Destroy(); if (AKAPCharacter* P = Player()) { if (P->Flashlight->Intensity > 0) P->ToggleFlashlight(); for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (!It->bPersistent && It->Faction == EKAPFaction::Monster) It->Destroy(); const FVector2D Pm = KAP::CM2M(P->GetActorLocation()); const FVector2D F(FMath::Cos(FMath::DegreesToRadians(30.f)), FMath::Sin(FMath::DegreesToRadians(30.f))); if (AKAPNPC* St = SpawnNPC((int32)ENPCKind::Irradiated, Pm + F * 26.f + FVector2D(-F.Y, F.X) * 9.f, TEXT("..."))) St->bStalker = true; } } },
			{ 100.f, [Shot]() { Shot(TEXT("20_stalker")); } },
			{ 101.f, [this]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 37.0; if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-12, 30, 0)); } },
			{ 102.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("wrench"); } }, { 102.8f, [Shot]() { Shot(TEXT("21_w_wrench")); } },
			{ 103.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("crowbar"); } }, { 103.8f, [Shot]() { Shot(TEXT("22_w_crowbar")); } },
			{ 104.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("pm"); } }, { 104.8f, [Shot]() { Shot(TEXT("23_w_pm")); } },
			{ 105.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("shotgun"); } }, { 105.8f, [Shot]() { Shot(TEXT("24_w_shotgun")); } },
			{ 106.f, [this]() { if (AKAPCharacter* P = Player()) { S().bPermitWeapon = true; S().Mag.Add("ak", 30); S().Ammo.Add("545", 90); P->Equip("ak"); } } }, { 106.5f, [this]() { if (AKAPCharacter* P = Player()) P->FireStart(); } }, { 106.53f, [Shot]() { Shot(TEXT("25_w_ak")); } }, { 106.6f, [this]() { if (AKAPCharacter* P = Player()) { P->FireStop(); S().Mag.Add("ak", 0); P->Reload(); } } }, { 106.95f, [Shot]() { Shot(TEXT("25b_ak_reload")); } },
			{ 107.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("sniper"); } }, { 107.8f, [Shot]() { Shot(TEXT("26_w_sniper")); } },
			{ 108.f, [this]() { if (AKAPCharacter* P = Player()) P->Equip("knife"); } }, { 108.8f, [Shot]() { Shot(TEXT("27_w_knife")); } },
			{ 109.3f, [this]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 24.0 + 17.4; if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-2, 270, 0)); if (AKAPCharacter* P = Player()) P->Equip("wrench"); } },
			{ 112.f, [Shot]() { Shot(TEXT("28_dusk")); } },
			{ 112.3f, [this]() { S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 24.0 + 6.3; if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-2, 90, 0)); } },
			{ 115.f, [Shot]() { Shot(TEXT("29_dawn")); } },
			{ 115.5f, [this]() { FGenericPlatformMisc::RequestExit(false); } },
		};
		for (FStep& St : Steps) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([F = St.F]() { F(); }), St.T, false); }
	}
}

void AKAPGameMode::MenuNewGame()
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const bool bRu = GI->S.bRuSubtitles;
	GI->S = FKAPSession();
	GI->S.bRuSubtitles = bRu;
	StartSession(false);
}
void AKAPGameMode::MenuContinue()
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (!GI->LoadSession()) { Toast(TEXT("Сақталған ойын жоқ"), "warn"); return; }
	StartSession(true);
}

void AKAPGameMode::StartSession(bool bLoad)
{
	bInMenu = false; bEnded = false; bPlayerDead = false;
	ClearMenuScene();
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	FKAPSession& Ss = GI->S;
	Weather->bOverride = false; Weather->OverrideK = 1.f;
	if (!bLoad)
	{
		Ss.Time = D->Cfg("startHour", 12.5f);
		Ss.Inv = { { "water", 1 }, { "bandage", 1 }, { "food", 1 } };
		Ss.Ammo = { { "flare", 3 } };
		Ss.Weapons = { "wrench", "knife", "crowbar", "flare" };
		WM->NewGameState(Ss);
		D->Brigades->ForeachRow<FKAPBrigadeRow>(TEXT("KAP"), [&](const FName& K, const FKAPBrigadeRow& B)
			{ FKAPBrigadeState St; St.Id = K; St.Pos = St.Base = FVector2D(B.X, B.Y); Ss.Brigades.Add(St); });
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : Builder->Vehicles)
			if (V.IsValid()) { FKAPVehicleSave Vs; Vs.Id = V->Id; Vs.Fuel = V->Fuel; Vs.Hp = V->Hp; Vs.State = V->State; Vs.Xf = V->Pawn->GetActorTransform(); Ss.VehicleStates.Add(Vs); }
		Ss.Weather.Reset();
	}
	SyncSessionToWorld();
	SpawnPersistentNPCs();
	if (!bLoad) Intro();
	else
	{
		if (AKAPCharacter* P = Player()) { P->bFrozen = false; P->Respawn(Ss.PlayerPos); }
		Toast(TEXT("Ойын жүктелді"), "ok");
	}
	GI->bSessionStarted = true;
}

void AKAPGameMode::SyncSessionToWorld()
{
	FKAPSession& Ss = S();
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < WM->Wells.Num(); i++) WM->RefreshVisual(Ss, i);
	for (const FKAPVehicleSave& Vs : Ss.VehicleStates)
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : Builder->Vehicles)
			if (V.IsValid() && V->Id == Vs.Id)
			{
				V->Fuel = Vs.Fuel; V->Hp = Vs.Hp; V->Flats = Vs.Flats; V->State = Vs.State;
				if (!Vs.Xf.GetLocation().IsNearlyZero()) V->Pawn->SetActorTransform(Vs.Xf, false, nullptr, ETeleportType::TeleportPhysics);
			}
	if (AKAPStructure* G = Builder->HQGate.Get()) G->SetActorHiddenInGame(Ss.Flags.Contains("gateOpen")), G->SetActorEnableCollision(!Ss.Flags.Contains("gateOpen"));
	if (AKAPCharacter* P = Player())
	{
		P->SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), Ss.Hp);
		P->SetAttr(UKAPSurvivalAttributeSet::GetMaxHealthAttribute(), Ss.MaxHp);
		P->SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), Ss.Water);
		P->SetAttr(UKAPSurvivalAttributeSet::GetBodyTempAttribute(), Ss.Temp);
		P->SetAttr(UKAPSurvivalAttributeSet::GetDoseAttribute(), Ss.Dose);
		P->SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), Ss.Stamina);
		P->Equip(Ss.Equipped);
	}
}

void AKAPGameMode::SyncPlayerToSession()
{
	FKAPSession& Ss = S();
	if (AKAPCharacter* P = Player())
	{
		Ss.PlayerPos = P->GetActorLocation();
		Ss.Hp = P->GetHp(); Ss.MaxHp = P->GetMaxHp(); Ss.Water = P->GetWater(); Ss.Temp = P->GetTemp(); Ss.Dose = P->GetDose(); Ss.Stamina = P->GetStamina();
	}
	Ss.VehicleStates.Reset();
	for (const TWeakObjectPtr<AKAPVehicleHandle>& V : Builder->Vehicles)
		if (V.IsValid()) { FKAPVehicleSave Vs; Vs.Id = V->Id; Vs.Fuel = V->Fuel; Vs.Hp = V->Hp; Vs.Flats = V->Flats; Vs.State = V->State; Vs.Xf = V->Pawn->GetActorTransform(); Ss.VehicleStates.Add(Vs); }
}

void AKAPGameMode::Save(bool bAuto)
{
	if (bInMenu || bEnded) return;
	SyncPlayerToSession();
	if (UKAPGameInstance::Get(this)->SaveSession()) { if (bAuto) Toast(TEXT("Автосақтау"), "info"); else Toast(TEXT("Ойын сақталды"), "ok"); }
	else Toast(TEXT("Сақтау мүмкін болмады"), "danger");
}
void AKAPGameMode::SetCheckpoint() { if (AKAPCharacter* P = Player()) S().Checkpoint = P->GetActorLocation(); }

// =====================================================================
// NPC
// =====================================================================
AKAPNPC* AKAPGameMode::SpawnNPC(int32 Kind, FVector2D P, const FString& Name, bool bPersistent, float Scale, bool bBoss)
{
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const float Z = KAPVis::GroundZ(GetWorld(), P.X * 100, P.Y * 100, 0) + 100 * Scale;
	AKAPNPC* N = GetWorld()->SpawnActor<AKAPNPC>(FVector(P.X * 100, P.Y * 100, Z), FRotator(0, Rng.FRandRange(0, 360), 0), SP);
	if (!N) return nullptr;
	N->Setup((ENPCKind)Kind, Name, Scale, bBoss);
	N->bPersistent = bPersistent;
	return N;
}

void AKAPGameMode::SpawnPersistentNPCs()
{
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) It->Destroy();
	NamedNPC.Reset(); Bosses.Reset();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	FKAPSession& Ss = S();
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	auto Mk = [&](ENPCKind K, FVector2D P, const FString& Name, FName Talk) { AKAPNPC* N = SpawnNPC((int32)K, P, Name, true); if (N) N->TalkId = Talk; return N; };
	NamedNPC.Add("toleg", Mk(ENPCKind::Elder, L("aksai") + FVector2D(3, 8), TEXT("Ақсақал Төлеген"), "toleg"));
	NamedNPC.Add("sagat", Mk(ENPCKind::Elder, L("josaly") + FVector2D(3, 8), TEXT("Ақсақал Сағат"), "sagat"));
	for (FName V : { FName("aksai"), FName("josaly") })
	{
		const FVector2D C = L(V);
		Mk(ENPCKind::Boy, C + FVector2D(-4, 6), TEXT("Ауыл баласы"), "child_boy");
		Mk(ENPCKind::Girl, C + FVector2D(-7, 5), TEXT("Ауыл қызы"), "child_girl");
		for (int32 i = 0; i < 4; i++) { AKAPNPC* N = Mk(ENPCKind::Villager, C + FVector2D(Rng.FRandRange(-25, 25), Rng.FRandRange(-25, 25)), TEXT("Ауыл тұрғыны"), "villager"); if (N) N->WanderR = 25; }
		for (int32 i = 0; i < (V == "aksai" ? 5 : 4); i++) { AKAPNPC* N = Mk(ENPCKind::Militia, C + FVector2D(Rng.FRandRange(-30, 30), Rng.FRandRange(-30, 30)), TEXT("Жасақшы"), "militia"); if (N) N->PatrolR = 55; }
	}
	NamedNPC.Add("sick_child", Mk(ENPCKind::SickChild, L("aksai") + FVector2D(5, 7), TEXT("Науқас бала"), "child_sick"));
	if (AKAPNPC* B = Mk(ENPCKind::Guard, L("guard1") + FVector2D(2, 2.5f), TEXT("Күзетші Бекзат"), "bekzat")) { B->PatrolR = 2; NamedNPC.Add("bekzat", B); }
	for (int32 i = 0; i < 3; i++) if (AKAPNPC* G = Mk(ENPCKind::Guard, L("hq") + FVector2D(-6 + i * 6, 48), TEXT("Кеніш күзетшісі"), "hqguard")) G->PatrolR = 6;
	// жұмысшылар
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < Ss.Workers.Num(); i++)
	{
		FKAPWorker& K = Ss.Workers[i];
		if (!K.EvacTo.IsEmpty()) continue;
		const FVector2D W = WM->Wells[K.WellId - 1].PosM();
		AKAPNPC* N = SpawnNPC((int32)ENPCKind::Worker, W + FVector2D(Rng.FRandRange(-6, 6), 6 + Rng.FRandRange(0, 4)), K.Name, true);
		if (!N) continue;
		N->TalkId = "worker"; N->WorkerIndex = i; N->WanderR = 9; N->Home = W + FVector2D(0, 7);
		if (K.State == 4) N->Kill(false);
	}
	if (QuestState("s_ruins") != EQuestState::Done)
	{
		const FVector2D Off[3] = { FVector2D(-20, 15), FVector2D(25, -30), FVector2D(40, 35) };
		for (int32 i = 0; i < 3; i++)
			if (!Ss.Flags.Contains(FName(*FString::Printf(TEXT("surv%d"), i))))
				if (AKAPNPC* N = Mk(ENPCKind::Survivor, L("ruins") + Off[i], TEXT("Тірі қалған тұрғын"), "survivor")) { N->SurvivorId = i; N->WanderR = 2; }
	}
	if (!Ss.Flags.Contains("missingFound"))
		if (AKAPNPC* N = Mk(ENPCKind::Worker, L("missing"), TEXT("Нұрлан Есенов"), "nurlan")) { N->bSick = true; N->WanderR = 1; NamedNPC.Add("nurlan", N); }
	SpawnEconomyNPCs();
}

// =====================================================================
// Хабарламалар
// =====================================================================
void AKAPGameMode::Radio(const FString& Msg, FName Kind)
{
	Toast(TEXT("Радио: ") + Msg, Kind);
	RadioLog.Add(KAP::FmtTime(S().Time) + TEXT("  ") + Msg);
	if (RadioLog.Num() > 60) RadioLog.RemoveAt(0);
	if (UKAPAudio* A = UKAPAudio::Get(this)) { A->SetRadio(1.f); bRadioPulse = true; if (!A->Speak(Msg, FVector::ZeroVector, Msg.Len() % 3 == 0, 2)) A->RadioVoice(FMath::Clamp(Msg.Len() * 0.055f, 1.2f, 4.f), Msg.Len() % 3 == 0); }
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, [this]() { if (UKAPAudio* A = UKAPAudio::Get(this)) A->SetRadio(0); }, 0.4f, false);
}
void AKAPGameMode::Toast(const FString& Msg, FName Kind) { if (AKAPHUD* H = AKAPHUD::Get(this)) H->AddToast(Msg, Kind); }
void AKAPGameMode::Subtitle(const FString& Kk, const FString& Ru, const FString& Who, float Dur)
{
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->SetSubtitle(Kk, S().bRuSubtitles ? Ru : FString(), Who, Dur);
	// ойыншы, диктор, рация, планшет — жазылған дауыспен (NPC өзі 3D айтады)
	if (Who.IsEmpty() || Who == TEXT("Сіз") || Who == TEXT("Радио") || Who == TEXT("Ана") || Who == TEXT("Планшет"))
		if (UKAPAudio* A = UKAPAudio::Get(this)) A->Speak(Kk, FVector::ZeroVector, Who == TEXT("Ана") || Who == TEXT("Планшет"), Who == TEXT("Радио") ? 2 : 1);
}
void AKAPGameMode::Dialog(const FString& Name, const FString& Text, TArray<FKAPDialogOption> Opts)
{
	if (Opts.Num() == 0) Opts.Add({ TEXT("Жабу"), nullptr, false });
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->ShowDialog(Name, Text, MoveTemp(Opts));
	// сөйлесіп тұрған NPC-нің дауысы (жынысы бойынша)
	if (UKAPAudio* A = UKAPAudio::Get(this)) { A->StopSpeech(); A->Speak(Text, FVector::ZeroVector, TalkNPC.IsValid() && TalkNPC->DisplayName == Name && TalkNPC->IsFemale(), 1); }
}

// =====================================================================
// Квесттер
// =====================================================================
EQuestState AKAPGameMode::QuestState(FName Id) const { const EQuestState* Q = S().Quests.Find(Id); return Q ? *Q : EQuestState::None; }
void AKAPGameMode::StartQuest(FName Id, bool bAnnounce)
{
	FKAPSession& Ss = S();
	if (Ss.Quests.Contains(Id)) return;
	Ss.Quests.Add(Id, EQuestState::Active);
	const FKAPQuestRow* Q = UKAPDataSubsystem::Get(this)->Quest(Id);
	if (bAnnounce && Q) Toast((Q->Kind == "side" ? Fmt(TEXT("Қосымша тапсырма: ")) : Fmt(TEXT("Жаңа тапсырма: "))) + Q->Title, "info");
	if (Id == "s_parts")
		for (const FVector2D& P : { FVector2D(700, 1000), FVector2D(980, 700), FVector2D(450, 820), FVector2D(150, 1000) })
		{
			AKAPInteractSpot* Sp = Builder->SpawnSpot("pickup", FVector(P.X * 100, P.Y * 100, Builder->Z(P) + 20), 220, true);
			Sp->Items.Add("car_part", 1);
		}
}
void AKAPGameMode::CompleteQuest(FName Id)
{
	FKAPSession& Ss = S();
	if (QuestState(Id) == EQuestState::Done) return;
	Ss.Quests.Add(Id, EQuestState::Done);
	if (const FKAPQuestRow* Q = UKAPDataSubsystem::Get(this)->Quest(Id)) Toast(TEXT("+ Орындалды: ") + Q->Title, "ok");
	UKAPAudio::Get(this)->Play(EKAPSound::RepairDone, 0.6f);
}
void AKAPGameMode::CompleteSide(FName Id)
{
	if (QuestState(Id) == EQuestState::Done) return;
	CompleteQuest(Id);
	FKAPSession& Ss = S();
	FString R;
	if (Id == "s_militia") { Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 30); R = GiveItems({ { "shotgun", 1 }, { "12g", 16 }, { "medkit", 1 } }); }
	else if (Id == "s_bandit") R = TEXT("Лагерь олжасы жерде жатыр");
	else if (Id == "s_missing") { Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 15); R = GiveItems({ { "suit", 1 }, { "iodine", 5 } }); }
	else if (Id == "s_parts") { for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "niva") V->State = "ok"; Ss.Flags.Add("niva", 1); R = TEXT("Нива жөнделді! ") + GiveItems({ { "fuel", 1 } }); }
	else if (Id == "s_water") { for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "water") V->State = "ok"; R = TEXT("Су тасушы ашылды, су қоймасынан су ішуге болады"); }
	else if (Id == "s_ruins") { Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 10); R = GiveItems({ { "antidote", 3 }, { "molotov", 1 }, { "molotov_ammo", 0 } }); Ss.Ammo.FindOrAdd("molotov") += 3; }
	if (!R.IsEmpty()) Toast(TEXT("Сыйлық: ") + R, "ok");
	if (Ss.RepMilitia >= 50 && !Ss.Flags.Contains("allyMsg")) { Ss.Flags.Add("allyMsg", 1); Radio(TEXT("Ауыл жасағы сізді одақтас деп таниды! Төлеген ақсақалмен сөйлесіңіз."), "ok"); }
	Save(true);
}

FString AKAPGameMode::QuestProgress(FName Id) const
{
	const FKAPSession& Ss = S();
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (Id == "m_wells") return FString::Printf(TEXT("Жұмыс істейді: %d/21 · жанған: %d"), WM->OkCount(Ss), Ss.Burned);
	if (Id == "m_towers") return FString::Printf(TEXT("%d/3"), Ss.Towers.Num());
	if (Id == "s_militia") return FString::Printf(TEXT("%d/6"), Ss.QuestData.FindRef("s_militia"));
	if (Id == "s_parts") return FString::Printf(TEXT("%d/4"), Ss.Inv.FindRef("car_part"));
	if (Id == "s_ruins") return FString::Printf(TEXT("%d/3"), Ss.QuestData.FindRef("s_ruins"));
	if (Id == "m_hq" && !Ss.bPermitAccess) return FString::Printf(TEXT("рұқсат: %d/%d"), Ss.WellsRepaired, (int32)UKAPDataSubsystem::Get(this)->Cfg("accessPermitWells", 5));
	return FString();
}

FString AKAPGameMode::ObjectiveText() const
{
	static const TCHAR* Order[] = { TEXT("m_shelter"), TEXT("m_tablet"), TEXT("m_wait"), TEXT("m_radiator"), TEXT("m_fixuaz"), TEXT("m_well1"), TEXT("m_fix1"), TEXT("m_guard"), TEXT("m_report"), TEXT("m_cause"), TEXT("m_hq") };
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	for (const TCHAR* Id : Order)
		if (QuestState(Id) == EQuestState::Active)
		{
			const FKAPQuestRow* Q = D->Quest(Id);
			const FString P = QuestProgress(Id);
			return (Q ? Q->Title : FString(Id)) + (P.IsEmpty() ? TEXT("") : TEXT(" · ") + P);
		}
	if (QuestState("m_wells") == EQuestState::Active)
	{
		const FKAPSession& Ss = S();
		TArray<int32> Bad;
		for (int32 i = 0; i < Ss.Wells.Num(); i++)
			if ((Ss.Wells[i].State == EWellState::Fault || Ss.Wells[i].State == EWellState::Leak) && Ss.Regions[GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[i].Row.Region]) Bad.Add(i);
		Bad.Sort([&](int32 A, int32 B) { return (Ss.Wells[A].State == EWellState::Leak ? -100 : 0) + Ss.Wells[A].Timer < (Ss.Wells[B].State == EWellState::Leak ? -100 : 0) + Ss.Wells[B].Timer; });
		if (Bad.Num())
		{
			FString T = TEXT("Жөндеу керек: ");
			for (int32 k = 0; k < FMath::Min(3, Bad.Num()); k++) T += FString::Printf(TEXT("%s№%d%s"), k ? TEXT(", ") : TEXT(""), Bad[k] + 1, Ss.Wells[Bad[k]].State == EWellState::Leak ? TEXT(" (ағу!)") : TEXT(""));
			return T;
		}
		return FString::Printf(TEXT("Ұңғымаларды бақылаңыз · мұнаралар %d/3"), Ss.Towers.Num());
	}
	return TEXT("—");
}

FString AKAPGameMode::GiveItems(const TMap<FName, int32>& Items)
{
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	TArray<FString> Out;
	for (const TPair<FName, int32>& P : Items)
	{
		if (P.Value <= 0) continue;
		const bool bWeapon = D->Weapon(P.Key) && !D->Items->FindRow<FKAPItemRow>(P.Key, TEXT("KAP"), false);
		const bool bItem = D->Items->FindRow<FKAPItemRow>(P.Key, TEXT("KAP"), false) != nullptr;
		if (bWeapon)
		{
			if (!Ss.Weapons.Contains(P.Key)) Ss.Weapons.Add(P.Key);
			Out.Add(D->Weapon(P.Key)->Title);
		}
		else if (bItem) { Ss.Inv.FindOrAdd(P.Key) += P.Value; Out.Add(FString::Printf(TEXT("%s ×%d"), *D->ItemName(P.Key), P.Value)); }
		else { Ss.Ammo.FindOrAdd(P.Key) += P.Value; Out.Add(FString::Printf(TEXT("%s ×%d"), *D->AmmoName(P.Key), P.Value)); }
	}
	return FString::Join(Out, TEXT(", "));
}
void AKAPGameMode::TakeItem(FName Id, int32 N) { int32& V = S().Inv.FindOrAdd(Id); V = FMath::Max(0, V - N); }
int32 AKAPGameMode::Count(FName Id) const { return S().Inv.FindRef(Id); }

// =====================================================================
// Тик
// =====================================================================
void AKAPGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	Dt = FMath::Min(Dt, 0.05f);
	if (!Weather) return;
	FKAPSession& Ss = S();
	UKAPAudio* Au = UKAPAudio::Get(this);
	if (bCredits) { CreditsT += Dt; return; }
	if (bInMenu)
	{
		Ss.Time += Dt * 0.01;
		Weather->UpdateEnv(Ss, Dt, 0);
		if (AKAPCharacter* P = Player()) if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
		{
			if (!CineCam) CineCam = GetWorld()->SpawnActor<ACameraActor>();
			const float T = GetWorld()->GetRealTimeSeconds();
			if (bMenuScene)
			{
				// қолмен ұстаған камера: баяу жақындап-алыстайды, сәл тербеледі
				const FVector Fw = (MenuLook - MenuCamBase).GetSafeNormal2D(), Rt(-Fw.Y, Fw.X, 0);
				const float Dolly = FMath::Sin(T * 0.07f) * 70.f;
				const FVector Sway = Rt * (FMath::Sin(T * 0.41f) * 9.f + FMath::Sin(T * 1.13f) * 2.5f) + FVector(0, 0, FMath::Sin(T * 0.53f) * 5.f + FMath::Sin(T * 1.7f) * 1.5f);
				CineCam->SetActorLocation(MenuCamBase + Fw * Dolly + Sway);
				FRotator R = (MenuLook - CineCam->GetActorLocation()).Rotation();
				R.Roll = FMath::Sin(T * 0.33f) * 0.8f;
				CineCam->SetActorRotation(R);
				CineCam->GetCameraComponent()->SetFieldOfView(52.f);
				// апат шамы жыпылықтайды, маңдай шамы аздап дірілдейді
				if (MenuBeacon) MenuBeacon->SetIntensity(1400.f * FMath::Pow(FMath::Max(0.f, FMath::Sin(T * 3.4f)), 3.f) + 40.f);
				if (MenuHeadlamp) MenuHeadlamp->SetIntensity(1500.f * (0.96f + 0.04f * FMath::Sin(T * 37.f)));
			}
			else
			{
				const float A = GetWorld()->GetTimeSeconds() * 0.03f;
				const FVector C(-78000, 64000, KAPVis::GroundZ(GetWorld(), -78000, 64000, 0));
				CineCam->SetActorLocation(C + FVector(FMath::Cos(A) * 6000, FMath::Sin(A) * 6000, 1800));
				CineCam->SetActorRotation((C - CineCam->GetActorLocation()).Rotation());
			}
			if (PC->GetViewTarget() != CineCam) PC->SetViewTarget(CineCam);
		}
		return;
	}
	if (bPaused) return;
	if (Cine.IsSet())
	{
		Cine->T += Dt;
		Cine->Update(Cine->T, Dt);
		Weather->UpdateEnv(Ss, Dt, 0);
		if (Cine.IsSet() && Cine->T >= Cine->Dur) EndCine();
		return;
	}
	if (bPlayerDead) return;
	Simulate(Dt);
	Director(Dt);
	StoryTick(Dt);
	ContractTick(Dt);
	Acc1 += Dt; Acc2 += Dt; Acc15 += Dt;
	if (Acc1 > 1) { Acc1 = 0; UpdateTrackers(); RefreshDangerZones(); }
	if (Acc2 > 2) { Acc2 = 0; GetWorld()->GetSubsystem<UWellManagerSubsystem>()->UpdatePredictions(Ss, Weather->Env.Temp); }
	if (Acc15 > 15) { Acc15 = 0; CheckEndings(); }
	if (Provoked > 0) Provoked -= Dt;
	// дыбыс деңгейлері
	if (Au)
	{
		const AKAPCharacter* P = Player();
		Au->SetWind(Weather->Env.Storm * 0.9f + 0.1f);
		Au->SetRain(Weather->Env.Rain);
		IndoorAu = FMath::FInterpTo(IndoorAu, P && (P->bIndoor || CurrentVehicle.IsValid()) ? 1.f : 0.f, Dt, 3.f);
		Au->SetIndoor(IndoorAu);
		Weather->Indoor = FMath::FInterpTo(Weather->Indoor, P && P->bIndoor ? 1.f : 0.f, Dt, 1.5f);
		if (P)
		{
			Au->Update(Dt, P->GetActorLocation(), Weather->Env.bNight, Weather->Env.Storm);
			Au->SetGeiger(P->DoseRate, Ss.Inv.FindRef("dosimeter") > 0);
			// қорқыныш: жақын тұрған дұшпан / аз денсаулық
			float Fear = FMath::Clamp(1.f - P->GetHp() / FMath::Max(1.f, P->GetMaxHp()), 0.f, 1.f) * 0.8f;
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
				if (It->IsHostileToPlayer()) Fear = FMath::Max(Fear, FMath::GetMappedRangeValueClamped(FVector2f(800.f, 3500.f), FVector2f(1.f, 0.f), (float)FVector::Dist(It->GetActorLocation(), P->GetActorLocation())));
			const bool bDangerNow = Fear > 0.6f;
			if (bDangerNow && !bWasDanger && StingerCd <= 0) { Au->Stinger(true); StingerCd = 40.f; }
			if (!bDangerNow && bWasDanger) DirRest = FMath::Max(DirRest, 45.f); // шайқастан кейін демалыс
			bWasDanger = bDangerNow; StingerCd -= Dt;
			Au->SetBody(1.f - P->GetStamina() / 100.f, Fear);
			// үрей фоны: түнде әлсіз, қауіп кезінде күшейеді; экранға қорқыныш эффектісі
			FearSm = FMath::FInterpTo(FearSm, Fear, Dt, Fear > FearSm ? 4.f : 0.6f);
			Weather->Fear = FearSm;
			Au->SetDread((Weather->Env.bNight ? 0.45f : 0.08f) + FearSm * 0.6f - IndoorAu * 0.15f, FearSm);
			// музыка: тыныш кезде әлсіз фон (түнде ғана), қауіп өскенде қабаттар қосылады
			Au->SetMusic(FearSm, FearSm > 0.2f ? 0.4f + FearSm * 0.6f : (Weather->Env.bNight ? 0.25f : 0.f));
			// баспанаға кіру/шығу — есік сықыры мен тарсылы (вагон — металл, ағаш еден — ағаш)
			if (P->bIndoor != bWasIndoor)
			{
				bWasIndoor = P->bIndoor;
				if (DoorCd <= 0) { Au->Door(P->GetActorLocation() + P->GetActorForwardVector() * 60.f, 0); DoorCd = 1.5f; UE_LOG(LogTemp, Display, TEXT("KAPTEST door indoor=%d"), P->bIndoor ? 1 : 0); }
			}
			DoorCd -= Dt;
			// домбыра: таң мен кеште, қауіп жоқ кезде, сирек (5–9 минутта бір)
			DombraCd -= Dt;
			{ const double Hh = FMath::Fmod(Ss.Time, 24.0); if (DombraCd <= 0 && FearSm < 0.1f && ((Hh > 5.5 && Hh < 7.5) || (Hh > 18.0 && Hh < 20.5))) { const float Dur = Au->Dombra(); Au->Duck(Dur, 0.35f); DombraCd = FMath::FRandRange(300.f, 540.f); UE_LOG(LogTemp, Display, TEXT("KAPTEST dombra")); } }
			// монстр жақында — фонарь «өліп» бара жатқандай жыпылықтайды
			if (P->Flashlight && P->Flashlight->Intensity > 0)
				P->Flashlight->SetIntensity(800.f * (FearSm > 0.55f && FMath::FRand() < 0.22f ? FMath::FRandRange(0.05f, 0.6f) : 1.f));
		}
		// «қауіп алдындағы тыныштық»: жасырын жүрген монстр жақын болса шегірткелер тынады, жел бәсеңдейді
		bool bLurk = false;
		if (P) for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (It->Faction == EKAPFaction::Monster && !It->bDead) { const float D = FVector::Dist(It->GetActorLocation(), P->GetActorLocation()); if (D > 2500.f && D < 7000.f) bLurk = true; }
		LurkK = FMath::FInterpTo(LurkK, bLurk ? 1.f : 0.f, Dt, 0.8f);
		Au->SetWind((Weather->Env.Storm * 0.9f + 0.1f) * (1.f - LurkK * 0.45f));
		Au->SetCrickets(Weather->Env.bNight && Weather->Env.Storm < 0.2f && Weather->Env.Rain < 0.2f ? 1.f - LurkK : 0.f);
		bool bFireNear = false;
		const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
		if (P) for (int32 i = 0; i < WM->Wells.Num(); i++)
			if ((Ss.Wells[i].State == EWellState::Fire || (Ss.Wells[i].State == EWellState::Fault && Ss.Wells[i].Fault == EFaultType::Fire)) && FVector2D::Distance(WM->Wells[i].PosM(), KAP::CM2M(P->GetActorLocation())) < 60) bFireNear = true;
		Au->SetFire(bFireNear ? 1.f : 0.f);
		const AKAPVehicleHandle* V = CurrentVehicle.Get();
		Au->SetEngine(V && V->CanDrive(), V ? V->SpeedKmh() / 3.6f : 0.f);
		if (V) Au->SetEngineLoad(V->Rpm01, V->Heat > 90 || V->Hp < 30 ? 0.18f : V->Heat > 75 ? 0.05f : 0.f);
	}
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Wind = Weather->Env.Wind;
}

void AKAPGameMode::Simulate(float Dt)
{
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const double Dh = Dt * 24.0 * TimeScale / D->Cfg("dayLengthSec", 2880);
	LastDh = Dh;
	Ss.Time += Dh;
	Weather->UpdateEnv(Ss, Dt, Dh);
	// найзағай электр ақауын тудыруы мүмкін
	if (Weather->Env.Lightning >= 0.99f)
	{
		FTimerHandle H; GetWorldTimerManager().SetTimer(H, [this]() { UKAPAudio::Get(this)->Play(EKAPSound::Thunder, 0.9f); }, 0.5f + Rng.FRand() * 1.5f, false);
		if (Ss.bSimActive && Rng.FRand() < 0.25f)
		{
			TArray<int32> C;
			for (int32 i = 0; i < Ss.Wells.Num(); i++) if (Ss.Wells[i].State == EWellState::Ok && Ss.Regions[WM->Wells[i].Row.Region]) C.Add(i);
			if (C.Num()) WM->StartFault(Ss, C[Rng.RandRange(0, C.Num() - 1)], EFaultType::Electric, TEXT("найзағай түсті"));
		}
		Weather->Env.Lightning = 0.98f;
	}
	WM->Simulate(Ss, Dh, Weather->Env.Temp, Weather->Env.Storm, Weather->Env.Rain);
	WM->UpdateBrigades(Ss, Dt, Dh, TimeScale);
	// жұмысшы NPC-лерінің визуалы
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
		if (It->WorkerIndex >= 0 && Ss.Workers.IsValidIndex(It->WorkerIndex))
		{
			const FKAPWorker& K = Ss.Workers[It->WorkerIndex];
			if (K.State == 4 && !It->bDead) It->Kill(false);
			It->bSick = K.State >= 2;
		}
	// жаңа аймаққа кіру — автосақтау
	if (AKAPCharacter* P = Player())
	{
		const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
		const int32 R = KAP::RegionOf(Pm.X, Pm.Y);
		if (!Ss.Visited[R] && Ss.bHasTablet) { Ss.Visited[R] = true; Toast(TEXT("Жаңа аймақ: ") + KAP::RegionName(R).ToString(), "info"); SetCheckpoint(); Save(true); }
	}
}

// =====================================================================
// Қауіп аймақтары, жолдар
// =====================================================================
void AKAPGameMode::RefreshDangerZones()
{
	Zones.Reset();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const FKAPSession& Ss = S();
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	Zones.Add({ L("bandit"), 260, Ss.Flags.Contains("banditCleared") ? 0.15 : 0.9 });
	Zones.Add({ L("ruins"), 210, 0.7 });
	for (FName N : { FName("nest1"), FName("nest2"), FName("nest3"), FName("nest4") }) Zones.Add({ L(N), 180, 0.6 });
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < WM->Wells.Num() && i < Ss.Wells.Num(); i++)
	{
		if (Ss.Wells[i].State == EWellState::Fire) Zones.Add({ WM->Wells[i].PosM(), 130, 0.65 });
		else if (Ss.Wells[i].State == EWellState::Leak) Zones.Add({ WM->Wells[i].PosM(), 90, 0.4 });
		else if (Ss.Wells[i].State == EWellState::Lost) Zones.Add({ WM->Wells[i].PosM(), 70, 0.25 });
	}
}
double AKAPGameMode::DangerAt(double X, double Y) const
{
	double D = 0;
	for (const FKAPDangerZone& Z : Zones)
	{
		const double Dd = FVector2D::Distance(FVector2D(X, Y), Z.P);
		if (Dd < Z.R) D = FMath::Max(D, Z.Level * (1 - Dd / Z.R * 0.6));
	}
	return FMath::Clamp(D + S().Mutation / 100.0 * 0.15, 0.0, 1.0);
}
double AKAPGameMode::DistToRoad(FVector2D P) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	double Best = 1e9;
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos, V = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, V) / FMath::Max(1e-6, V.SizeSquared()), 0.0, 1.0);
		Best = FMath::Min(Best, FVector2D::Distance(P, A + V * T));
	}
	return Best;
}
float AKAPGameMode::RadiationZoneAt(FVector2D P) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	float R = 0;
	for (FName N : { FName("nest1"), FName("nest2"), FName("nest3"), FName("nest4"), FName("ruins") })
		if (const FKAPLocationRow* L = D->Loc(N)) if (FVector2D::Distance(P, FVector2D(L->X, L->Y)) < 40) R += 0.15f;
	return R;
}

// =====================================================================
// HMM — дауылдағы монстрларды бақылау (Forward алгоритмі)
// =====================================================================
void AKAPGameMode::UpdateTrackers()
{
	const bool bActive = Weather->Env.Storm > 0.4f || Weather->Env.Fog > 0.6f;
	for (float& V : Heat) V = 0;
	bHeatActive = bActive; HeatCount = 0;
	AKAPCharacter* P = Player();
	if (!P) return;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
	{
		AKAPNPC* N = *It;
		if (N->bDead || N->Faction != EKAPFaction::Monster) continue;
		const FVector2D Nm = KAP::CM2M(N->GetActorLocation());
		if (!bActive || FVector2D::Distance(Nm, Pm) > 300) { N->Tracker.Reset(); continue; }
		if (!N->Tracker.IsValid()) { N->Tracker = MakeShared<KAPAI::FSoundHMM>(); N->Tracker->Init(1200, 64); N->Tracker->InitAround(Pm, 300); }
		N->Tracker->Predict(Pm);
		N->LastObs = KAPAI::FSoundHMM::Sample(Nm, Pm, Rng);
		N->Tracker->Update(N->LastObs, Pm);
		for (int32 k = 0; k < Heat.Num(); k++) Heat[k] += N->Tracker->B[k];
		HeatCount++;
	}
}

// =====================================================================
// Монстрлар режиссері
// =====================================================================
void AKAPGameMode::Director(float Dt)
{
	DirT -= Dt;
	if (DirT > 0) return;
	DirT = 2;
	FKAPSession& Ss = S();
	AKAPCharacter* P = Player();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!P || !Ss.bHasTablet) return;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	// боссар
	static const TCHAR* BossNames[4] = { TEXT("Ақ жал — алып қасқыр"), TEXT("Құм айдаһары"), TEXT("Көкжиек алыбы"), TEXT("Қаражар алыбы") };
	static const FVector2D BossPos[4] = { FVector2D(-700, 100), FVector2D(880, 100), FVector2D(960, -300), FVector2D(-850, -500) };
	static const ENPCKind BossKind[4] = { ENPCKind::Wolf, ENPCKind::Worm, ENPCKind::Irradiated, ENPCKind::Irradiated };
	for (int32 i = 0; i < 4; i++)
	{
		if (!Ss.BossAlive[i] || Bosses.Contains(i)) continue;
		if (FVector2D::Distance(Pm, BossPos[i]) < 240)
		{
			AKAPNPC* B = SpawnNPC((int32)BossKind[i], BossPos[i], BossNames[i], true, BossKind[i] == ENPCKind::Irradiated ? 2.3f : 2.6f, true);
			if (B) { B->BossId = FName(*FString::Printf(TEXT("boss%d"), i)); Bosses.Add(i, B); Toast(FString(TEXT("! Аймақ боссы жақын: ")) + BossNames[i], "danger"); }
		}
	}
	// бандит лагері
	if (!Ss.Flags.Contains("banditCleared") && !Ss.Flags.Contains("banditSpawned") && FVector2D::Distance(Pm, L("bandit")) < 330)
	{
		Ss.Flags.Add("banditSpawned", 1);
		for (int32 i = 0; i < 7; i++) if (AKAPNPC* B = SpawnNPC((int32)ENPCKind::Bandit, L("bandit") + FVector2D(Rng.FRandRange(-20, 20), Rng.FRandRange(-20, 20)), TEXT("Бандит"), true)) { B->bCampMember = true; B->PatrolR = 25; }
	}
	// мутацияға айналған жұмысшылар
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = Ss.PendingMutants.Num() - 1; i >= 0; i--)
	{
		const FVector2D W = WM->Wells[Ss.PendingMutants[i] - 1].PosM();
		if (FVector2D::Distance(Pm, W) > 200) continue;
		SpawnNPC((int32)ENPCKind::Irradiated, W + FVector2D(6, 6), TEXT("Сәулеленген адам"));
		Ss.PendingMutants.RemoveAt(i);
	}
	int32 Monsters = 0, Actors = 0, Bandits = 0;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) { Actors++; if (!It->bDead && It->Faction == EKAPFaction::Monster && !It->bBoss) Monsters++; if (!It->bDead && It->Faction == EKAPFaction::Bandit) Bandits++; }
	bool bNearNest = false;
	for (FName N : { FName("nest1"), FName("nest2"), FName("nest3"), FName("nest4") }) if (FVector2D::Distance(Pm, L(N)) < 260) bNearNest = true;
	bool bSafe = P->bIndoor;
	for (FName N : { FName("aksai"), FName("josaly"), FName("hq") }) if (FVector2D::Distance(Pm, L(N)) < 120) bSafe = true;
	float Target = D->Cfg("monsterBase", 4) * (Weather->Env.bNight ? D->Cfg("nightMul", 2.5f) : 1.f) * (1 + Ss.Mutation / 60.f) * (Weather->Env.Storm > 0.5f ? 1.5f : 1.f) + (bNearNest ? 4 : 0);
	if (!Ss.bSimActive) Target *= 0.35f;
	if (bSafe) Target *= 0.3f;
	// «аңдушы»: түнде көз шетінде (35–60°) алыстан пайда болады; қарасаң — жоғалады
	StalkerCd -= 2.f;
	if (Weather->Env.bNight && !bSafe && StalkerCd <= 0)
	{
		bool bHave = false;
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->bStalker) bHave = true;
		if (!bHave)
		{
			const float CamYaw = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetControlRotation().Yaw : 0.f;
			const float A = FMath::DegreesToRadians(CamYaw + (Rng.FRand() < 0.5f ? -1.f : 1.f) * Rng.FRandRange(35.f, 60.f));
			const FVector2D Sp = Pm + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rng.FRandRange(32.f, 45.f);
			if (AKAPNPC* St = SpawnNPC((int32)ENPCKind::Irradiated, Sp, TEXT("..."), false)) { St->bStalker = true; StalkerCd = Rng.FRandRange(90.f, 160.f); }
		}
	}
	DirRest -= 2.f;
	if (Monsters < Target && Actors < 230 && DirRest <= 0)
	{
		// спаун ойыншының көзінен тыс (артында ±70°), кемінде 110 м қашықтықта
		const float CamYaw = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetControlRotation().Yaw : 0.f;
		const float A = FMath::DegreesToRadians(CamYaw + 180.f + Rng.FRandRange(-70.f, 70.f)), Dd = Rng.FRandRange(110, 230);
		const FVector2D Sp(FMath::Clamp(Pm.X + FMath::Cos(A) * Dd, -1150.0, 1150.0), FMath::Clamp(Pm.Y + FMath::Sin(A) * Dd, -1150.0, 1150.0));
		bool bInVillage = false;
		for (FName N : { FName("aksai"), FName("josaly"), FName("hq") }) if (FVector2D::Distance(Sp, L(N)) < 90) bInVillage = true;
		if (!bInVillage)
		{
			const float R = Rng.FRand();
			if (R < 0.45f) { const int32 N = 3 + Rng.RandRange(0, 2); for (int32 i = 0; i < N; i++) SpawnNPC((int32)ENPCKind::Wolf, Sp + FVector2D(Rng.FRandRange(-4, 4), Rng.FRandRange(-4, 4)), TEXT("Мутант қасқыр")); if (Rng.FRand() < 0.6f) UKAPAudio::Get(this)->Play(EKAPSound::Howl, FMath::Clamp(1.f - Dd / 300.f, 0.15f, 0.6f)); }
			else if (R < 0.75f || DistToRoad(Sp) < 20) SpawnNPC((int32)ENPCKind::Irradiated, Sp, TEXT("Сәулеленген адам"));
			else SpawnNPC((int32)ENPCKind::Worm, Sp, TEXT("Құм құрты"));
		}
	}
	// жолдағы бандиттер (4-аймақ)
	if (Ss.bSimActive && !Ss.Flags.Contains("banditCleared") && KAP::RegionOf(Pm.X, Pm.Y) == 3 && Rng.FRand() < 0.05f && Bandits < 4)
	{
		const float A = Rng.FRandRange(0, UE_TWO_PI);
		for (int32 i = 0; i < 3; i++) if (AKAPNPC* B = SpawnNPC((int32)ENPCKind::Bandit, Pm + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 140 + FVector2D(i * 3, 0), TEXT("Жол тонаушы"))) B->PatrolR = 40;
		Radio(TEXT("Байқаңыз: маңайда бандиттер тобы жүр."), "warn");
	}
}

void AKAPGameMode::SpawnReinforcement(AKAPNPC* C)
{
	AKAPCharacter* P = Player();
	const float A = Rng.FRandRange(0, UE_TWO_PI);
	for (int32 i = 0; i < 2; i++)
		if (AKAPNPC* B = SpawnNPC((int32)ENPCKind::Bandit, KAP::CM2M(C->GetActorLocation()) + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 50 + FVector2D(i * 2, 0), TEXT("Бандит")))
			if (P) B->Alert(KAP::CM2M(P->GetActorLocation()), 20);
}

void AKAPGameMode::OnNPCAttackedByPlayer(AKAPNPC* N)
{
	FKAPSession& Ss = S();
	if (N->Faction == EKAPFaction::Militia || N->Faction == EKAPFaction::Guard) { Ss.RepMilitia = FMath::Max(-100.f, Ss.RepMilitia - 12); Provoked = 60; }
	if (N->Faction == EKAPFaction::Civil || N->Faction == EKAPFaction::Worker) { Ss.RepMilitia -= 25; Provoked = 60; }
}
void AKAPGameMode::Provoke(const FString& Msg)
{
	if (Provoked <= 0) Toast(Msg, "danger");
	Provoked = 90; S().RepMilitia = FMath::Max(-100.f, S().RepMilitia - 20);
}

void AKAPGameMode::OnActorKilled(AKAPNPC* N, bool bByPlayer)
{
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (bByPlayer) Ss.Kills++;
	ContractKill(N, bByPlayer);
	const FVector2D Nm = KAP::CM2M(N->GetActorLocation());
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	auto Drop = [&](TMap<FName, int32> Items)
	{
		AKAPInteractSpot* Sp = Builder->SpawnSpot("pickup", N->GetActorLocation() - FVector(0, 0, 60), 220, true);
		Sp->Items = Items;
	};
	if (N->bBoss)
	{
		const int32 I = FCString::Atoi(*N->BossId.ToString().RightChop(4));
		if (Ss.BossAlive.IsValidIndex(I)) Ss.BossAlive[I] = false;
		Ss.Mutation = FMath::Clamp(Ss.Mutation + D->Cfg("mutationPerBoss", -10), 0.f, 100.f);
		Radio(TEXT("Босс жойылды: ") + N->DisplayName + TEXT(". Мутация таралуы баяулады."), "ok");
		Drop({ { "medkit", 2 }, { "antidote", 2 }, { "545", 30 }, { "engine_part", 1 } });
	}
	if (N->Faction == EKAPFaction::Monster && bByPlayer)
	{
		if (QuestState("s_militia") == EQuestState::Active && FVector2D::Distance(Nm, L("aksai")) < 300)
		{
			int32& C = Ss.QuestData.FindOrAdd("s_militia");
			C++;
			if (C >= 6) CompleteSide("s_militia");
		}
		if (FVector2D::Distance(Nm, L("aksai")) < 200) Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 2);
	}
	if (N->Faction == EKAPFaction::Bandit)
	{
		if (Rng.FRand() < 0.7f) Drop(Rng.FRand() < 0.5f ? TMap<FName, int32>{ { "9mm", 8 + Rng.RandRange(0, 8) }, { "water", 1 } } : TMap<FName, int32>{ { "545", 15 }, { "pipe", 1 } });
		if (N->bCampMember)
		{
			bool bAny = false;
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (*It != N && It->bCampMember && !It->bDead) bAny = true;
			if (!bAny)
			{
				Ss.Flags.Add("banditCleared", 1);
				Radio(TEXT("Бандит лагері тазартылды! Жолдар қауіпсіз."), "ok");
				Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 20);
				AKAPInteractSpot* Sp = Builder->SpawnSpot("pickup", FVector(L("bandit").X * 100, L("bandit").Y * 100, Builder->Z(L("bandit")) + 20), 250, true);
				Sp->Items = { { "ak", 1 }, { "545", 60 }, { "molotov", 4 }, { "pump_part", 2 }, { "cable", 2 }, { "sealant", 1 } };
				if (QuestState("s_bandit") == EQuestState::Active) CompleteSide("s_bandit");
			}
		}
	}
	if ((N->Faction == EKAPFaction::Militia || N->Faction == EKAPFaction::Guard) && bByPlayer) Ss.RepMilitia = FMath::Max(-100.f, Ss.RepMilitia - 30);
}

// =====================================================================
// Ойыншы өлімі
// =====================================================================
void AKAPGameMode::OnPlayerDied()
{
	if (CurrentVehicle.IsValid()) ExitVehicle();
	bPlayerDead = true;
}
void AKAPGameMode::RespawnPlayer()
{
	bPlayerDead = false;
	FKAPSession& Ss = S();
	AKAPCharacter* P = Player();
	if (!P) return;
	P->Respawn(Ss.Checkpoint.IsNearlyZero() ? P->GetActorLocation() : Ss.Checkpoint);
	Provoked = 0;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (!It->bPersistent && FVector::Dist(It->GetActorLocation(), P->GetActorLocation()) < 12000.f) It->Destroy();
	Toast(TEXT("Соңғы чекпоинт. Уақыт пен ұңғымалар күйі өзгерген жоқ."), "info");
}

// =====================================================================
// Ұңғыма жөндеу
// =====================================================================
void AKAPGameMode::StartWellRepair(int32 Index)
{
	FKAPSession& Ss = S();
	const FKAPWellSave& W = Ss.Wells[Index];
	if (W.State != EWellState::Fault && W.State != EWellState::Leak) return;
	if (Index != 0 && !Ss.bSimActive && QuestState("m_fix1") != EQuestState::Done) { Toast(TEXT("Алдымен №1 ұңғыманы жөндеңіз"), "warn"); return; }
	TArray<FString> Steps = KAP::RepairSteps(W.Fault);
	if (W.State == EWellState::Leak) Steps.Insert(TEXT("valve"), 0);
	AKAPHUD::Get(this)->StartMinigame(FString::Printf(TEXT("№%d ұңғыма"), Index + 1), Steps, [this, Index](bool bOk)
	{
		if (!bOk) return;
		UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
		WM->RepairWell(S(), Index);
		OnWellRepaired(Index, false);
	});
}

void AKAPGameMode::OnWellRepaired(int32 Index, bool bByBrigade)
{
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UKAPAudio::Get(this)->Play(EKAPSound::RepairDone, 0.7f);
	if (!bByBrigade) Toast(FString::Printf(TEXT("№%d ұңғыма жөнделді! Жұмысшылар қауіпсіз."), Index + 1), "ok");
	if (!bByBrigade) AddMoney(250, FString::Printf(TEXT("QazUran: №%d ұңғыманы жөндегені үшін"), Index + 1));
	if (Index == 0 && QuestState("m_fix1") == EQuestState::Active)
	{
		CompleteQuest("m_fix1"); StartQuest("m_guard");
		const FKAPLocationRow* G = D->Loc("guard1");
		if (G) Player()->Navigator->SetTarget(FVector2D(G->X, G->Y), TEXT("Кеніш күзеті бекеті"));
		Subtitle(TEXT("Сорғы қайта жұмыс істеп тұр. Енді күзетпен сөйлесейін."), TEXT("Насос снова работает. Теперь поговорю с охраной."), TEXT("Сіз"), 4);
	}
	if (!Ss.bBrigadesUnlocked && Ss.WellsRepaired >= D->Cfg("brigadeUnlockWells", 3) && Ss.bSimActive)
	{
		Ss.bBrigadesUnlocked = true;
		Radio(TEXT("Кеніш: үш жөндеу бригадасы жұмысқа дайын. Планшеттегі «Бригадалар» бөлімінен кесте құрыңыз."), "info");
	}
	if (!Ss.bPermitAccess && Ss.WellsRepaired >= D->Cfg("accessPermitWells", 5))
	{
		Ss.bPermitAccess = true;
		Radio(TEXT("Күзет: сізге кеніш басқармасына кіру рұқсаты берілді. КамАЗ та сіздің қолыңызда."), "ok");
		for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "kamaz") V->State = "ok";
		StartQuest("m_hq");
	}
	if (!bByBrigade) { SetCheckpoint(); Save(true); }
}

// =====================================================================
// CSP
// =====================================================================
void AKAPGameMode::RunCSP(bool bBT)
{
	TArray<KAPAI::FCSPBrigade> B; TArray<KAPAI::FCSPWell> W; double Hpm = 0;
	GetWorld()->GetSubsystem<UWellManagerSubsystem>()->BuildCSPInput(S(), B, W, Hpm);
	CSPResult = bBT ? KAPAI::SolveBacktracking(B, W, Hpm, 0.6) : KAPAI::SolveHillClimb(B, W, Hpm, 0.6, 25, Rng.RandRange(0, 100000));
	CSPBrigadeIds.Reset();
	for (const KAPAI::FCSPBrigade& X : B) CSPBrigadeIds.Add(X.Id);
}
void AKAPGameMode::DispatchCSP()
{
	if (!CSPResult.IsSet()) return;
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < CSPBrigadeIds.Num(); i++) if (CSPResult->Assign.IsValidIndex(i) && CSPResult->Assign[i] > 0) WM->Dispatch(S(), CSPBrigadeIds[i], CSPResult->Assign[i]);
	CSPResult.Reset();
}

// =====================================================================
// Ұйқы, киім
// =====================================================================
void AKAPGameMode::UseWearToggle(FName Id)
{
	FKAPSession& Ss = S();
	const FString N = UKAPDataSubsystem::Get(this)->ItemName(Id);
	if (Ss.Wear.Contains(Id)) { Ss.Wear.Remove(Id); Toast(N + TEXT(" шешілді"), "info"); }
	else { Ss.Wear.Add(Id); Toast(N + TEXT(" киілді"), "info"); }
}

void AKAPGameMode::Sleep()
{
	FKAPSession& Ss = S();
	AKAPCharacter* P = Player();
	if (!Ss.bSimActive) { P->SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), 100); Toast(TEXT("Демалдыңыз"), "ok"); return; }
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < 72; i++) { const double Dh = 6.0 / 72.0; Ss.Time += Dh; Weather->UpdateEnv(Ss, 0.1f, Dh); WM->Simulate(Ss, Dh, Weather->Env.Temp, Weather->Env.Storm, Weather->Env.Rain); WM->UpdateBrigades(Ss, 10.0, Dh, 1.f); }
	P->SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), 100);
	P->SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), P->GetHp() + 30);
	P->SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), P->GetWater() - 20);
	P->SetAttr(UKAPSurvivalAttributeSet::GetBodyTempAttribute(), 36.6f);
	SetCheckpoint();
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->Fade = 1.f;
	Save(true);
	Toast(TEXT("6 сағат ұйықтадыңыз. Автосақтау."), "info");
}

// =====================================================================
// Мұнаралар, жәшіктер, әрекет нүктелері
// =====================================================================
void AKAPGameMode::ActivateTower(FName Id, int32 Region)
{
	FKAPSession& Ss = S();
	Ss.Towers.Add(Id);
	Ss.Regions[Region] = true;
	if (TWeakObjectPtr<AKAPStructure>* T = Builder->Places.Find(Id)) if (T->IsValid() && (*T)->Blinker) KAPVis::Apply(Cast<UMaterialInstanceDynamic>((*T)->Blinker->GetMaterial(0)), FKAPMat::Glow(FLinearColor(0, 1, 0.3f), 80));
	Radio(TEXT("Сигнал мұнарасы қосылды: ") + KAP::RegionName(Region).ToString() + TEXT(" картада ашылды."), "ok");
	if (Ss.Towers.Num() >= 3) CompleteQuest("m_towers");
	if (Region == 2) StartQuest("s_ruins");
	SetCheckpoint();
	Save(true);
}

static TMap<FName, int32> LootTable(const FString& T)
{
	if (T == TEXT("wagon0")) return { { "medkit", 1 }, { "water", 2 }, { "pump_part", 1 }, { "dosimeter", 1 }, { "flaregun", 1 }, { "flare_ammo", 4 }, { "cable", 1 } };
	if (T.StartsWith(TEXT("wagon"))) return { { "water", 2 }, { "food", 1 }, { "bandage", 1 }, { "pipe", 1 }, { "cable", 1 }, { "iodine", 2 } };
	if (T == TEXT("aksai") || T == TEXT("josaly")) return { { "water", 2 }, { "food", 2 }, { "pump_part", 1 }, { "sealant", 1 }, { "extinguisher", 1 }, { "fuel", 1 } };
	if (T == TEXT("hq")) return { { "medkit", 2 }, { "antidote", 2 }, { "suit", 1 }, { "pipe", 2 }, { "cable", 2 }, { "sealant", 2 }, { "extinguisher", 2 }, { "pump_part", 2 }, { "545", 60 }, { "9mm", 24 } };
	if (T == TEXT("res")) return { { "water", 3 }, { "fuel", 2 }, { "coat", 1 }, { "tire", 1 }, { "engine_part", 1 } };
	if (T == TEXT("tower3")) return { { "sniper", 1 }, { "762", 15 }, { "hat", 1 } };
	const int32 N = FCString::Atoi(*T.RightChop(4));
	return (N % 2) ? TMap<FName, int32>{ { "pipe", 1 }, { "cable", 1 }, { "sealant", 1 } } : TMap<FName, int32>{ { "pump_part", 1 }, { "extinguisher", 1 }, { "water", 1 } };
}

void AKAPGameMode::OpenCrate(FName Id)
{
	FKAPSession& Ss = S();
	const double* Next = Ss.CrateRefill.Find(Id);
	if (Next && Ss.Time < *Next) { Toast(TEXT("Жәшік бос. Келесі толтыру: ") + KAP::FmtDur(*Next - Ss.Time) + TEXT(" кейін"), "info"); return; }
	const FString T = Id.ToString();
	Ss.CrateRefill.Add(Id, (T == TEXT("wagon0") || T == TEXT("tower3")) ? 1e12 : Ss.Time + 24);
	Toast(TEXT("Жәшіктен алынды: ") + GiveItems(LootTable(T)), "ok");
	UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.6f);
}

FString AKAPGameMode::SpotText(const AKAPInteractSpot* Sp) const
{
	{ const FString E = EconomySpotText(Sp); if (!E.IsEmpty()) return E; }
	const FKAPSession& Ss = S();
	const FString A = Sp->Action.ToString();
	if (A == TEXT("tablet")) return Ss.bHasTablet ? FString() : TEXT("Планшетті алу");
	if (A.StartsWith(TEXT("bed:"))) return Ss.bHasTablet ? (Ss.bSimActive ? TEXT("Ұйықтау (6 сағ) · автосақтау") : TEXT("Демалу")) : FString();
	if (A == TEXT("radiator")) return QuestState("m_radiator") == EQuestState::Active ? TEXT("Радиаторды шешіп алу") : FString();
	if (A.StartsWith(TEXT("crate:"))) { const double* N = Ss.CrateRefill.Find(FName(*A.RightChop(6))); return (N && *N > 1e11) ? FString() : TEXT("Жәшікті ашу"); }
	if (A.StartsWith(TEXT("tower:"))) return (Ss.Towers.Contains(FName(*A.RightChop(6))) || !Ss.bHasTablet) ? FString() : TEXT("Сигнал мұнарасын қосу");
	if (A == TEXT("res_pump")) return QuestState("s_water") == EQuestState::Active ? TEXT("Сорғыны жөндеу") : QuestState("s_water") == EQuestState::Done ? TEXT("Су ішу (толық)") : FString();
	if (A == TEXT("hq_gate")) return Ss.Flags.Contains("gateOpen") ? FString() : Ss.bPermitAccess ? TEXT("Қақпаны ашу") : TEXT("Қақпа жабық — кіру рұқсаты керек");
	if (A == TEXT("hq_desk")) return QuestState("m_cause") == EQuestState::Active ? TEXT("Апат журналын оқу") : FString();
	if (A == TEXT("hq_radio")) return QuestState("m_report") == EQuestState::Active ? TEXT("Аймақ туралы есеп жіберу (финал)") : FString();
	if (A == TEXT("pickup")) return TEXT("Алу");
	return FString();
}

void AKAPGameMode::SpotUse(AKAPInteractSpot* Sp)
{
	if (EconomySpotUse(Sp)) return;
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const FString A = Sp->Action.ToString();
	AKAPCharacter* P = Player();
	if (A == TEXT("tablet") && !Ss.bHasTablet)
	{
		Ss.bHasTablet = true;
		if (Sp->Visual) Sp->SetActorHiddenInGame(true);
		CompleteQuest("m_tablet"); StartQuest("m_wait");
		UKAPAudio::Get(this)->Beep(true);
		Subtitle(TEXT("Планшет қосылды. QazUran кеніш желісіне қосылу..."), TEXT("Планшет включён. Подключение к сети рудника QazUran..."), TEXT("Планшет"), 5);
		Toast(TEXT("Tab — планшетті ашу. Ұңғымалар мониторы, жұмысшылар, карта, ауа райы — бәрі осында."), "info");
		WaitT = 32;
	}
	else if (A.StartsWith(TEXT("bed:"))) Sleep();
	else if (A == TEXT("radiator") && QuestState("m_radiator") == EQuestState::Active)
	{
		Ss.Inv.FindOrAdd("radiator") = 1;
		UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.6f);
		CompleteQuest("m_radiator"); StartQuest("m_fixuaz");
		for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "bukhanka") P->Navigator->SetTarget(KAP::CM2M(V->Pawn->GetActorLocation()), TEXT("УАЗ «Буханка»"));
		Subtitle(TEXT("Ескі, бірақ жарамды. Енді Буханкаға."), TEXT("Старый, но рабочий. Теперь к «Буханке»."), TEXT("Сіз"), 4);
	}
	else if (A.StartsWith(TEXT("crate:"))) OpenCrate(FName(*A.RightChop(6)));
	else if (A.StartsWith(TEXT("tower:")))
	{
		const FName Id(*A.RightChop(6));
		ActivateTower(Id, Id == "tower1" ? 1 : Id == "tower2" ? 2 : 3);
	}
	else if (A == TEXT("res_pump"))
	{
		if (QuestState("s_water") == EQuestState::Done) { P->SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), 100); Toast(TEXT("Су толтырылды"), "ok"); return; }
		AKAPHUD::Get(this)->StartMinigame(TEXT("Су қоймасы сорғысы"), { TEXT("valve"), TEXT("part:pump_part"), TEXT("pressure") }, [this](bool bOk) { if (bOk) CompleteSide("s_water"); });
	}
	else if (A == TEXT("hq_gate"))
	{
		if (!Ss.bPermitAccess) { Toast(FString::Printf(TEXT("Кеніш басқармасына кіру рұқсаты жоқ (%d/%d ұңғыма)"), Ss.WellsRepaired, (int32)D->Cfg("accessPermitWells", 5)), "warn"); return; }
		Ss.Flags.Add("gateOpen", 1);
		if (AKAPStructure* G = Builder->HQGate.Get()) { G->SetActorHiddenInGame(true); G->SetActorEnableCollision(false); }
		Toast(TEXT("Қақпа ашылды"), "ok");
	}
	else if (A == TEXT("hq_desk") && QuestState("m_cause") == EQuestState::Active)
	{
		Ss.Inv.FindOrAdd("logbook") = 1;
		Dialog(TEXT("Апат журналы · QazUran (ойдан шығарылған құжат)"),
			TEXT("14 маусым. Шығымды арттыру үшін ерітіндідегі қышқыл концентрациясы нормадан 40%-ға көтерілді. Бұйрық — басқармадан.\n")
			TEXT("21 маусым. №7, №13 ұңғымаларда қысым датчиктері ескерту берді. Тексеру «жоспар орындалғанша» кейінге қалдырылды.\n")
			TEXT("2 шілде. Бақылау ұңғымасының қаптамасы жарылды. Ерітінді жерасты горизонтынан тыс шықты. Хабарлау тоқтатылды.\n")
			TEXT("3 шілде. Апат. Басшылық эвакуацияланды. Жұмысшыларға хабарланбады.\n\nСебеп анық: ескертулер еленбеді, адамдардың өмірі жоспардан төмен қойылды."),
			{ { TEXT("Бұны бүкіл аймақ білуі керек."), [this]() { CompleteQuest("m_cause"); StartQuest("m_report"); Toast(TEXT("Басқарма радиосынан есеп жіберуге болады — бұл финалды бастайды"), "info"); }, false } });
	}
	else if (A == TEXT("hq_radio") && QuestState("m_report") == EQuestState::Active) SendReport();
	else if (A == TEXT("pickup"))
	{
		Toast(TEXT("Алынды: ") + GiveItems(Sp->Items), "ok");
		const bool bPart = Sp->Items.Contains("car_part");
		UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.6f);
		Sp->Destroy();
		if (bPart && Ss.Inv.FindRef("car_part") >= 4) Toast(TEXT("Барлық бөлшек жиналды — Жосалыдағы Сағат ақсақалға апарыңыз"), "ok");
	}
}

// =====================================================================
// Көліктер
// =====================================================================
void AKAPGameMode::VehicleInteract(AKAPVehicleHandle* V)
{
	FKAPSession& Ss = S();
	if (V->State == "locked")
	{
		Toast(V->Id == "btr" ? TEXT("БТР — жасақтармен одақтас болғанда (қатынас ≥ 50) Төлеген ақсақал береді") : V->Id == "kamaz" ? TEXT("КамАЗ — басқармаға кіру рұқсатымен ашылады") : TEXT("Су тасушы — су қоймасын қалпына келтіргенде ашылады"), "info");
		return;
	}
	if (V->State == "radiator")
	{
		if (QuestState("m_fixuaz") != EQuestState::Active) { Toast(TEXT("Радиатор қажет"), "warn"); return; }
		AKAPHUD::Get(this)->StartMinigame(TEXT("УАЗ «Буханка» радиаторы"), { TEXT("part:radiator"), TEXT("pressure") }, [this, V](bool bOk)
		{
			if (!bOk) return;
			V->State = "ok";
			S().Regions[0] = true;
			CompleteQuest("m_fixuaz"); StartQuest("m_well1");
			const FKAPWellRuntime& W = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0];
			Player()->Navigator->SetTarget(W.PosM(), TEXT("№1 ұңғыма"));
			Radio(TEXT("Планшетте 1-аймақтың картасы ашылды. Навигатор №1 ұңғымаға жол көрсетеді."), "ok");
			Toast(TEXT("Көлікке E арқылы мініңіз. W/S — газ/тежеу, A/D — руль, Space — қол тежегіш, E — түсу."), "info");
		});
		return;
	}
	if (V->State == "parts") { Toast(FString::Printf(TEXT("Нивaға 4 бөлшек керек (%d/4)"), Ss.Inv.FindRef("car_part")), "info"); return; }
	if (V->State == "broken") { AKAPHUD::Get(this)->StartMinigame(V->Def.Title + TEXT(" двигателі"), { TEXT("part:engine_part"), TEXT("wires") }, [this, V](bool bOk) { if (bOk) { V->State = "ok"; V->Hp = FMath::Max(V->Hp, 70.f); Toast(TEXT("Двигатель жөнделді"), "ok"); } }); return; }
	if (V->Flats && Ss.Inv.FindRef("tire") > 0) { AKAPHUD::Get(this)->StartMinigame(TEXT("Дөңгелек ауыстыру"), { TEXT("part:tire") }, [this, V](bool bOk) { if (bOk) { V->Flats = FMath::Max(0, V->Flats - 1); Toast(TEXT("Дөңгелек ауыстырылды"), "ok"); } }); return; }
	if (V->Fuel < V->Def.FuelCap * 0.5f && Ss.Inv.FindRef("fuel") > 0) { TakeItem("fuel", 1); V->Fuel = FMath::Min(V->Def.FuelCap, V->Fuel + 30); Toast(TEXT("+30 л жанармай"), "ok"); return; }
	EnterVehicle(V);
}

void AKAPGameMode::EnterVehicle(AKAPVehicleHandle* V)
{
	AKAPCharacter* P = Player();
	APlayerController* PC = P ? Cast<APlayerController>(P->GetController()) : nullptr;
	if (!PC || !V->Pawn) return;
	CurrentVehicle = V; V->bDriven = true;
	P->SetActorHiddenInGame(true); P->SetActorEnableCollision(false);
	P->GetCharacterMovement()->DisableMovement();
	P->Flashlight->SetVisibility(false);
	PC->Possess(V->Pawn);
	V->Pawn->DoHandbrakeStop();
	if (V->Id == "water" && P->GetWater() < 95) { P->SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), 100); Toast(TEXT("Су тасушыдан су ішілді"), "ok"); }
	if (V->Fuel < 3) Toast(TEXT("Жанармай жоқ! Канистр табыңыз."), "warn");
}

void AKAPGameMode::ExitVehicle()
{
	AKAPVehicleHandle* V = CurrentVehicle.Get();
	AKAPCharacter* P = Player();
	if (!V || !P) return;
	if (V->SpeedKmh() > 12) { Toast(TEXT("Алдымен тоқтаңыз"), "warn"); return; }
	APlayerController* PC = Cast<APlayerController>(V->Pawn->GetController());
	V->Pawn->DoThrottle(0); V->Pawn->DoHandbrakeStart();
	V->bDriven = false;
	const FVector Side = V->Pawn->GetActorLocation() - V->Pawn->GetActorRightVector() * 260.f + FVector(0, 0, 60);
	P->SetActorLocation(Side, false, nullptr, ETeleportType::TeleportPhysics);
	P->SetActorHiddenInGame(false); P->SetActorEnableCollision(true);
	P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	P->Flashlight->SetVisibility(true);
	if (PC) { PC->Possess(P); PC->SetControlRotation(FRotator(0, V->Pawn->GetActorRotation().Yaw - 90, 0)); }
	CurrentVehicle = nullptr;
}

// =====================================================================
// Сюжет тригерлері
// =====================================================================
void AKAPGameMode::StoryTick(float Dt)
{
	FKAPSession& Ss = S();
	AKAPCharacter* P = Player();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!P) return;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	if (QuestState("m_shelter") == EQuestState::Active && P->bIndoor && FVector2D::Distance(Pm, L("wagon0")) < 10)
	{
		CompleteQuest("m_shelter"); StartQuest("m_tablet");
		Subtitle(TEXT("Осы жерде күте тұрайын. Үстелде бір нәрсе жатыр..."), TEXT("Пережду здесь. На столе что-то лежит..."), TEXT("Сіз"), 5);
	}
	if (QuestState("m_wait") == EQuestState::Active)
	{
		const float Old = WaitT;
		WaitT -= Dt;
		if (Old >= 20 && WaitT < 20) Radio(TEXT("Планшет: дауыл 1 сағаттан кейін басылады. Вагоннан шықпаңыз."), "info");
		if (WaitT <= 0)
		{
			Weather->bOverride = false;
			Ss.Weather.Reset();
			FKAPWeatherSeg Seg; Seg.Type = EWeatherType::Clear; Seg.Start = Ss.Time - 0.2; Seg.Dur = 5; Ss.Weather.Add(Seg);
			CompleteQuest("m_wait"); StartQuest("m_radiator");
			P->Navigator->SetTarget(L("oldcar"), TEXT("Ескі көлік қаңқасы"));
			Radio(TEXT("Дауыл басылды. Буханкаға радиатор керек — 60 м жердегі ескі көлік қаңқасын тексеріңіз."), "ok");
		}
	}
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (QuestState("m_well1") == EQuestState::Active && FVector2D::Distance(Pm, WM->Wells[0].PosM()) < 18)
	{
		CompleteQuest("m_well1"); StartQuest("m_fix1");
		Subtitle(TEXT("Сорғы тоқтаған. Бөлшек вагондағы жәшікте болуы керек еді... Пульттен бастайын."), TEXT("Насос встал. Деталь должна быть в ящике в вагоне... Начну с пульта."), TEXT("Сіз"), 6);
		if (Ss.Inv.FindRef("pump_part") <= 0) Toast(TEXT("Сорғы бөлшегі керек — вагон-үйдегі жәшікте бар"), "warn");
	}
	if (QuestState("m_hq") == EQuestState::Active && FVector2D::Distance(Pm, L("hq")) < 45)
	{
		CompleteQuest("m_hq"); StartQuest("m_cause");
		Subtitle(TEXT("Басқарма бос... Құжаттар кеңседе болуы керек."), TEXT("Управление пустует... Документы должны быть в конторе."), TEXT("Сіз"), 5);
	}
	if (Ss.bSimActive && !Ss.Flags.Contains("waterRadio"))
	{
		WaterRadioT += Dt;
		if (WaterRadioT > 50) { Ss.Flags.Add("waterRadio", 1); Radio(TEXT("Кеніш: су қоймасының сорғысы істен шықты. Жөндесеңіз — су тасушы көлік беріледі."), "info"); StartQuest("s_water"); }
	}
	if (TWeakObjectPtr<AKAPNPC>* N = NamedNPC.Find("nurlan"))
		if (AKAPNPC* Nn = N->Get())
			if (Nn->bFollowPlayer && FVector2D::Distance(KAP::CM2M(Nn->GetActorLocation()), L("josaly")) < 50)
			{
				Nn->bFollowPlayer = false; Nn->GoTo = L("josaly"); Nn->bVanishOnArrive = true; Nn->bPersistent = false;
				Ss.Flags.Add("missingFound", 1);
				CompleteSide("s_missing");
			}
}

// =====================================================================
// Диалогтар
// =====================================================================
void AKAPGameMode::Talk(AKAPNPC* N)
{
	TalkNPC = N;
	if (N && N->IsChild())
	{
		const TCHAR* Line = N->bSick ? TEXT("Бүгін тағы әлсіз болып тұрмын... Анам дәрігер келеді деді. Терезеден достарымның ойнап жүргенін көріп отырмын.") : N->Kind == ENPCKind::Girl ? TEXT("Біз осы ауылда тұрамыз. Інім бүгін сыртқа шыға алмады. Тезірек жазылса екен...") : TEXT("Ауылдан алысқа баруға болмайды дейді. Ағалар ұңғымаларды жөндеп болғанда, далада еркін ойнаймыз ба?");
		Dialog(N->DisplayName, Line, { { TEXT("Аман бол. Өзіңді күт."), nullptr, false } });
		return;
	}
	if (EconomyTalk(N)) return; // полиция, сатушы, әскер (KAPEconomy.cpp)
	FKAPSession& Ss = S();
	const int32 Day = FMath::FloorToInt32(Ss.Time / 24.0);
	auto PartsGift = [this, Day, &Ss](FName Who) -> FKAPDialogOption
	{
		const FName Key(*(Who.ToString() + TEXT("Day")));
		return { TEXT("Бөлшек сұрау (күніне бір рет)"), [this, Key, Day]()
			{
				S().Flags.Add(Key, Day);
				static const FName Pool[7] = { "pipe", "cable", "sealant", "pump_part", "extinguisher", "water", "fuel" };
				TMap<FName, int32> It; for (int32 i = 0; i < 2; i++) It.FindOrAdd(Pool[Rng.RandRange(0, 6)])++;
				Toast(TEXT("Берілді: ") + GiveItems(It), "ok");
			}, Ss.Flags.Contains(Key) && (int32)Ss.Flags[Key] == Day };
	};
	const FString Name = N->DisplayName;
	const FName T = N->TalkId;
	if (T == "bekzat")
	{
		if (QuestState("m_guard") == EQuestState::Active)
		{
			Dialog(Name, TEXT("Сен жаңа инженерсің бе? №1-ді жөндедің — көрдім. Жарайсың.\nЖағдай нашар: даланы мутанттар мен бандиттер басып алды. Қарусыз жүру — өлім.\nМә, қару рұқсатын планшетіңе жаздым. Мына ПМ-ді ал. Бірақ ауылдарда абай бол — жасақтар сенімсіз адамды ұнатпайды."),
				{ { TEXT("Рахмет. Ұңғымалардың жағдайы қандай?"), [this, Name]()
				{
					FKAPSession& S2 = S();
					S2.bPermitWeapon = true;
					Toast(TEXT("Қару рұқсаты берілді! ") + GiveItems({ { "pm", 1 }, { "9mm", 32 } }), "ok");
					S2.Mag.Add("pm", 8);
					CompleteQuest("m_guard"); StartQuest("m_wells"); StartQuest("m_towers");
					S2.bSimActive = true;
					Player()->Navigator->Clear();
					SetCheckpoint(); Save(true);
					Dialog(Name, TEXT("Жиырма бір ұңғыма, бәрі планшетте. Байқа: бұзылған ұңғыма уақыт өте нашарлайды — алдымен ағу, сосын өрт. Ағып жатқан жерде жұмысшылар сәулеленеді.\nБасқа аймақтарды көру үшін сигнал мұнараларын қос. Бес ұңғыманы жөндесең, басқармаға кіру рұқсатын аласың.\nАқсайдағы Төлеген ақсақалға соқ — ол көмектеседі."), { { TEXT("Түсіндім."), nullptr, false } });
				}, false } });
		}
		else Dialog(Name, Ss.bPermitAccess ? TEXT("Басқармаға рұқсатың бар. Апаттың себебін анықта — бәріміз соны күтеміз.") : FString::Printf(TEXT("Ұңғымаларды жөндей бер. Бесеуін жөндесең, басқармаға рұқсат беремін. (%d/5)"), Ss.WellsRepaired), {});
		return;
	}
	if (T == "toleg")
	{
		TArray<FKAPDialogOption> O;
		if (!Ss.Quests.Contains("s_militia")) O.Add({ TEXT("Ауылға көмек керек пе?"), [this, Name]() { StartQuest("s_militia"); Dialog(Name, TEXT("Түнде мутант қасқырлар ауылға жақындап жүр. Жігіттеріміз шаршады. Ауыл маңындағы алтауын жойсаң, сені өз адамымыз деп танимыз."), { { TEXT("Жасаймын."), nullptr, false } }); }, false });
		if (!Ss.Quests.Contains("s_bandit")) O.Add({ TEXT("Бандиттер туралы не білесіз?"), [this, Name]() { StartQuest("s_bandit"); Dialog(Name, TEXT("Қаражардың солтүстік-батысында лагерь құрған. Жолда тонайды, ұңғымалардан бөлшек ұрлайды. Олар топпен шабуылдайды, жасырынады — абай бол."), { { TEXT("Лагерьді тазартамын."), nullptr, false } }); }, false });
		for (auto& V : Builder->Vehicles)
			if (V.IsValid() && V->Id == "btr" && V->State == "locked" && Ss.RepMilitia >= 50)
				O.Add({ TEXT("Бронетранспортерді бере аласыз ба?"), [this]() { for (auto& X : Builder->Vehicles) if (X.IsValid() && X->Id == "btr") X->State = "ok"; Toast(TEXT("БТР сіздің қолыңызда!"), "ok"); }, false });
		O.Add(PartsGift("toleg"));
		O.Add({ TEXT("Сау болыңыз."), nullptr, false });
		if (Ss.RepMilitia <= -30) Dialog(Name, TEXT("Сенімен сөйлесетін ештеңем жоқ."), {});
		else Dialog(Name, Ss.RepMilitia >= 50 ? TEXT("Қош келдің, балам. Ауыл саған қарыздар.") : TEXT("Ассалаумағалейкум, инженер. Біздің Ерлан да сол кеніште, №3-те істейді... Анасы күнде жолға қарайды."), O);
		return;
	}
	if (T == "sagat")
	{
		TArray<FKAPDialogOption> O;
		if (!Ss.Quests.Contains("s_missing")) O.Add({ TEXT("Не болды, ақсақал?"), [this, Name]() { StartQuest("s_missing"); Dialog(Name, TEXT("Нұрлан Есенов кеше мотоциклмен оңтүстіктегі құмды төбелерге кетіп, қайтпады. Сондай жерде мутант құрттар бар..."), { { TEXT("Іздеп көремін."), nullptr, false } }); }, false });
		if (!Ss.Quests.Contains("s_parts")) O.Add({ TEXT("Нива сіздікі ме?"), [this, Name]() { StartQuest("s_parts"); Dialog(Name, TEXT("Иә, бірақ бөлшектері жоқ. Төрт бөлшек дала бойына шашылып қалды — бандиттер тонаған. Тапсаң, Нива сенікі."), { { TEXT("Табамын."), nullptr, false } }); }, false });
		if (QuestState("s_parts") == EQuestState::Active && Ss.Inv.FindRef("car_part") >= 4) O.Add({ TEXT("Бөлшектерді әкелдім."), [this]() { S().Inv.Add("car_part", 0); CompleteSide("s_parts"); }, false });
		O.Add(PartsGift("sagat"));
		O.Add({ TEXT("Сау болыңыз."), nullptr, false });
		Dialog(Name, TEXT("Жосалыға қош келдің. Ауыл аман, бірақ қашанғы?.."), O);
		return;
	}
	if (T == "worker" && Ss.Workers.IsValidIndex(N->WorkerIndex))
	{
		FKAPWorker& K = Ss.Workers[N->WorkerIndex];
		static const TCHAR* St[] = { TEXT("сау"), TEXT("уланған"), TEXT("ауру"), TEXT("өлім алдында"), TEXT("қайтыс болды") };
		FString Tx = FString::Printf(TEXT("%s, %d жаста. %s.\nДоза: %d мЗв · күйі: %s."), *K.Name, K.Age, *K.Family, FMath::RoundToInt32(K.Dose), St[FMath::Clamp(K.State, 0, 4)]);
		Tx += K.State == 0 ? TEXT("\n— Жағдайымыз жақсы, инженер. Тек ұңғымаларды тастап кетпеңіз.") : K.State == 1 ? TEXT("\n— Басым айналып тұр... жүрегім айнып жүр.") : K.State == 2 ? TEXT("\n— Тұра алмай жатырмын... Көмектесіңізші.") : TEXT("\n— ...(әрең дем алып жатыр)");
		if (K.bSpecial) Tx += TEXT("\n— Анам Ақсайда күтіп отыр. Оқуымды бітіріп, оған үй салып беремін деп едім...");
		const int32 WI = N->WorkerIndex;
		TWeakObjectPtr<AKAPNPC> WN = N;
		TArray<FKAPDialogOption> O;
		O.Add({ FString::Printf(TEXT("Йод таблеткасын беру (%d)"), Ss.Inv.FindRef("iodine")), [this, WI]() { TakeItem("iodine", 1); S().Workers[WI].IodineUntil = S().Time + 12; Toast(S().Workers[WI].Name + TEXT(" йод қабылдады — 12 сағат доза жиналуы баяулайды"), "ok"); }, Ss.Inv.FindRef("iodine") <= 0 });
		O.Add({ FString::Printf(TEXT("Антидот беру (%d)"), Ss.Inv.FindRef("antidote")), [this, WI]() { TakeItem("antidote", 1); FKAPWorker& K2 = S().Workers[WI]; K2.Dose = FMath::Max(0.f, K2.Dose - 120); K2.State = UWellManagerSubsystem::HealthState(K2.Dose, UKAPDataSubsystem::Get(this)); Toast(K2.Name + TEXT(": доза −120 мЗв"), "ok"); }, Ss.Inv.FindRef("antidote") <= 0 });
		O.Add({ TEXT("Эвакуациялау (ауылға жіберу)"), [this, WI, WN]()
		{
			FKAPWorker& K2 = S().Workers[WI];
			UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
			const FVector2D Wp = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[K2.WellId - 1].PosM();
			const FKAPLocationRow* A = D->Loc("aksai"); const FKAPLocationRow* J = D->Loc("josaly");
			const bool bA = FVector2D::Distance(Wp, FVector2D(A->X, A->Y)) < FVector2D::Distance(Wp, FVector2D(J->X, J->Y));
			K2.EvacTo = bA ? TEXT("Ақсай") : TEXT("Жосалы");
			if (AKAPNPC* Np = WN.Get()) { const FVector2D T2 = bA ? FVector2D(A->X, A->Y) : FVector2D(J->X, J->Y); Np->GoTo = KAP::CM2M(Np->GetActorLocation()) + (T2 - KAP::CM2M(Np->GetActorLocation())) * 0.2; Np->bVanishOnArrive = true; Np->TalkId = NAME_None; }
			Toast(K2.Name + TEXT(" ") + K2.EvacTo + TEXT(" ауылына эвакуацияланды"), "ok");
		}, false });
		if (!Ss.Quests.Contains("s_missing") && GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[K.WellId - 1].Row.Region == 1)
			O.Add({ TEXT("Не естідіңіз?"), [this, K]() { StartQuest("s_missing"); Dialog(K.Name, TEXT("Біздің Нұрлан Есенов оңтүстіктегі құмды төбелерге кетіп жоғалды. Тапсаңыз екен..."), { { TEXT("Іздеймін."), nullptr, false } }); }, false });
		O.Add({ TEXT("Шыдаңыз. Мен ұңғыманы жөндеймін."), nullptr, false });
		Dialog(Name, Tx, O);
		return;
	}
	if (T == "nurlan")
	{
		TWeakObjectPtr<AKAPNPC> WN = N;
		if (!N->bFollowPlayer) Dialog(Name, TEXT("Құдайға шүкір, адам келді! Аяғым жараланған, мотоцикл сынып қалды... Жосалыға жеткізіп салыңызшы."), { { TEXT("Соңымнан еріңіз."), [this, WN]() { if (AKAPNPC* X = WN.Get()) { X->bFollowPlayer = true; X->bSick = false; } Toast(TEXT("Нұрланды Жосалыға дейін жеткізіңіз"), "info"); }, false } });
		else Dialog(Name, TEXT("Мен соңыңыздамын."), {});
		return;
	}
	if (T == "survivor")
	{
		TWeakObjectPtr<AKAPNPC> WN = N;
		Dialog(Name, TEXT("Сіз... шын адамсыз ба? Біз осы қирандының ішінде үш күн жасырындық. Анау алып... түнде келеді."), { { TEXT("Жосалыға барыңыз, жол таза."), [this, WN]()
		{
			AKAPNPC* X = WN.Get(); if (!X) return;
			FKAPSession& S2 = S();
			S2.Flags.Add(FName(*FString::Printf(TEXT("surv%d"), X->SurvivorId)), 1);
			int32& C = S2.QuestData.FindOrAdd("s_ruins"); C++;
			const FKAPLocationRow* J = UKAPDataSubsystem::Get(this)->Loc("josaly");
			X->GoTo = FVector2D(J->X, J->Y); X->bVanishOnArrive = true; X->TalkId = NAME_None; X->bPersistent = false;
			Toast(FString::Printf(TEXT("Тірі қалған адам құтқарылды (%d/3)"), C), "ok");
			StartQuest("s_ruins");
			if (C >= 3) CompleteSide("s_ruins");
		}, false } });
		return;
	}
	if (T == "militia") { Dialog(Name, Ss.RepMilitia >= 50 ? TEXT("Ағай, біз сіздің жағыңыздамыз. Мутанттарды көрсеңіз, айғайлаңыз.") : Ss.bPermitWeapon ? TEXT("Ауылда тәртіп сақта. Мутанттар көбейіп барады.") : TEXT("Тоқта! Қаруың болса — рұқсатың бар ма? Рұқсатсыз қару ұстасаң, оқ атамыз."), {}); return; }
	if (T == "hqguard") { Dialog(Name, Ss.bPermitAccess ? TEXT("Өтіңіз, инженер. Кеңсе сол жақта.") : TEXT("Басқармаға кіру тек рұқсатпен. Күзет бастығы Бекзатқа барыңыз."), {}); return; }
	static const TCHAR* Lines[] = { TEXT("Су таусылып барады... Ұңғымалар тоқтаса, ауыл да өледі."), TEXT("Түнде даладан біртүрлі ұлыған дауыс естіледі."), TEXT("Балам кеніште істейді. Оны көрсеңіз, сәлем айтыңыз."), TEXT("Бұрын дала тыныш еді..."), TEXT("Дауыл кезінде үйден шықпаңыз — олар дыбысқа келеді.") };
	Dialog(Name, Lines[Rng.RandRange(0, 4)], {});
}

// =====================================================================
// Концовкалар
// =====================================================================
void AKAPGameMode::CheckEndings()
{
	if (bInMenu || bEnded || Cine.IsSet()) return;
	const FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (!Ss.Workers.Num()) return;
	const float DeadPct = WM->DeadCount(Ss) / (float)Ss.Workers.Num();
	if (Ss.Burned > D->Cfg("failBurned", 8) || DeadPct > D->Cfg("failDeadPct", 0.6f) || Ss.Mutation >= 100)
	{
		const FString Why = Ss.Burned > D->Cfg("failBurned", 8) ? FString::Printf(TEXT("Ұңғымалардың %d-і жанып кетті."), Ss.Burned) :
			DeadPct > D->Cfg("failDeadPct", 0.6f) ? FString::Printf(TEXT("Жұмысшылардың %d%%-ы қаза тапты."), FMath::RoundToInt32(DeadPct * 100)) : TEXT("Мутация таралуы 100%-ға жетті.");
		Ending(false, Why);
	}
}

void AKAPGameMode::SendReport()
{
	const FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const int32 Ok = WM->OkCount(Ss);
	int32 Active = 0; for (const FKAPWellSave& W : Ss.Wells) if (W.State == EWellState::Leak || W.State == EWellState::Fire) Active++;
	const int32 NeedW = (int32)D->Cfg("successWells", 11);
	const float NeedM = D->Cfg("successMutation", 70);
	const bool b1 = Ok >= NeedW, b2 = Active == 0, b3 = Ss.Mutation < NeedM;
	const FString Tx = FString::Printf(TEXT("%s Жұмыс істейтін ұңғыма ≥ %d (қазір %d)\n%s Ағу немесе өрт жоқ (қазір %d)\n%s Мутация < %d%% (қазір %d%%)"),
		b1 ? TEXT("✔") : TEXT("✘"), NeedW, Ok, b2 ? TEXT("✔") : TEXT("✘"), Active, b3 ? TEXT("✔") : TEXT("✘"), (int32)NeedM, FMath::RoundToInt32(Ss.Mutation));
	if (b1 && b2 && b3) Dialog(TEXT("Басқарма радиосы"), TEXT("Шарттар орындалды:\n") + Tx + TEXT("\n\nЕсеп жіберілсін бе?"), { { TEXT("Есепті жіберу"), [this]() { Ending(true, FString()); }, false }, { TEXT("Әлі емес"), nullptr, false } });
	else Dialog(TEXT("Басқарма радиосы"), TEXT("Құтқару шарттары орындалмаған:\n") + Tx + TEXT("\n\nҚазір жіберсеңіз, орталық аймақты жабады — «Қашу» соңы."), { { TEXT("Бәрібір жіберу"), [this]() { Ending(false, TEXT("Аймақ ерте жабылды.")); }, false }, { TEXT("Жоқ, жұмысты жалғастырамын"), nullptr, false } });
}

void AKAPGameMode::Ending(bool bSuccess, const FString& Why)
{
	if (bEnded) return;
	bEnded = true;
	if (AKAPHUD* H = AKAPHUD::Get(this)) { H->CloseAll(); }
	if (CurrentVehicle.IsValid()) ExitVehicle();
	FKAPSession& Ss = S();
	// Ерлан — екі соңда да қайтыс болады
	bool bErlanWasAlive = false;
	for (FKAPWorker& K : Ss.Workers) if (K.bSpecial) { bErlanWasAlive = K.State < 4; K.State = 4; K.EvacTo.Reset(); K.Dose = FMath::Max(K.Dose, UKAPDataSubsystem::Get(this)->Cfg("doseDeath", 650)); }
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (!It->bPersistent) It->Destroy();
	Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true;
	if (bSuccess) EndingSuccess(bErlanWasAlive); else EndingFail(Why, bErlanWasAlive);
}

// =====================================================================
// Кинематика қозғалтқышы (Sequencer баламасы — камера кілттік кадрлары кодта)
// =====================================================================
void AKAPGameMode::StartCine(float Dur, TFunction<void(float, float)> Update, TFunction<void()> OnEnd)
{
	bCinematic = true;
	if (!CineCam) CineCam = GetWorld()->SpawnActor<ACameraActor>();
	CineCam->GetCameraComponent()->SetConstraintAspectRatio(false);
	if (AKAPCharacter* P = Player())
	{
		P->bFrozen = true;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetViewTargetWithBlend(CineCam, 0.f);
	}
	FCine C; C.Dur = Dur; C.Update = MoveTemp(Update); C.OnEnd = MoveTemp(OnEnd);
	Cine = MoveTemp(C);
}
void AKAPGameMode::EndCine()
{
	if (!Cine.IsSet()) return;
	TFunction<void()> OnEnd = Cine->OnEnd;
	Cine.Reset();
	bCinematic = false;
	Weather->VisOverride = -1.f;
	if (AKAPCharacter* P = Player())
	{
		P->bFrozen = false;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetViewTargetWithBlend(P, 0.4f);
	}
	if (OnEnd) OnEnd();
}
void AKAPGameMode::SkipCinematic()
{
	if (!Cine.IsSet() || !bCineSkippable) return;
	Cine->Update(Cine->Dur, 0.016f);
	EndCine();
}
void AKAPGameMode::CineCamera(FVector Pos, FVector LookAt, float Fov)
{
	if (!CineCam) return;
	CineCam->SetActorLocation(Pos);
	CineCam->SetActorRotation((LookAt - Pos).Rotation());
	CineCam->GetCameraComponent()->SetFieldOfView(Fov);
}

static float Ease(float T) { return T * T * (3 - 2 * T); }
static float Seg(float T, float A, float B) { return FMath::Clamp((T - A) / (B - A), 0.f, 1.f); }

void AKAPGameMode::Intro()
{
	FKAPSession& Ss = S();
	Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true;
	AKAPVehicleHandle* V = nullptr;
	for (auto& X : Builder->Vehicles) if (X.IsValid() && X->Id == "bukhanka") V = X.Get();
	if (!V) return;
	const FVector From(-83000, 76200, 0), To(-99500, 81800, 0);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y - From.Y, To.X - From.X));
	V->Pawn->GetMesh()->SetSimulatePhysics(false);
	AKAPNPC* Hero = SpawnNPC((int32)ENPCKind::Worker, FVector2D(-995, 818), TEXT("Сіз"), true);
	if (Hero) { Hero->SetActorHiddenInGame(true); Hero->bPersistent = true; CineActors.Add(Hero); }
	struct FLine { float T; const TCHAR* Kk; const TCHAR* Ru; const TCHAR* Who; };
	static const FLine Lines[] = {
		{ 11, TEXT("...ш-ш-ш... QazUran кенішінде апат... жерасты ұңғымаларының бірнешеуі істен шықты..."), TEXT("...ш-ш-ш... авария на руднике QazUran... несколько подземных скважин вышли из строя..."), TEXT("Радио") },
		{ 17, TEXT("...жұмысшылардың арасында сәулеленгендер бар... барлық жөндеуші-инженерлер №1 ұңғымаға..."), TEXT("...среди рабочих есть облучённые... всем инженерам-ремонтникам прибыть к скважине №1..."), TEXT("Радио") },
		{ 24, TEXT("...ауа райы ескертуі... оңтүстік-батыстан құм дауы... ш-ш-ш..."), TEXT("...штормовое предупреждение... с юго-запада песчаная бу... ш-ш-ш..."), TEXT("Радио") },
		{ 31, TEXT("Тағы да қызып кетті ме? Шыда, Буханка, аз қалды..."), TEXT("Опять перегрелась? Потерпи, Буханка, немного осталось..."), TEXT("Сіз") },
		{ 40, TEXT("Радиатор қайнап кетті... Енді жаяу қалдым."), TEXT("Радиатор закипел... Теперь я пешком."), TEXT("Сіз") },
		{ 49, TEXT("Анау не?.. Құм дауылы! Тез баспана табу керек!"), TEXT("Что это?.. Песчаная буря! Нужно срочно найти укрытие!"), TEXT("Сіз") },
	};
	TSharedRef<int32> Li = MakeShared<int32>(0);
	TSharedRef<bool> bSteam = MakeShared<bool>(false);
	TSharedRef<bool> bPlaced = MakeShared<bool>(false);
	TWeakObjectPtr<AKAPVehicleHandle> WV = V;
	TWeakObjectPtr<AKAPNPC> WH = Hero;
	Subtitle(TEXT("KAP Simulator"), FString(), FString(), 6);
	StartCine(62.f, [this, WV, WH, From, To, Yaw, Li, bSteam, bPlaced](float T, float Dt)
	{
		AKAPVehicleHandle* V = WV.Get(); if (!V) return;
		while (*Li < (int32)UE_ARRAY_COUNT(Lines) && T >= Lines[*Li].T)
		{
			Subtitle(Lines[*Li].Kk, Lines[*Li].Ru, Lines[*Li].Who, 6);
			if (FCString::Strcmp(Lines[*Li].Who, TEXT("Радио")) == 0) { UKAPAudio::Get(this)->SetRadio(1); FTimerHandle H; GetWorldTimerManager().SetTimer(H, [this]() { UKAPAudio::Get(this)->SetRadio(0); }, 1.2f, false); }
			(*Li)++;
		}
		// көлік физикамен жүреді (автопилот): дөңгелектер айналады, суспензия шайқалады, шаң мен із қалады
		if (!*bPlaced)
		{
			*bPlaced = true;
			V->Pawn->GetMesh()->SetSimulatePhysics(true);
			V->Pawn->SetActorLocationAndRotation(FVector(From.X, From.Y, KAPVis::GroundZ(GetWorld(), From.X, From.Y, 0) + 90), FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
			V->Pawn->GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
		}
		const bool bDrive = T >= 10 && T < 34;
		if (V->bAutoPilot && !bDrive) V->Pawn->DoHandbrakeStart();
		V->bAutoPilot = bDrive;
		V->AutoTarget = FVector(To.X, To.Y, 0);
		V->AutoMaxKmh = 30.f;
		const FVector P = V->Pawn->GetActorLocation();
		UKAPAudio::Get(this)->SetEngine(bDrive || T < 34, V->SpeedKmh() / 3.6f);
		UKAPAudio::Get(this)->SetEngineLoad(V->Rpm01, T > 28 ? 0.22f : 0.f);
		const float CurYaw = V->Pawn->GetActorRotation().Yaw;
		const FVector Fw = FRotator(0, CurYaw, 0).Vector(), Rt = FRotator(0, CurYaw + 90, 0).Vector();
		if (T < 10)
		{
			const float K = Ease(T / 10);
			const FVector W1(-78000, 64000, KAPVis::GroundZ(GetWorld(), -78000, 64000, 0));
			CineCamera(W1 + FVector(-3000 + K * 6000, 4000 - K * 2000, 9000 - K * 3000), W1, 60);
		}
		else if (T < 22) { const float K = Seg(T, 10, 22); CineCamera(P + Rt * (1400 - K * 400) + Fw * (-1200 + K * 2000) + FVector(0, 0, 300), P + FVector(0, 0, 120), 55); }
		else if (T < 34) CineCamera(P - Fw * 900 + FVector(0, 0, 350), P + Fw * 1000, 70);
		AKAPFXManager* FX = AKAPFXManager::Get(GetWorld());
		if (FX && T > 28 && T < 46 && FMath::FRand() < Dt * 14) FX->Spawn(T > 34 ? EKAPFX::Steam : EKAPFX::Smoke, P + Fw * 200 + FVector(0, 0, 150), T > 34 ? 2.f : 1.f);
		if (T > 34 && !*bSteam) { *bSteam = true; UKAPAudio::Get(this)->Play(EKAPSound::Steam, 0.6f); }
		AKAPNPC* H = WH.Get();
		if (T >= 34 && T < 46 && H)
		{
			H->SetActorHiddenInGame(false);
			const float K = Seg(T, 35, 38);
			const FVector Side = P - Rt * 160, Front = P + Fw * 320;
			const FVector Hp = FMath::Lerp(Side, Front, K);
			H->SetActorLocation(FVector(Hp.X, Hp.Y, KAPVis::GroundZ(GetWorld(), Hp.X, Hp.Y, 0) + 92));
			H->SetActorRotation(FRotator(0, K < 1 ? CurYaw : CurYaw + 180, 0));
			if (FX && T > 39 && FMath::FRand() < Dt * 25) FX->Spawn(EKAPFX::Steam, P + Fw * 190 + FVector(0, 0, 180), 2.5f);
			const float Ck = Seg(T, 34, 46);
			const float Ca = CurYaw + 50 - Ck * 30;
			CineCamera(P + FRotator(0, Ca, 0).Vector() * 700 + FVector(0, 0, 220), Front + FVector(0, 0, 130), 55);
		}
		if (T >= 46)
		{
			const float K = Seg(T, 46, 58);
			Weather->VisOverride = T < 50 ? 300.f : FMath::Lerp(300.f, 15.f, Seg(T, 50, 60));
			if (T > 50) { Weather->OverrideType = EWeatherType::Storm; }
			UKAPAudio::Get(this)->SetWind(0.2f + K * 0.8f);
			const FVector Hl = H ? H->GetActorLocation() : P;
			const float Lk = Ease(Seg(T, 46, 52));
			CineCamera(Hl + FVector(300, 250, 90), FMath::Lerp(Hl, Hl + FVector(-30000, 30000, 2500), Lk), 60);
			if (FX && FMath::FRand() < Dt * 40 * K) FX->Spawn(EKAPFX::Dust, Hl + FVector(FMath::FRandRange(-2000.f, 2000.f), FMath::FRandRange(-2000.f, 2000.f), 200), 3.f);
		}
	}, [this, WV, WH, Yaw]()
	{
		FKAPSession& S2 = S();
		Weather->bOverride = true; Weather->OverrideType = EWeatherType::Storm;
		Weather->Env.Storm = 1.f;
		if (AKAPNPC* H = WH.Get()) H->Destroy();
		AKAPCharacter* P = Player();
		AKAPVehicleHandle* V = WV.Get();
		UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
		const FKAPLocationRow* Wg = D->Loc("wagon0");
		if (V && P && Wg)
		{
			V->bAutoPilot = false;
			if (FVector::Dist2D(V->Pawn->GetActorLocation(), FVector(-99500, 81800, 0)) > 3000.f) V->Pawn->SetActorLocationAndRotation(FVector(-99500, 81800, KAPVis::GroundZ(GetWorld(), -99500, 81800, 0) + 80), FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
			V->Pawn->GetMesh()->SetSimulatePhysics(true);
			const FVector2D Vp = KAP::CM2M(V->Pawn->GetActorLocation()), Wp(Wg->X, Wg->Y);
			const FVector2D Sp = Vp + (Wp - Vp).GetSafeNormal() * 4.0;
			P->SetActorLocation(FVector(Sp.X * 100, Sp.Y * 100, KAPVis::GroundZ(GetWorld(), Sp.X * 100, Sp.Y * 100, 0) + 110), false, nullptr, ETeleportType::TeleportPhysics);
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(0, FMath::RadiansToDegrees(FMath::Atan2(Wp.Y - Sp.Y, Wp.X - Sp.X)), 0));
			S2.Checkpoint = P->GetActorLocation();
			P->Equip("wrench");
		}
		StartQuest("m_shelter", false);
		Subtitle(TEXT("Баспана табыңыз"), TEXT("Найдите укрытие"), FString(), 6);
		Toast(TEXT("Бағдар — вагон-үй шамының жарығы. WASD — жүру, тышқан — қарау, E — әрекет, Tab — планшет, M — карта, Q — көрсеткіштер."), "info");
	});
}

void AKAPGameMode::EndingFail(const FString& Why, bool bErlanWasAlive)
{
	FKAPSession& Ss = S();
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	for (int32 i = 0; i < Ss.Wells.Num(); i++) if (Ss.Wells[i].State != EWellState::Lost) { Ss.Wells[i].State = EWellState::Fire; WM->RefreshVisual(Ss, i); }
	Ss.Time = FMath::FloorToDouble(Ss.Time / 24.0) * 24.0 + 17.6;
	AKAPVehicleHandle* V = nullptr;
	for (auto& X : Builder->Vehicles) if (X.IsValid() && X->Id == "bukhanka") V = X.Get();
	if (!V) { Credits(false); return; }
	V->Pawn->GetMesh()->SetSimulatePhysics(false);
	TArray<TWeakObjectPtr<AKAPNPC>> Wolves;
	for (int32 i = 0; i < 6; i++) if (AKAPNPC* W = SpawnNPC((int32)ENPCKind::Wolf, FVector2D(-285, -205), TEXT(""), true)) { W->SetActorEnableCollision(false); W->GetCharacterMovement()->DisableMovement(); Wolves.Add(W); CineActors.Add(W); }
	static const FVector2D Path[5] = { FVector2D(-285, -205), FVector2D(0, 300), FVector2D(-250, 300), FVector2D(-600, 480), FVector2D(-820, 760) };
	Subtitle(TEXT("«Қашу». ") + Why, TEXT("Финал «Побег»."), FString(), 6);
	TWeakObjectPtr<AKAPVehicleHandle> WV = V;
	StartCine(26.f, [this, WV, Wolves](float T, float Dt)
	{
		AKAPVehicleHandle* V = WV.Get(); if (!V) return;
		const float K = T / 26.f;
		const int32 Sg = FMath::Min(3, FMath::FloorToInt32(K * 4));
		const float Lk = K * 4 - Sg;
		const FVector2D A = Path[Sg], B = Path[Sg + 1], P2 = FMath::Lerp(A, B, Lk);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		const FVector P(P2.X * 100, P2.Y * 100, KAPVis::GroundZ(GetWorld(), P2.X * 100, P2.Y * 100, 0) + 60);
		V->Pawn->SetActorLocationAndRotation(P, FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
		UKAPAudio::Get(this)->SetEngine(true, 20); UKAPAudio::Get(this)->SetFire(0.6f);
		const FVector Fw = FRotator(0, Yaw, 0).Vector(), Rt = FRotator(0, Yaw + 90, 0).Vector();
		for (int32 i = 0; i < Wolves.Num(); i++) if (AKAPNPC* W = Wolves[i].Get())
		{
			const FVector Wp = P - Fw * (1000 + i * 300 - FMath::Sin(T * 2 + i) * 200) + Rt * ((i - 3) * 200);
			W->SetActorLocationAndRotation(FVector(Wp.X, Wp.Y, KAPVis::GroundZ(GetWorld(), Wp.X, Wp.Y, 0) + 45), FRotator(0, Yaw, 0));
		}
		if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) if (FMath::FRand() < Dt * 10) FX->Spawn(EKAPFX::Smoke, P + FVector(FMath::FRandRange(-20000.f, 20000.f), FMath::FRandRange(-20000.f, 20000.f), 1000), 8.f);
		CineCamera(P - Fw * 1600 + Rt * 600 + FVector(0, 0, 600), P + FVector(0, 0, 100), 65);
		if (T > 8 && T - Dt <= 8) Subtitle(TEXT("Аймақ жабылды. Артта — жанып жатқан дала."), TEXT("Зона закрыта. Позади — горящая степь."), FString(), 6);
		if (T > 16 && T - Dt <= 16) Subtitle(TEXT("Біз оларды құтқара алмадық..."), TEXT("Мы не смогли их спасти..."), TEXT("Сіз"), 6);
	}, [this, bErlanWasAlive]() { for (auto& A : CineActors) if (A.IsValid()) A->Destroy(); CineActors.Reset(); MotherScene(bErlanWasAlive, false); });
}

void AKAPGameMode::EndingSuccess(bool bErlanWasAlive)
{
	FKAPSession& Ss = S();
	Ss.Time = FMath::FloorToDouble(Ss.Time / 24.0) * 24.0 + 32.0;
	const FVector2D W = GetWorld()->GetSubsystem<UWellManagerSubsystem>()->Wells[0].PosM();
	Subtitle(TEXT("«Құтқару». Ұңғымалар қалпына келді, мутация тоқтады."), TEXT("Финал «Спасение». Скважины восстановлены, мутация остановлена."), FString(), 6);
	StartCine(22.f, [this, W](float T, float Dt)
	{
		const float K = T / 22.f, A = K * 1.4f;
		const FVector C(W.X * 100, W.Y * 100, KAPVis::GroundZ(GetWorld(), W.X * 100, W.Y * 100, 0));
		CineCamera(C + FVector(FMath::Cos(A) * (4000 + K * 6000), FMath::Sin(A) * (4000 + K * 6000), 1500 + K * 4000), C + FVector(0, 0, 200), 60);
		if (T > 7 && T - Dt <= 7) Subtitle(TEXT("Кеніш қайта жұмыс істеп тұр. Тірі қалған жұмысшылар отбасыларына оралады."), TEXT("Рудник снова работает. Выжившие рабочие возвращаются к семьям."), FString(), 7);
		if (T > 15 && T - Dt <= 15) Subtitle(TEXT("Бірақ бәрі бірдей оралған жоқ..."), TEXT("Но вернулись не все..."), FString(), 6);
	}, [this, bErlanWasAlive]() { MotherScene(bErlanWasAlive, true); });
}

void AKAPGameMode::MotherScene(bool bErlanWasAlive, bool bSuccess)
{
	FKAPSession& Ss = S();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const FKAPLocationRow* A = D->Loc("aksai");
	const FVector2D C(A->X + 75, A->Y - 10);
	Ss.Time = FMath::FloorToDouble(Ss.Time / 24.0) * 24.0 + 17.35;
	Weather->VisOverride = 1200;
	const float Z = KAPVis::GroundZ(GetWorld(), C.X * 100, C.Y * 100, 0);
	AKAPNPC* Mother = SpawnNPC((int32)ENPCKind::Mother, C, TEXT("Ана"), true);
	AKAPNPC* Body = SpawnNPC((int32)ENPCKind::Worker, C + FVector2D(0, 2), TEXT("Ерлан"), true);
	AKAPNPC* Hero = SpawnNPC((int32)ENPCKind::Worker, C + FVector2D(4.5f, 2.5f), TEXT("Сіз"), true);
	AKAPNPC* W1 = SpawnNPC((int32)ENPCKind::Worker, C + FVector2D(-3, 4), TEXT(""), true);
	AKAPNPC* W2 = SpawnNPC((int32)ENPCKind::Worker, C + FVector2D(-2, 5.5f), TEXT(""), true);
	for (AKAPNPC* N : { Mother, Body, Hero, W1, W2 }) if (N) { N->GetCharacterMovement()->DisableMovement(); CineActors.Add(N); }
	if (Body) { Body->SetActorRotation(FRotator(0, 90, 0)); Body->Kill(false); }
	if (Mother) { Mother->SetActorRotation(FRotator(0, 90, 0)); Mother->GetMesh()->SetRelativeLocation(FVector(0, 0, -135)); }
	if (Hero) Hero->SetActorRotation(FRotator(0, 200, 0));
	if (W1) W1->SetActorRotation(FRotator(0, 30, 0));
	if (W2) W2->SetActorRotation(FRotator(0, 20, 0));
	struct FLine { float T; FString Kk, Ru, Who; };
	TArray<FLine> Lines = {
		{ 1, bErlanWasAlive ? TEXT("Соңғы күні №3 ұңғымада қышқыл қайта ағып кетті. Ерлан Сейітов клапанды жабу үшін ішке кірді. Басқаларды құтқарды... өзі оралмады.") : TEXT("Ерлан Сейітов, 19 жас. Сәулеленуден қайтыс болды."),
			bErlanWasAlive ? TEXT("В последний день на скважине №3 снова потекла кислота. Ерлан Сеитов вошёл закрыть клапан. Он спас остальных... а сам не вернулся.") : TEXT("Ерлан Сеитов, 19 лет. Умер от облучения."), FString() },
		{ 10, TEXT("Балам... Ерлан... құлыным менің..."), TEXT("Сынок... Ерлан... жеребёночек мой..."), TEXT("Ана") },
		{ 17, TEXT("Оқуыңды бітіремін деп едің ғой... Қайтып келемін деп едің ғой..."), TEXT("Ты же обещал доучиться... Обещал вернуться..."), TEXT("Ана") },
		{ 25, TEXT("Балам-ау... жалғызым-ау..."), TEXT("Сыночек... единственный мой..."), TEXT("Ана") },
	};
	TSharedRef<int32> Li = MakeShared<int32>(0);
	TSharedRef<bool> bLament = MakeShared<bool>(false);
	TWeakObjectPtr<AKAPNPC> WM = Mother;
	const FVector Cc(C.X * 100, C.Y * 100, Z);
	StartCine(48.f, [this, Lines, Li, bLament, WM, Cc](float T, float Dt)
	{
		FKAPSession& S2 = S();
		while (*Li < Lines.Num() && T >= Lines[*Li].T) { Subtitle(Lines[*Li].Kk, Lines[*Li].Ru, Lines[*Li].Who, 7); (*Li)++; }
		if (T > 9 && !*bLament) { *bLament = true; UKAPAudio::Get(this)->StopLoops(); UKAPAudio::Get(this)->Lament(); FTimerHandle H; GetWorldTimerManager().SetTimer(H, [this]() { UKAPAudio::Get(this)->Lament(); }, 20.f, false); }
		if (AKAPNPC* M = WM.Get()) M->GetMesh()->SetRelativeRotation(FRotator(FMath::Sin(T * 2.2f) * 3.f, -90, 0));
		S2.Time += Dt * 0.004;
		// камера баяу алыстайды, күн батып бара жатыр
		const float K = Ease(Seg(T, 4, 46));
		const float A = -0.6f + K * 0.5f, R = 400 + K * 5500;
		CineCamera(Cc + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R + 100, 150 + K * 2200), Cc + FVector(0, 100, 80 + K * 200), 55);
	}, [this, bSuccess]() { for (auto& A : CineActors) if (A.IsValid()) A->Destroy(); CineActors.Reset(); Credits(bSuccess); });
}

void AKAPGameMode::Credits(bool bSuccess)
{
	FKAPSession& Ss = S();
	UKAPAudio::Get(this)->StopLoops();
	FString T = bSuccess ? TEXT("«Құтқару»\n\n") : TEXT("«Қашу»\n\n");
	T += FString::Printf(TEXT("Жөнделген ұңғымалар: %d · жанған: %d · мутация: %d%%\n\n"), Ss.WellsRepaired, Ss.Burned, FMath::RoundToInt32(Ss.Mutation));
	TArray<FString> Saved, Dead;
	for (const FKAPWorker& K : Ss.Workers) (K.State < 4 ? Saved : Dead).Add(K.Name + (K.bSpecial ? TEXT(" (19 жас)") : FString::Printf(TEXT(" · №%d"), K.WellId)));
	T += FString::Printf(TEXT("ҚҰТҚАРЫЛҒАН ЖҰМЫСШЫЛАР · %d\n"), Saved.Num()) + FString::Join(Saved, TEXT("\n"));
	T += FString::Printf(TEXT("\n\nҚАЗА ТАПҚАНДАР · %d\n"), Dead.Num()) + FString::Join(Dead, TEXT("\n"));
	T += TEXT("\n\n\nУран өндірісіндегі адамдардың ауыр еңбегі мен тәуекеліне арналады.\n\nKAP Simulator · Оқиғалар мен кейіпкерлер ойдан шығарылған.\n«QazUran» — ойдан шығарылған компания.\n\n\nEsc — басты мәзір");
	CreditsText = T; CreditsT = 0; bCredits = true; bCinematic = true;
}
