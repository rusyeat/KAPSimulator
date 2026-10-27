// KAPStoryExpansion.cpp — «Кезек аяқталған жоқ»: сценарий пакетін (Codex, 27.10.2026) ойынға қосу.
// S01–S12 сахналары бұрынғы тапсырмаларға тоқылған. Екі зұлым:
//  • директор Серік Сәрсенов — рациядағы «Кезекті жаппаңдар» деген дауыс, кейін пара ұсынады, есеп арнасын жабады,
//    ретрансляторға жалдамалы күзет қояды (босс шайқасы жоқ — күші бұйрықта, құжатта, адамдардың қорқынышында);
//  • Сапар Аманов — Қаражардағы топтың басшысы, басқарма қақпасының алдына жол бөгетін қояды.
// Жаңа кедергілер негізгі жолда: Бекзаттың рұқсаты (S06) → Сапардың бөгеті → апат журналы → директордың қоңырауы →
// бұғатталған арна (ретранслятор) → есеп алдындағы шолу → финал. Белгілер FKAPSession::Flags ішінде "st_*".
#include "KAPGameMode.h"
#include "KAPGameInstance.h"
#include "KAPDataSubsystem.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/UI/KAPHUD.h"
#include "KAP/Audio/KAPAudio.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace
{
	bool Fl(const FKAPSession& S, const TCHAR* K) { return S.Flags.Contains(FName(K)); }
	double FlV(const FKAPSession& S, const TCHAR* K) { const double* V = S.Flags.Find(FName(K)); return V ? *V : 0.0; }
	void SetFl(FKAPSession& S, const TCHAR* K, double V = 1.0) { S.Flags.Add(FName(K), V); }
	// Сапардың бөгеті басқарма қақпасының сыртында, №3-аймақ мұнарасынан келетін жолда
	const FVector2D RoadblockC(666.f, -710.f);
	const float RoadblockYaw = -8.37f;
	const FVector2D TowerRepeater(700.f, -480.f);
	const TCHAR* ErlanMsgShirt = TEXT("Апа, жейде келді. Жіп те келді. Гараждың есігін өзің боямай тұр. Бірге таңдаймыз деп едік қой.");
	const TCHAR* ErlanMsgNoShirt = TEXT("Апа, мені уайымдап отырғаныңды білемін. Қасымда адамдар бар. Гараждың есігін өзің боямай тұр. Бірге таңдаймыз деп едік қой.");
}

// =====================================================================
// Көмекшілер
// =====================================================================
void AKAPGameMode::SDialog(const FString& Name, const FString& Text, TArray<FKAPDialogOption> Opts)
{
	if (Opts.Num() == 0) Opts.Add({ TEXT("Жабу"), nullptr, false });
	StoryOpts = Opts;
	Dialog(Name, Text, MoveTemp(Opts));
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY dialog [%s] opts=%d: %s"), *Name, StoryOpts.Num(), *Text.Left(90).Replace(TEXT("\n"), TEXT(" ")));
}

void AKAPGameMode::StoryPick(int32 I)
{
	if (!StoryOpts.IsValidIndex(I)) { UE_LOG(LogTemp, Warning, TEXT("KAPSTORY pick %d: no option"), I); return; }
	const FKAPDialogOption O = StoryOpts[I];
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY pick %d: %s%s"), I, *O.Label, O.bDisabled ? TEXT(" (disabled)") : TEXT(""));
	if (O.bDisabled) return;
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->CloseAll();
	StoryOpts.Reset();
	if (O.Fn) O.Fn();
}

bool AKAPGameMode::StoryReading() const
{
	const AKAPHUD* H = AKAPHUD::Get(this);
	return H && H->IsDialogOpen();
}

void AKAPGameMode::StoryTarget(FVector2D P, const FString& Title)
{
	if (AKAPCharacter* Pc = Player()) if (Pc->Navigator) Pc->Navigator->SetTarget(P, Title);
}

int32 AKAPGameMode::StoryErlanIndex() const
{
	const FKAPSession& Ss = S();
	for (int32 i = 0; i < Ss.Workers.Num(); i++) if (Ss.Workers[i].bSpecial) return i;
	return -1;
}

AKAPNPC* AKAPGameMode::StoryErlan() const
{
	const int32 I = StoryErlanIndex();
	if (I < 0) return nullptr;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->WorkerIndex == I) return *It;
	return nullptr;
}

bool AKAPGameMode::StoryErlanAlive() const
{
	const int32 I = StoryErlanIndex();
	if (I < 0 || S().Workers[I].State >= 4) return false;
	const AKAPNPC* N = StoryErlan();
	return !N || !N->bDead;
}

// =====================================================================
// Адамдар: Мәрия апа (Ерланның анасы), Сапар Аманов
// =====================================================================
void AKAPGameMode::StorySpawnPeople()
{
	// сессия басталды (жаңа ойын не жүктеу): бұрынғы сюжет нысандарын тазалап, тик керегін қайта қояды
	for (auto& A : StoryProps) if (A.IsValid()) A->Destroy();
	if (StoryRepeater.IsValid()) StoryRepeater->Destroy();
	StoryProps.Reset(); StoryRoadblockNPCs.Reset(); StoryHiredNPCs.Reset();
	bStoryRoadblockSpawned = bStoryHiredSpawned = bStoryBagsSpawned = bStoryLedgerSpawned = bStoryHoldWait = bStoryRoadblockAsked = false;
	const FKAPLocationRow* A = UKAPDataSubsystem::Get(this)->Loc("aksai");
	if (!A) return;
	// Мәрия апа — Ақсайдың шығыс жағындағы үйдің алдында (тірі кеніштің кейіпкерлері батыс жақта)
	const FVector2D Want(A->X + 28, A->Y - 4);
	AKAPStructure* Home = nullptr; float Best = 6000.f;
	for (TActorIterator<AKAPStructure> It(GetWorld()); It; ++It)
		if (It->Type == EStructType::House) { const float Dd = FVector2D::Distance(KAP::CM2M(It->GetActorLocation()), Want); if (Dd < Best) { Best = Dd; Home = *It; } }
	FVector2D P = Want, Face = FVector2D(A->X, A->Y);
	if (Home && Best < 60.f)
	{
		const FVector Rt = Home->GetActorRightVector();
		const FVector2D R2(Rt.X, Rt.Y), Hp = KAP::CM2M(Home->GetActorLocation());
		P = Hp + R2 * (Home->BuildSize.Y / 200.f + 2.6f);
		Face = P + R2 * 10.f;
		if (Builder)
		{
			const FVector2D Side(R2.Y, -R2.X);
			if (AKAPStructure* Tb = Builder->SpawnStruct((int32)EStructType::Crate, P + Side * 1.3f - R2 * 0.4f, FMath::RadiansToDegrees(FMath::Atan2(R2.Y, R2.X)), FVector(1), FLinearColor::Gray, 44)) StoryProps.Add(Tb);
		}
	}
	if (AKAPNPC* M = SpawnNPC((int32)ENPCKind::Mother, P, TEXT("Мәрия апа"), true))
	{
		M->TalkId = "mariya"; M->WanderR = 1.5f; M->Home = P; M->FaceTo(Face);
		NamedNPC.Add("mariya", M);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY spawn people: mariya=(%.0f %.0f) house=%d erlan=%d"), P.X, P.Y, Home && Best < 60.f ? 1 : 0, StoryErlanIndex());
}

// =====================================================================
// Тапсырма мәтіндері
// =====================================================================
FString AKAPGameMode::StoryProgress(FName Id) const
{
	const FKAPSession& Ss = S();
	if (Id == "st_erlan") return StoryErlanAlive() ? TEXT("№3 ұңғымадағы Ерланмен сөйлесіңіз") : TEXT("№3 ұңғымаға барыңыз");
	if (Id == "story_erlan_family") return !Fl(Ss, TEXT("st_mother_met")) ? TEXT("Ақсайдағы Мәрия апаға барыңыз") : Fl(Ss, TEXT("st_shirt_taken")) && !Fl(Ss, TEXT("st_shirt_delivered")) ? TEXT("Жейдені №3-тегі Ерланға апарыңыз") : TEXT("Ерланға барыңыз");
	if (Id == "story_gate_testimony") return TEXT("Күзет бекетіндегі Бекзатпен сөйлесіңіз");
	if (Id == "story_roadblock")
	{
		int32 N = 0; for (const auto& W : StoryRoadblockNPCs) if (W.IsValid() && !W->bDead) N++;
		return FString::Printf(TEXT("басқарма қақпасындағы бөгет · %d адам"), N);
	}
	if (Id == "story_director_call") return TEXT("Кеңседегі телефонға жауап беріңіз (үстел)");
	if (Id == "story_repeater") return Count("cable") >= 1 ? TEXT("3-аймақ мұнарасындағы ретрансляторды қосыңыз") : TEXT("Кабель керек (0/1) → 3-аймақ мұнарасы");
	if (Id == "story_bags") return FString::Printf(TEXT("%d сөмке → Ақсайдағы Төлеген ақсақал"), Ss.QuestData.FindRef("story_bags"));
	return FString();
}

FString AKAPGameMode::StoryGuidance() const
{
	const FKAPSession& Ss = S();
	if (QuestState("story_gate_testimony") == EQuestState::Active) return TEXT("Күзет бекетіндегі Бекзатқа қарап E басыңыз. Сұрақтарыңызды қойып, басқарма рұқсатын алыңыз.");
	if (QuestState("story_roadblock") == EQuestState::Active) return TEXT("Басқарма қақпасының алдындағы бөгетке жақындаңыз. Жол ақысын төлеуге, Бекзаттың атын атауға (ол жол ашуға келіссе) немесе күшпен өтуге болады.");
	if (QuestState("m_hq") == EQuestState::Active && Fl(Ss, TEXT("st_office_key"))) return TEXT("Басқарма қақпасына қарап E басып ашыңыз да, кеңсеге кіріңіз.");
	if (QuestState("story_director_call") == EQuestState::Active) return TEXT("Кеңсе үстеліндегі шырылдаған телефонға қарап E басыңыз.");
	if (QuestState("story_repeater") == EQuestState::Active) return TEXT("3-аймақ мұнарасына барыңыз. Кабель ×1 керек. Ретрансляторды директор жалдаған адамдар күзетіп тұр — абай болыңыз.");
	if (QuestState("m_report") == EQuestState::Active && Fl(Ss, TEXT("st_journal"))) return TEXT("Басқарма радиосына барыңыз: алдымен есепке не кіретінін қарайсыз, сосын техникалық шарттар тексеріледі.");
	if (QuestState("st_erlan") == EQuestState::Active) return TEXT("№3 ұңғымаға барып, Ерланға қарап E басыңыз. Ұңғыманың құбырын да жөндеңіз.");
	return FString();
}

FString AKAPGameMode::StoryObjective() const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	static const TCHAR* Order[] = { TEXT("story_gate_testimony"), TEXT("story_roadblock"), TEXT("story_director_call"), TEXT("story_repeater"), TEXT("st_erlan") };
	for (const TCHAR* Id : Order)
		if (QuestState(Id) == EQuestState::Active)
		{
			const FKAPQuestRow* Q = D ? D->Quest(Id) : nullptr;
			return (Q ? Q->Title : FString(Id)) + TEXT(" · ") + StoryProgress(Id);
		}
	return FString();
}

// =====================================================================
// Тик: сахналардың шарттары, зұлымдардың әрекеттері, кедергілер
// =====================================================================
void AKAPGameMode::StoryExpTick(float Dt)
{
	if (bInMenu || bEnded || bPaused || Cine.IsSet()) return;
	FKAPSession& Ss = S();
	AKAPCharacter* P = Player();
	AKAPHUD* H = AKAPHUD::Get(this);
	if (!P || !H) return;
	const bool bDlg = H->IsDialogOpen();
	// S01 оқып тұрғанда дауылды күту уақыты тоқтайды
	if (bStoryHoldWait) { if (bDlg) WaitT += Dt; else bStoryHoldWait = false; }
	StoryAcc += Dt;
	if (StoryAcc < 0.5f) return;
	const float Step = StoryAcc; StoryAcc = 0.f;
	const bool bFree = !H->IsModal();
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	auto L = [&](FName Id) { const FKAPLocationRow* R = D->Loc(Id); return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };

	// ---- S01. Рациядағы бос орын (дауылды күту кезінде) ----
	if (QuestState("m_wait") == EQuestState::Active && !Fl(Ss, TEXT("st_S01")) && WaitT < 26 && WaitT > 4 && bFree)
	{
		SetFl(Ss, TEXT("st_S01"));
		bStoryHoldWait = true;
		Radio(TEXT("№3, жауап беріңіз. Кезек журналына кім қол қойды?"), "warn");
		SDialog(TEXT("Рация · кеніш арнасы"),
			TEXT("[Планшетте «Ауысым қалыпты» деген жасыл белгі жанып тұр. Терезе сыртында сирена.]\n\n")
			TEXT("Диспетчер: №3, жауап беріңіз. Кезек журналына кім қол қойды?\n")
			TEXT("Жас жігіттің дауысы (жазба): Мен жұмыстамын. Адамдар әлі іште.\n")
			TEXT("Белгісіз ер дауысы: Кезекті жаппаңдар. Таңертең топ келеді.\n\n")
			TEXT("[Сигнал үзіледі. Жалпы журналда «Топ жіберілді» деген жол бар, бірақ жіберілген уақыты бос.]"),
			{ { TEXT("Дауыл басылса, жөндеуге кірісемін."), [this]() { bStoryHoldWait = false; }, false },
			  { TEXT("Жіберілген топты нақтылауды сұраймын."), [this]() { bStoryHoldWait = false; SetFl(S(), TEXT("st_clarify")); Toast(TEXT("Диспетчерге сұрау жіберілді"), "info"); }, false } });
		return;
	}

	// ---- Диспетчер: №3 ұңғыма мен Ерлан (күзеттен кейін) ----
	if (QuestState("m_wells") == EQuestState::Active && QuestState("st_erlan") == EQuestState::None)
	{
		double& T = Ss.Flags.FindOrAdd("st_dispT"); T += Step;
		if (T > 25)
		{
			if (Fl(Ss, TEXT("st_clarify"))) Radio(TEXT("Диспетчер: топтың шыққан уақыты көрсетілмеген. Қайта сұрау жібердім."), "info");
			Radio(TEXT("Диспетчер: №3 ұңғымада құбыр ағып тұр. Ерлан Сейітов сонда, жауап бермей жатыр. Соғып шығыңызшы."), "warn");
			StartQuest("st_erlan");
			if (WM->Wells.IsValidIndex(2)) StoryTarget(WM->Wells[2].PosM(), TEXT("№3 ұңғыма · Ерлан"));
		}
	}
	// Ерлан ерте қаза тапса: тірі сахна жоқ, тек сөмкесі
	if (QuestState("st_erlan") == EQuestState::Active && !StoryErlanAlive() && WM->Wells.IsValidIndex(2) && FVector2D::Distance(Pm, WM->Wells[2].PosM()) < 25)
	{
		SetFl(Ss, TEXT("st_erlan_dead_early"));
		CompleteQuest("st_erlan");
		Radio(TEXT("Бұл Ерлан Сейітовтің сөмкесі. №3-те жұмыс істеген."), "info");
		StartQuest("story_erlan_family");
	}

	// ---- S06-дан кейін: Бекзат жұртқа өзі айтады ----
	if (Fl(Ss, TEXT("st_bekzat_public")) && !Fl(Ss, TEXT("st_bekzat_public_done")))
	{
		double& T = Ss.Flags.FindOrAdd("st_bekzat_publicT"); T += Step;
		if (T > 90) { SetFl(Ss, TEXT("st_bekzat_public_done")); Radio(TEXT("Бекзат: жұмысшыларды жинадым. Басшылардың көлігін мен өткізгенімді өзім айттым. Ұрысқандар болды. Бірақ енді бәрі біледі."), "info"); }
	}

	// ---- S08. Нұрлан куәгер (Жосалыға жеткен соң) ----
	if (Fl(Ss, TEXT("missingFound")) && !Fl(Ss, TEXT("st_S08")) && bFree)
	{
		SetFl(Ss, TEXT("st_S08"));
		TalkNPC = NamedNPC.FindRef("nurlan");
		SDialog(TEXT("Нұрлан Есенов"),
			TEXT("[Жосалының шетінде аяғын созып отырады.]\nРақмет... Бір нәрсе айтайын: апат түні басқарманың көліктері ашық хабардан бұрын кетті. Өз көзіммен көрдім.\n")
			TEXT("Қараңғыда сағатқа қарамадым. Таң ата электрикпен сөйлестім. Уақытын содан тексеріңдер."),
			{ { TEXT("Атыңды жариялауға келісесің бе?"), [this]() { SetFl(S(), TEXT("st_nurlan_witness")); SDialog(TEXT("Нұрлан Есенов"), TEXT("Әзірге жазбаңдар. Айтқанымды тексеріңдер."), { { TEXT("Келісімсіз атың шықпайды."), nullptr, false } }); }, false },
			  { TEXT("Атыңды айтпаймын. Уақытты құжаттан тексереміз."), [this]() { SetFl(S(), TEXT("st_nurlan_witness")); Toast(TEXT("Нұрланның айғағы: басқарма көліктері хабардан бұрын кеткен. Уақытын тексеру керек."), "info"); }, false } });
		return;
	}

	// ---- Сапар Аманов: Қаражар лагері (S08B) ----
	if (Fl(Ss, TEXT("banditSpawned")) && !Fl(Ss, TEXT("banditCleared")) && !Fl(Ss, TEXT("st_sapar_dead")) && !NamedNPC.FindRef("sapar").IsValid() && FVector2D::Distance(Pm, L("bandit")) < 330)
		StorySpawnSapar();
	if (AKAPNPC* Sp = NamedNPC.FindRef("sapar").Get())
		if (!Sp->bDead && !Fl(Ss, TEXT("st_S08B")) && bFree && Sp->Hp >= Sp->MaxHp && FVector2D::Distance(Pm, KAP::CM2M(Sp->GetActorLocation())) < 115)
		{
			SetFl(Ss, TEXT("st_S08B"));
			StorySaparTalk();
			return;
		}
	if (Fl(Ss, TEXT("banditCleared")) && !bStoryBagsSpawned && Builder)
	{
		// лагерь тазартылды: жұмысшылардың сөмкелері (алынбағандары ғана, жүктегенде де)
		bStoryBagsSpawned = true;
		const int32 Bits = Ss.QuestData.FindRef("story_bags_bits");
		const FVector2D C = L("bandit");
		static const FVector2D Off[3] = { FVector2D(-6, 5), FVector2D(4, 7), FVector2D(8, -3) };
		for (int32 k = 0; k < 3; k++)
		{
			if (Bits & (1 << k)) continue;
			const FVector2D Bp = C + Off[k];
			if (AKAPInteractSpot* Sp = Builder->SpawnSpot(FName(*FString::Printf(TEXT("story:bag%d"), k)), FVector(Bp.X * 100, Bp.Y * 100, Builder->Z(Bp) + 20), 200, true)) StoryProps.Add(Sp);
		}
		if (!Fl(Ss, TEXT("st_bags_seen"))) { SetFl(Ss, TEXT("st_bags_seen")); Toast(TEXT("Лагерьде жұмысшылардың аты жазылған құрал сөмкелері жатыр"), "info"); }
	}
	if (Fl(Ss, TEXT("st_sapar_dead")) && !Fl(Ss, TEXT("st_ledger")) && !bStoryLedgerSpawned && Builder)
	{
		bStoryLedgerSpawned = true;
		const FVector2D Lp = L("bandit") + FVector2D(0, 10);
		if (AKAPInteractSpot* Sp = Builder->SpawnSpot("story:ledger", FVector(Lp.X * 100, Lp.Y * 100, Builder->Z(Lp) + 20), 220, true)) StoryProps.Add(Sp);
	}

	// ---- Сапардың жол бөгеті (басқарма қақпасы) ----
	if (QuestState("story_roadblock") == EQuestState::Active)
	{
		if (!bStoryRoadblockSpawned) StorySpawnRoadblock();
		if (AKAPNPC* Lt = StoryRoadblockNPCs.Num() ? StoryRoadblockNPCs[0].Get() : nullptr)
		{
			const float Dd = FVector2D::Distance(Pm, KAP::CM2M(Lt->GetActorLocation()));
			if (Dd > 70) bStoryRoadblockAsked = false;
			if (!Lt->bDead && Lt->Faction != EKAPFaction::Bandit && !bStoryRoadblockAsked && Dd < 38 && bFree) { bStoryRoadblockAsked = true; StoryRoadblockTalk(Lt); return; }
		}
	}

	// ---- Директордың қоңырауы: телефон шырылдайды ----
	if (QuestState("story_director_call") == EQuestState::Active && FVector2D::Distance(Pm, L("hq")) < 60)
	{
		StoryRingT -= Step;
		if (StoryRingT <= 0) { StoryRingT = 6.f; if (UKAPAudio* A = UKAPAudio::Get(this)) { A->Beep(true); FTimerHandle Hh; GetWorldTimerManager().SetTimer(Hh, FTimerDelegate::CreateWeakLambda(this, [this]() { if (UKAPAudio* A2 = UKAPAudio::Get(this)) A2->Beep(true); }), 0.35f, false); } }
	}

	// ---- Үзілген арна: ретранслятор және директордың жалдамалы күзеті ----
	if (QuestState("story_repeater") == EQuestState::Active && Builder)
	{
		if (!StoryRepeater.IsValid())
		{
			const FVector2D R = TowerRepeater + FVector2D(4, 4);
			StoryRepeater = Builder->SpawnSpot("story:repeater", FVector(R.X * 100, R.Y * 100, Builder->Z(R) + 100), 260, true);
		}
		if (!bStoryHiredSpawned && !Fl(Ss, TEXT("st_hired_cleared")))
		{
			bStoryHiredSpawned = true;
			for (int32 i = 0; i < 3; i++)
				if (AKAPNPC* G = SpawnNPC((int32)ENPCKind::Bandit, TowerRepeater + FVector2D(-12 + i * 12, (i % 2) ? 10 : -9), TEXT("Жалдамалы күзетші"), true))
				{ G->PatrolR = 14; G->TalkId = NAME_None; StoryHiredNPCs.Add(G); }
		}
		if (!Fl(Ss, TEXT("st_serik_warn")) && FVector2D::Distance(Pm, TowerRepeater) < 170)
		{
			SetFl(Ss, TEXT("st_serik_warn"));
			Radio(TEXT("Белгісіз нөмір: ретрансляторға тиіспеңіз, инженер. Ол — компания мүлкі. Күзет заңды. Есепті мен өзім жіберемін."), "danger");
		}
	}
	else if (StoryRepeater.IsValid()) StoryRepeater->Destroy();
}

// =====================================================================
// Диалогтар
// =====================================================================
bool AKAPGameMode::StoryTalk(AKAPNPC* N)
{
	if (bStoryBypass || !N) return false;
	TalkNPC = N;
	FKAPSession& Ss = S();
	const FName T = N->TalkId;
	TWeakObjectPtr<AKAPNPC> WN = N;
	auto Bypass = [this, WN]() { if (AKAPNPC* X = WN.Get()) { bStoryBypass = true; Talk(X); bStoryBypass = false; } };

	if (T == "mariya") { StoryMariyaTalk(N); return true; }
	if (T == "sapar_post") { StoryRoadblockTalk(N); return true; }
	if (T == "worker" && N->WorkerIndex >= 0 && N->WorkerIndex == StoryErlanIndex() && StoryErlanAlive()) { StoryErlanTalk(N); return true; }
	if (T == "bekzat")
	{
		if (QuestState("story_gate_testimony") == EQuestState::Active) { StoryBekzatS06(N); return true; }
		if (Fl(Ss, TEXT("st_S06_q2")) && (!Fl(Ss, TEXT("st_bekzat_testimony")) || !Fl(Ss, TEXT("st_bekzat_public")) || !Fl(Ss, TEXT("st_bekzat_road"))))
		{
			StoryBekzatS06(N); return true;
		}
		return false;
	}
	if (T == "aigerim" && Fl(Ss, TEXT("st_erlan_met")) && !Fl(Ss, TEXT("st_S04")) && StoryErlanAlive())
	{
		SetFl(Ss, TEXT("st_S04"));
		const bool bPower = Fl(Ss, TEXT("clinicPower"));
		if (bPower) Subtitle(TEXT("Енді шуына шағымданбаңдар. Кеше тым тыныш болды."), TEXT("Теперь на шум не жалуйтесь. Вчера слишком тихо было."), TEXT("Механик Рүстем"), 4.f);
		SDialog(N->DisplayName,
			FString(bPower ? TEXT("[Генератор қосылғанда бөлме тынды. Айгерім шамға емес, адамдардың жүзіне қарайды. Сосын сізді оңаша шақырады.]\n")
				: TEXT("[Фонарьдың жарығында дәптерін жабады да, сізді оңаша шақырады.]\n"))
			+ TEXT("Ерланды көрдің ғой. Оның жағдайы ауыр. Мен «бәрі жақсы» деп айта алмаймын.\nБірақ қазір бізді естиді. Айтатын сөзің болса, кейінге қалдырма."),
			{ { TEXT("Түсіндім. Оған барамын."), nullptr, false }, { TEXT("Басқа сұрағым бар."), Bypass, false } });
		return true;
	}
	if (T == "daniyar" && Fl(Ss, TEXT("manifestHidden")) && (!Fl(Ss, TEXT("st_manifest_corrected")) || (Fl(Ss, TEXT("st_refund_due")) && Ss.Money >= 500)))
	{
		TArray<FKAPDialogOption> O;
		if (!Fl(Ss, TEXT("st_manifest_corrected")))
			O.Add({ TEXT("Кешегі мәліметті түзетеміз. Бастапқы көшірме де қалады."), [this]()
			{
				FKAPSession& S2 = S();
				SetFl(S2, TEXT("st_manifest_corrected"));
				if (S2.Money >= 500) AddMoney(-500, TEXT("Даниярға артық ақы қайтарылды")); else { SetFl(S2, TEXT("st_refund_due")); Toast(TEXT("Ақша жетпеді: 500 ₸ қарыз қалды. Түзету бәрібір тіркелді."), "warn"); }
				S2.RepMilitia = FMath::Min(100.f, S2.RepMilitia + 5);
				Radio(TEXT("КПП: жүк тізіміне түзету енгізілді. Бастапқы көшірме мен түзету уақыты бірге сақталды."), "info");
				SDialog(TEXT("Жүргізуші Данияр"), TEXT("[Қолы сәл дірілдейді.]\nЖарайды. Қай жеріне қол қоямын?\n…Балаларымның дерегін қоспаңдар. Қалғанын өзім түсіндіремін."), { { TEXT("Қол қойыңыз. Сіздің атыңыз жарияланбайды."), nullptr, false } });
			}, false });
		else
			O.Add({ TEXT("Қарызды қайтарамын (500 ₸)."), [this]() { S().Flags.Remove("st_refund_due"); AddMoney(-500, TEXT("Даниярға артық ақы қайтарылды")); }, false });
		O.Add({ TEXT("Басқа сұрағым бар."), Bypass, false });
		SDialog(N->DisplayName, TEXT("[Көзін тайдырады. Қолындағы құжаттың бұрышын мыжып отыр.]\nЕнді екеуміз де қағазға қарамаймыз, солай ма?"), MoveTemp(O));
		return true;
	}
	if (T == "maira" && Fl(Ss, TEXT("samplesLogged")) && !Fl(Ss, TEXT("st_S07")))
	{
		SDialog(N->DisplayName, TEXT("[Үлгілердің жері мен уақытын қайта тексеріп отыр.]"),
			{ { TEXT("Бәрін дәлелдедік пе?"), [this, Bypass]()
			{
				FKAPSession& S2 = S();
				SetFl(S2, TEXT("st_S07"));
				FString Tx = TEXT("Судың осы нүктелерде қандай болғанын тіркедік. Кім бұйрық бергені бұл бөтелкелерде жазылмаған.");
				if (Fl(S2, TEXT("manifestHidden")) && !Fl(S2, TEXT("st_manifest_corrected"))) Tx += TEXT("\nЖүк тізімі «тапсырма аяқталды» деп белгіленген екен. Маған аяқталған тапсырма емес, өзгертілмеген көшірме керек.");
				else if (QuestState("l_manifest") == EQuestState::Done) Tx += TEXT("\nЖүк тізімімен салыстырдым. Пломбадағы сәйкессіздік пен су өлшемдері бір күнге түседі.");
				Tx += TEXT("\n\n[Телефонына ұлынан хабар келеді: «Билет алып қойдым». Ол экранды өшіріп, жауап жазады.]\nКетемін. Бірақ бүгінгі өлшемімді біреудің ертең өшіруіне қалдырмаймын.");
				SDialog(TEXT("Зертханашы Майра"), Tx, { { TEXT("Өлшемдер сақталады."), nullptr, false }, { TEXT("Басқа сұрағым бар."), Bypass, false } });
			}, false }, { TEXT("Басқа сұрағым бар."), Bypass, false } });
		return true;
	}
	if (T == "toleg" && QuestState("story_bags") == EQuestState::Active && Ss.QuestData.FindRef("story_bags") > 0)
	{
		SDialog(N->DisplayName, TEXT("[Сізге, сосын қолыңыздағы сөмкелерге қарайды.]\nБұл не, балам?"),
			{ { TEXT("Қаражардан жұмысшылардың сөмкелерін әкелдім."), [this]()
			{
				FKAPSession& S2 = S();
				const int32 Nb = S2.QuestData.FindRef("story_bags");
				CompleteQuest("story_bags");
				S2.RepMilitia = FMath::Min(100.f, S2.RepMilitia + 10.f * Nb);
				SDialog(TEXT("Ақсақал Төлеген"), FString::Printf(TEXT("[Сөмкелердегі аттарды бір-бірлеп оқиды. %d сөмке.]\nМынаның иесін әйелі таниды. Мынау — Жосалының жігітінікі... Иесі жоқтары да бар. Аттарын өшірмейміз.\nСапар су бергенде ақысын осылай алған екен. Ауыл мұны ұмытпайды."), Nb),
					{ { TEXT("Иелеріне жеткізіңіздер."), nullptr, false } });
				if (const AKAPNPC* Dn = NamedNPC.FindRef("daniyar").Get()) if (!Dn->bDead) Subtitle(TEXT("Мына сөмкені иесі таныса болды. Менің атымды айтпай-ақ қой."), TEXT("Главное, чтобы хозяин узнал свою сумку. Моё имя можно не называть."), TEXT("Жүргізуші Данияр"), 5.f);
			}, false }, { TEXT("Басқа әңгіме."), Bypass, false } });
		return true;
	}
	return false;
}

// S02 → жейде → S11
void AKAPGameMode::StoryErlanTalk(AKAPNPC* N)
{
	FKAPSession& Ss = S();
	TWeakObjectPtr<AKAPNPC> WN = N;
	auto Bypass = [this, WN]() { if (AKAPNPC* X = WN.Get()) { bStoryBypass = true; Talk(X); bStoryBypass = false; } };
	const FString Nm = TEXT("Ерлан Сейітов");
	// ---- S02. Он тоғыздағы шебер ----
	if (!Fl(Ss, TEXT("st_erlan_met")))
	{
		auto Future = [this, Nm]()
		{
			SDialog(Nm, TEXT("Қыста оқуға қайта тапсырамын. Гараж ашсам деймін. Әзірге атын ойлап қойдым."),
				{ { TEXT("Қандай?"), [this, Nm]()
				{
					SDialog(Nm, TEXT("«Ертең дайын». Ауылда бәрібір соны айтады ғой.\n[Планшетіңізге айналма жолды белгілейді.]\nҚақпақты жаппай кетпеңіз. Жел қайта ашады.\nАнам Ақсайда тұрады — Мәрия. Көрсеңіз, сәлем айтыңыз. Тек «үйге қайт» демесін."),
						{ { TEXT("Сәлеміңді жеткіземін."), [this]()
						{
							FKAPSession& S2 = S();
							SetFl(S2, TEXT("st_erlan_met")); SetFl(S2, TEXT("st_S02"));
							if (QuestState("st_erlan") == EQuestState::Active) CompleteQuest("st_erlan");
							StartQuest("story_erlan_family");
							if (const AKAPNPC* M = NamedNPC.FindRef("mariya").Get()) StoryTarget(KAP::CM2M(M->GetActorLocation()), TEXT("Мәрия апа · Ақсай"));
							Save(true);
						}, false } });
				}, false } });
		};
		SDialog(Nm, TEXT("[Құрал сөмкесінің үзілген бауын бекітіп отыр. Сіздің кілтіңізге көз салады.]\nМынаның сабы зауыттан осындай ма, әлде сіз де құлатып алғансыз ба?"),
			{ { TEXT("Есікті ұстап қалуға қолдандым."), [this, Nm, Future]()
			{
				SDialog(Nm, TEXT("Онда жақсы құрал екен. Біздікі өз жұмысын да әрең істейді.\n[Орнынан тез тұрады да, бүйірін ұстап, ауырсынғанын жасырады.]\nЖарақат алғанмын. Айгерім көрді. Апат түні адамдарды шығарып жүргенде..."),
					{ { TEXT("Сөмкеңнің бауын бірге бекітейік."), [this, Future]()
					{
						if (AKAPHUD* H = AKAPHUD::Get(this)) H->StartMinigame(TEXT("Ерланның сөмкесі"), { TEXT("Бауды тігу"), TEXT("Түйінді тарту") }, [this, Future](bool bOk)
						{
							if (bOk) { SetFl(S(), TEXT("st_erlan_help")); Subtitle(TEXT("Міне, енді жыл шыдайды. Рақмет."), TEXT("Вот, теперь год продержится. Спасибо."), TEXT("Ерлан Сейітов"), 4.f); }
							Future();
						});
					}, false },
					  { TEXT("Жұмыстан бөлек немен айналысасың?"), Future, false } });
			}, false } });
		return;
	}
	const bool bShirtPending = Fl(Ss, TEXT("st_shirt_taken")) && !Fl(Ss, TEXT("st_shirt_delivered"));
	const bool bAct3 = QuestState("m_cause") == EQuestState::Done;
	const FKAPWorker& K = Ss.Workers[N->WorkerIndex];
	TArray<FKAPDialogOption> O;
	if (bShirtPending)
		O.Add({ TEXT("Анаң жейде беріп жіберді."), [this, Nm]()
		{
			FKAPSession& S2 = S();
			SetFl(S2, TEXT("st_shirt_delivered"));
			if (QuestState("story_erlan_family") == EQuestState::Active) CompleteQuest("story_erlan_family");
			SDialog(Nm, TEXT("[Жейдені ашқанда қалтасынан жіп орамы түседі. Ұзақ қарап қалады.]\nБәрін тігіп жүреді. Бала кезде шалбарымды әдейі жыртсам да, жөндеп беретін.\n[Жейдені киеді.]"),
				{ { TEXT("Саған жарасады."), nullptr, false } });
		}, false });
	// ---- S11. Қайтуға арналған жейде (III акт) ----
	if (bAct3 && !Fl(Ss, TEXT("st_S11")))
		O.Add({ TEXT("Қалайсың, Ерлан?"), [this, Nm]()
		{
			const bool bSeen = Fl(S(), TEXT("st_S02"));
			const bool bShirt = Fl(S(), TEXT("st_shirt_delivered"));
			SDialog(Nm, FString(bShirt ? TEXT("[Анасы берген жейденің түймесін қадап отыр. Саусағы бағынбай, көмек сұрайды. Бұл жолы «өзім істеймін» демейді.]\n")
				: TEXT("[Жұмыс киімінің түймесін қадап отыр. Саусағы бағынбай, көмек сұрайды.]\n")) + TEXT("Көмектесіңізші... Гараждың атын өзгерттім."),
				{ { bSeen ? TEXT("«Бүгін дайын» ба?") : TEXT("Қандай ат қоясың?"), [this, Nm, bSeen]()
				{
					SDialog(Nm, FString(bSeen ? TEXT("Жоқ. Атымды ғана жазамын. ") : TEXT("Атымды ғана жазамын. ")) + TEXT("Бірдеңе дұрыс болмаса, кімді іздеу керегін білсін.\nАнам ұрысса, тыңдай салыңызшы. Қорыққанда солай сөйлейді."),
						{ { TEXT("Өзің де айтарсың."), [this, Nm]() { SetFl(S(), TEXT("st_S11")); SDialog(Nm, TEXT("[Әлсіз күлімсірейді.]\nАйтамын. Бірақ сіз де естіп тұрыңыз."), { { TEXT("Естимін."), nullptr, false } }); }, false },
						  { TEXT("Жанында боламын."), [this, Nm]() { SetFl(S(), TEXT("st_S11")); SetFl(S(), TEXT("st_stay_promise")); SDialog(Nm, TEXT("Рақмет. Ол жалғыз отырмасын."), { { TEXT("Жалғыз қалмайды."), nullptr, false } }); }, false },
						  { TEXT("Айтқың келгенді жазып қояйық."), [this, Nm]()
						  {
							  FKAPSession& S2 = S();
							  SetFl(S2, TEXT("st_S11")); SetFl(S2, TEXT("st_erlan_message"), Fl(S2, TEXT("st_shirt_delivered")) ? 2.0 : 1.0);
							  SDialog(Nm, FString(TEXT("[Планшетке жазады. Дауысы бірқалыпты.]\n«")) + (Fl(S2, TEXT("st_shirt_delivered")) ? ErlanMsgShirt : ErlanMsgNoShirt) + TEXT("»"),
								  { { TEXT("Жазба сақталды."), nullptr, false } });
						  }, false } });
				}, false } });
		}, false });
	O.Add({ TEXT("Жағдайың, дозаң қалай? (дәрі, эвакуация)"), Bypass, false });
	O.Add({ TEXT("Кейін соғамын."), nullptr, false });
	static const TCHAR* Say[4] = { TEXT("Сау-саламатпын. Тек бүйірім сыздайды."), TEXT("Басым айналып тұр... бірақ жұмыс тоқтамасын."), TEXT("Отыра тұрайын. Аяғым бағынбай жатыр."), TEXT("...(әрең дем алады, бірақ сізді таниды)") };
	SDialog(Nm, FString::Printf(TEXT("%s\n%s"), bShirtPending ? TEXT("[Сөмкенің бауы бекітулі. Сізді көріп күлімсірейді.]") : TEXT("[Құралын сүртіп отыр.]"), Say[FMath::Clamp(K.State, 0, 3)]), MoveTemp(O));
}

// S03. Екі кесе
void AKAPGameMode::StoryMariyaTalk(AKAPNPC* N)
{
	FKAPSession& Ss = S();
	const FString Nm = TEXT("Мәрия апа");
	const bool bAlive = StoryErlanAlive();
	if (!Fl(Ss, TEXT("st_mother_met")))
	{
		auto Finish = [this, Nm](bool bNoPromise)
		{
			FKAPSession& S2 = S();
			SetFl(S2, TEXT("st_mother_met")); SetFl(S2, TEXT("st_S03")); SetFl(S2, TEXT("st_shirt_taken"));
			StartQuest("story_erlan_family");
			Toast(TEXT("Мәрия апа: ") + GiveItems({ { "food", 1 }, { "water", 1 } }) + TEXT(" · Ерланның таза жейдесі"), "ok");
			if (bNoPromise) SDialog(Nm, TEXT("Уәде сұраған жоқпын, балам. Жалғыз қалмасын дедім."), { { TEXT("Жалғыз қалмайды."), nullptr, false } });
			if (AKAPNPC* E = StoryErlan()) StoryTarget(KAP::CM2M(E->GetActorLocation()), TEXT("Ерлан · №3 ұңғыма"));
			Save(true);
		};
		auto Ask = [this, Nm, Finish]()
		{
			SDialog(Nm, TEXT("[Үстелден таза жейдені алып, бүктеп береді.]\nМынаны апарып берші. Жейдесі сол біреу ғана.\n«Үйге қайт» десем, ұрысып қаламыз. Сен айтсаң, бәлкім тыңдар."),
				{ { TEXT("Жағдайын сұрап барамын."), [Finish]() { Finish(false); }, false },
				  { TEXT("Қайтып келетінін уәде ете алмаймын."), [Finish]() { Finish(true); }, false } });
		};
		if (!bAlive)
		{
			// Ерлан бұрын қаза тапқан: жеңіл әңгіме жоқ
			SDialog(Nm, TEXT("[Үйінің алдында құтқарушыларға тамақ жинап отыр. Жәшіктің үстінде екі кесе.]\nЕрланды көрдің бе?"),
				{ { TEXT("Мәрия апа... Ерлан қаза тапты. Жаныңызға отыруыма бола ма?"), [this, Nm]()
				{
					FKAPSession& S2 = S();
					SetFl(S2, TEXT("st_mother_met")); SetFl(S2, TEXT("st_S03")); SetFl(S2, TEXT("st_mother_news"));
					if (QuestState("story_erlan_family") == EQuestState::Active) CompleteQuest("story_erlan_family");
					SDialog(Nm, TEXT("Қашан?\n[Ұзақ үнсіздік. Кеселерді бір-біріне жақындатады.]\nСөмкесі табылса, алып келерсің. Ішінде өз сызған қағаздары бар."), { { TEXT("Алып келемін."), nullptr, false } });
				}, false } });
			return;
		}
		const bool bMet = Fl(Ss, TEXT("st_erlan_met"));
		SDialog(Nm, TEXT("[Үйінің алдында құтқарушыларға тамақ жинап отыр. Жәшіктің үстінде екі кесе тұр.]\nЕрланды көрдің бе? Қызыл сөмкесін сүйретіп жүр ме?"),
			{ bMet ? FKAPDialogOption{ TEXT("Көрдім. Бауы үзіліпті."), [this, Nm, Ask]() { SDialog(Nm, TEXT("Жаңа сөмке әперем десем, «гаражға жинап жүрмін» дейді. Бәріне өзі жеткісі келеді."), { { TEXT("Ол өзі мықты жігіт."), Ask, false } }); }, false }
				   : FKAPDialogOption{ TEXT("Әлі көрмедім. №3-ке барамын."), Ask, false } });
		return;
	}
	// кейінгі кездесулер
	FString Tx;
	if (Fl(Ss, TEXT("st_mother_news"))) Tx = TEXT("[Кеселерді жуып отыр. Сізге ештеңе демей, орын көрсетеді.]");
	else if (Fl(Ss, TEXT("st_shirt_delivered"))) Tx = TEXT("[Қантты майдалап сындырып отыр.]\nЖейдені киді ме? Жақсы. Енді гаражының есігін не түске бояйды екен...");
	else if (Fl(Ss, TEXT("st_shirt_taken"))) Tx = TEXT("Жейдені бердің бе? Асықпа, бірақ кешіктірме. Ол суыққа шыдамайды.");
	else Tx = TEXT("Тамақ ішіп кет. Аш жүрсең, ешкімге көмектесе алмайсың.");
	SDialog(Nm, Tx, { { TEXT("Сау болыңыз, апа."), nullptr, false } });
	(void)N;
}

// S06. Ашылған қақпа — басқарма рұқсаты енді Бекзаттың қолынан беріледі
void AKAPGameMode::StoryBekzatS06(AKAPNPC* N)
{
	FKAPSession& Ss = S();
	const FString Nm = N ? N->DisplayName : FString(TEXT("Күзетші Бекзат"));
	TWeakObjectPtr<AKAPNPC> WN = N;
	auto Again = [this, WN]() { StoryBekzatS06(WN.Get()); };
	const bool bQ1 = Fl(Ss, TEXT("st_S06_q1")), bQ2 = Fl(Ss, TEXT("st_S06_q2"));
	const bool bPending = QuestState("story_gate_testimony") == EQuestState::Active;
	TArray<FKAPDialogOption> O;
	if (!bQ1) O.Add({ TEXT("Оларды шығарғанда жұмысшыларға не айттыңыз?"), [this, Nm, Again]() { SetFl(S(), TEXT("st_S06_q1")); SDialog(Nm, TEXT("Орындарыңда қалыңдар дедім."), { { TEXT("..."), Again, false } }); }, false });
	else if (!bQ2) O.Add({ TEXT("Қауіп барын білдіңіз бе?"), [this, Nm, Again]()
	{
		SetFl(S(), TEXT("st_S06_q2"));
		SDialog(Nm, TEXT("Білдім. Қаншалық екенін білмедім. Бірақ олардың көлігін өткіздім. Өзгелерге рұқсат күттірдім.\n[Бұйрықты көрсетеді: «Басқарма көліктерін кедергісіз өткізу. Жұмысшыларға — кезекті аяқтау». Төменде өзінің қолы тұр.]"), { { TEXT("..."), Again, false } });
	}, false });
	if (bQ2)
	{
		if (!Fl(Ss, TEXT("st_bekzat_testimony"))) O.Add({ TEXT("Түпнұсқаны сақтаңыз. Қолыңыз да көрініп тұрсын."), [this, Nm, Again]()
		{
			SetFl(S(), TEXT("st_bekzat_testimony"));
			Toast(TEXT("Бекзаттың куәлігі мен қақпа бұйрығының түпнұсқасы сақталды"), "ok");
			SDialog(Nm, TEXT("[Бұйрықты конвертке салып, сізге көшірмесін береді.]\nКешір деп тұрған жоқпын. Қай жерде тұрғанымды жазып қой."), { { TEXT("Жазамын."), Again, false } });
		}, false });
		if (!Fl(Ss, TEXT("st_bekzat_public"))) O.Add({ TEXT("Жұртқа өзіңіз айтыңыз."), [this, Nm, Again]() { SetFl(S(), TEXT("st_bekzat_public")); SDialog(Nm, TEXT("[Ұзақ үнсіздік.]\nЕртең таңертең жұмысшыларды жинаймын. Өзім айтамын."), { { TEXT("..."), Again, false } }); }, false });
		if (!Fl(Ss, TEXT("st_bekzat_road"))) O.Add({ TEXT("Қалған адамдарға жол ашыңыз."), [this, Nm, Again]()
		{
			SetFl(S(), TEXT("st_bekzat_road"));
			SDialog(Nm, TEXT("Жолды қараймын. Қалған көліктерді адамдарға береміз.\nСапардың адамдары басқарма жақта жүр дейді. Менің жігіттерім сол жолда болады — керек болса, атымды ата."), { { TEXT("..."), Again, false } });
		}, false });
		if (!Fl(Ss, TEXT("st_S06_angry"))) O.Add({ TEXT("Сіз оларды өлімге қалдырдыңыз."), [this, Nm, Again]() { SetFl(S(), TEXT("st_S06_angry")); SDialog(Nm, TEXT("[Көзін тайдырмайды.]\nИә. Сондықтан осындамын. Ашулансаң да, рұқсатты саған беремін."), { { TEXT("..."), Again, false } }); }, false });
		if (bPending) O.Add({ TEXT("Рұқсатты беріңіз. Басқармаға барамын."), [this, Nm]()
		{
			FKAPSession& S2 = S();
			S2.bPermitAccess = true;
			SetFl(S2, TEXT("st_office_key"));
			for (auto& V : Builder->Vehicles) if (V.IsValid() && V->Id == "kamaz") V->State = "ok";
			CompleteQuest("story_gate_testimony");
			StartQuest("m_hq");
			StartQuest("story_roadblock");
			SDialog(Nm, TEXT("[Рұқсат қағазына қол қояды.]\nМә, басқарманың рұқсаты мен директор кабинетінің кілті. КамАЗ да сенің қолыңда.\nАбай бол: соңғы екі күнде қақпа маңында Сапардың адамдары жүр. Бос қоймаға көз салып жүр."),
				{ { TEXT("Түсіндім."), [this]()
				{
					StoryTarget(RoadblockC, TEXT("Басқарма қақпасы · бөгет"));
					FTimerHandle Hh;
					GetWorldTimerManager().SetTimer(Hh, FTimerDelegate::CreateWeakLambda(this, [this]()
					{
						Radio(TEXT("Белгісіз дауыс: инженер, мен Сапар Амановпын. Басқармаға баратын жол енді менің адамдарымның қолында. Сөйлесіп өтесің бе, әлде... өзің білесің."), "danger");
					}), 8.f, false);
				}, false } });
			SetCheckpoint(); Save(true);
		}, false });
	}
	O.Add({ TEXT("Кейін сөйлесеміз."), nullptr, false });
	FString Tx;
	if (bPending && !bQ1) Tx = TEXT("[Үстелде басқарма рұқсатының қағазы жатыр. Тізімнің жоғарғы жолында басшылардың кеткен уақыты: 03:40.]\nБес ұңғыма. Уәдемде тұрдым — рұқсат дайын. Бірақ қолымды қоймас бұрын бір нәрсені білуің керек.");
	else if (bPending) Tx = TEXT("[Бекзат қағаздан көзін алмай отыр.]");
	else Tx = TEXT("[Бекзат журналды жауып, сізге қарайды.]\nТағы не дейсің?");
	SDialog(Nm, Tx, MoveTemp(O));
}

// 5 ұңғыма жөнделді: рұқсатты бірден бермей, Бекзатқа шақырады (S06)
void AKAPGameMode::StoryPermitReady()
{
	FKAPSession& Ss = S();
	if (Ss.bPermitAccess || QuestState("story_gate_testimony") != EQuestState::None) return;
	StartQuest("story_gate_testimony");
	Radio(TEXT("Күзет: бес ұңғыма жөнделді. Бекзат сізді бекетке шақырады — басқарма рұқсатын өз қолымен бергісі келеді."), "info");
	if (const FKAPLocationRow* G = UKAPDataSubsystem::Get(this)->Loc("guard1")) StoryTarget(FVector2D(G->X, G->Y), TEXT("Күзет бекеті · Бекзат"));
}

// =====================================================================
// Сапардың жол бөгеті
// =====================================================================
void AKAPGameMode::StorySpawnRoadblock()
{
	if (!Builder) return;
	bStoryRoadblockSpawned = true;
	for (auto& A : StoryProps) if (A.IsValid() && A->ActorHasTag("story_roadblock")) A->Destroy(); // тек бөгеттің нысандары (Мәрияның жәшігі қалады)
	const FVector2D Along(FMath::Cos(FMath::DegreesToRadians(RoadblockYaw)), FMath::Sin(FMath::DegreesToRadians(RoadblockYaw)));
	const FVector2D Back(Along.Y, -Along.X); // қақпа жақ (оңтүстік)
	// бөгет жолдың үстінде тұруы керек: SpawnStruct жолдағы әшекейді өшіреді, сондықтан тікелей қоямыз
	for (float Sg : { -4.6f, 4.6f })
	{
		const FVector2D Bp = RoadblockC + Along * Sg;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AKAPStructure* B = GetWorld()->SpawnActor<AKAPStructure>(FVector(Bp.X * 100, Bp.Y * 100, Builder->Z(Bp) - 5), FRotator(0, RoadblockYaw, 0), SP))
		{
			B->Type = EStructType::Barrier; B->BuildSize = FVector(800, 60, 110);
			B->Build(EStructType::Barrier, B->BuildSize, FLinearColor(0.45f, 0.38f, 0.2f), 41);
			B->Tags.Add("story_roadblock"); StoryProps.Add(B);
		}
	}
	if (AKAPStructure* Cr = Builder->SpawnStruct((int32)EStructType::Crate, RoadblockC + Back * 3.f + Along * 9.f, 20.f, FVector(1), FLinearColor::Gray, 42)) { Cr->Tags.Add("story_roadblock"); StoryProps.Add(Cr); }
	if (AKAPStructure* Fire = Builder->SpawnStruct((int32)EStructType::Campfire, RoadblockC + Back * 5.f - Along * 8.f, 0.f, FVector(1), FLinearColor::White, 43)) { Fire->Tags.Add("story_roadblock"); StoryProps.Add(Fire); }
	StoryRoadblockNPCs.Reset();
	const FVector2D Pos[4] = { RoadblockC + Back * 2.5f, RoadblockC + Back * 4.f + Along * 5.f, RoadblockC + Back * 4.5f - Along * 5.f, RoadblockC + Back * 6.f + Along * 1.f };
	for (int32 i = 0; i < 4; i++)
		if (AKAPNPC* B = SpawnNPC((int32)ENPCKind::Bandit, Pos[i], i == 0 ? TEXT("Жақсылық · Сапардың адамы") : TEXT("Сапардың адамы"), true))
		{
			B->Faction = EKAPFaction::Civil; // сөйлесу аяқталғанша атпайды
			B->TalkId = i == 0 ? FName("sapar_post") : NAME_None;
			B->WanderR = 2.f; B->PatrolR = 3.f; B->Home = Pos[i];
			B->FaceTo(RoadblockC - Back * 30.f);
			StoryRoadblockNPCs.Add(B);
		}
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY roadblock spawned: %d"), StoryRoadblockNPCs.Num());
}

void AKAPGameMode::StoryRoadblockTalk(AKAPNPC* N)
{
	FKAPSession& Ss = S();
	TalkNPC = N;
	const FString Nm = TEXT("Жақсылық · Сапардың адамы");
	TArray<FKAPDialogOption> O;
	auto Leave = [this](const FString& Why)
	{
		for (auto& W : StoryRoadblockNPCs) if (AKAPNPC* X = W.Get()) if (!X->bDead)
		{
			X->TalkId = NAME_None; X->GoTo = FVector2D(-120, -470) + FVector2D(FMath::FRandRange(-10.f, 10.f), 0); X->bVanishOnArrive = true; X->bPersistent = false;
		}
		StoryResolveRoadblock(Why);
	};
	O.Add({ FString::Printf(TEXT("Жол ақысын төлеймін (1500 ₸, қолымда %d)"), Ss.Money), [this, Leave]()
	{
		AddMoney(-1500, TEXT("Сапарға «жол ақысы»"));
		SetFl(S(), TEXT("st_roadblock_paid"));
		Leave(TEXT("Сапар: ақыңды есепке жаздым, инженер. Менде бәрі жазулы."));
	}, Ss.Money < 1500 });
	if (Fl(Ss, TEXT("st_bekzat_road")))
		O.Add({ TEXT("Бекзаттың жігіттері осы жолда. Кетіңдер."), [this, Leave]()
		{
			SetFl(S(), TEXT("st_roadblock_bekzat"));
			Radio(TEXT("Бекзат: жігіттер шықты. Сапардың адамдары басқарма жолынан кетті."), "ok");
			Leave(TEXT("Сапар: Бекзат та батыр болып шықты ма? Жарайды. Қаражарда кездесеміз."));
		}, false });
	O.Add({ TEXT("Бұл жол ешкімдікі емес. Жолдан кетіңдер."), [this]()
	{
		SetFl(S(), TEXT("st_roadblock_fight"));
		for (auto& W : StoryRoadblockNPCs) if (AKAPNPC* X = W.Get()) if (!X->bDead) { X->Faction = EKAPFaction::Bandit; X->TalkId = NAME_None; if (AKAPCharacter* P = Player()) X->Alert(KAP::CM2M(P->GetActorLocation()), 12.f); }
		Radio(TEXT("Сапар: өзің таңдадың, инженер."), "danger");
	}, false });
	O.Add({ TEXT("Ойланып көремін."), nullptr, false });
	SDialog(Nm, TEXT("[Темір бөшкелер мен тақтайдан жасалған бөгет. Жігіттер қаруын жасырмайды.]\nТоқта, инженер. Сапар ағаның бұйрығы: басқарманың қоймасы енді біздің күзетте. Өту — ақылы.\nҚұралыңды қалдыр, не бір жарым мың теңге. Біз де ешкімнен тегін алған жоқпыз."), MoveTemp(O));
}

void AKAPGameMode::StoryResolveRoadblock(const FString& Line)
{
	if (QuestState("story_roadblock") != EQuestState::Active) return;
	CompleteQuest("story_roadblock");
	for (auto& A : StoryProps) if (A.IsValid() && A->ActorHasTag("story_roadblock")) A->Destroy(); // тек бөгеттің нысандары (Мәрияның жәшігі қалады)
	if (!Line.IsEmpty()) Radio(Line, "warn");
	if (QuestState("s_bandit") == EQuestState::None) StartQuest("s_bandit");
	StoryTarget(FVector2D(661.03, -745), TEXT("Басқарма қақпасы"));
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY roadblock resolved: paid=%d bekzat=%d fight=%d"), Fl(S(), TEXT("st_roadblock_paid")) ? 1 : 0, Fl(S(), TEXT("st_roadblock_bekzat")) ? 1 : 0, Fl(S(), TEXT("st_roadblock_fight")) ? 1 : 0);
	SetCheckpoint(); Save(true);
}

// =====================================================================
// Сапар Аманов — Қаражар лагерінің басшысы
// =====================================================================
void AKAPGameMode::StorySpawnSapar()
{
	const FKAPLocationRow* R = UKAPDataSubsystem::Get(this)->Loc("bandit");
	if (!R) return;
	if (AKAPNPC* Sp = SpawnNPC((int32)ENPCKind::Bandit, FVector2D(R->X, R->Y + 10), TEXT("Сапар Аманов"), true, 1.05f))
	{
		Sp->bCampMember = true; Sp->PatrolR = 6.f; Sp->Hp = Sp->MaxHp = 320.f; Sp->Dmg *= 1.4f; Sp->TalkId = NAME_None;
		NamedNPC.Add("sapar", Sp);
		UE_LOG(LogTemp, Display, TEXT("KAPSTORY sapar spawned"));
	}
}

void AKAPGameMode::StorySaparTalk()
{
	const FString Nm = TEXT("Сапар Аманов · алыстан");
	TalkNPC = NamedNPC.FindRef("sapar");
	SDialog(Nm, TEXT("[Лагерь шетінде жұмысшылардың аты жазылған құрал сөмкелері үйіліп жатыр. Біреу алыстан дауыстайды.]\nЖол керек болса, сөйлесеміз. Қолыңдағы құралды қалдырсаң, қай жермен өтуге болатынын айтамын."),
		{ { TEXT("Мыналар жұмысшылардың заттары."), [this, Nm]()
		{
			SDialog(Nm, TEXT("Өздері берді. Су керек болды. Мен бағасын жасырған жоқпын."),
				{ { TEXT("Таңдайтын басқа жолы болды ма?"), [this, Nm]()
				{
					SDialog(Nm, TEXT("Мен де ешкімнен тегін алған жоқпын.\n[Күліп, қаруын иығына іледі.]\nБасшыларың қашқанда кім су берді? Мен. Кім жол көрсетті? Мен. Енді сен келіп бәрін бұзғың келе ме?"),
						{ { TEXT("Кетемін. Әзірге."), [this]() { Toast(TEXT("Сапар сізді атпады. Лагерь күтіп тұр."), "info"); }, false },
						  { TEXT("Сенің «келісімің» бітті."), [this]() { Toast(TEXT("Сапар Аманов: «Онда кел, инженер»"), "danger"); }, false } });
				}, false } });
		}, false } });
}

void AKAPGameMode::StoryOnKilled(AKAPNPC* N, bool bByPlayer)
{
	if (!N) return;
	FKAPSession& Ss = S();
	if (NamedNPC.FindRef("sapar").Get() == N)
	{
		SetFl(Ss, TEXT("st_sapar_dead"));
		Radio(TEXT("Сапар Аманов қаза тапты. Лагерьде оның есеп дәптері жатыр."), "warn");
	}
	bool bWas = false, bAlive = false;
	for (auto& W : StoryRoadblockNPCs) if (W.Get() == N) bWas = true; else if (W.IsValid() && !W->bDead) bAlive = true;
	if (bWas && !bAlive) StoryResolveRoadblock(TEXT("Сапар: менің жігіттерімді өлтірдің... Мұны да есепке жаздым, инженер."));
	bool bHiredWas = false, bHiredAlive = false;
	for (auto& W : StoryHiredNPCs) if (W.Get() == N) bHiredWas = true; else if (W.IsValid() && !W->bDead) bHiredAlive = true;
	if (bHiredWas && !bHiredAlive) { SetFl(Ss, TEXT("st_hired_cleared")); Toast(TEXT("Ретрансляторды күзеткендер жоқ"), "ok"); }
	(void)bByPlayer;
}

// =====================================================================
// Әрекет нүктелері: қақпа, журнал, телефон, радио, ретранслятор, сөмкелер
// =====================================================================
FString AKAPGameMode::StorySpotText(const AKAPInteractSpot* Sp) const
{
	if (!Sp) return FString();
	const FString A = Sp->Action.ToString();
	if (A == TEXT("hq_gate") && QuestState("story_roadblock") == EQuestState::Active) return TEXT("Қақпа алдында Сапардың бөгеті тұр");
	if (A == TEXT("hq_desk") && QuestState("story_director_call") == EQuestState::Active) return TEXT("Шырылдаған телефонға жауап беру");
	if (A == TEXT("hq_radio") && QuestState("story_repeater") == EQuestState::Active) return TEXT("Есеп арнасы бұғатталған — ретранслятор");
	if (A == TEXT("story:repeater")) return QuestState("story_repeater") == EQuestState::Active ? TEXT("Ретрансляторды қосу (кабель ×1)") : FString();
	if (A.StartsWith(TEXT("story:bag"))) return TEXT("Жұмысшының құрал сөмкесін алу");
	if (A == TEXT("story:ledger")) return TEXT("Сапардың есеп дәптерін алу");
	return FString();
}

bool AKAPGameMode::StorySpotUse(AKAPInteractSpot* Sp)
{
	if (!Sp) return false;
	FKAPSession& Ss = S();
	const FString A = Sp->Action.ToString();
	if (A == TEXT("hq_gate") && QuestState("story_roadblock") == EQuestState::Active) { Toast(TEXT("Қақпа алдында Сапардың адамдары тұр — алдымен жолды босатыңыз"), "warn"); return true; }
	if (A == TEXT("hq_desk") && QuestState("m_cause") == EQuestState::Active) { StoryReadJournal(0); return true; }
	if (A == TEXT("hq_desk") && QuestState("story_director_call") == EQuestState::Active) { StoryDirectorCall(); return true; }
	if (A == TEXT("hq_radio") && QuestState("story_repeater") == EQuestState::Active) { Toast(TEXT("Арна директордың кодымен бұғатталған. 3-аймақ мұнарасындағы ретрансляторды қосыңыз."), "warn"); return true; }
	if (A == TEXT("hq_radio") && QuestState("m_report") == EQuestState::Active && Fl(Ss, TEXT("st_journal"))) { StoryReportReview(); return true; }
	if (A == TEXT("story:repeater"))
	{
		if (QuestState("story_repeater") != EQuestState::Active) return true;
		if (AKAPHUD* H = AKAPHUD::Get(this)) H->StartMinigame(TEXT("Ретранслятор"), { TEXT("wires"), TEXT("part:cable"), TEXT("Таратқышты қосу") }, [this](bool bOk) { if (bOk) StoryRepeaterDone(); });
		return true;
	}
	if (A.StartsWith(TEXT("story:bag")))
	{
		// иесінің аты — Қаражар маңындағы (4-аймақ) нақты жұмысшылардан
		const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
		const int32 Bag = FMath::Clamp(FCString::Atoi(*A.RightChop(9)), 0, 2);
		FString BagOwner = TEXT("аты өшіп қалған");
		int32 Seen = 0;
		for (const FKAPWorker& K : Ss.Workers) if (!K.bSpecial && WM->Wells.IsValidIndex(K.WellId - 1) && WM->Wells[K.WellId - 1].Row.Region == 3) { if (Seen++ == Bag * 3) { BagOwner = FString::Printf(TEXT("%s, №%d"), *K.Name, K.WellId); break; } }
		Ss.QuestData.FindOrAdd("story_bags_bits") |= 1 << Bag;
		Ss.QuestData.FindOrAdd("story_bags")++;
		StartQuest("story_bags");
		Toast(TEXT("Құрал сөмкесі: ") + BagOwner + TEXT(". Төлеген ақсақалға тапсырыңыз."), "ok");
		UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.6f);
		Sp->Destroy();
		return true;
	}
	if (A == TEXT("story:ledger"))
	{
		SetFl(Ss, TEXT("st_ledger"));
		Sp->Destroy();
		SDialog(TEXT("Сапардың есеп дәптері"),
			TEXT("Су · 2 канистр — №19 жұмысшысының кілт жиынтығы.\nЖол көрсету · Жосалыға — бір паспорт (қайтарылмаған).\nЖанармай · 40 л — КамАЗ аккумуляторы.\nДәрі · 1 қорап — неке жүзігі.\n\n")
			TEXT("Соңғы бетте: «Басқарма көліктерін көрдім — 3 шілде, 04:10. Апат туралы хабар — 06:30. Ешкім жұмысшыларды іздеген жоқ.»\n\n[Нұрланның айтқан уақытын растайды.]"),
			{ { TEXT("Дәптерді Әлияға апарамын."), nullptr, false } });
		return true;
	}
	return false;
}

// S09. Бос кеңсе — апат журналы, үш жазба, таныс формулировка
void AKAPGameMode::StoryReadJournal(int32 Page)
{
	FKAPSession& Ss = S();
	Ss.Inv.FindOrAdd("logbook") = 1;
	const FString Nm = TEXT("Апат журналы · QazUran (ойдан шығарылған құжат)");
	if (Page == 0)
	{
		SDialog(Nm,
			TEXT("[Бос орындықтар. Директор кабинетінде шай ізі қалған кесе, отбасылық сурет, «Ауысым тоқтатылмасын» деген қағаз.]\n\n")
			TEXT("14 маусым. Шығымды арттыру үшін ерітіндідегі қышқыл концентрациясы нормадан 40%-ға көтерілді. Бұйрық — басқармадан.\n")
			TEXT("21 маусым. №7, №13 ұңғымаларда қысым датчиктері ескерту берді. Тексеру «жоспар орындалғанша» кейінге қалдырылды.\n")
			TEXT("2 шілде. Бақылау ұңғымасының қаптамасы жарылды. Ерітінді жерасты горизонтынан тыс шықты. Хабарлау тоқтатылды.\n")
			TEXT("3 шілде. Апат. Басшылық эвакуацияланды. Жұмысшыларға хабарланбады."),
			{ { TEXT("Келесі бет →"), [this]() { StoryReadJournal(1); }, false } });
		return;
	}
	if (Page == 1)
	{
		SDialog(Nm,
			TEXT("Қосымша жазбалар:\n")
			TEXT("• Майраның сұрауы, 21 маусым: «Датчик көрсеткішін қайта тексеруді сұраймын. Алдыңғы ескерту жабылған жоқ.»\n")
			TEXT("• Директор С. Сәрсеновтің бұрыштамасы: «Ауысым тоқтатылмасын. Жауабын өзім беремін.»\n")
			TEXT("• Кету тізімі, 3 шілде, 03:40: басқарманың 6 қызметкері, 2 көлік. Жұмысшыларға қалған бұйрық: «Кезекті аяқтаңдар.»\n\n")
			TEXT("[Рациядағы «Кезекті жаппаңдар» деген дауыс есіңізге түседі. Бұл — директордың өзі.]"),
			{ { TEXT("Соңғы бет →"), [this]() { StoryReadJournal(2); }, false } });
		return;
	}
	auto Finish = [this](bool bAdmit)
	{
		FKAPSession& S2 = S();
		if (bAdmit) SetFl(S2, TEXT("st_engineer_admit"));
		SetFl(S2, TEXT("st_journal")); SetFl(S2, TEXT("st_S09"));
		CompleteQuest("m_cause");
		StartQuest("story_director_call");
		StoryRingT = 0.f;
		Toast(TEXT("Кім бұйрық бергенін, кім орындағанын планшетке бөлек көшірдіңіз"), "info");
		FTimerHandle Hh;
		GetWorldTimerManager().SetTimer(Hh, FTimerDelegate::CreateWeakLambda(this, [this]() { if (QuestState("story_director_call") == EQuestState::Active) Toast(TEXT("Үстелдегі қызметтік телефон шырылдап тұр"), "warn"); }), 3.f, false);
		SetCheckpoint(); Save(true);
	};
	SDialog(Nm, TEXT("Соңғы бетте таныс формулировка тұр:\n\n«Ескерту жойылғанша уақытша қабылдансын.»\n\n[Бір кездері басқа нысанда сіз де осындай актіге толық тексермей қол қойғансыз. Ол кезде ешкім зардап шекпеген еді.]"),
		{ { TEXT("Мен де осындайға қол қойғанмын."), [Finish]() { Finish(true); }, false },
		  { TEXT("Көшірмесін аламын."), [Finish]() { Finish(false); }, false } });
}

// S10. Директор әлі байланыста — пара, қысым, арнаны жабу
void AKAPGameMode::StoryDirectorCall()
{
	const FString Nm = TEXT("Серік Сәрсенов · қызметтік байланыс");
	auto Close = [this](int32 Mode)
	{
		FKAPSession& S2 = S();
		SetFl(S2, TEXT("st_S10")); SetFl(S2, TEXT("st_director_mode"), Mode);
		CompleteQuest("story_director_call");
		if (Mode == 2)
		{
			SetFl(S2, TEXT("st_soft_requested"));
			StartQuest("m_report");
			Toast(TEXT("Арна ашық. Есеп нұсқасын радиоға барғанда таңдайсыз."), "info");
		}
		else
		{
			StartQuest("story_repeater");
			Radio(TEXT("Басқарма радиосы: есеп арнасы директордың кодымен бұғатталды."), "danger");
			StoryTarget(TowerRepeater, TEXT("Ретранслятор · 3-аймақ мұнарасы"));
		}
		SetCheckpoint(); Save(true);
	};
	auto Offer = [this, Nm, Close]()
	{
		SDialog(Nm, TEXT("[Кідіріс.]\nЖағдайды сырттан реттеймін деп ойладым.\nТыңдаңыз. Қызметкерлердің өтемақысын өзім өткіземін. Есепте тек жабдық ақауы қалсын."),
			{ { TEXT("Кімге не болғанын да, бұйрықты да жіберемін."), [this, Nm, Close]()
			{
				SDialog(Nm, TEXT("Сіз ертең кетесіз. Мен мұның бәрімен өмір сүремін."),
					{ { TEXT("Осында қалғандар да өмір сүрмек болды."), [this, Close]()
					{
						SDialog(TEXT("Байланыс үзілді"), TEXT("[Тұтқадан қысқа сигнал. Планшетте: «Басқарма радиосы: есеп арнасы директордың кодымен бұғатталды».]\nБосс шайқасы жоқ. Оның күші — қалған бұйрықта, құжатта және адамдардың қорқынышында."),
							{ { TEXT("Арнаны өзім ашамын."), [Close]() { Close(0); }, false } });
					}, false } });
			}, false },
			  { TEXT("Адамдарды шығарамыз. Құжатқа кейін жауап бересіз."), [this, Nm, Close]()
			{
				SDialog(Nm, TEXT("Құжатқа мен жауап беремін. Арнаға да.\n[Байланыс үзіледі.]"), { { TEXT("..."), [Close]() { Close(1); }, false } });
			}, false },
			  { TEXT("Есептің нұсқасын қараймын. Әлі жіберген жоқпын."), [this, Nm, Close]()
			{
				SDialog(Nm, TEXT("Дұрыс шешім. Арна ашық. Тексерушілерге жабдық ақауы туралы нұсқа жеткілікті."), { { TEXT("..."), [Close]() { Close(2); }, false } });
			}, false } });
	};
	SDialog(Nm, TEXT("[Тұтқадан тыныш, шаршаған дауыс.]\nИнженер, солай ма? Көшірмені толық жіберіңіз. Ресми арнамен. Жұртқа үзінді таратпаңыз."),
		{ { TEXT("Сіз бұл жерде емессіз. Жұмысшылар осында қалды."), [this, Nm, Offer]()
		{
			SDialog(Nm, TEXT("Ол кеніш жабылса, екі ауылдың несие төлейтін ақшасы қалмайтын еді. Мен оны талай рет тоқтатпай алып шықтым."),
				{ { TEXT("Ескертуді көрдіңіз бе?"), [this, Nm, Offer]()
				{
					SDialog(Nm, TEXT("Көрдім."), { { TEXT("Сосын өзіңіз кеттіңіз."), Offer, false } });
				}, false } });
		}, false } });
}

void AKAPGameMode::StoryRepeaterDone()
{
	if (QuestState("story_repeater") != EQuestState::Active) return;
	CompleteQuest("story_repeater");
	StartQuest("m_report");
	Radio(TEXT("Ретранслятор: арна қалпына келді. Басқарма радиосы есеп жібере алады. Директордың коды енді жұмыс істемейді."), "ok");
	if (Builder && Builder->HQOffice.IsValid()) StoryTarget(KAP::CM2M(Builder->HQOffice->BedPos), TEXT("Басқарма радиосы"));
	SetCheckpoint(); Save(true);
}

// S12 басы: есеп алдындағы шолу — техникалық шарт пен дәлел бөлек
void AKAPGameMode::StoryReportReview()
{
	FKAPSession& Ss = S();
	auto Mark = [](bool b) { return b ? TEXT("✔") : TEXT("✘"); };
	const bool bManifest = Fl(Ss, TEXT("manifestRead")) && (Fl(Ss, TEXT("manifestHonest")) || Fl(Ss, TEXT("st_manifest_corrected")));
	const bool bSoft = Fl(Ss, TEXT("st_soft_requested"));
	const int32 Mode = (int32)FlV(Ss, TEXT("st_report_mode"));
	FString Tx = TEXT("Есепке не кіреді:\n");
	Tx += FString::Printf(TEXT("%s Апат журналының көшірмесі\n"), Mark(Ss.Inv.FindRef("logbook") > 0));
	Tx += FString::Printf(TEXT("%s Бекзаттың куәлігі мен қақпа бұйрығы\n"), Mark(Fl(Ss, TEXT("st_bekzat_testimony"))));
	Tx += FString::Printf(TEXT("%s Майраның су өлшемдері\n"), Mark(Fl(Ss, TEXT("samplesLogged"))));
	Tx += FString::Printf(TEXT("%s Жүк тізімі тексерілген (адал не түзетілген)\n"), Mark(bManifest));
	Tx += FString::Printf(TEXT("%s Куәгерлердің аты қорғалған (Әлия)\n"), Mark(Fl(Ss, TEXT("witnessProtected"))));
	Tx += FString::Printf(TEXT("%s Кету уақыты расталған (Нұрлан, Сапардың дәптері)\n"), Mark(Fl(Ss, TEXT("st_nurlan_witness")) && Fl(Ss, TEXT("st_ledger"))));
	Tx += bSoft ? (Mode == 1 ? TEXT("\nНұсқа: ЖҰМСАРТЫЛҒАН — тек жабдық ақауы. Директордың бұйрығы кірмейді.") : TEXT("\nНұсқа: ТҮПНҰСҚА — директор жұмсартуды сұраған, бірақ сіз әлі келіскен жоқсыз."))
		: TEXT("\nНұсқа: ТҮПНҰСҚА — журнал, бұйрық, кету тізімі.");
	Tx += TEXT("\n\nДәлел адамдарды өздігінен құтқармайды. Ұңғыманы жөндеу де жасырылған қылмысты өздігінен ашпайды.");
	TArray<FKAPDialogOption> O;
	if (bSoft)
	{
		O.Add({ TEXT("Түпнұсқаны жіберемін."), [this]() { SetFl(S(), TEXT("st_report_mode"), 0); StoryReportReview(); }, Mode == 0 });
		O.Add({ TEXT("Директордың нұсқасын жіберемін (тек жабдық ақауы)."), [this]() { SetFl(S(), TEXT("st_report_mode"), 1); StoryReportReview(); }, Mode == 1 });
	}
	O.Add({ TEXT("Техникалық шарттарды тексеру →"), [this]() { SendReport(); }, false });
	O.Add({ TEXT("Әлі ерте."), nullptr, false });
	SDialog(TEXT("Есеп алдындағы шолу"), Tx, MoveTemp(O));
}

// =====================================================================
// Финал: Ақсайдағы соңғы көрініс (S12) және титр
// =====================================================================
float AKAPGameMode::StoryMotherLines(bool bErlanWasAlive, bool bSuccess, TArray<FKAPStoryLine>& Out, float& LamentT)
{
	FKAPSession& Ss = S();
	if (AKAPNPC* M = NamedNPC.FindRef("mariya").Get()) M->Destroy();
	SetFl(Ss, TEXT("st_final_committed"));
	SetFl(Ss, TEXT("st_final_erlan_alive"), bErlanWasAlive ? 1 : 0);
	const bool bShirtD = Fl(Ss, TEXT("st_shirt_delivered")), bShirtT = Fl(Ss, TEXT("st_shirt_taken"));
	const int32 Msg = (int32)FlV(Ss, TEXT("st_erlan_message"));
	const bool bSoft = (int32)FlV(Ss, TEXT("st_report_mode")) == 1;
	float T = 1.f;
	auto Add = [&](float Dt, const FString& Kk, const FString& Ru, const FString& Who) { Out.Add({ T, Kk, Ru, Who }); T += Dt; };
	if (bErlanWasAlive)
	{
		Add(7, TEXT("Ерлан эвакуацияға жеткізілді. Апат түнгі жарақаты ауыр еді. Айгерім соңына дейін қасында болды."), TEXT("Ерлана довезли до эвакуации. Травма с ночи аварии была тяжёлой. Айгерим была рядом до конца."), FString());
		Add(5, TEXT("Қасында болдым. Соңына дейін."), TEXT("Я была рядом. До конца."), TEXT("Дәрігер Айгерім"));
	}
	else Add(7, TEXT("Ерлан Сейітов, 19 жас. Денесі мен заттары №3-тен ауылға жеткізілді."), TEXT("Ерлан Сеитов, 19 лет. Тело и вещи доставили с третьей скважины в посёлок."), FString());
	Add(5, TEXT("Жейдесін бердің бе?"), TEXT("Ты передал ему рубашку?"), TEXT("Ана"));
	if (bShirtD) Add(6, TEXT("Бердім. Өзі киді."), TEXT("Передал. Он её надел."), TEXT("Сіз"));
	else if (bShirtT) Add(6, TEXT("Үлгермедім. Осында алып келдім."), TEXT("Не успел. Я принёс её сюда."), TEXT("Сіз"));
	else Add(6, TEXT("Жейдесін жеткізе алмадым."), TEXT("Я не смог передать ему рубашку."), TEXT("Сіз"));
	Add(7, TEXT("Таңертең екінші кесені жуып қойып едім."), TEXT("Я утром вторую чашку вымыла."), TEXT("Ана"));
	if (Msg > 0 && bErlanWasAlive)
	{
		Add(5, TEXT("Ерлан хабар қалдырған. Тыңдағыңыз келе ме?"), TEXT("Ерлан оставил сообщение. Хотите послушать?"), TEXT("Сіз"));
		Add(4, TEXT("Қосшы."), TEXT("Включи."), TEXT("Ана"));
		Add(10, Msg == 2 ? ErlanMsgShirt : ErlanMsgNoShirt,
			Msg == 2 ? TEXT("Мам, рубашку передали. Нитки тоже. Дверь мастерской пока сама не крась. Мы же вместе цвет выбрать хотели.") : TEXT("Мам, знаю, что ты волнуешься. Я не один, рядом люди. Дверь мастерской пока сама не крась. Мы же вместе цвет выбрать хотели."),
			TEXT("Ерлан (жазба)"));
	}
	LamentT = T;
	Add(7, TEXT("Балам... Ерлан... құлыным менің..."), TEXT("Сынок... Ерлан... жеребёночек мой..."), TEXT("Ана"));
	Add(8, TEXT("Оқуыңды бітіремін деп едің ғой... Қайтып келемін деп едің ғой..."), TEXT("Ты же обещал доучиться... Обещал вернуться..."), TEXT("Ана"));
	Add(8, TEXT("Балам-ау... жалғызым-ау..."), TEXT("Сыночек... единственный мой..."), TEXT("Ана"));
	if (bSuccess && !bSoft)
		Add(8, Fl(Ss, TEXT("st_bekzat_testimony")) ? TEXT("Кеніште жұмыс тоқтап, тексеру басталды. Бекзат куәлік берді. Майра өлшемдерін тапсырды. Директордың ісі тергеуде.")
			: TEXT("Кеніште жұмыс тоқтап, тексеру басталды. Майра өлшемдерін тапсырды. Директордың ісі тергеуде."),
			TEXT("Работа на руднике остановлена, началась проверка. Дело директора расследуется."), FString());
	else if (bSuccess) Add(7, TEXT("Ұңғыманы түзеттің. Енді қағаздағысын да түзету керек."), TEXT("Скважину ты починил. Теперь нужно исправить и то, что на бумаге."), TEXT("Зертханашы Майра"));
	else
	{
		Add(7, TEXT("Барлығын алып шыға алмадық. Шыққандарды атымен жаз."), TEXT("Всех вывезти не смогли. Тех, кто выбрался, запиши поимённо."), TEXT("Дәрігер Айгерім"));
		if (!Fl(Ss, TEXT("witnessProtected")) || !Fl(Ss, TEXT("st_journal"))) Add(6, TEXT("Ерланның атын дұрыс жазыпсыңдар ма?"), TEXT("Имя Ерлана правильно написали?"), TEXT("Ана"));
	}
	UE_LOG(LogTemp, Display, TEXT("KAPSTORY mother scene: alive=%d success=%d shirt=%d/%d msg=%d soft=%d lines=%d dur=%.0f"), bErlanWasAlive ? 1 : 0, bSuccess ? 1 : 0, bShirtT ? 1 : 0, bShirtD ? 1 : 0, Msg, bSoft ? 1 : 0, Out.Num(), T + 4.f);
	return T + 4.f;
}

FString AKAPGameMode::StoryCreditsText() const
{
	const FKAPSession& Ss = S();
	FString T = TEXT("\n\nОҚИҒА\n");
	const bool bSent = Fl(Ss, TEXT("st_final_report_sent"));
	T += !bSent ? TEXT("Есеп: жіберілмеді — аймақ ерте жабылды\n") : (int32)FlV(Ss, TEXT("st_report_mode")) == 1 ? TEXT("Есеп: жұмсартылды — тек «жабдық ақауы»\n") : TEXT("Есеп: түпнұсқа — журнал, бұйрық, кету тізімі\n");
	T += Fl(Ss, TEXT("st_bekzat_testimony")) ? TEXT("Бекзат: куәлік берді\n") : TEXT("Бекзат: куәлігі тіркелмеді\n");
	T += Fl(Ss, TEXT("st_shirt_delivered")) ? TEXT("Жейде: Ерланға жеткізілді\n") : Fl(Ss, TEXT("st_shirt_taken")) ? TEXT("Жейде: анасына қайтарылды\n") : TEXT("Жейде: алынбады\n");
	if (FlV(Ss, TEXT("st_erlan_message")) > 0) T += TEXT("Ерланның хабары: анасына жетті\n");
	T += Fl(Ss, TEXT("st_sapar_dead")) ? TEXT("Сапар Аманов: Қаражарда қаза тапты\n") : TEXT("Сапар Аманов: Қаражарда қалды\n");
	T += Fl(Ss, TEXT("st_journal")) && (!bSent || (int32)FlV(Ss, TEXT("st_report_mode")) == 0) ? TEXT("Директор Серік Сәрсенов: ісі тергеуде\n") : TEXT("Директор Серік Сәрсенов: ресми есепте аты жоқ\n");
	T += TEXT("\nЕрлан Сейітов, 19 жас.");
	return T;
}
