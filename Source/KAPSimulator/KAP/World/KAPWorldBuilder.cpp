#include "KAPWorldBuilder.h"
#include "KAPStructure.h"
#include "KAPVisuals.h"
#include "KAPNature.h"
#include "KAP/NPC/KAPAnimal.h"
#include "KAP/NPC/KAPLife.h"
#include "KAPTerrain.h"
#include "WellActor.h"
#include "WellManagerSubsystem.h"
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

AKAPWorldBuilder::AKAPWorldBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

float AKAPWorldBuilder::Z(FVector2D P) const { return KAPVis::GroundZ(GetWorld(), P.X * 100.f, P.Y * 100.f, 0.f); }
float AKAPWorldBuilder::RoadZ(FVector2D P) const { const float G = Z(P); return Nature ? Nature->DeckZ(P, G) : G; }

AKAPStructure* AKAPWorldBuilder::SpawnStruct(int32 Type, FVector2D P, float Yaw, FVector Size, FLinearColor Color, int32 Seed, bool bBright)
{
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const EStructType T = (EStructType)Type;
	// қоршау: жол қиып өтетін жерлерде ойық қалдырып, бірнеше бөлікке бөлеміз
	if (T == EStructType::Fence && !bSplittingFence)
	{
		const FRotator Rt(0, Yaw, 0);
		const FVector2D Ax(Rt.Vector().X, Rt.Vector().Y);
		const float Half = Size.X * 0.5f;
		TArray<bool> Blocked; const int32 N = FMath::Max(1, FMath::CeilToInt32(Size.X / 100.f));
		for (int32 i = 0; i <= N; i++) Blocked.Add(DistToRoadCm(P * 100.0 + Ax * (-Half + Size.X * i / N)) < 450.f);
		AKAPStructure* First = nullptr; int32 i0 = -1;
		bSplittingFence = true;
		for (int32 i = 0; i <= N + 1; i++)
		{
			const bool bFree = i <= N && !Blocked[i];
			if (bFree && i0 < 0) i0 = i;
			if (!bFree && i0 >= 0)
			{
				const float A = -Half + Size.X * i0 / N, B = -Half + Size.X * (i - 1) / N;
				if (B - A >= 200.f)
				{
					const FVector2D Mid = P + Ax * ((A + B) * 0.5f / 100.f);
					AKAPStructure* Pc = SpawnStruct(Type, Mid, Yaw, FVector(B - A, Size.Y, Size.Z), Color, Seed, bBright);
					if (!First) First = Pc;
				}
				i0 = -1;
			}
		}
		bSplittingFence = false;
		if (Blocked.Contains(true)) UE_LOG(LogTemp, Display, TEXT("KAPROAD fence (%.0f, %.0f): жолға ойық қалдырылды"), P.X, P.Y);
		return First;
	}
	AKAPStructure* S = GetWorld()->SpawnActor<AKAPStructure>(FVector(P.X * 100, P.Y * 100, Z(P) - 5), FRotator(0, Yaw, 0), SP);
	if (!S) return nullptr;
	S->Type = T; S->BuildSize = Size;
	if (T != EStructType::Fence && OnRoad(S, 150.f))
	{
		const bool bDecor = T == EStructType::House || T == EStructType::Ruin || T == EStructType::Yurt || T == EStructType::Tent || T == EStructType::Campfire
			|| T == EStructType::Barrier || T == EStructType::Crate || T == EStructType::Silo;
		if (bDecor)
		{
			// әшекей ғимарат жолдың үстінде — алып тастаймыз
			UE_LOG(LogTemp, Display, TEXT("KAPROAD removed type=%d at (%.0f, %.0f) m"), Type, P.X, P.Y);
			S->Destroy();
			return nullptr;
		}
		// сюжетке керек ғимарат: жолдан шетке жылжытамыз (жақын бос орын, 4–60 м)
		bool bMoved = false;
		const FVector2D H = S->FootprintHalf();
		for (float Rr = 4.f; Rr <= 60.f && !bMoved; Rr += 2.f)
			for (int32 k = 0; k < 16 && !bMoved; k++)
			{
				const float A = k / 16.f * UE_TWO_PI;
				const FVector2D Q = P + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (Rr + H.GetMax() / 100.f);
				S->SetActorLocation(FVector(Q.X * 100, Q.Y * 100, Z(Q) - 5));
				if (!OnRoad(S, 150.f)) { bMoved = true; UE_LOG(LogTemp, Display, TEXT("KAPROAD moved type=%d (%.0f, %.0f) -> (%.0f, %.0f) m"), Type, P.X, P.Y, Q.X, Q.Y); }
			}
		if (!bMoved) S->SetActorLocation(FVector(P.X * 100, P.Y * 100, Z(P) - 5));
	}
	S->Build(T, Size, Color, Seed, bBright);
	return S;
}

AKAPInteractSpot* AKAPWorldBuilder::SpawnSpot(FName Action, FVector PosCm, float Radius, bool bBox)
{
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKAPInteractSpot* S = GetWorld()->SpawnActor<AKAPInteractSpot>(PosCm, FRotator::ZeroRotator, SP);
	if (!S) return nullptr;
	S->Action = Action; S->Radius = Radius;
	if (bBox) S->Visual = KAPVis::Add(S, S->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 15), FVector(50, 40, 30), FKAPMat::Paint(FLinearColor(0.2f, 0.35f, 0.15f), 0.7f), FRotator::ZeroRotator, false);
	return S;
}

static void Label(AActor* A, const FString& Text, FVector Pos, float Size, FColor C)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(A);
	T->SetupAttachment(A->GetRootComponent()); T->SetWorldLocation(Pos);
	T->SetText(FText::FromString(Text)); T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(Size); T->SetTextRenderColor(C);
	T->RegisterComponent();
}

void AKAPWorldBuilder::BuildAll()
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (!D || !WM) return;
	FRandomStream R(777);
	const auto L = [&](FName Id) { const FKAPLocationRow* Row = D->Loc(Id); return Row ? FVector2D(Row->X, Row->Y) : FVector2D::ZeroVector; };
	const FLinearColor HouseCols[5] = { FLinearColor(0.78f, 0.7f, 0.55f), FLinearColor(0.7f, 0.6f, 0.45f), FLinearColor(0.62f, 0.52f, 0.38f), FLinearColor(0.82f, 0.76f, 0.62f), FLinearColor(0.58f, 0.48f, 0.36f) };

	// ---- ұңғымалар ----
	for (int32 i = 0; i < WM->Wells.Num(); i++)
	{
		const FKAPWellRow& Row = WM->Wells[i].Row;
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWellActor* A = GetWorld()->SpawnActor<AWellActor>(FVector(Row.X * 100, Row.Y * 100, Z(FVector2D(Row.X, Row.Y)) - 5), FRotator(0, R.FRandRange(0, 360), 0), SP);
		A->Build(i, Row);
		WM->Wells[i].Actor = A;
		if (i % 3 != 0) SpawnSpot(FName(*FString::Printf(TEXT("crate:well%d"), Row.Id)), A->GetActorTransform().TransformPosition(FVector(350, -400, 40)), 200, true);
	}
	// ---- №1 ұңғыма жанындағы сорғы цехы ----
	if (WM->Wells.Num()) { const FVector2D W0 = WM->Wells[0].PosM(); SpawnStruct((int32)EStructType::Workshop, W0 + FVector2D(-38, 26), 0, FVector(1800, 1200, 700), FLinearColor::Gray, 11); }
	// ---- вагон-үйлер ----
	for (const TCHAR* Id : { TEXT("wagon0"), TEXT("wagon1"), TEXT("wagon2"), TEXT("wagon3") })
	{
		AKAPStructure* S = SpawnStruct((int32)EStructType::Wagon, L(Id), 0, FVector(800, 300, 280), FLinearColor(0.62f, 0.52f, 0.36f), 1, FCString::Strcmp(Id, TEXT("wagon0")) == 0);
		Places.Add(Id, S);
		SpawnSpot(FName(*FString::Printf(TEXT("bed:%s"), Id)), S->BedPos, 200);
		SpawnSpot(FName(*FString::Printf(TEXT("crate:%s"), Id)), S->CratePos, 200, true);
		if (FCString::Strcmp(Id, TEXT("wagon0")) == 0)
		{
			AKAPInteractSpot* T = SpawnSpot("tablet", S->TablePos, 200);
			T->Visual = KAPVis::Add(T, T->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 2), FVector(30, 22, 3), FKAPMat::Paint(FLinearColor(0.08f, 0.08f, 0.07f), 0.4f), FRotator::ZeroRotator, false);
			KAPVis::Add(T, T->GetRootComponent(), EKAPShape::Cube, FVector(0, 0, 4), FVector(26, 18, 1), FKAPMat::Glow(FLinearColor(0.2f, 0.5f, 1.f), 6), FRotator::ZeroRotator, false);
		}
	}
	// ---- ескі көлік қаңқасы (радиатор) ----
	Wreck = SpawnStruct((int32)EStructType::Wreck, L("oldcar"), 60, FVector(1), FLinearColor::Gray);
	SpawnSpot("radiator", Wreck->GetActorLocation() + FVector(0, 0, 80), 350);
	// ---- күзет бекеті ----
	SpawnStruct((int32)EStructType::GuardPost, L("guard1"), 0, FVector(1), FLinearColor::Gray);
	// ---- ауылдар ----
	for (const TCHAR* Id : { TEXT("aksai"), TEXT("josaly") })
	{
		const FVector2D C = L(Id);
		// үйлер ауылға кіретін жолдардың бойымен қатар тұрады (көше), есігі жолға қарайды, артында аула.
		// Орталықта алаң (киіз үй, от), бірінші орын — дүкенге (Ақсайда)
		const bool bAksai = FCString::Strcmp(Id, TEXT("aksai")) == 0;
		const int32 Ni = D->Graph.Nodes.IndexOfByPredicate([&](const KAPAI::FRoadNode& N) { return N.Id == FName(Id); });
		TArray<FVector2D> Dirs;
		if (Ni != INDEX_NONE) for (const TPair<int32, int32>& Ad : D->Graph.Nodes[Ni].Adj) Dirs.Add((D->Graph.Nodes[Ad.Key].Pos - C).GetSafeNormal());
		if (Dirs.Num() == 0) Dirs.Add(FVector2D(1, 0));
		if (Dirs.Num() == 1) Dirs.Add(-Dirs[0]); // тұйық ауыл — көше әрі қарай жалғасады
		// алаң жолдардан алыс жаққа: жол бағыттарының қосындысына қарама-қарсы
		FVector2D Away(0, 0); for (const FVector2D& Dv : Dirs) Away -= Dv;
		Away = Away.SizeSquared() > 0.04 ? Away.GetSafeNormal() : FVector2D(-Dirs[0].Y, Dirs[0].X);
		const FVector2D AwayN(Away.Y, -Away.X);
		const FVector2D Plaza = C + Away * 10.0, SiloP = C + Away * 12.0 + AwayN * 24.0;
		if (Dirs.Num() < 3) Dirs.Add(Away); // жолсыз бүйір көше (құмды өткел)
		TArray<FVector2D> Used; Used.Add(C); // алаң
		TArray<float> UsedR; UsedR.Add(16.f);
		if (const FKAPLocationRow* Pr = D->Loc("police")) if (FVector2D::Distance(FVector2D(Pr->X, Pr->Y), C) < 150.f) { Used.Add(FVector2D(Pr->X, Pr->Y)); UsedR.Add(26.f); }
		Used[0] = Plaza; Used.Add(SiloP); UsedR.Add(6.f); // алаң, су мұнарасы
		auto Free = [&](FVector2D P, float Rr) { for (int32 k = 0; k < Used.Num(); k++) if (FVector2D::Distance(P, Used[k]) < Rr + UsedR[k]) return false; return true; };
		int32 Built = 0; bool bShopDone = !bAksai;
		for (float T : { 20.f, 35.f, 50.f, 65.f })
			for (int32 di = 0; di < FMath::Min(4, Dirs.Num()); di++)
				for (float Sd : { 1.f, -1.f })
				{
					if (Built >= 14) break;
					const FVector2D Dk = Dirs[di], Nk(Dk.Y, -Dk.X);
					const float Wc = R.FRandRange(760, 950), Dc = R.FRandRange(560, 680);
					const float Off = 4.f + 1.5f + 4.f + Dc / 200.f;
					const FVector2D P = C + Dk * (T + R.FRandRange(-2.f, 2.f)) + Nk * Sd * Off;
					const FVector2D ToRoad = -Nk * Sd;
					const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-ToRoad.X, ToRoad.Y));
					if (!Free(P, 7.5f)) continue;
					// аула басқа жолға шықпасын (артқы бұрыштар, бүйір ортасы)
					const FVector2D Bk = P - ToRoad * ((Dc / 200.f) + 9.f), Sdv(ToRoad.Y, -ToRoad.X);
					const float Hw = Wc / 200.f + 3.f;
					if (DistToRoadCm((Bk + Sdv * Hw) * 100.0) < 500.f || DistToRoadCm((Bk - Sdv * Hw) * 100.0) < 500.f || DistToRoadCm((P + Sdv * Hw) * 100.0) < 500.f || DistToRoadCm((P - Sdv * Hw) * 100.0) < 500.f) continue;
					if (!bShopDone)
					{
						// дүкен: жолға қарап, ауласыз; сатушы есік алдындағы сөренің артында тұрады
						ShopSlot = C + Dk * T + Nk * Sd * (4.f + 1.5f + 2.6f + 1.f); ShopYaw = Yaw; bShopSlot = true; bShopDone = true;
						Used.Add(ShopSlot); UsedR.Add(7.f);
						continue;
					}
					if (SpawnStruct((int32)EStructType::House, P, Yaw, FVector(Wc, Dc, 320), HouseCols[R.RandRange(0, 4)], Built + (bAksai ? 0 : 50), true))
					{
						Used.Add(P); UsedR.Add(7.5f); Built++;
					}
				}
		UE_LOG(LogTemp, Display, TEXT("KAPVILLAGE %s: %d houses, shop slot=%d"), Id, Built, bAksai && bShopSlot ? 1 : 0);
		SpawnStruct((int32)EStructType::Yurt, Plaza + Away * 4.0 + AwayN * 5.0, 0, FVector(1), FLinearColor::White);
		SpawnStruct((int32)EStructType::Campfire, Plaza - AwayN * 3.0, 0, FVector(1), FLinearColor::White);
		SpawnStruct((int32)EStructType::Barrier, C + FVector2D(-62, 0), 90, FVector(800, 60, 110), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Barrier, C + FVector2D(62, 0), 90, FVector(800, 60, 110), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Silo, SiloP, 0, FVector(1), FLinearColor::Gray);
		SpawnSpot(FName(*FString::Printf(TEXT("crate:%s"), Id)), FVector((C.X - 8) * 100, (C.Y + 2) * 100, Z(C + FVector2D(-8, 2)) + 40), 200, true);
		Label(this, FCString::Strcmp(Id, TEXT("aksai")) == 0 ? TEXT("AKSAI") : TEXT("ZHOSALY"), FVector((C.X - 66) * 100, C.Y * 100, Z(C) + 600), 160, FColor(242, 193, 78));
	}
	// ---- қираған ауыл ----
	{
		const FVector2D C = L("ruins");
		for (int32 i = 0; i < 12; i++)
		{
			const float A = R.FRandRange(0, UE_TWO_PI), Rr = R.FRandRange(15, 60);
			SpawnStruct((int32)EStructType::Ruin, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, R.RandRange(0, 3) * 90.f, FVector(R.FRandRange(600, 900), R.FRandRange(500, 700), 320), FLinearColor::Gray, 100 + i);
		}
	}
	// ---- кеніш басқармасы ----
	{
		const FVector2D C = L("hq");
		// басқарманың негізгі цехы — ішіне кіруге болады; кез келген ұңғымадағы апатқа сирена қосылады
		if (AKAPStructure* Ws = SpawnStruct((int32)EStructType::Workshop, C + FVector2D(-20, -25), 0, FVector(2200, 1200, 700), FLinearColor::Gray, 5)) Ws->AlarmRadiusM = 5000.f;
		SpawnStruct((int32)EStructType::House, C + FVector2D(25, -20), 90, FVector(1600, 1000, 320), FLinearColor(0.5f, 0.54f, 0.56f), 6);
		SpawnStruct((int32)EStructType::House, C + FVector2D(20, 25), 0, FVector(1400, 800, 320), FLinearColor(0.55f, 0.56f, 0.5f), 7);
		HQOffice = SpawnStruct((int32)EStructType::HQOffice, C + FVector2D(-15, 10), 0, FVector(1), FLinearColor::Gray);
		SpawnSpot("hq_desk", HQOffice->TablePos, 220);
		SpawnSpot("hq_radio", HQOffice->BedPos, 220);
		for (int32 i = 0; i < 4; i++) SpawnStruct((int32)EStructType::Silo, C + FVector2D(30 + i * 4.5f, -40), 0, FVector(1), FLinearColor::Gray);
		const float Rf = 55;
		SpawnStruct((int32)EStructType::Fence, C + FVector2D(0, -Rf), 0, FVector(Rf * 200, 20, 240), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Fence, C + FVector2D(-Rf, 0), 90, FVector(Rf * 200, 20, 240), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Fence, C + FVector2D(Rf, 0), 90, FVector(Rf * 200, 20, 240), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Fence, C + FVector2D(-Rf / 2 - 4, Rf), 0, FVector((Rf - 8) * 100, 20, 240), FLinearColor::Gray);
		SpawnStruct((int32)EStructType::Fence, C + FVector2D(Rf / 2 + 4, Rf), 0, FVector((Rf - 8) * 100, 20, 240), FLinearColor::Gray);
		HQGate = SpawnStruct((int32)EStructType::Gate, C + FVector2D(0, Rf), 0, FVector(1), FLinearColor::Gray);
		SpawnSpot("hq_gate", FVector(C.X * 100, (C.Y + Rf + 2) * 100, Z(C + FVector2D(0, Rf + 2)) + 100), 450);
		Label(this, TEXT("QazUran"), FVector(C.X * 100, (C.Y + Rf) * 100, Z(C) + 900), 260, FColor(160, 210, 255));
		SpawnSpot("crate:hq", FVector((C.X + 25) * 100, (C.Y - 12) * 100, Z(C + FVector2D(25, -12)) + 40), 200, true);
	}
	// ---- су қоймасы ----
	{
		AKAPStructure* T = SpawnStruct((int32)EStructType::Tank, L("res"), 0, FVector(1), FLinearColor::Gray);
		SpawnSpot("res_pump", T->GetActorLocation() + FVector(1400, 260, 120), 320);
		SpawnSpot("crate:res", T->GetActorLocation() + FVector(1400, 500, 40), 200, true);
	}
	// ---- мұнаралар ----
	for (const TCHAR* Id : { TEXT("tower1"), TEXT("tower2"), TEXT("tower3") })
	{
		AKAPStructure* T = SpawnStruct((int32)EStructType::Tower, L(Id), 0, FVector(1), FLinearColor::Gray);
		Places.Add(Id, T);
		SpawnSpot(FName(*FString::Printf(TEXT("tower:%s"), Id)), T->GetActorLocation() + FVector(240, 0, 100), 260);
	}
	SpawnSpot("crate:tower3", FVector((L("tower3").X + 3) * 100, (L("tower3").Y + 3) * 100, Z(L("tower3") + FVector2D(3, 3)) + 40), 200, true);
	// ---- бандит лагері ----
	{
		const FVector2D C = L("bandit");
		const FLinearColor TentC[3] = { FLinearColor(0.25f, 0.28f, 0.17f), FLinearColor(0.32f, 0.26f, 0.16f), FLinearColor(0.18f, 0.21f, 0.24f) };
		for (int32 i = 0; i < 6; i++) { const float A = R.FRandRange(0, UE_TWO_PI), Rr = R.FRandRange(10, 30); SpawnStruct((int32)EStructType::Tent, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, 0, FVector(1), TentC[i % 3], i); }
		SpawnStruct((int32)EStructType::Campfire, C, 0, FVector(1), FLinearColor::White);
		for (int32 i = 0; i < 6; i++) { const float A = i / 6.f * UE_TWO_PI; SpawnStruct((int32)EStructType::Barrier, C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 35, FMath::RadiansToDegrees(A) + 90, FVector(500, 60, 110), FLinearColor::Gray); }
	}
	// ---- мутант ұялары ----
	for (const TCHAR* Id : { TEXT("nest1"), TEXT("nest2"), TEXT("nest3"), TEXT("nest4") }) SpawnStruct((int32)EStructType::Nest, L(Id), 0, FVector(1), FLinearColor::White, FCString::Strlen(Id) * 13);
	// табиғат: көпір аралықтары жолдан бұрын (жол таспасы көпір үстімен өтеді)
	Nature = GetWorld()->SpawnActor<AKAPNature>();
	if (Nature) Nature->PlanBridges(UKAPDataSubsystem::Get(this), [this](FVector2D P) { return Z(P); });
	BuildRoads();
	ClearRoadOverlaps(); // тексеріс: жолда ғимарат қалмағанын логқа жазады
	BuildVegetation();
	BuildVehicles();
	AKAPStructure::InvalidateCache();
}

/** Жол беті: әр жолға жер бедеріне жабысқан таспа (процедуралық меш).
 *  Қауіпсіз жолдар (Danger ≤ 0.1) — асфальт: 7 см қалың қабат, жиегі көлбеу, соқтығысы бар (дөңгелек үстінде жүреді);
 *  қалғаны — топырақ жол: екі із ±0.72 м (көлік дөңгелегінің аралығы), жиегі құмға сіңеді. */
bool AKAPWorldBuilder::BuildRoadSurfaces()
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UMaterialInterface* RoadM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Road.M_KAP_Road"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UTexture* DA = LoadObject<UTexture>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadDirt_A.T_KAP_RoadDirt_A"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UTexture* DN = LoadObject<UTexture>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadDirt_N.T_KAP_RoadDirt_N"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UTexture* DM = LoadObject<UTexture>(nullptr, TEXT("/Game/KAP/Textures/T_KAP_RoadDirt_M.T_KAP_RoadDirt_M"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!RoadM || !DA || !DN || !DM || !D) return false;
	UMaterialInstanceDynamic* MatA = UMaterialInstanceDynamic::Create(RoadM, this);
	UMaterialInstanceDynamic* MatD = UMaterialInstanceDynamic::Create(RoadM, this);
	MatD->SetTextureParameterValue("TexA", DA); MatD->SetTextureParameterValue("TexN", DN); MatD->SetTextureParameterValue("TexM", DM);
	MatD->SetScalarParameterValue("SandDrift", 0.3f);
	MatA->SetScalarParameterValue("SandDrift", 0.3f);
	// көлденең қима: x (м, оң — сол жаққа), биіктік (см)
	static const float AX[9] = { -4.0f, -3.45f, -3.1f, -1.55f, 0.f, 1.55f, 3.1f, 3.45f, 4.0f };
	static const float AZ[9] = { 1.5f, 1.8f, 7.f, 7.5f, 8.f, 7.5f, 7.f, 1.8f, 1.5f };
	static const float DX[9] = { -3.0f, -1.9f, -1.0f, -0.72f, 0.f, 0.72f, 1.0f, 1.9f, 3.0f };
	static const float DZ[9] = { 1.5f, 3.5f, 2.5f, 0.8f, 3.f, 0.8f, 2.5f, 3.5f, 1.5f };
	TArray<int32> Deg; Deg.Init(0, D->Graph.Nodes.Num());
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges) { Deg[E.A]++; Deg[E.B]++; }
	int32 NA = 0, ND = 0, NV = 0;
	for (int32 e = 0; e < D->Graph.Edges.Num(); e++)
	{
		const KAPAI::FRoadEdge& E = D->Graph.Edges[e];
		const bool bAsph = E.BaseDanger <= 0.1001;
		const float* Xs = bAsph ? AX : DX; const float* Zs = bAsph ? AZ : DZ;
		const float Wm = bAsph ? 8.f : 6.f, Lm = bAsph ? 32.f : 24.f;
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos;
		const float Len = (float)FVector2D::Distance(A, B);
		if (Len < 1.f) continue;
		const FVector2D Dir = (B - A) / Len;
		const FVector2D Lft(Dir.Y, -Dir.X);
		const float Ext = Wm * 0.5f; // қиылыста жалғасу үшін ұшы түйіннен әрі шығады
		const int32 Rows = FMath::Max(2, FMath::CeilToInt32((Len + 2.f * Ext) / 2.f) + 1);
		const float Bias = (e % 5) * 0.35f + (bAsph ? 0.f : -0.2f); // қабаттасқан жолдар жыпылықтамасын
		const float VOff = (float)((e * 7919) % 97) / 97.f * Lm;
		const bool bEndA = Deg[E.A] <= 1, bEndB = Deg[E.B] <= 1;
		TArray<FVector> P; TArray<FVector> Nn; TArray<FVector2D> UV; TArray<FLinearColor> VC; TArray<FProcMeshTangent> Tg; TArray<int32> Tri;
		P.Reserve(Rows * 9); UV.Reserve(Rows * 9); VC.Reserve(Rows * 9);
		for (int32 r = 0; r < Rows; r++)
		{
			const float S = -Ext + (Len + 2.f * Ext) * r / (Rows - 1);
			// ұшында (түйіннен әрі) қабат жерге қарай жұқарады; тұйық ұшта — біртіндеп мөлдір
			float K = 1.f, Al = 1.f;
			if (S < 0.f) { K = 1.f - (-S / Ext) * 0.85f; if (bEndA) Al = FMath::Clamp((S + Ext) / 4.f, 0.f, 1.f); }
			if (S > Len) { K = 1.f - ((S - Len) / Ext) * 0.85f; if (bEndB) Al = FMath::Clamp((Len + Ext - S) / 4.f, 0.f, 1.f); }
			const FVector2D C = A + Dir * S;
			for (int32 i = 0; i < 9; i++)
			{
				const FVector2D Q = C + Lft * Xs[i];
				P.Add(FVector(Q.X * 100.0, Q.Y * 100.0, RoadZ(Q) + Zs[i] * K + Bias));
				UV.Add(FVector2D(0.5 + Xs[i] / Wm, (S + VOff) / Lm));
				VC.Add(FLinearColor(1, 1, 1, Al));
			}
		}
		Nn.SetNum(P.Num()); Tg.SetNum(P.Num());
		for (int32 r = 0; r < Rows; r++) for (int32 i = 0; i < 9; i++)
		{
			const FVector Along = P[FMath::Min(r + 1, Rows - 1) * 9 + i] - P[FMath::Max(r - 1, 0) * 9 + i];
			const FVector Across = P[r * 9 + FMath::Min(i + 1, 8)] - P[r * 9 + FMath::Max(i - 1, 0)];
			FVector N = FVector::CrossProduct(Across, Along).GetSafeNormal();
			if (N.Z < 0) N = -N;
			Nn[r * 9 + i] = N;
			Tg[r * 9 + i] = FProcMeshTangent(Across.GetSafeNormal(), false);
		}
		Tri.Reserve((Rows - 1) * 8 * 6);
		for (int32 r = 0; r + 1 < Rows; r++) for (int32 i = 0; i < 8; i++)
		{
			const int32 V0 = r * 9 + i, V1 = V0 + 1, V2 = V0 + 9, V3 = V2 + 1;
			Tri.Append({ V0, V2, V1, V1, V2, V3 }); // UE алдыңғы беті: жоғарыдан қарағанда сағат тілімен
		}
		UProceduralMeshComponent* Pm = NewObject<UProceduralMeshComponent>(this);
		Pm->SetupAttachment(RootComponent);
		Pm->bUseAsyncCooking = true;
		Pm->SetCastShadow(false);
		Pm->bAffectDistanceFieldLighting = false;
		Pm->SetCollisionProfileName(bAsph ? TEXT("BlockAll") : TEXT("NoCollision"));
		Pm->CreateMeshSection_LinearColor(0, P, Tri, Nn, UV, VC, Tg, bAsph);
		Pm->SetMaterial(0, bAsph ? (UMaterialInterface*)MatA : (UMaterialInterface*)MatD);
		Pm->RegisterComponent();
		Pm->ComponentTags.Add(bAsph ? FName("SurfAsphalt") : FName("SurfDirt"));
		RoadMeshes.Add(Pm);
		(bAsph ? NA : ND)++; NV += P.Num();
	}
	UE_LOG(LogTemp, Display, TEXT("KAPROAD surfaces: asphalt=%d dirt=%d verts=%d"), NA, ND, NV);
	return true;
}

void AKAPWorldBuilder::BuildRoads()
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!BuildRoadSurfaces())
	{
		// ескі нұсқа (жол активтері жоқ болса): тегіс топырақ жолақтар
		Roads = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Roads->SetStaticMesh(KAPVis::Mesh(EKAPShape::CubeLow));
		Roads->SetupAttachment(RootComponent);
		{ FKAPMat Rd = FKAPMat::Paint(FLinearColor(0.42f, 0.32f, 0.21f), 0.97f); Rd.VarAmt = 0.06f; Rd.VarScale = 0.003f; Rd.Grime = 0.35f; Rd.DustTop = 0.f; Roads->SetMaterial(0, KAPVis::MakeMID(Roads, Rd)); }
		Roads->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Roads->SetCastShadow(false);
		Roads->RegisterComponent();
		for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
		{
			const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos;
			const int32 Steps = FMath::Max(2, FMath::CeilToInt32(E.Len / 6.0));
			const FVector2D Dir = (B - A).GetSafeNormal();
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			for (int32 s = 0; s < Steps; s++)
			{
				const FVector2D P0 = FMath::Lerp(A, B, (double)s / Steps), P1 = FMath::Lerp(A, B, (double)(s + 1) / Steps);
				const FVector2D Pm = (P0 + P1) * 0.5;
				const float Z0 = Z(P0), Z1 = Z(P1);
				const float Len = (float)FVector2D::Distance(P0, P1) * 100.f;
				const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Z1 - Z0, Len));
				Roads->AddInstance(FTransform(FRotator(Pitch, Yaw, 0), FVector(Pm.X * 100, Pm.Y * 100, (Z0 + Z1) * 0.5f + 4), FVector(Len / 100.f * 1.08f, 5.f, 0.08f)));
			}
		}
	}
	// ---- электр желісі жол бойымен: ағаш/бетон бағаналар (~45 м), траверс, изолятор, салбыраған үш сым ----
	auto MakeH = [&](EKAPShape S, const FKAPMat& M, bool bCol, float Cull, bool bShadow)
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetStaticMesh(KAPVis::Mesh(S));
		H->SetupAttachment(RootComponent);
		H->SetMaterial(0, KAPVis::MakeMID(H, M));
		H->SetCollisionEnabled(bCol ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		H->SetCullDistances(0, (int32)Cull);
		H->SetCastShadow(bShadow);
		H->RegisterComponent();
		return H;
	};
	FKAPMat PoleM = FKAPMat::Paint(FLinearColor(0.22f, 0.16f, 0.1f), 0.9f); PoleM.Ribs = 0.3f; PoleM.RibPeriod = 6.f; PoleM.Grime = 0.9f; PoleM.DustTop = 0.4f;
	UHierarchicalInstancedStaticMeshComponent* Poles = MakeH(EKAPShape::CylLow, PoleM, true, 60000, true);
	UHierarchicalInstancedStaticMeshComponent* Arms = MakeH(EKAPShape::CubeLow, FKAPMat::RustySteel(FLinearColor(0.25f, 0.24f, 0.22f), 0.6f), false, 40000, true);
	UHierarchicalInstancedStaticMeshComponent* Insul = MakeH(EKAPShape::CylLow, FKAPMat::Paint(FLinearColor(0.55f, 0.6f, 0.62f), 0.2f), false, 15000, false);
	UHierarchicalInstancedStaticMeshComponent* Wires = MakeH(EKAPShape::CylLow, FKAPMat::Metal(FLinearColor(0.08f, 0.08f, 0.08f), 0.5f), false, 30000, false);
	FRandomStream PR(77);
	// бағана қоюға болмайтын жерлер: елді мекен, ұңғыма, көлік тұрағы, кіріспедегі Буханка жолы
	TArray<FVector2D> NoPole; TArray<float> NoPoleR;
	D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L) { NoPole.Add(FVector2D(L.X, L.Y)); NoPoleR.Add(L.R + 6); });
	D->Wells->ForeachRow<FKAPWellRow>(TEXT("KAP"), [&](const FName&, const FKAPWellRow& W) { NoPole.Add(FVector2D(W.X, W.Y)); NoPoleR.Add(16); });
	D->Vehicles->ForeachRow<FKAPVehicleRow>(TEXT("KAP"), [&](const FName&, const FKAPVehicleRow& V) { NoPole.Add(FVector2D(V.X, V.Y)); NoPoleR.Add(12); });
	for (int32 i = 0; i <= 10; i++) { NoPole.Add(FMath::Lerp(FVector2D(-830, 762), FVector2D(-995, 818), i / 10.0)); NoPoleR.Add(14); }
	TArray<FBox2D> NoPoleBox;
	for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It) for (const FBox2D& Bx : It->IndoorBoxes) NoPoleBox.Add(Bx.ExpandBy(600.0));
	auto WetP = [](FVector2D P) { float Hw; return KAPTerrain::ChannelDist(P.X, P.Y, Hw) < Hw * 1.6f + 3.f || KAPTerrain::LakeDist(P.X, P.Y) < KAPTerrain::LakeR + 12.f || KAPTerrain::OasisDist(P.X, P.Y) < KAPTerrain::OasisR + 6.f; };
	auto PoleOk = [&](FVector2D P) { if (WetP(P)) return false; for (int32 i = 0; i < NoPole.Num(); i++) if (FVector2D::Distance(P, NoPole[i]) < NoPoleR[i]) return false; for (const FBox2D& Bx : NoPoleBox) if (Bx.IsInside(P * 100.0)) return false; return true; };
	for (int32 e = 0; e < D->Graph.Edges.Num(); e++)
	{
		if ((e % 3) == 2) continue; // барлық жолда емес
		const KAPAI::FRoadEdge& E = D->Graph.Edges[e];
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos;
		const FVector2D Dir = (B - A).GetSafeNormal(), Nrm(-Dir.Y, Dir.X);
		const int32 N = FMath::Max(1, FMath::FloorToInt32(E.Len / 45.0));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		TArray<FVector> Tops[3];
		for (int32 i = 0; i <= N; i++)
		{
			const FVector2D P = FMath::Lerp(A, B, (double)i / N) + Nrm * 9.0;
			if (!PoleOk(P)) { for (int32 w = 0; w < 3; w++) Tops[w].Add(FVector(0, 0, -1e7)); continue; } // үзіліс
			const float G = Z(P);
			const float Tilt = PR.FRandRange(-3.f, 3.f) + (PR.FRand() < 0.12f ? PR.FRandRange(-9.f, 9.f) : 0.f); // кейбірі қисайған
			const FRotator Rt(Tilt, Yaw + 90.f, 0);
			const FVector Base(P.X * 100, P.Y * 100, G - 60.f);
			const FVector Up = Rt.RotateVector(FVector::UpVector);
			Poles->AddInstance(FTransform(Rt, Base + Up * 500.f, FVector(0.26f, 0.26f, 10.f)));
			const FVector Top = Base + Up * 960.f;
			Arms->AddInstance(FTransform(FRotator(Tilt, Yaw + 90.f, 0), Top, FVector(0.1f, 2.2f, 0.1f)));
			const FVector Side = FRotator(0, Yaw + 90.f, 0).Vector();
			for (int32 w = 0; w < 3; w++)
			{
				const FVector Ins = Top + Side * ((w - 1) * 95.f) + FVector(0, 0, 18.f);
				Insul->AddInstance(FTransform(FRotator::ZeroRotator, Ins, FVector(0.08f, 0.08f, 0.22f)));
				Tops[w].Add(Ins + FVector(0, 0, 10.f));
			}
		}
		// сымдар: әр аралық 4 сегмент, ортасы ~1.4 м салбырайды
		for (int32 w = 0; w < 3; w++)
			for (int32 i = 0; i + 1 < Tops[w].Num(); i++)
			{
				const FVector P0 = Tops[w][i], P1 = Tops[w][i + 1];
				if (P0.Z < -1e6 || P1.Z < -1e6) continue;
				FVector Prev = P0;
				for (int32 s = 1; s <= 4; s++)
				{
					const float T = s / 4.f;
					const FVector Pt = FMath::Lerp(P0, P1, T) - FVector(0, 0, 140.f * 4.f * T * (1.f - T));
					const FVector D2 = Pt - Prev;
					Wires->AddInstance(FTransform(FRotationMatrix::MakeFromZ(D2.GetSafeNormal()).Rotator(), (Prev + Pt) * 0.5f, FVector(0.018f, 0.018f, D2.Size() / 100.f)));
					Prev = Pt;
				}
			}
	}
	// ---- тасталған заттар: тат басқан бөшкелер, дөңгелектер, бетон блоктар (жол маңында) ----
	UHierarchicalInstancedStaticMeshComponent* Junk = MakeH(EKAPShape::Cylinder, FKAPMat::RustySteel(FLinearColor(0.2f, 0.22f, 0.3f), 0.8f), true, 25000, true);
	UHierarchicalInstancedStaticMeshComponent* Tires = MakeH(EKAPShape::CylLow, FKAPMat::Paint(FLinearColor(0.03f, 0.03f, 0.03f), 0.95f), false, 15000, true);
	UHierarchicalInstancedStaticMeshComponent* Blocks = MakeH(EKAPShape::Cube, FKAPMat::Concrete(), true, 30000, true);
	for (int32 i = 0; i < 160; i++)
	{
		const KAPAI::FRoadEdge& E = D->Graph.Edges[PR.RandRange(0, D->Graph.Edges.Num() - 1)];
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos;
		const FVector2D Dir = (B - A).GetSafeNormal(), Nrm(-Dir.Y, Dir.X);
		const FVector2D P = FMath::Lerp(A, B, (double)PR.FRand()) + Nrm * PR.FRandRange(6.0f, 22.0f) * (PR.FRand() < 0.5f ? -1.0 : 1.0);
		if (!PoleOk(P)) continue;
		const FVector W(P.X * 100, P.Y * 100, Z(P));
		const float T = PR.FRand();
		if (T < 0.5f)
		{
			const int32 C = PR.RandRange(1, 4);
			for (int32 k = 0; k < C; k++)
			{
				const bool bFall = PR.FRand() < 0.35f;
				Junk->AddInstance(FTransform(bFall ? FRotator(90, PR.FRandRange(0.f, 180.f), 0) : FRotator(PR.FRandRange(-6.f, 6.f), 0, PR.FRandRange(-6.f, 6.f)), W + FVector(PR.FRandRange(-90.f, 90.f), PR.FRandRange(-90.f, 90.f), bFall ? 26.f : 40.f), FVector(0.58f, 0.58f, 0.88f)));
			}
		}
		else if (T < 0.8f) { for (int32 k = 0; k < PR.RandRange(1, 3); k++) Tires->AddInstance(FTransform(FRotator(PR.FRandRange(-10.f, 10.f), PR.FRandRange(0.f, 360.f), 0), W + FVector(0, 0, 12 + k * 22), FVector(0.8f, 0.8f, 0.24f))); }
		else Blocks->AddInstance(FTransform(FRotator(PR.FRandRange(-8.f, 8.f), PR.FRandRange(0.f, 360.f), PR.FRandRange(-5.f, 5.f)), W + FVector(0, 0, 30), FVector(1.8f, 0.7f, 0.8f)));
	}
}

void AKAPWorldBuilder::BuildVegetation()
{
	// шөп пен егін — ландшафт материалының шөп түрлері (GPU), сексеуіл, ағаштар, су, таулар — AKAPNature.
	// Мұнда тек далаға шашылған тастар қалды (судан, егістен, жолдан аулақ).
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	FRandomStream R(42);
	auto MakeH = [&](EKAPShape S, FKAPMat M, bool bCol, float Cull)
	{
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetStaticMesh(KAPVis::Mesh(S));
		H->SetupAttachment(RootComponent);
		H->SetMaterial(0, KAPVis::MakeMID(H, M));
		H->SetCollisionEnabled(bCol ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		H->SetCullDistances(0, (int32)Cull);
		H->RegisterComponent();
		return H;
	};
	FKAPMat RockM = FKAPMat::Concrete(FLinearColor(0.42f, 0.34f, 0.26f)); RockM.Panel = 0.f; RockM.DustTop = 1.f;
	Rocks = MakeH(EKAPShape::Rock, RockM, true, 80000);
	TArray<FVector2D> Avoid; TArray<float> AvoidR;
	D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L) { Avoid.Add(FVector2D(L.X, L.Y)); AvoidR.Add(L.R + 8); });
	D->Wells->ForeachRow<FKAPWellRow>(TEXT("KAP"), [&](const FName&, const FKAPWellRow& W) { Avoid.Add(FVector2D(W.X, W.Y)); AvoidR.Add(14); });
	auto Bad = [&](FVector2D P)
	{
		for (int32 i = 0; i < Avoid.Num(); i++) if (FVector2D::Distance(P, Avoid[i]) < AvoidR[i]) return true;
		float Hw;
		if (KAPTerrain::ChannelDist(P.X, P.Y, Hw) < Hw * 1.6f + 3.f || KAPTerrain::LakeDist(P.X, P.Y) < KAPTerrain::LakeR + 10.f || KAPTerrain::OasisDist(P.X, P.Y) < KAPTerrain::OasisR + 5.f) return true;
		return KAPTerrain::FieldMask(P.X, P.Y) > 0.f || KAPTerrain::WetMask(P.X, P.Y) > 0.3f;
	};
	auto NearRoad = [&](FVector2D P)
	{
		double Best = 1e9;
		for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
		{
			const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos, V = B - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(P - A, V) / V.SizeSquared(), 0.0, 1.0);
			Best = FMath::Min(Best, FVector2D::Distance(P, A + V * T));
		}
		return Best;
	};
	// табиғат актері: егістік картасы (FieldMask) осында құрылады — тастардан бұрын
	if (Nature) Nature->Build(this);
	// жануарлар (табындар, түлкі, түнгі қасқырлар) және ауыл адамдарының күн тәртібі
	GetWorld()->SpawnActor<AKAPFauna>();
	GetWorld()->SpawnActor<AKAPLife>();
	int32 Placed = 0, Tries = 0;
	while (Placed < 650 && Tries++ < 8000)
	{
		const FVector2D P(R.FRandRange(-1140, 1140), R.FRandRange(-1140, 1140));
		if (Bad(P) || NearRoad(P) < 6) continue;
		const bool bBig = R.FRand() < 0.3f;
		const float S = bBig ? R.FRandRange(1.6f, 3.8f) : R.FRandRange(0.3f, 1.0f);
		Rocks->AddInstance(FTransform(FRotator(R.FRandRange(-20, 20), R.FRandRange(0, 360), R.FRandRange(-20, 20)), FVector(P.X * 100, P.Y * 100, Z(P) - S * 25), FVector(S * R.FRandRange(0.9f, 1.5f), S, S * 0.75f)));
		Placed++;
	}
}

void AKAPWorldBuilder::BuildVehicles()
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	UClass* Cls = LoadClass<AKAPSimulatorPawn>(nullptr, TEXT("/Game/VehicleTemplate/Blueprints/OffroadCar/BP_OffroadCar_Pawn.BP_OffroadCar_Pawn_C"));
	if (!Cls) { UE_LOG(LogTemp, Error, TEXT("KAP: BP_OffroadCar_Pawn табылмады")); return; }
	D->Vehicles->ForeachRow<FKAPVehicleRow>(TEXT("KAP"), [&](const FName& Key, const FKAPVehicleRow& Row)
	{
		const FVector2D P(Row.X, Row.Y);
		FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AKAPSimulatorPawn* V = GetWorld()->SpawnActor<AKAPSimulatorPawn>(Cls, FVector(P.X * 100, P.Y * 100, Z(P) + 120), FRotator(0, Row.Yaw, 0), SP);
		if (!V) return;
		// корпус түсі (материалда «Color»/«BaseColor»/«Tint» параметрі болса)
		if (USkeletalMeshComponent* M = V->GetMesh())
		{
			for (int32 i = 0; i < M->GetNumMaterials(); i++)
			{
				UMaterialInstanceDynamic* MID = M->CreateAndSetMaterialInstanceDynamic(i);
				if (!MID) continue;
				for (const TCHAR* PName : { TEXT("Color"), TEXT("BaseColor"), TEXT("Tint"), TEXT("Body Color"), TEXT("Paint Color") }) MID->SetVectorParameterValue(PName, Row.Color);
			}
		}
		AKAPVehicleHandle* H = GetWorld()->SpawnActor<AKAPVehicleHandle>(V->GetActorLocation(), FRotator::ZeroRotator);
		H->Id = Key; H->Def = Row; H->Pawn = V; H->State = Row.State; H->Fuel = Row.FuelCap * (Key == "bukhanka" ? 0.6f : 0.4f);
		Vehicles.Add(H);
	});
}

/** Жол (ені 5 м) ғимараттың табанымен қиылыса ма: жол бойынша 1 м сайын нүктелерді ғимараттың жергілікті осьтеріне ауыстырып тексереміз */
bool AKAPWorldBuilder::OnRoad(const AKAPStructure* S, float Margin) const
{
	const FVector2D H = S->FootprintHalf();
	if (H.IsZero()) return false;
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	const FTransform Xf = S->GetActorTransform();
	const float Rad = H.Size() + 300.f;
	const FVector2D C(S->GetActorLocation());
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		const FVector2D A = D->Graph.Nodes[E.A].Pos * 100.0, B = D->Graph.Nodes[E.B].Pos * 100.0;
		// алдымен жылдам: сегментке дейінгі қашықтық
		const FVector2D V = B - A;
		const double Tt = FMath::Clamp(FVector2D::DotProduct(C - A, V) / FMath::Max(1.0, V.SizeSquared()), 0.0, 1.0);
		if (FVector2D::Distance(C, A + V * Tt) > Rad) continue;
		const int32 N = FMath::Max(2, FMath::CeilToInt32(V.Size() / 100.0));
		for (int32 i = 0; i <= N; i++)
		{
			const FVector2D P = FMath::Lerp(A, B, (double)i / N);
			const FVector L = Xf.InverseTransformPositionNoScale(FVector(P.X, P.Y, Xf.GetLocation().Z));
			if (FMath::Abs(L.X) < H.X + 250.f + Margin && FMath::Abs(L.Y) < H.Y + 250.f + Margin) return true;
		}
	}
	return false;
}

void AKAPWorldBuilder::ClearRoadOverlaps()
{
	int32 Removed = 0;
	TArray<AKAPStructure*> Kill;
	for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It)
	{
		AKAPStructure* S = *It;
		if (!OnRoad(S, 0.f)) continue;
		UE_LOG(LogTemp, Display, TEXT("KAPROAD on-road: type=%d at (%.0f, %.0f) m size=%s"), (int32)S->Type, S->GetActorLocation().X / 100.f, S->GetActorLocation().Y / 100.f, *S->BuildSize.ToString());
		Kill.Add(S);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPROAD total on-road structures: %d"), Kill.Num());
}
float AKAPWorldBuilder::DistToRoadCm(FVector2D Pcm) const
{
	const UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	double Best = 1e12;
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		const FVector2D A = D->Graph.Nodes[E.A].Pos * 100.0, B = D->Graph.Nodes[E.B].Pos * 100.0, V = B - A;
		const double Tt = FMath::Clamp(FVector2D::DotProduct(Pcm - A, V) / FMath::Max(1.0, V.SizeSquared()), 0.0, 1.0);
		Best = FMath::Min(Best, FVector2D::Distance(Pcm, A + V * Tt));
	}
	return (float)Best;
}