// -kapstorytest: кеңейтілген сюжетті басынан аяғына дейін өтеді (S01 → Ерлан → Мәрия → Бекзат S06 → Сапардың бөгеті →
// апат журналы → директордың қоңырауы → ретранслятор → S11 → есеп шолуы → «Құтқару» финалы → Ана сахнасы → титр).
// Журналда KAPSTORY CHECK жолдары, скриншоттар KAPS_*.png. Соңында KAPSTORY RESULT pass/fail.
#include "KAPGameMode.h"
#include "KAPGameInstance.h"
#include "KAPDataSubsystem.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/UI/KAPHUD.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "TimerManager.h"

void AKAPGameMode::StartStoryTest()
{
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY test start"));
	struct FStep { float T; TFunction<void()> F; };
	TSharedRef<TArray<FStep>> St = MakeShared<TArray<FStep>>();
	TSharedRef<int32> Fails = MakeShared<int32>(0), Checks = MakeShared<int32>(0);
	float Clock = 0.f;
	auto Then = [St, &Clock](float Dt, TFunction<void()> F) { Clock += Dt; St->Add({ Clock, MoveTemp(F) }); };
	auto Shot = [](const FString& N) { FScreenshotRequest::RequestScreenshot(TEXT("KAPS_") + N + TEXT(".png"), true, false); UE_LOG(LogTemp, Display, TEXT("KAPSTORY shot %s"), *N); };
	auto Check = [Fails, Checks](const TCHAR* Name, bool bOk) { (*Checks)++; if (!bOk) (*Fails)++; UE_LOG(LogTemp, Display, TEXT("KAPSTORY CHECK %s %s"), Name, bOk ? TEXT("ok") : TEXT("FAIL")); };
	auto Pick = [this](const TCHAR* Sub)
	{
		for (int32 i = 0; i < StoryOpts.Num(); i++) if (StoryOpts[i].Label.Contains(Sub)) { StoryPick(i); return; }
		FString All; for (const FKAPDialogOption& O : StoryOpts) All += TEXT(" | ") + O.Label;
		UE_LOG(LogTemp, Warning, TEXT("KAPSTORY pick '%s' not found:%s"), Sub, *All);
	};
	auto Go = [this](FVector2D Pm, FVector2D LookM)
	{
		AKAPCharacter* P = Player(); APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!P || !PC) return;
		P->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		P->GetCharacterMovement()->StopMovementImmediately();
		const FVector Cm(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0) + 165.f);
		P->SetActorLocation(Cm, false, nullptr, ETeleportType::TeleportPhysics);
		const FVector Lk(LookM.X * 100, LookM.Y * 100, KAPVis::GroundZ(GetWorld(), LookM.X * 100, LookM.Y * 100, 0) + 150.f);
		PC->SetControlRotation((Lk - Cm).Rotation());
	};
	auto GoTo = [this, Go](AActor* A, float DistM)
	{
		if (!A) { UE_LOG(LogTemp, Warning, TEXT("KAPSTORY go: no actor")); return; }
		const FVector2D T = KAP::CM2M(A->GetActorLocation());
		const FVector2D Dir = FVector2D(A->GetActorForwardVector().X, A->GetActorForwardVector().Y).GetSafeNormal();
		Go(T + (Dir.IsNearlyZero() ? FVector2D(1, 0) : Dir) * DistM, T);
	};
	auto Q =[this](const TCHAR* Id) { return (int32)QuestState(Id); }; // 0 None, 1 Active, 2 Done
	auto F = [this](const TCHAR* K) { return S().Flags.Contains(FName(K)); };
	auto Log = [this](const TCHAR* Tag)
	{
		FString Qs; for (const auto& P : S().Quests) Qs += FString::Printf(TEXT(" %s=%d"), *P.Key.ToString(), (int32)P.Value);
		FString Fs; for (const auto& P : S().Flags) if (P.Key.ToString().StartsWith(TEXT("st_"))) Fs += TEXT(" ") + P.Key.ToString();
		UE_LOG(LogTemp, Display, TEXT("KAPSTORY state %s: quests:%s | flags:%s | money=%d permit=%d"), Tag, *Qs, *Fs, S().Money, S().bPermitAccess ? 1 : 0);
	};

	// ---- бастау ----
	Then(3.f, [this]() { if (bInMenu) MenuNewGame(); });
	Then(3.f, [this]() { if (bInMenu) MenuNewGame(); });
	Then(4.f, [this]() { if (bCinematic) SkipCinematic(); });
	Then(2.f, [this, Check]() {
		if (bCinematic) SkipCinematic();
		Weather->bOverride = true; Weather->OverrideType = EWeatherType::Clear; Weather->bInstant = true;
		S().Time = FMath::FloorToDouble(S().Time / 24) * 24 + 24.0 + 10.5;
		DirRest = 1e6f; // тест кезінде кездейсоқ монстр шықпасын
		if (AKAPCharacter* P = Player()) if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { if (PC->GetPawn() != P) PC->Possess(P); PC->SetViewTarget(P); }
		// ұңғыма баланс: баяу құлдырау
		const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
		const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
		int32 Open = 0; for (const FKAPWellSave& W : S().Wells) if (W.State == EWellState::Fault) Open++;
		UE_LOG(LogTemp, Display, TEXT("KAPSTORY balance hazard=%.3f stage1=%.0f stage2=%.0f fire=%.0f open=%d canRandom(w2)=%d"), D->Cfg("hazardScale", -1), D->Cfg("stage1Hours", -1), D->Cfg("stage2Hours", -1), D->Cfg("fireHours", -1), Open, WM->CanRandomFault(S(), 1) ? 1 : 0);
		Check(TEXT("balance_config"), FMath::IsNearlyEqual(D->Cfg("hazardScale", -1), 0.03f) && D->Cfg("stage1Hours", -1) == 10.f && D->Cfg("stage2Hours", -1) == 8.f);
		Check(TEXT("balance_prefaults"), Open == 4 && S().Wells.IsValidIndex(2) && S().Wells[2].State == EWellState::Fault);
		Check(TEXT("balance_cap"), !WM->CanRandomFault(S(), 1));
		Check(TEXT("mariya_spawned"), NamedNPC.FindRef("mariya").IsValid());
	});
	// ---- S01: дауылды күту ----
	Then(2.f, [this]() {
		static const TCHAR* Ids[] = { TEXT("m_shelter"), TEXT("m_tablet") };
		for (const TCHAR* Id : Ids) { StartQuest(Id, false); CompleteQuest(Id); }
		StartQuest("m_wait", false); S().bHasTablet = true; WaitT = 25.f;
	});
	Then(1.5f, [Shot]() { Shot(TEXT("01_S01_radio")); });
	Then(0.5f, [this, Check, F, Pick]() { Check(TEXT("S01_shown"), F(TEXT("st_S01")) && StoryReading()); Pick(TEXT("нақтылау")); });
	// ---- күзетке дейін өткізу ----
	Then(1.f, [this, Check, F]() {
		Check(TEXT("S01_clarify"), F(TEXT("st_clarify")));
		static const TCHAR* Ids[] = { TEXT("m_wait"), TEXT("m_radiator"), TEXT("m_fixuaz"), TEXT("m_well1"), TEXT("m_fix1"), TEXT("m_guard") };
		for (const TCHAR* Id : Ids) { StartQuest(Id, false); CompleteQuest(Id); }
		S().bSimActive = true; S().bPermitWeapon = true;
		StartQuest("m_wells"); StartQuest("m_towers");
		S().Flags.Add("st_dispT", 24.0);
	});
	Then(2.5f, [this, Check, Q, GoTo]() { Check(TEXT("st_erlan_started"), Q(TEXT("st_erlan")) == 1); Check(TEXT("erlan_npc"), StoryErlan() != nullptr); GoTo(StoryErlan(), 3.2f); });
	// ---- S02: Ерлан ----
	Then(1.5f, [this]() { Talk(StoryErlan()); });
	Then(0.7f, [Shot]() { Shot(TEXT("02_S02_erlan_a")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Есікті")); });
	Then(0.7f, [Shot]() { Shot(TEXT("03_S02_erlan_injury")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Жұмыстан бөлек")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Қандай")); });
	Then(0.7f, [Shot]() { Shot(TEXT("04_S02_garage")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Сәлеміңді")); });
	Then(0.5f, [this, Check, Q, F, GoTo]() {
		Check(TEXT("S02_done"), Q(TEXT("st_erlan")) == 2 && Q(TEXT("story_erlan_family")) == 1 && F(TEXT("st_erlan_met")));
		GoTo(NamedNPC.FindRef("mariya").Get(), 3.f);
	});
	// ---- S03: Мәрия апа ----
	Then(1.5f, [this]() { Talk(NamedNPC.FindRef("mariya").Get()); });
	Then(0.7f, [Shot]() { Shot(TEXT("05_S03_mariya")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Көрдім")); });
	Then(0.5f, [Pick]() { Pick(TEXT("мықты")); });
	Then(0.7f, [Shot]() { Shot(TEXT("06_S03_shirt")); });
	Then(0.5f, [Pick]() { Pick(TEXT("уәде ете алмаймын")); });
	Then(0.7f, [Shot]() { Shot(TEXT("07_S03_no_promise")); });
	Then(0.5f, [this, Check, F, Pick, GoTo]() { Pick(TEXT("Жалғыз")); Check(TEXT("S03_shirt"), F(TEXT("st_shirt_taken")) && F(TEXT("st_mother_met"))); GoTo(StoryErlan(), 3.2f); });
	// ---- жейде ----
	Then(1.5f, [this]() { Talk(StoryErlan()); });
	Then(0.7f, [Shot]() { Shot(TEXT("08_erlan_hub")); });
	Then(0.5f, [Pick]() { Pick(TEXT("жейде")); });
	Then(0.7f, [Shot]() { Shot(TEXT("09_S04_shirt_delivered")); });
	Then(0.5f, [this, Check, Q, F, Pick, GoTo]() {
		Pick(TEXT("жарасады"));
		Check(TEXT("shirt_delivered"), F(TEXT("st_shirt_delivered")) && Q(TEXT("story_erlan_family")) == 2);
		GoTo(NamedNPC.FindRef("aigerim").Get(), 3.f);
	});
	// ---- S04: Айгерім (бар болса) ----
	Then(1.5f, [this]() { if (AKAPNPC* A = NamedNPC.FindRef("aigerim").Get()) Talk(A); });
	Then(0.7f, [Shot]() { Shot(TEXT("10_S04_aigerim")); });
	Then(0.5f, [this, Check, F, Pick]() { if (NamedNPC.FindRef("aigerim").IsValid()) Check(TEXT("S04_aigerim"), F(TEXT("st_S04"))); Pick(TEXT("барамын")); });
	// ---- S06: бес ұңғыма → Бекзат ----
	Then(1.f, [this, Check, Q, GoTo]() {
		S().WellsRepaired = 5; StoryPermitReady();
		Check(TEXT("S06_called"), Q(TEXT("story_gate_testimony")) == 1 && !S().bPermitAccess);
		GoTo(NamedNPC.FindRef("bekzat").Get(), 3.f);
	});
	Then(1.5f, [this]() { Talk(NamedNPC.FindRef("bekzat").Get()); });
	Then(0.7f, [Shot]() { Shot(TEXT("11_S06_bekzat")); });
	Then(0.4f, [Pick]() { Pick(TEXT("не айттыңыз")); });
	Then(0.4f, [Pick]() { Pick(TEXT("...")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Қауіп барын")); });
	Then(0.7f, [Shot]() { Shot(TEXT("12_S06_order")); });
	Then(0.4f, [Pick]() { Pick(TEXT("...")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Түпнұсқаны")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Жазамын")); });
	Then(0.4f, [Pick]() { Pick(TEXT("жол ашыңыз")); });
	Then(0.4f, [Pick]() { Pick(TEXT("...")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Рұқсатты беріңіз")); });
	Then(0.7f, [Shot]() { Shot(TEXT("13_S06_permit")); });
	Then(0.5f, [this, Check, Q, F, Pick, Log]() {
		Pick(TEXT("Түсіндім"));
		bool bKamaz = false; for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "kamaz" && V->State == "ok") bKamaz = true;
		Check(TEXT("S06_permit"), S().bPermitAccess && Q(TEXT("m_hq")) == 1 && Q(TEXT("story_roadblock")) == 1 && F(TEXT("st_bekzat_testimony")) && F(TEXT("st_bekzat_road")) && bKamaz);
		Log(TEXT("after_S06"));
	});
	// ---- Сапардың бөгеті ----
	Then(2.f, [this, Check, Go]() {
		int32 N = 0; for (auto& W : StoryRoadblockNPCs) if (W.IsValid()) N++;
		Check(TEXT("roadblock_spawned"), N == 4);
		// қақпа бөгет шешілмей ашылмайды
		if (AKAPInteractSpot* G = FindSpotForTest(TEXT("hq_gate"))) SpotUse(G);
		Check(TEXT("gate_blocked"), !S().Flags.Contains("gateOpen"));
		Go(FVector2D(670.f, -680.f), FVector2D(666.f, -712.f));
	});
	Then(2.f, [this, Check]() { Check(TEXT("roadblock_dialog"), StoryReading()); });
	Then(0.3f, [Shot]() { Shot(TEXT("14_roadblock_dialog")); });
	Then(0.5f, [Pick]() { Pick(TEXT("Ойланып")); });
	Then(1.0f, [Shot]() { Shot(TEXT("15_roadblock_scene")); });
	Then(0.5f, [this]() { if (StoryRoadblockNPCs.Num()) Talk(StoryRoadblockNPCs[0].Get()); });
	Then(0.5f, [Pick]() { Pick(TEXT("Бекзаттың")); });
	Then(1.f, [this, Check, Q, Go]() {
		Check(TEXT("roadblock_resolved"), Q(TEXT("story_roadblock")) == 2 && Q(TEXT("s_bandit")) >= 1);
		if (AKAPInteractSpot* G = FindSpotForTest(TEXT("hq_gate"))) SpotUse(G);
		Check(TEXT("gate_opens"), S().Flags.Contains("gateOpen"));
		if (Builder->HQOffice.IsValid()) { const FVector2D Tp = KAP::CM2M(Builder->HQOffice->TablePos); Go(Tp + FVector2D(1.6f, 1.2f), Tp); }
	});
	// ---- S09: апат журналы ----
	Then(2.f, [this, Check, Q]() { Check(TEXT("m_hq_done"), Q(TEXT("m_hq")) == 2 && Q(TEXT("m_cause")) == 1); if (AKAPInteractSpot* D = FindSpotForTest(TEXT("hq_desk"))) SpotUse(D); });
	Then(0.7f, [Shot]() { Shot(TEXT("16_S09_journal")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Келесі")); });
	Then(0.7f, [Shot]() { Shot(TEXT("17_S09_orders")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Соңғы")); });
	Then(0.7f, [Shot]() { Shot(TEXT("18_S09_signed")); });
	Then(0.4f, [Pick]() { Pick(TEXT("қол қойғанмын")); });
	Then(1.f, [this, Check, Q, F]() {
		Check(TEXT("S09_done"), Q(TEXT("m_cause")) == 2 && Q(TEXT("story_director_call")) == 1 && F(TEXT("st_engineer_admit")) && S().Inv.FindRef("logbook") == 1);
		if (AKAPInteractSpot* D = FindSpotForTest(TEXT("hq_desk"))) SpotUse(D);
	});
	// ---- S10: директор ----
	Then(0.7f, [Shot]() { Shot(TEXT("19_S10_director")); });
	Then(0.4f, [Pick]() { Pick(TEXT("бұл жерде емессіз")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Ескертуді")); });
	Then(0.4f, [Pick]() { Pick(TEXT("өзіңіз кеттіңіз")); });
	Then(0.7f, [Shot]() { Shot(TEXT("20_S10_offer")); });
	Then(0.4f, [Pick]() { Pick(TEXT("бұйрықты да")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Осында қалғандар")); });
	Then(0.7f, [Shot]() { Shot(TEXT("21_S10_cut")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Арнаны")); });
	Then(1.f, [this, Check, Q, Go]() {
		Check(TEXT("S10_refused"), Q(TEXT("story_director_call")) == 2 && Q(TEXT("story_repeater")) == 1 && Q(TEXT("m_report")) == 0);
		if (AKAPInteractSpot* R = FindSpotForTest(TEXT("hq_radio"))) SpotUse(R);
		Check(TEXT("radio_blocked"), !bEnded && Q(TEXT("m_report")) == 0);
		Go(FVector2D(700.f - 40.f, -480.f + 22.f), FVector2D(700.f, -480.f));
	});
	// ---- ретранслятор ----
	Then(2.5f, [this, Check, F, Shot]() {
		int32 N = 0; for (auto& W : StoryHiredNPCs) if (W.IsValid() && !W->bDead) N++;
		Check(TEXT("hired_spawned"), N == 3);
		Check(TEXT("serik_warning"), F(TEXT("st_serik_warn")));
		Shot(TEXT("22_repeater_guards"));
	});
	Then(1.f, [this, Check, F]() {
		for (auto& W : StoryHiredNPCs) if (AKAPNPC* X = W.Get()) if (!X->bDead) X->Kill(true);
		Check(TEXT("hired_cleared"), F(TEXT("st_hired_cleared")));
		S().Inv.FindOrAdd("cable") += 1;
		if (AKAPInteractSpot* R = FindSpotForTest(TEXT("story:repeater"))) SpotUse(R);
	});
	Then(1.f, [this, Check, Shot]() { Check(TEXT("repeater_minigame"), AKAPHUD::Get(this) && AKAPHUD::Get(this)->IsModal()); Shot(TEXT("23_repeater_minigame")); });
	Then(0.5f, [this, Check, Q, GoTo]() { if (AKAPHUD* H = AKAPHUD::Get(this)) H->CloseAll(); StoryRepeaterDone(); Check(TEXT("repeater_done"), Q(TEXT("story_repeater")) == 2 && Q(TEXT("m_report")) == 1); GoTo(StoryErlan(), 3.2f); });
	// ---- S11: Ерланның хабары ----
	Then(1.5f, [this]() { Talk(StoryErlan()); });
	Then(0.4f, [Pick]() { Pick(TEXT("Қалайсың")); });
	Then(0.7f, [Shot]() { Shot(TEXT("24_S11_buttons")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Бүгін дайын")); });
	Then(0.4f, [Pick]() { Pick(TEXT("жазып қояйық")); });
	Then(0.7f, [Shot]() { Shot(TEXT("25_S11_message")); });
	Then(0.4f, [this, Check, F, Pick, Go, Log]() {
		Pick(TEXT("сақталды"));
		Check(TEXT("S11_message"), F(TEXT("st_S11")) && S().Flags.FindRef("st_erlan_message") == 2.0);
		// «Құтқару» шарттары: барлық ұңғыма жұмыс істейді, мутация төмен
		UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
		for (int32 i = 0; i < S().Wells.Num(); i++) if (S().Wells[i].State != EWellState::Ok) WM->RepairWell(S(), i);
		S().Mutation = 12.f;
		if (Builder->HQOffice.IsValid()) { const FVector2D Bp = KAP::CM2M(Builder->HQOffice->BedPos); Go(Bp + FVector2D(1.6f, 1.2f), Bp); }
		Log(TEXT("before_report"));
	});
	// ---- S12: есеп алдындағы шолу → финал ----
	Then(1.5f, [this]() { if (AKAPInteractSpot* R = FindSpotForTest(TEXT("hq_radio"))) SpotUse(R); });
	Then(0.7f, [this, Check, Shot]() { Check(TEXT("review_shown"), StoryReading()); Shot(TEXT("26_S12_review")); });
	Then(0.4f, [Pick]() { Pick(TEXT("Техникалық")); });
	Then(0.7f, [Shot]() { Shot(TEXT("27_report_check")); });
	Then(0.3f, [this, Check]() {
		// SendReport — бастапқы код (StoryOpts емес), «Есепті жіберу» батырмасының әрекеті
		Check(TEXT("report_dialog"), StoryReading());
		if (AKAPHUD* H = AKAPHUD::Get(this)) H->CloseAll();
		Ending(true, FString());
	});
	Then(1.f, [this, Check, F]() { Check(TEXT("ending_started"), bEnded && F(TEXT("st_final_report_sent"))); });
	Then(8.f, [Shot]() { Shot(TEXT("28_ending_success")); });
	Then(16.f, [this, Check, F]() { Check(TEXT("mother_scene"), F(TEXT("st_final_committed")) && !NamedNPC.FindRef("mariya").IsValid()); });
	Then(5.f, [Shot]() { Shot(TEXT("29_mother_aigerim")); });
	Then(9.f, [Shot]() { Shot(TEXT("30_mother_shirt")); });
	Then(12.f, [Shot]() { Shot(TEXT("31_mother_cups")); });
	Then(14.f, [Shot]() { Shot(TEXT("32_mother_message")); });
	Then(22.f, [Shot]() { Shot(TEXT("33_mother_lament")); });
	Then(14.f, [Shot]() { Shot(TEXT("34_epilogue")); });
	Then(24.f, [this, Check, Shot]() { Check(TEXT("credits"), bCredits && CreditsText.Contains(TEXT("ОҚИҒА")) && CreditsText.Contains(TEXT("Бекзат: куәлік берді"))); Shot(TEXT("35_credits")); UE_LOG(LogTemp, Display, TEXT("KAPSTORY credits: %s"), *CreditsText.Replace(TEXT("\n"), TEXT(" / "))); });
	Then(2.f, [Fails, Checks]() { UE_LOG(LogTemp, Display, TEXT("KAPSTORY RESULT %s (%d/%d checks ok)"), *Fails == 0 ? TEXT("pass") : TEXT("fail"), *Checks - *Fails, *Checks); FGenericPlatformMisc::RequestExit(false); });
	for (const FStep& Sx : *St) { FTimerHandle H; GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateWeakLambda(this, [Fn = Sx.F]() { Fn(); }), Sx.T, false); }
}

AKAPInteractSpot* AKAPGameMode::FindSpotForTest(const TCHAR* Action) const
{
	for (TActorIterator<AKAPInteractSpot> It(GetWorld()); It; ++It) if (It->Action == FName(Action)) return *It;
	UE_LOG(LogTemp, Warning, TEXT("KAPSTORY no spot %s"), Action);
	return nullptr;
}
