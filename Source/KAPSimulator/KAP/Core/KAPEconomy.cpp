// Экономика: теңге, бүйір жұмыстар (келісімшарттар), қару-жарақ дүкені, Ақсай полиция бөлімі және әскери аймақ.
// Тақталар: police (бандиттер, патруль, ұрланған жүк), trader (жеткізу, сынық, су), army (мутант ұясы, жанармай, барлау).
#include "KAPGameMode.h"
#include "KAPGameInstance.h"
#include "KAPDataSubsystem.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Player/NavigatorComponent.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/World/KAPVehicleSpec.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAP/UI/KAPHUD.h"
#include "Components/TextRenderComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

namespace
{
	FVector2D LocOf(const UObject* Ctx, FName Id)
	{
		const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(Ctx);
		const FKAPLocationRow* R = D ? D->Loc(Id) : nullptr;
		return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector;
	}
	FString LocTitle(const UObject* Ctx, FName Id)
	{
		const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(Ctx);
		const FKAPLocationRow* R = D ? D->Loc(Id) : nullptr;
		return R ? R->Title : Id.ToString();
	}
	/** Компоненттерді ұстайтын қарапайым актер (көлік, белгі, қаптар) */
	AActor* SpawnProp(UWorld* W, FVector Pos, float Yaw)
	{
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* A = W->SpawnActor<AActor>(AActor::StaticClass(), Pos, FRotator(0, Yaw, 0), SP);
		if (!A) return nullptr;
		USceneComponent* R = NewObject<USceneComponent>(A, TEXT("Root"));
		A->SetRootComponent(R); R->RegisterComponent();
		A->SetActorLocationAndRotation(Pos, FRotator(0, Yaw, 0));
		return A;
	}
	UTextRenderComponent* Sign(AActor* A, const FString& Text, FVector Rel, float Yaw, float Size, FColor C)
	{
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
		T->SetupAttachment(A->GetRootComponent()); T->SetRelativeLocationAndRotation(Rel, FRotator(0, Yaw, 0));
		KAPVis::SignText(T);
		T->SetText(FText::FromString(Text)); T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(Size); T->SetTextRenderColor(C); T->RegisterComponent();
		return T;
	}
	UStaticMesh* LoadSM(const FString& N) { return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Meshes/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet); }
	/** Тұрақ көлігі (физикасыз): тегіс корпус мештері + дөңгелектер + соқтығысу қорабы */
	AActor* ParkedVehicle(UWorld* W, const FString& Id, FVector Pos, float Yaw, FLinearColor Paint, FLinearColor Second)
	{
		const FKAPVehSpec* Sp = KAPVehSpec::Find(Id);
		AActor* A = SpawnProp(W, Pos, Yaw);
		if (!A || !Sp) return A;
		FKAPMat P = FKAPMat::Paint(Paint, 0.45f); P.Grime = 0.25f; P.Metallic = 0.3f; P.DustTop = 0.8f;
		FKAPMat S2 = FKAPMat::Paint(Second, 0.6f); S2.Grime = 0.35f; S2.DustTop = 0.8f;
		FKAPMat Gl = FKAPMat::Metal(FLinearColor(0.035f, 0.045f, 0.05f), 0.04f); Gl.DustTop = 0.12f; Gl.Grime = 0.06f;
		FKAPMat Dk = FKAPMat::Paint(FLinearColor(0.022f, 0.022f, 0.02f), 0.82f);
		const FKAPMat Tr = FKAPMat::Metal(FLinearColor(0.72f, 0.72f, 0.7f), 0.22f);
		const FKAPMat Rm[5] = { P, S2, Gl, Dk, Tr };
		for (int32 r = 0; r < 5; r++) if (UStaticMesh* M = LoadSM(FString::Printf(TEXT("SM_KAP_Veh_%s_%d"), *Id, r))) KAPVis::AddMesh(A, A->GetRootComponent(), M, FVector::ZeroVector, FVector::OneVector, Rm[r])->bCastDynamicShadow = true;
		UStaticMesh* Tire = LoadSM(TEXT("SM_KAP_Tire")); UStaticMesh* Rim = LoadSM(TEXT("SM_KAP_Rim"));
		FKAPMat TireM = FKAPMat::Paint(FLinearColor(0.032f, 0.03f, 0.028f), 0.9f); TireM.DustTop = 0.9f;
		FKAPMat RimM = FKAPMat::Paint(FLinearColor(0.16f, 0.17f, 0.15f), 0.5f); RimM.Metallic = 0.45f;
		const FVector WS(2.f * Sp->R / 100.f, Sp->TireW / 100.f, 2.f * Sp->R / 100.f);
		TArray<float> Axles = { Sp->WB * 0.5f, -Sp->WB * 0.5f };
		if (Id == TEXT("btr")) Axles = { Sp->WB * 0.5f, Sp->WB * 0.5f - 135.f, -Sp->WB * 0.5f, -Sp->WB * 0.5f - 135.f };
		for (float X : Axles) for (float Sg : { -1.f, 1.f })
		{
			if (Tire) KAPVis::AddMesh(A, A->GetRootComponent(), Tire, FVector(X, Sg * Sp->Track, Sp->R), WS, TireM);
			if (Rim) KAPVis::AddMesh(A, A->GetRootComponent(), Rim, FVector(X, Sg * Sp->Track, Sp->R), WS, RimM, FRotator(0, Sg > 0 ? 0.f : 180.f, 0));
		}
		// соқтығысу (көзге көрінбейтін қорап)
		UBoxComponent* Bx = NewObject<UBoxComponent>(A);
		Bx->SetupAttachment(A->GetRootComponent());
		const bool bBig = Id == TEXT("btr") || Id == TEXT("kamaz");
		Bx->SetBoxExtent(bBig ? FVector(380, 145, 110) : FVector(210, 95, 95));
		Bx->SetRelativeLocation(FVector(bBig ? -60.f : 0.f, 0, bBig ? 120.f : 100.f));
		Bx->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Bx->RegisterComponent();
		return A;
	}
}

// =====================================================================
// Ақша
// =====================================================================
void AKAPGameMode::AddMoney(int32 Amount, const FString& Why)
{
	if (Amount == 0) return;
	FKAPSession& Ss = S();
	Ss.Money = FMath::Max(0, Ss.Money + Amount);
	Toast(FString::Printf(TEXT("%s%d ₸ · %s"), Amount > 0 ? TEXT("+") : TEXT(""), Amount, *Why), Amount > 0 ? "ok" : "info");
	if (UKAPAudio* A = UKAPAudio::Get(this)) { A->Play(EKAPSound::Pickup, 0.5f); if (Amount > 0) A->PlayBank("MetalClick", FVector::ZeroVector, 0.5f, 1.3f, true); }
}

// =====================================================================
// Дүкен
// =====================================================================
const TArray<AKAPGameMode::FShopItem>& AKAPGameMode::ShopItems()
{
	static const TArray<FShopItem> L = {
		// 0 — қару
		{ "knife", 300, 1, 0, false }, { "crowbar", 450, 1, 0, false }, { "flaregun", 900, 1, 0, true }, { "pm", 1800, 1, 0, true },
		{ "shotgun", 3200, 1, 0, true }, { "ak", 7500, 1, 0, true }, { "sniper", 11000, 1, 0, true }, { "molotov", 150, 1, 0, true }, { "flare", 60, 1, 0, false },
		// 1 — оқ-дәрі (буда)
		{ "9mm", 150, 16, 1, true }, { "12g", 180, 8, 1, true }, { "545", 360, 30, 1, true }, { "762", 420, 10, 1, true }, { "flare_ammo", 120, 3, 1, false },
		// 2 — дәрі мен азық
		{ "medkit", 300, 1, 2, false }, { "bandage", 80, 1, 2, false }, { "water", 40, 1, 2, false }, { "food", 50, 1, 2, false }, { "iodine", 150, 1, 2, false }, { "antidote", 500, 1, 2, false },
		// 3 — жабдық
		{ "armor", 2600, 1, 3, false }, { "suit", 2200, 1, 3, false }, { "dosimeter", 700, 1, 3, false }, { "hat", 120, 1, 3, false }, { "coat", 300, 1, 3, false }, { "fuel", 200, 1, 3, false },
	};
	return L;
}

int32 AKAPGameMode::ShopPrice(const FShopItem& It, FName Kind) const
{
	// әскери қойма: қару мен оқ-дәрі 20% арзан
	const float K = (Kind == "army" && (It.Cat == 0 || It.Cat == 1 || It.Id == "armor")) ? 0.8f : 1.f;
	return FMath::RoundToInt32(It.Price * K / 10.f) * 10;
}

bool AKAPGameMode::CanBuy(const FShopItem& It, FName Kind, FString& Why) const
{
	const FKAPSession& Ss = S();
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const FKAPWeaponRow* W = D->Weapon(It.Id);
	const bool bWeaponItem = W && It.Cat == 0;
	if (bWeaponItem && W->Kind != "throw" && Ss.Weapons.Contains(It.Id)) { Why = TEXT("Бар"); return false; }
	if ((It.Id == "armor" || It.Id == "suit" || It.Id == "dosimeter" || It.Id == "hat" || It.Id == "coat") && Ss.Inv.FindRef(It.Id) > 0) { Why = TEXT("Бар"); return false; }
	if (It.bPermit && !Ss.bPermitWeapon) { Why = TEXT("Рұқсат керек"); return false; }
	if (Ss.Money < ShopPrice(It, Kind)) { Why = TEXT("Ақша жетпейді"); return false; }
	return true;
}

void AKAPGameMode::Buy(const FShopItem& It, FName Kind)
{
	FString Why;
	if (!CanBuy(It, Kind, Why)) { Toast(Why, "warn"); return; }
	FKAPSession& Ss = S();
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const int32 Price = ShopPrice(It, Kind);
	Ss.Money -= Price;
	FString Got;
	const FKAPWeaponRow* W = D->Weapon(It.Id);
	if (W && It.Cat == 0)
	{
		Got = GiveItems({ { It.Id, 1 } });
		if (W->Kind == "throw" && !W->Ammo.IsNone()) Ss.Ammo.FindOrAdd(W->Ammo) += It.Qty;
	}
	else Got = GiveItems({ { It.Id, It.Qty } });
	Toast(FString::Printf(TEXT("Сатып алынды: %s (−%d ₸)"), *Got, Price), "ok");
	if (UKAPAudio* A = UKAPAudio::Get(this)) { A->Play(EKAPSound::Pickup, 0.6f); A->PlayBank(It.Cat == 0 ? FName("DrawKnife") : FName("Cloth"), FVector::ZeroVector, 0.6f, 1.f, true); }
}

void AKAPGameMode::SellScrap()
{
	FKAPSession& Ss = S();
	const int32 N = Ss.Inv.FindRef("scrap");
	if (N <= 0) { Toast(TEXT("Сататын сынық жоқ"), "warn"); return; }
	Ss.Inv.Remove("scrap");
	AddMoney(N * 70, FString::Printf(TEXT("%d металл сынығы сатылды"), N));
}

// =====================================================================
// Келісімшарттар
// =====================================================================
FKAPContract* AKAPGameMode::FindContract(int32 Uid)
{
	for (FKAPContract& C : S().Contracts) if (C.Uid == Uid) return &C;
	return nullptr;
}

TArray<FKAPContract*> AKAPGameMode::ContractsOf(FName Board, int32 State)
{
	TArray<FKAPContract*> Out;
	for (FKAPContract& C : S().Contracts) if ((Board.IsNone() || C.Board == Board) && C.State == State) Out.Add(&C);
	return Out;
}

FString AKAPGameMode::ContractProgress(const FKAPContract& C) const
{
	const FKAPSession& Ss = S();
	if (C.Type == "bounty" || C.Type == "nest") return FString::Printf(TEXT("Жойылды: %d / %d"), C.Have, C.Need);
	if (C.Type == "scrap") return FString::Printf(TEXT("Жиналды: %d / %d"), C.Have, C.Need);
	if (C.Type == "patrol") return FString::Printf(TEXT("Нүкте: %d / %d"), C.Have, C.Need);
	if (C.Type == "deliver") return Ss.Inv.FindRef("cargo") > 0 ? TEXT("Жүк сізде — мекенжайға жеткізіңіз") : TEXT("Жүк жоғалды");
	if (C.Type == "supply") return FString::Printf(TEXT("Су құтысы: %d / %d"), FMath::Min(Ss.Inv.FindRef("water"), C.Need), C.Need);
	if (C.Type == "fuel") return FString::Printf(TEXT("Жанармай канистрі: %d / %d"), FMath::Min(Ss.Inv.FindRef("fuel"), C.Need), C.Need);
	if (C.Type == "recover") return TEXT("Жүкті бандиттерден тартып алыңыз");
	if (C.Type == "recon") return TEXT("Нүктеге жетіңіз");
	return FString();
}

void AKAPGameMode::RefreshContracts(bool bForce)
{
	FKAPSession& Ss = S();
	const int32 Day = FMath::FloorToInt32(Ss.Time / 24.0);
	if (!bForce && Ss.ContractDay == Day) return;
	Ss.ContractDay = Day;
	// ескі ұсыныстар мен жабылғандарды тазалау (белсенділер қалады)
	Ss.Contracts.RemoveAll([Day](const FKAPContract& C) { return C.State == 0 || (C.State >= 2 && C.Day < Day - 1); });
	FRandomStream R(Day * 7919 + 13);
	const FVector2D Police = LocOf(this, "police"), Shop = LocOf(this, "shop"), Army = LocOf(this, "army");
	const FVector2D Villages[2] = { LocOf(this, "aksai"), LocOf(this, "josaly") };
	// ауыл мен аймақтардан алыс, карта ішіндегі кездейсоқ нүкте
	auto FarPoint = [&](FVector2D From, float Rmin, float Rmax) -> FVector2D
	{
		for (int32 t = 0; t < 40; t++)
		{
			const float A = R.FRandRange(0, UE_TWO_PI), D = R.FRandRange(Rmin, Rmax);
			const FVector2D P = From + FVector2D(FMath::Cos(A), FMath::Sin(A)) * D;
			if (FMath::Abs(P.X) > 960 || FMath::Abs(P.Y) > 960) continue;
			if (FVector2D::Distance(P, Villages[0]) < 140 || FVector2D::Distance(P, Villages[1]) < 140 || InArmyZone(P) || FVector2D::Distance(P, Army) < 120) continue;
			return P;
		}
		return From + FVector2D(Rmin, 0);
	};
	auto Round10 = [](float V) { return FMath::RoundToInt32(V / 10.f) * 10; };
	auto Make = [&](FName Board, FName Type) -> FKAPContract&
	{
		FKAPContract& C = Ss.Contracts.AddDefaulted_GetRef();
		C.Board = Board; C.Type = Type; C.Uid = Ss.NextUid++; C.Day = Day; C.State = 0;
		return C;
	};
	// ---- полиция ----
	{
		FKAPContract& C = Make("police", "bounty");
		C.Need = R.RandRange(3, 5);
		C.Points = { FarPoint(Police, 200, 480) };
		C.Title = TEXT("Бандит тобын жою");
		C.Desc = FString::Printf(TEXT("Жолда жүргізушілерді тонаған %d бандит көрінді. Оларды жойыңыз — полиция сыйақы төлейді."), C.Need);
		C.TargetName = TEXT("Бандиттер тобы");
		C.Reward = Round10(260.f * C.Need + R.FRandRange(0, 200));
	}
	{
		FKAPContract& C = Make("police", "patrol");
		TArray<FName> Pool = { "guard1", "wagon0", "oldcar", "res", "wagon1", "tower1", "josaly" };
		for (int32 i = Pool.Num() - 1; i > 0; i--) Pool.Swap(i, R.RandRange(0, i));
		C.Need = 3;
		for (int32 i = 0; i < 3; i++) C.Points.Add(LocOf(this, Pool[i]) + FVector2D(R.FRandRange(-8, 8), R.FRandRange(-8, 8)));
		C.Title = TEXT("Жол патрулі");
		C.Desc = FString::Printf(TEXT("Үш нүктені аралап шығыңыз: %s → %s → %s. Күдікті ештеңе болмаса, хабарлаңыз."), *LocTitle(this, Pool[0]), *LocTitle(this, Pool[1]), *LocTitle(this, Pool[2]));
		C.TargetName = LocTitle(this, Pool[0]);
		float Len = FVector2D::Distance(Police, C.Points[0]) + FVector2D::Distance(C.Points[0], C.Points[1]) + FVector2D::Distance(C.Points[1], C.Points[2]);
		C.Reward = Round10(250.f + Len * 0.25f);
	}
	{
		FKAPContract& C = Make("police", "recover");
		C.Points = { FarPoint(Police, 250, 520) };
		C.Need = 1;
		C.Title = TEXT("Ұрланған жүкті қайтару");
		C.Desc = TEXT("Бандиттер дүкеннің жүгін тартып алды. Жәшікті тауып, күзетшілерін бейтараптаңыз.");
		C.TargetName = TEXT("Ұрланған жүк");
		C.Reward = Round10(R.FRandRange(850, 1150));
	}
	// ---- дүкен ----
	{
		static const FName Dest[] = { "josaly", "wagon1", "guard1", "wagon2", "res", "wagon3" };
		const FName D = Dest[R.RandRange(0, UE_ARRAY_COUNT(Dest) - 1)];
		FKAPContract& C = Make("trader", "deliver");
		C.Points = { LocOf(this, D) };
		C.Title = FString::Printf(TEXT("Жүк жеткізу: %s"), *LocTitle(this, D));
		C.Desc = TEXT("Ерболаттың жәшігін мекенжайға аман жеткізіңіз. Жәшік қабылдағанда беріледі.");
		C.TargetName = LocTitle(this, D);
		C.Reward = Round10(150.f + FVector2D::Distance(Shop, C.Points[0]) * 0.8f);
	}
	{
		static const FName Site[] = { "ruins", "oldcar", "nest1", "nest2", "missing" };
		const FName St = Site[R.RandRange(0, UE_ARRAY_COUNT(Site) - 1)];
		FKAPContract& C = Make("trader", "scrap");
		C.Need = R.RandRange(4, 6);
		const FVector2D Base = LocOf(this, St) + FVector2D(R.FRandRange(-30, 30), R.FRandRange(-30, 30));
		for (int32 i = 0; i < C.Need; i++) { const float A = R.FRandRange(0, UE_TWO_PI); C.Points.Add(Base + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R.FRandRange(6, 35)); }
		C.Title = TEXT("Металл сынығын жинау");
		C.Desc = FString::Printf(TEXT("%s маңынан %d үйінді сынық жинаңыз. Сынықты өзіңізде қалдырып, кейін сатуға да болады."), *LocTitle(this, St), C.Need);
		C.TargetName = LocTitle(this, St);
		C.Reward = Round10(70.f * C.Need + 150.f);
	}
	{
		const FName V = R.FRand() < 0.5f ? FName("josaly") : FName("wagon1");
		FKAPContract& C = Make("trader", "supply");
		C.Need = 3;
		C.Points = { LocOf(this, V) };
		C.Title = FString::Printf(TEXT("Ауыз су: %s"), *LocTitle(this, V));
		C.Desc = TEXT("Ол жақтағы адамдарға 3 құты су жетпей жатыр. Суды өзіңіз табыңыз немесе дүкеннен сатып алыңыз.");
		C.TargetName = LocTitle(this, V);
		C.Reward = Round10(380.f + FVector2D::Distance(Shop, C.Points[0]) * 0.3f);
	}
	// ---- әскер ----
	{
		static const FName Nests[] = { "nest1", "nest2", "nest3", "nest4" };
		const FName Nt = Nests[R.RandRange(0, 3)];
		FKAPContract& C = Make("army", "nest");
		C.Need = R.RandRange(5, 7);
		C.Points = { LocOf(this, Nt) + FVector2D(R.FRandRange(-15, 15), R.FRandRange(-15, 15)) };
		C.Title = TEXT("Мутант ұясын тазарту");
		C.Desc = FString::Printf(TEXT("Барлау %d сәулеленген мутантты анықтады. Түнге дейін ұяны тазартыңыз."), C.Need);
		C.TargetName = TEXT("Мутант ұясы");
		C.Reward = Round10(230.f * C.Need + R.FRandRange(100, 300));
	}
	{
		FKAPContract& C = Make("army", "fuel");
		C.Need = 2;
		C.Points = { Army + FVector2D(-66, 0) };
		C.Title = TEXT("Әскерге жанармай");
		C.Desc = TEXT("Генераторларға 2 канистр жанармай керек. Вагон-үйлердің жәшіктерінен табыңыз немесе сатып алыңыз.");
		C.TargetName = TEXT("Әскери аймақ қақпасы");
		C.Reward = 950;
	}
	{
		static const FName Rc[] = { "ruins", "tower2", "tower3", "hq", "bandit" };
		const FName Pt = Rc[R.RandRange(0, UE_ARRAY_COUNT(Rc) - 1)];
		FKAPContract& C = Make("army", "recon");
		C.Points = { LocOf(this, Pt) + FVector2D(R.FRandRange(-10, 10), R.FRandRange(-10, 10)) };
		C.Title = FString::Printf(TEXT("Барлау: %s"), *LocTitle(this, Pt));
		C.Desc = TEXT("Аймаққа жетіп, жағдайды көзбен көріңіз. Қауіпті — абай болыңыз.");
		C.TargetName = LocTitle(this, Pt);
		C.Reward = Round10(450.f + FVector2D::Distance(Army, C.Points[0]) * 0.45f);
	}
}

void AKAPGameMode::TrackContract(int32 Uid)
{
	FKAPContract* C = FindContract(Uid);
	AKAPCharacter* P = Player();
	if (!C || !P || C->State != 1) return;
	S().Tracked = Uid;
	if (!C->Points.Num()) return;
	const int32 I = C->Type == "patrol" ? FMath::Clamp(C->Have, 0, C->Points.Num() - 1) : 0;
	if (P->Navigator) P->Navigator->SetTarget(C->Points[I], C->TargetName);
}

namespace
{
	void SpawnPile(AKAPGameMode* GM, int32 Uid, FVector2D Pm, bool bCargo)
	{
		if (!GM->Builder) return;
		const float Z = GM->Builder->Z(Pm);
		AKAPInteractSpot* Sp = GM->Builder->SpawnSpot(FName(*FString::Printf(TEXT("cpick:%d"), Uid)), FVector(Pm.X * 100, Pm.Y * 100, Z + 30), 240, false);
		if (!Sp) return;
		if (bCargo)
		{
			FKAPMat Wd = FKAPMat::Paint(FLinearColor(0.32f, 0.22f, 0.12f), 0.85f); Wd.Ribs = 0.4f; Wd.RibPeriod = 8.f;
			Sp->Visual = KAPVis::Add(Sp, Sp->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 20), FVector(80, 60, 55), Wd, FRotator(0, 20, 0), true);
			KAPVis::Add(Sp, Sp->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 48), FVector(84, 18, 3), FKAPMat::RustySteel(), FRotator(0, 20, 0), false);
		}
		else
		{
			// сынық үйіндісі: тот басқан табақтар мен құбырлар
			FRandomStream R(Uid * 31 + (int32)Pm.X);
			Sp->Visual = KAPVis::Add(Sp, Sp->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 5), FVector(70, 55, 12), FKAPMat::Sheet(FLinearColor(0.35f, 0.3f, 0.25f), 0.8f), FRotator(R.FRandRange(-8, 8), R.FRandRange(0, 90), R.FRandRange(-8, 8)), false);
			for (int32 i = 0; i < 3; i++) KAPVis::Add(Sp, Sp->GetRootComponent(), EKAPShape::Cylinder, FVector(R.FRandRange(-20, 20), R.FRandRange(-15, 15), 16), FVector(8, 8, R.FRandRange(50, 90)), FKAPMat::RustySteel(), FRotator(90, R.FRandRange(0, 180), 0), false);
			KAPVis::Add(Sp, Sp->GetRootComponent(), EKAPShape::Cube, FVector(10, -8, 22), FVector(40, 30, 6), FKAPMat::Sheet(FLinearColor(0.4f, 0.12f, 0.08f), 0.9f), FRotator(25, R.FRandRange(0, 90), 10), false);
		}
	}
}

void AKAPGameMode::AcceptContract(int32 Uid)
{
	FKAPContract* C = FindContract(Uid);
	if (!C || C->State != 0) return;
	if (ContractsOf(NAME_None, 1).Num() >= 3) { Toast(TEXT("Бір мезгілде ең көбі 3 жұмыс алуға болады"), "warn"); return; }
	C->State = 1; C->Have = 0;
	FKAPSession& Ss = S();
	if (C->Type == "deliver") Ss.Inv.FindOrAdd("cargo") += 1;
	// нысаналар (жаулар, үйінділер)
	if (C->Type == "bounty" || C->Type == "nest" || C->Type == "recover")
	{
		const int32 N = C->Type == "recover" ? 3 : C->Need;
		for (int32 i = 0; i < N; i++)
		{
			const FVector2D P = C->Points[0] + FVector2D(Rng.FRandRange(-10, 10), Rng.FRandRange(-10, 10));
			if (AKAPNPC* E = SpawnNPC((int32)(C->Type == "nest" ? ENPCKind::Irradiated : ENPCKind::Bandit), P, C->Type == "nest" ? TEXT("Мутант") : TEXT("Бандит"), true))
			{
				if (C->Type != "recover") E->ContractUid = Uid;
				E->Home = P; E->PatrolR = 14;
			}
		}
	}
	if (C->Type == "recover") SpawnPile(this, Uid, C->Points[0], true);
	if (C->Type == "scrap") for (const FVector2D& P : C->Points) SpawnPile(this, Uid, P, false);
	Toast(FString::Printf(TEXT("Жұмыс қабылданды: %s · %d ₸"), *C->Title, C->Reward), "info");
	TrackContract(Uid);
	if (UKAPAudio* A = UKAPAudio::Get(this)) A->Beep(true);
}

void AKAPGameMode::AbandonContract(int32 Uid)
{
	FKAPContract* C = FindContract(Uid);
	if (!C || C->State != 1) return;
	C->State = 3;
	if (C->Type == "deliver") TakeItem("cargo", 1);
	for (TActorIterator<AKAPInteractSpot> It(GetWorld()); It; ++It) if (It->Action == FName(*FString::Printf(TEXT("cpick:%d"), Uid))) It->Destroy();
	Toast(FString::Printf(TEXT("Жұмыстан бас тартылды: %s"), *C->Title), "warn");
	if (S().Tracked == Uid) { S().Tracked = 0; if (AKAPCharacter* P = Player()) if (P->Navigator) P->Navigator->bHasTarget = false; }
}

namespace
{
	void Finish(AKAPGameMode* GM, FKAPContract& C)
	{
		FKAPSession& Ss = GM->S();
		C.State = 2;
		GM->AddMoney(C.Reward, C.Title);
		if (C.Board == "police") Ss.RepMilitia = FMath::Min(100.f, Ss.RepMilitia + 4);
		if (C.Board == "army" && !Ss.Flags.Contains("armyPass"))
		{
			Ss.Flags.Add("armyPass", 1);
			GM->Toast(TEXT("Майор Сейітов: «Сенімді адам екенсің». Әскери аймаққа кіру рұқсаты берілді — ішіндегі қоймада қару арзан."), "ok");
		}
		if (Ss.Tracked == C.Uid)
		{
			Ss.Tracked = 0;
			if (AKAPCharacter* P = GM->Player()) if (P->Navigator) P->Navigator->bHasTarget = false;
			// келесі белсенді жұмысқа бағыттау
			for (const FKAPContract& O : Ss.Contracts) if (O.State == 1) { GM->TrackContract(O.Uid); break; }
		}
	}
}

void AKAPGameMode::ContractKill(AKAPNPC* N, bool bByPlayer)
{
	if (!N || !bByPlayer) return;
	// полиция сыйақысы (бандит, мутант)
	if (N->Faction == EKAPFaction::Bandit) AddMoney(80, TEXT("Полиция сыйақысы: бандит"));
	else if (N->Kind == ENPCKind::Irradiated) AddMoney(50, TEXT("Полиция сыйақысы: мутант"));
	else if (N->Kind == ENPCKind::Worm) AddMoney(150, TEXT("Полиция сыйақысы: құм құрты"));
	if (N->ContractUid <= 0) return;
	if (FKAPContract* C = FindContract(N->ContractUid))
		if (C->State == 1)
		{
			C->Have++;
			if (C->Have >= C->Need) Finish(this, *C);
			else Toast(FString::Printf(TEXT("%s · %s"), *C->Title, *ContractProgress(*C)), "info");
		}
}

void AKAPGameMode::ContractTick(float Dt)
{
	FKAPSession& Ss = S();
	ContractT -= Dt;
	ArmyHostile = FMath::Max(0.f, ArmyHostile - Dt);
	AKAPCharacter* P = Player();
	if (!P) return;
	const FVector2D Pm = KAP::CM2M(P->GetActorLocation());
	// ---- әскери аймақ: рұқсатсыз кіру → ескерту → атыс ----
	if (bArmyBuilt && !CurrentVehicle.IsValid())
	{
		const bool bIn = InArmyZone(Pm) && !Ss.Flags.Contains("armyPass");
		if (bIn)
		{
			ArmyInT += Dt;
			const int32 Stage = ArmyInT < 7.f ? 0 : ArmyInT < 14.f ? 1 : 2;
			if (Stage >= ArmyWarnStage)
			{
				AKAPNPC* Best = nullptr; double Bd = 1e9;
				for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
					if (It->Kind == ENPCKind::Soldier && !It->bDead) { const double D = FVector::Dist(It->GetActorLocation(), P->GetActorLocation()); if (D < Bd) { Bd = D; Best = *It; } }
				if (Best) Best->ArmyWarn(Stage);
				ArmyWarnStage = Stage + 1;
				if (Stage == 2) ArmyHostile = 45.f;
			}
			if (Stage == 2) ArmyHostile = FMath::Max(ArmyHostile, 5.f);
		}
		else if (!InArmyZone(Pm)) { ArmyInT = FMath::Max(0.f, ArmyInT - Dt * 2.f); if (ArmyInT <= 0.f) ArmyWarnStage = 0; }
	}
	if (ContractT > 0.f) return;
	ContractT = 0.4f;
	RefreshContracts(false);
	for (FKAPContract& C : Ss.Contracts)
	{
		if (C.State != 1 || !C.Points.Num()) continue;
		const float Rr = C.Type == "recon" ? 22.f : 14.f;
		if (C.Type == "patrol")
		{
			if (C.Have < C.Points.Num() && FVector2D::Distance(Pm, C.Points[C.Have]) < 16.f)
			{
				C.Have++;
				if (C.Have >= C.Need) Finish(this, C);
				else { Toast(FString::Printf(TEXT("Патруль: %d / %d нүкте"), C.Have, C.Need), "info"); if (Ss.Tracked == C.Uid && P->Navigator) P->Navigator->SetTarget(C.Points[C.Have], TEXT("Патруль нүктесі")); }
			}
			continue;
		}
		if ((C.Type == "deliver" || C.Type == "supply" || C.Type == "fuel" || C.Type == "recon") && FVector2D::Distance(Pm, C.Points[0]) < Rr)
		{
			if (C.Type == "deliver") { if (Count("cargo") > 0) { TakeItem("cargo", 1); Finish(this, C); } }
			else if (C.Type == "supply") { if (Count("water") >= C.Need) { TakeItem("water", C.Need); Finish(this, C); } else if (C.Have == 0) { C.Have = 1; Toast(FString::Printf(TEXT("Мұнда %d құты су керек"), C.Need), "warn"); } }
			else if (C.Type == "fuel") { if (Count("fuel") >= C.Need) { TakeItem("fuel", C.Need); Finish(this, C); } else if (C.Have == 0) { C.Have = 1; Toast(FString::Printf(TEXT("Әскерге %d канистр жанармай керек"), C.Need), "warn"); } }
			else Finish(this, C);
		}
	}
}

FString AKAPGameMode::EconomySpotText(const AKAPInteractSpot* Sp) const
{
	const FString A = Sp->Action.ToString();
	if (A.StartsWith(TEXT("cpick:")))
	{
		const FKAPContract* C = const_cast<AKAPGameMode*>(this)->FindContract(FCString::Atoi(*A.RightChop(6)));
		if (!C || C->State != 1) return TEXT(" ");
		return C->Type == "recover" ? TEXT("Ұрланған жүкті алу") : TEXT("Металл сынығын жинау");
	}
	if (A == TEXT("army_board")) return TEXT("Әскери аймақ: ережелер");
	return FString();
}

bool AKAPGameMode::EconomySpotUse(AKAPInteractSpot* Sp)
{
	const FString A = Sp->Action.ToString();
	if (A.StartsWith(TEXT("cpick:")))
	{
		FKAPContract* C = FindContract(FCString::Atoi(*A.RightChop(6)));
		if (C && C->State == 1)
		{
			if (C->Type == "recover") { Sp->Destroy(); Finish(this, *C); }
			else
			{
				const FVector2D Pm = KAP::CM2M(Sp->GetActorLocation());
				int32 Best = INDEX_NONE; double Bd = 1e9;
				for (int32 i = 0; i < C->Points.Num(); i++) { const double D = FVector2D::Distance(C->Points[i], Pm); if (D < Bd) { Bd = D; Best = i; } }
				if (Best != INDEX_NONE) C->Points.RemoveAt(Best);
				C->Have++;
				S().Inv.FindOrAdd("scrap") += 1;
				if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->PlayBank("ImpMetal", Sp->GetActorLocation(), 0.8f, 0.9f);
				Sp->Destroy();
				if (C->Have >= C->Need) Finish(this, *C);
				else Toast(FString::Printf(TEXT("Металл сынығы · %d / %d"), C->Have, C->Need), "info");
			}
		}
		else Sp->Destroy();
		return true;
	}
	if (A == TEXT("army_board"))
	{
		Dialog(TEXT("Әскери аймақ"), TEXT("ӘСКЕРИ АЙМАҚ. Рұқсатсыз кіруге тыйым салынады — күзет ескертусіз оқ атуға құқылы.\nРұқсат алу үшін қақпа жанындағы Майор Сейітовтан жұмыс алып, орындаңыз."), {});
		return true;
	}
	return false;
}

// =====================================================================
// Сөйлесу: полиция капитаны, сатушы, майор
// =====================================================================
bool AKAPGameMode::EconomyTalk(AKAPNPC* N)
{
	const FName T = N->TalkId;
	AKAPHUD* H = AKAPHUD::Get(this);
	if (!H) return false;
	FKAPSession& Ss = S();
	if (T == "police")
	{
		RefreshContracts(false);
		Dialog(N->DisplayName, Ss.bPermitWeapon
			? TEXT("Ақсайда тәртіп біздің мойнымызда. Бандиттер жолдарды тонап жүр, мутанттар ауылға жақындады. Көмектессең — ақысын төлейміз.")
			: TEXT("Ақсайда тәртіп біздің мойнымызда. Қару рұқсатың жоқ екен — оны кеніш күзетінен ал. Ал жұмысқа қазір де келісемін."),
			{ { TEXT("Жұмыстарды көрсет"), [H]() { H->ShowJobs("police"); }, false }, { TEXT("Сау болыңыз"), nullptr, false } });
		return true;
	}
	if (T == "trader")
	{
		RefreshContracts(false);
		Dialog(N->DisplayName, TEXT("Ассалаумағалейкум! Қару, оқ-дәрі, дәрі-дәрмек — бәрі бар. Сынық металл әкелсең, сатып аламын. Жүк тасуға адам да керек."),
			{ { TEXT("Дүкен"), [H]() { H->ShowShop("trader"); }, false }, { TEXT("Жұмыс бар ма?"), [H]() { H->ShowJobs("trader"); }, false },
			  { FString::Printf(TEXT("Сынық сату (%d дана)"), Ss.Inv.FindRef("scrap")), [this]() { SellScrap(); }, Ss.Inv.FindRef("scrap") <= 0 }, { TEXT("Сау болыңыз"), nullptr, false } });
		return true;
	}
	if (T == "army")
	{
		RefreshContracts(false);
		const bool bPass = Ss.Flags.Contains("armyPass");
		Dialog(N->DisplayName, bPass
			? TEXT("Қош келдің. Аймаққа кіруге рұқсатың бар. Қоймадан қару алуға болады, жұмыс та табылады.")
			: TEXT("Тоқта. Бұл — әскери аймақ, бөгде адамға кіруге болмайды. Бірақ бізге сенімді адам керек: бір тапсырманы орында — рұқсат аласың."),
			{ { TEXT("Тапсырмалар"), [H]() { H->ShowJobs("army"); }, false }, { TEXT("Әскери қойма"), [H]() { H->ShowShop("army"); }, !bPass }, { TEXT("Түсіндім"), nullptr, false } });
		return true;
	}
	return false;
}

// =====================================================================
// Орындар: полиция бөлімі, дүкен, әскери аймақ
// =====================================================================
bool AKAPGameMode::InArmyZone(FVector2D Pm) const
{
	return bArmyBuilt && FMath::Abs(Pm.X - ArmyC.X) < ArmyHalf.X && FMath::Abs(Pm.Y - ArmyC.Y) < ArmyHalf.Y;
}

void AKAPGameMode::BuildEconomyPlaces()
{
	if (!Builder) return;
	UWorld* W = GetWorld();
	auto Zp = [&](FVector2D P) { return Builder->Z(P); };
	auto At = [&](FVector2D P, float Up = 0.f) { return FVector(P.X * 100, P.Y * 100, Zp(P) + Up); };
	const FColor Blue(40, 90, 210), Red(200, 30, 25), AmberC(242, 193, 78);
	// ---------------- Ақсай полиция бөлімі ----------------
	{
		const FVector2D C = LocOf(this, "police");
		AKAPStructure* Office = Builder->SpawnStruct((int32)EStructType::HQOffice, C, 0, FVector(1), FLinearColor::Gray);
		const FVector2D Oc = Office ? KAP::CM2M(Office->GetActorLocation()) : C;
		if (AActor* A = SpawnProp(W, At(Oc), 0))
		{
			Sign(A, TEXT("ПОЛИЦИЯ"), FVector(-560, 0, 430), 180, 150, Blue);
			Sign(A, TEXT("Ақсай полиция бөлімі · Отдел полиции"), FVector(-560, 0, 330), 180, 34, FColor(230, 230, 230));
			// жалауша бағаны
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(-700, 380, 400), FVector(8, 8, 800), FKAPMat::Metal(FLinearColor(0.7f, 0.7f, 0.7f), 0.3f), FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(-700, 445, 720), FVector(4, 130, 70), FKAPMat::Paint(FLinearColor(0.05f, 0.55f, 0.75f), 0.8f), FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(-697, 445, 722), FVector(30, 30, 2), FKAPMat::Glow(FLinearColor(1.f, 0.8f, 0.1f), 0.6f), FRotator(0, 0, 90), false);
			// полиция түсі: қабырғалардағы көк жолақ, шатырдағы жарық блогы
			const FKAPMat BlueBand = FKAPMat::Paint(FLinearColor(0.05f, 0.18f, 0.55f), 0.5f);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(-518, 0, 250), FVector(6, 800, 46), BlueBand, FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(518, 0, 250), FVector(6, 800, 46), BlueBand, FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(0, -418, 250), FVector(1000, 6, 46), BlueBand, FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(-300, 0, 352), FVector(30, 60, 18), FKAPMat::Glow(FLinearColor(1.f, 0.05f, 0.03f), 6), FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(-300, 70, 352), FVector(30, 60, 18), FKAPMat::Glow(FLinearColor(0.05f, 0.2f, 1.f), 6), FRotator::ZeroRotator, false);
		}
		// патруль көліктері: ақ-көк УАЗ, шатырында жарық блогы
		for (int32 i = 0; i < 2; i++)
		{
			const FVector2D Vp = Oc + FVector2D(-14.f, -6.f + i * 9.f);
			if (AActor* V = ParkedVehicle(W, TEXT("uaz469"), At(Vp), 90, FLinearColor(0.86f, 0.87f, 0.9f), FLinearColor(0.05f, 0.12f, 0.36f)))
			{
				KAPVis::Add(V, V->GetRootComponent(), EKAPShape::Cube, FVector(-40, -20, 205), FVector(22, 34, 10), FKAPMat::Glow(FLinearColor(1.f, 0.05f, 0.03f), 4), FRotator::ZeroRotator, false);
				KAPVis::Add(V, V->GetRootComponent(), EKAPShape::Cube, FVector(-40, 20, 205), FVector(22, 34, 10), FKAPMat::Glow(FLinearColor(0.05f, 0.2f, 1.f), 4), FRotator::ZeroRotator, false);
				for (float Sg : { -1.f, 1.f }) Sign(V, TEXT("ПОЛИЦИЯ"), FVector(-20, Sg * 87.f, 90), Sg * 90.f, 22, Blue);
			}
		}
	}
	// ---------------- Ерболаттың дүкені ----------------
	{
		const FVector2D C = LocOf(this, "shop");
		AKAPStructure* K = Builder->SpawnStruct((int32)EStructType::GuardPost, C, 0, FVector(1), FLinearColor(0.55f, 0.42f, 0.28f));
		const FVector2D Kc = K ? KAP::CM2M(K->GetActorLocation()) : C;
		if (AActor* A = SpawnProp(W, At(Kc), 0))
		{
			Sign(A, TEXT("ДҮКЕН · ҚАРУ-ЖАРАҚ"), FVector(0, 0, 420), 0, 46, AmberC);
			Sign(A, TEXT("ДҮКЕН · ҚАРУ-ЖАРАҚ"), FVector(0, 0, 420), 180, 46, AmberC);
			// сөре: жәшіктер, бөшке, қару тіреуіші
			FKAPMat Wd = FKAPMat::Paint(FLinearColor(0.3f, 0.2f, 0.11f), 0.85f); Wd.Ribs = 0.4f; Wd.RibPeriod = 8.f;
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(330, 0, 50), FVector(80, 220, 100), Wd, FRotator::ZeroRotator, true);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(330, 0, 103), FVector(90, 230, 6), FKAPMat::Paint(FLinearColor(0.15f, 0.1f, 0.06f), 0.6f), FRotator::ZeroRotator, false);
			for (int32 i = 0; i < 4; i++) KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(300 + (i % 2) * 70.f, -320 + (i / 2) * 70.f, 30), FVector(60, 60, 60), Wd, FRotator(0, i * 12.f, 0), true);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(320, 330, 50), FVector(60, 60, 100), FKAPMat::Sheet(FLinearColor(0.45f, 0.08f, 0.05f), 0.6f), FRotator::ZeroRotator, true);
		}
	}
	// ---------------- әскери аймақ ----------------
	{
		const FVector2D C = LocOf(this, "army");
		if (C.IsZero()) return;
		ArmyC = C; bArmyBuilt = true;
		const float Hx = ArmyHalf.X, Hy = ArmyHalf.Y, Gate = 7.f;
		const FLinearColor Olive(0.26f, 0.29f, 0.17f), Khaki(0.45f, 0.4f, 0.28f);
		// қоршау (батыста қақпа)
		Builder->SpawnStruct((int32)EStructType::Fence, C + FVector2D(0, -Hy), 0, FVector(Hx * 200, 20, 280), FLinearColor::Gray);
		Builder->SpawnStruct((int32)EStructType::Fence, C + FVector2D(0, Hy), 0, FVector(Hx * 200, 20, 280), FLinearColor::Gray);
		Builder->SpawnStruct((int32)EStructType::Fence, C + FVector2D(Hx, 0), 90, FVector(Hy * 200, 20, 280), FLinearColor::Gray);
		Builder->SpawnStruct((int32)EStructType::Fence, C + FVector2D(-Hx, -(Hy + Gate) * 0.5f), 90, FVector((Hy - Gate) * 100, 20, 280), FLinearColor::Gray);
		Builder->SpawnStruct((int32)EStructType::Fence, C + FVector2D(-Hx, (Hy + Gate) * 0.5f), 90, FVector((Hy - Gate) * 100, 20, 280), FLinearColor::Gray);
		Builder->SpawnStruct((int32)EStructType::Gate, C + FVector2D(-Hx, 0), 90, FVector(1), FLinearColor::Gray);
		// бақылау мұнаралары, шатырлар, штаб
		for (float Sx : { -1.f, 1.f }) for (float Sy : { -1.f, 1.f }) Builder->SpawnStruct((int32)EStructType::Tower, C + FVector2D(Sx * (Hx - 4), Sy * (Hy - 4)), 0, FVector(1), FLinearColor::Gray);
		for (int32 i = 0; i < 3; i++) for (int32 j = 0; j < 2; j++) Builder->SpawnStruct((int32)EStructType::Tent, C + FVector2D(-10 + i * 16.f, -30 + j * 14.f), 90, FVector(1), Olive, 40 + i * 2 + j);
		Builder->SpawnStruct((int32)EStructType::Workshop, C + FVector2D(28, 22), 0, FVector(1400, 1000, 600), Olive, 21);
		if (AActor* A = SpawnProp(W, At(C), 0))
		{
			const FVector Gp(-Hx * 100, 0, 0);
			// ескерту тақталары (қақпаның сыртында)
			for (float Sg : { -1.f, 1.f })
			{
				const FVector B = Gp + FVector(-900, Sg * 700, 0);
				KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, B + FVector(0, 0, 220), FVector(8, 360, 180), FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.8f), 0.6f), FRotator::ZeroRotator, true);
				for (float Sy : { -1.f, 1.f }) KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, B + FVector(0, Sy * 160, 80), FVector(10, 10, 160), FKAPMat::RustySteel(), FRotator::ZeroRotator, false);
				Sign(A, TEXT("ӘСКЕРИ АЙМАҚ"), B + FVector(-6, 0, 270), 180, 52, Red);
				Sign(A, TEXT("Кіруге тыйым салынады!"), B + FVector(-6, 0, 215), 180, 30, FColor(25, 25, 25));
				Sign(A, TEXT("ВОЕННАЯ ЗОНА · ВХОД ВОСПРЕЩЁН"), B + FVector(-6, 0, 170), 180, 22, FColor(25, 25, 25));
			}
			// құм қаптары (қақпа маңындағы бекініс)
			FKAPMat Sand = FKAPMat::Paint(Khaki, 0.95f); Sand.Ribs = 0.25f; Sand.RibPeriod = 30.f; Sand.Grime = 0.8f; Sand.DustTop = 1.f;
			for (float Sg : { -1.f, 1.f }) for (int32 r = 0; r < 3; r++)
				KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, Gp + FVector(-350, Sg * (900 + (r % 2) * 25.f), 25 + r * 42.f), FVector(110, 420, 44), Sand, FRotator(0, 0, (r % 2) * 2.f), true);
			// жалау: көгілдір мата, алтын күн
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(1500, -900, 500), FVector(10, 10, 1000), FKAPMat::Metal(FLinearColor(0.7f, 0.7f, 0.7f), 0.3f), FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(1500, -820, 920), FVector(4, 160, 85), FKAPMat::Paint(FLinearColor(0.05f, 0.55f, 0.75f), 0.8f), FRotator::ZeroRotator, false);
			KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(1497, -820, 922), FVector(36, 36, 2), FKAPMat::Glow(FLinearColor(1.f, 0.8f, 0.1f), 0.6f), FRotator(0, 0, 90), false);
			// бөшкелер мен жәшіктер
			FKAPMat Drum = FKAPMat::Sheet(FLinearColor(0.2f, 0.25f, 0.14f), 0.6f);
			for (int32 i = 0; i < 6; i++) KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cylinder, FVector(2600 + (i % 3) * 75.f, 2400 + (i / 3) * 75.f, 50), FVector(60, 60, 100), Drum, FRotator::ZeroRotator, true);
			FKAPMat Box = FKAPMat::Paint(FLinearColor(0.22f, 0.26f, 0.14f), 0.8f); Box.Panel = 0.6f; Box.PanelW = 40.f;
			for (int32 i = 0; i < 5; i++) KAPVis::Add(A, A->GetRootComponent(), EKAPShape::Cube, FVector(-1600 + i * 110.f, 3600, 40 + (i % 2) * 0.f), FVector(100, 70, 80), Box, FRotator(0, i * 7.f, 0), true);
			// прожекторлар (мұнаралардан ішке қарай)
			for (float Sx : { -1.f, 1.f })
			{
				USpotLightComponent* L = NewObject<USpotLightComponent>(A);
				L->SetupAttachment(A->GetRootComponent());
				L->SetRelativeLocation(FVector(Sx * (Hx - 4) * 100, -(Hy - 4) * 100, 1100));
				L->SetRelativeRotation((FVector(-Sx * 2000, 2500, -1000)).Rotation());
				L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(9000.f); L->SetAttenuationRadius(6000.f);
				L->SetOuterConeAngle(22.f); L->SetInnerConeAngle(10.f); L->SetLightColor(FLinearColor(0.9f, 0.95f, 1.f)); L->SetCastShadows(false);
				L->RegisterComponent();
			}
		}
		// техника: БТР-лар мен әскери УАЗ
		ParkedVehicle(W, TEXT("btr"), At(C + FVector2D(-40, 16)), 0, Olive, Olive);
		ParkedVehicle(W, TEXT("btr"), At(C + FVector2D(-40, -16)), 0, Olive, Olive);
		ParkedVehicle(W, TEXT("uaz469"), At(C + FVector2D(5, 30)), 90, Olive, Olive * 0.8f);
		ParkedVehicle(W, TEXT("uaz469"), At(C + FVector2D(14, 30)), 90, Olive, Olive * 0.8f);
		// ережелер тақтасы
		Builder->SpawnSpot("army_board", At(C + FVector2D(-Hx - 9, 7), 120), 260);
	}
	// ---------------- дизель генераторлары (меш + нақты жазылған гул): баспаналар, күзет, басқарма, полиция, әскер ----------------
	{
		FRandomStream Gr(4242);
		auto Gen = [&](FVector2D P, float Yaw)
		{
			AActor* A = SpawnProp(W, At(P), Yaw);
			if (!A) return;
			USceneComponent* R = A->GetRootComponent();
			const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.22f, 0.22f, 0.21f), 0.55f);
			FKAPMat Body = FKAPMat::Paint(FLinearColor(0.62f, 0.45f, 0.06f), 0.6f); Body.Grime = 0.8f; Body.Rust = 0.25f;
			FKAPMat Alt = FKAPMat::Paint(FLinearColor(0.12f, 0.2f, 0.12f), 0.55f); Alt.Grime = 0.7f;
			FKAPMat Tank = FKAPMat::Paint(FLinearColor(0.35f, 0.05f, 0.03f), 0.6f); Tank.Grime = 0.75f; Tank.Rust = 0.3f;
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(0, -36, 5), FVector(175, 9, 10), Steel, FRotator::ZeroRotator, true);
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(0, 36, 5), FVector(175, 9, 10), Steel, FRotator::ZeroRotator, true);
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(0, 0, 20), FVector(160, 66, 22), Tank, FRotator::ZeroRotator, true);   // жанармай багы (рама)
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(-28, 0, 62), FVector(78, 56, 60), Body, FRotator::ZeroRotator, true);   // қозғалтқыш
			KAPVis::Add(A, R, EKAPShape::Cylinder, FVector(42, 0, 58), FVector(48, 48, 56), Alt, FRotator(90, 0, 0), true);   // генератор
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(-72, 0, 62), FVector(10, 50, 50), Steel, FRotator::ZeroRotator, false); // радиатор торы
			KAPVis::Add(A, R, EKAPShape::Cylinder, FVector(-40, 18, 112), FVector(9, 9, 46), Steel, FRotator::ZeroRotator, false); // түтін құбыры
			KAPVis::Add(A, R, EKAPShape::Cylinder, FVector(-40, 18, 92), FVector(16, 16, 22), Steel, FRotator::ZeroRotator, false); // бәсеңдеткіш
			KAPVis::Add(A, R, EKAPShape::Cube, FVector(78, 0, 62), FVector(14, 30, 34), FKAPMat::Paint(FLinearColor(0.3f, 0.3f, 0.3f), 0.5f), FRotator::ZeroRotator, false); // басқару қорабы
			KAPVis::Add(A, R, EKAPShape::Sphere, FVector(86, 8, 70), FVector(4, 4, 4), FKAPMat::Glow(FLinearColor(0.2f, 1.f, 0.3f), 20), FRotator::ZeroRotator, false);
			if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->AttachGenerator(R, FVector(0, 0, 60), 0.9f);
		};
		// жолдан, ғимараттан аулақ бос орын
		auto Free = [&](FVector2D C, float R0)
		{
			for (int32 k = 0; k < 12; k++)
			{
				const float A = k * UE_TWO_PI / 12.f + 0.6f;
				const FVector2D Q = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R0;
				if (DistToRoad(Q) > 6.0) return Q;
			}
			return C + FVector2D(R0, 0);
		};
		for (FName Id : { FName("wagon0"), FName("wagon1"), FName("wagon2"), FName("wagon3"), FName("guard1"), FName("hq") })
		{
			FVector2D Cp = LocOf(this, Id);
			if (TWeakObjectPtr<AKAPStructure>* S = Builder->Places.Find(Id)) if (S->IsValid()) Cp = KAP::CM2M((*S)->GetActorLocation());
			Gen(Free(Cp, Id == "hq" ? 30.f : 8.5f), Gr.FRandRange(0.f, 360.f));
		}
		Gen(Free(LocOf(this, "police"), 14.f), 0.f);
		if (bArmyBuilt) Gen(ArmyC + FVector2D(22, -24), 90.f);
	}
}

void AKAPGameMode::SpawnEconomyNPCs()
{
	FKAPSession& Ss = S();
	auto Mk = [&](ENPCKind K, FVector2D P, const FString& Name, FName Talk) { AKAPNPC* N = SpawnNPC((int32)K, P, Name, true); if (N) N->TalkId = Talk; return N; };
	// полиция
	const FVector2D Pc = LocOf(this, "police");
	if (!Pc.IsZero())
	{
		if (AKAPNPC* N = Mk(ENPCKind::Police, Pc + FVector2D(-7, 2), TEXT("Полиция капитаны Әділет"), "police")) { N->PatrolR = 2; NamedNPC.Add("police", N); }
		for (int32 i = 0; i < 3; i++) if (AKAPNPC* N = Mk(ENPCKind::Police, Pc + FVector2D(Rng.FRandRange(-20, 5), Rng.FRandRange(-15, 15)), TEXT("Полицей"), "policeman")) N->PatrolR = 35;
		const FVector2D J = LocOf(this, "josaly");
		for (int32 i = 0; i < 2; i++) if (AKAPNPC* N = Mk(ENPCKind::Police, J + FVector2D(Rng.FRandRange(-20, 20), Rng.FRandRange(-20, 20)), TEXT("Полицей"), "policeman")) N->PatrolR = 45;
	}
	// сатушы
	const FVector2D Sh = LocOf(this, "shop");
	if (!Sh.IsZero()) if (AKAPNPC* N = Mk(ENPCKind::Trader, Sh + FVector2D(3.2f, 0), TEXT("Сатушы Ерболат"), "trader")) { N->PatrolR = 1; N->WanderR = 1; NamedNPC.Add("trader", N); }
	// әскер
	if (bArmyBuilt)
	{
		const FVector2D C = ArmyC;
		if (AKAPNPC* N = Mk(ENPCKind::Soldier, C + FVector2D(-ArmyHalf.X - 6, -4), TEXT("Майор Ерлан Сейітов"), "army")) { N->PatrolR = 1; NamedNPC.Add("army", N); }
		for (float Sg : { -1.f, 1.f }) if (AKAPNPC* N = Mk(ENPCKind::Soldier, C + FVector2D(-ArmyHalf.X + 2, Sg * 6), TEXT("Сарбаз"), "soldier")) N->PatrolR = 2;
		for (int32 i = 0; i < 8; i++) if (AKAPNPC* N = Mk(ENPCKind::Soldier, C + FVector2D(Rng.FRandRange(-45, 45), Rng.FRandRange(-35, 35)), TEXT("Сарбаз"), "soldier")) N->PatrolR = 30;
	}
	// белсенді жұмыстардың нысаналарын қайта қою (жүктелген ойын)
	for (FKAPContract& C : Ss.Contracts)
	{
		if (C.State != 1 || !C.Points.Num()) continue;
		if (C.Type == "bounty" || C.Type == "nest")
			for (int32 i = C.Have; i < C.Need; i++)
				if (AKAPNPC* E = SpawnNPC((int32)(C.Type == "nest" ? ENPCKind::Irradiated : ENPCKind::Bandit), C.Points[0] + FVector2D(Rng.FRandRange(-10, 10), Rng.FRandRange(-10, 10)), C.Type == "nest" ? TEXT("Мутант") : TEXT("Бандит"), true))
				{ E->ContractUid = C.Uid; E->PatrolR = 14; }
		if (C.Type == "recover") SpawnPile(this, C.Uid, C.Points[0], true);
		if (C.Type == "scrap") for (const FVector2D& P : C.Points) SpawnPile(this, C.Uid, P, false);
	}
}
