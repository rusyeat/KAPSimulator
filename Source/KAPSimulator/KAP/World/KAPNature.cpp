#include "KAPNature.h"
#include "KAPTerrain.h"
#include "KAPWorldBuilder.h"
#include "KAPVisuals.h"
#include "KAPStructure.h"
#include "KAPFXManager.h"
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "WeatherManager.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAP/Player/KAPCharacter.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

namespace
{
	UStaticMesh* LoadSM(const FString& Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Meshes/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	/** Арна кесілген жер (су мен жағаның тік беті): көпір керек */
	bool Carved(float X, float Y)
	{
		float Hw; const float Dc = KAPTerrain::ChannelDist(X, Y, Hw);
		if (Dc < Hw * 1.45f + 1.5f) return true;
		if (KAPTerrain::LakeDist(X, Y) < KAPTerrain::LakeR + 8.f) return true;
		if (KAPTerrain::OasisDist(X, Y) < KAPTerrain::OasisR + 4.f) return true;
		return false;
	}
	/** Су немесе жаға маңы (ағаш/бұта/бағана қойылмайды) */
	bool NearWater(float X, float Y, float Margin)
	{
		float Hw; const float Dc = KAPTerrain::ChannelDist(X, Y, Hw);
		if (Dc < Hw * 1.5f + Margin) return true;
		if (KAPTerrain::LakeDist(X, Y) < KAPTerrain::LakeR + 8.f + Margin) return true;
		if (KAPTerrain::OasisDist(X, Y) < KAPTerrain::OasisR + 3.f + Margin) return true;
		return false;
	}
}

// ---------------------------------------------------------------- отын үйіндісі
AKAPWoodPile::AKAPWoodPile()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}
void AKAPWoodPile::BeginPlay()
{
	Super::BeginPlay();
	KAPReg::Add(KAPReg::Interactables(), (AActor*)this);
	UStaticMesh* Log = LoadSM(TEXT("SM_KAP_SaxaulLog"));
	FKAPMat Bark = FKAPMat::Paint(FLinearColor(0.36f, 0.31f, 0.25f), 0.93f); Bark.Grime = 0.7f; Bark.VarAmt = 0.18f; Bark.DustTop = 0.5f;
	if (Log) for (int32 i = 0; i < FMath::Clamp(Amount, 2, 5); i++)
		KAPVis::AddMesh(this, RootComponent, Log, FVector(FMath::FRandRange(-25.f, 25.f), (i - 1.5f) * 11.f, 5.f + (i % 2) * 8.f), FVector(1.f), Bark, FRotator(FMath::FRandRange(-8.f, 8.f), FMath::FRandRange(-25.f, 25.f), FMath::FRandRange(0.f, 360.f)), false);
}
void AKAPWoodPile::EndPlay(const EEndPlayReason::Type R)
{
	KAPReg::Remove(KAPReg::Interactables(), (AActor*)this);
	Super::EndPlay(R);
}
FString AKAPWoodPile::GetInteractText(AKAPCharacter*) const { return FString::Printf(TEXT("Сексеуіл отынын жинау (×%d)"), Amount); }
void AKAPWoodPile::Interact(AKAPCharacter*)
{
	if (AKAPGameMode* GM = AKAPGameMode::Get(this))
	{
		const FString Got = GM->GiveItems({ { FName("firewood"), Amount } });
		GM->Toast(TEXT("Алынды: ") + Got, "ok");
	}
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Impact(GetActorLocation(), 2, 0.55f);
	Destroy();
}

// ---------------------------------------------------------------- табиғат
AKAPNature::AKAPNature()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

AKAPNature* AKAPNature::Get(const UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AKAPNature> It(const_cast<UWorld*>(W)); It; ++It) return *It;
	return nullptr;
}

float AKAPNature::RoadDist(FVector2D P) const
{
	const UKAPDataSubsystem* D = Data.Get();
	if (!D) return 1e9f;
	double Best = 1e9;
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos, V = B - A;
		const double T = FMath::Clamp(FVector2D::DotProduct(P - A, V) / FMath::Max(1e-6, V.SizeSquared()), 0.0, 1.0);
		Best = FMath::Min(Best, FVector2D::Distance(P, A + V * T));
	}
	return (float)Best;
}

bool AKAPNature::Clear(FVector2D P, float RoadMargin, float SiteMargin) const
{
	const UKAPDataSubsystem* D = Data.Get();
	if (!D) return true;
	if (RoadMargin > 0 && RoadDist(P) < RoadMargin) return false;
	bool bOk = true;
	D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L) { if (FVector2D::Distance(P, FVector2D(L.X, L.Y)) < L.R + SiteMargin) bOk = false; });
	if (!bOk) return false;
	D->Wells->ForeachRow<FKAPWellRow>(TEXT("KAP"), [&](const FName&, const FKAPWellRow& W) { if (FVector2D::Distance(P, FVector2D(W.X, W.Y)) < 9.f + SiteMargin) bOk = false; });
	if (!bOk) return false;
	D->Vehicles->ForeachRow<FKAPVehicleRow>(TEXT("KAP"), [&](const FName&, const FKAPVehicleRow& V) { if (FVector2D::Distance(P, FVector2D(V.X, V.Y)) < 6.f + SiteMargin) bOk = false; });
	if (!bOk) return false;
	for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It)
		for (const FBox2D& Bx : It->IndoorBoxes) if (Bx.ExpandBy(400.0).IsInside(P * 100.0)) return false;
	return true;
}

// ---------------------------------------------------------------- көпірлер
void AKAPNature::PlanBridges(UKAPDataSubsystem* D, TFunctionRef<float(FVector2D)> GroundZcm)
{
	Data = D;
	Bridges.Reset();
	if (!D) return;
	for (const KAPAI::FRoadEdge& E : D->Graph.Edges)
	{
		const FVector2D A = D->Graph.Nodes[E.A].Pos, B = D->Graph.Nodes[E.B].Pos;
		const float L = (float)FVector2D::Distance(A, B);
		if (L < 1.f) continue;
		const FVector2D Dir = (B - A) / L;
		TArray<FVector2f> Spans;
		float S0 = -1.f;
		for (float S = 0.f; S <= L + 0.25f; S += 0.5f)
		{
			const FVector2D P = A + Dir * FMath::Min(S, L);
			const bool bC = Carved((float)P.X, (float)P.Y);
			if (bC && S0 < 0.f) S0 = S;
			if ((!bC || S + 0.5f > L + 0.25f) && S0 >= 0.f) { Spans.Add(FVector2f(S0, S)); S0 = -1.f; }
		}
		// ұштарын жағаға шығару, жақын аралықтарды біріктіру
		TArray<FVector2f> Merged;
		for (FVector2f Sp : Spans)
		{
			Sp.X = FMath::Max(0.f, Sp.X - 4.f); Sp.Y = FMath::Min(L, Sp.Y + 4.f);
			if (Merged.Num() && Sp.X - Merged.Last().Y < 8.f) Merged.Last().Y = Sp.Y; else Merged.Add(Sp);
		}
		const bool bAsph = E.BaseDanger <= 0.1001;
		for (const FVector2f& Sp : Merged)
		{
			FBridge Br;
			Br.A = A + Dir * Sp.X; Br.B = A + Dir * Sp.Y;
			Br.ZA = GroundZcm(Br.A) + 8.f; Br.ZB = GroundZcm(Br.B) + 8.f;
			Br.W = bAsph ? 9.4f : 6.8f; Br.bAsph = bAsph;
			Bridges.Add(Br);
			UE_LOG(LogTemp, Display, TEXT("KAPNATURE bridge %s–%s: (%.0f,%.0f)→(%.0f,%.0f) L=%.0f m"), *D->Graph.Nodes[E.A].Id.ToString(), *D->Graph.Nodes[E.B].Id.ToString(), Br.A.X, Br.A.Y, Br.B.X, Br.B.Y, Sp.Y - Sp.X);
		}
	}
}

float AKAPNature::DeckZ(FVector2D P, float G) const
{
	for (const FBridge& Br : Bridges)
	{
		const FVector2D V = Br.B - Br.A;
		const double L2 = V.SizeSquared();
		if (L2 < 1.0) continue;
		const double T = FVector2D::DotProduct(P - Br.A, V) / L2;
		if (T < -0.02 || T > 1.02) continue;
		const double Lat = FMath::Abs(FVector2D::CrossProduct(V / FMath::Sqrt(L2), P - Br.A));
		if (Lat > Br.W * 0.5 + 1.0) continue;
		return FMath::Max(G, FMath::Lerp(Br.ZA, Br.ZB, (float)FMath::Clamp(T, 0.0, 1.0)));
	}
	return G;
}

void AKAPNature::BuildBridges()
{
	FKAPMat Deck = FKAPMat::Concrete(FLinearColor(0.5f, 0.48f, 0.44f)); Deck.Grime = 0.9f;
	FKAPMat Rail = FKAPMat::RustySteel(FLinearColor(0.32f, 0.33f, 0.31f), 0.65f);
	FKAPMat Pier = FKAPMat::Concrete(FLinearColor(0.42f, 0.4f, 0.37f)); Pier.Grime = 1.f;
	for (const FBridge& Br : Bridges)
	{
		const FVector A(Br.A.X * 100, Br.A.Y * 100, Br.ZA), B(Br.B.X * 100, Br.B.Y * 100, Br.ZB);
		const FVector D = B - A;
		const float L = D.Size();
		if (L < 100.f) continue;
		const FRotator Rot = D.Rotation();
		const FVector Fw = D / L, Rt = FRotator(0, Rot.Yaw + 90.f, 0).Vector();
		const float Th = 45.f;
		// төсем: үсті жол бетінен 3 см төмен (асфальт таспасы үстінде жатады; топырақ жолда — төсемнің өзі)
		KAPVis::Add(this, RootComponent, EKAPShape::Cube, (A + B) * 0.5f - FVector(0, 0, Th * 0.5f + (Br.bAsph ? 3.f : 0.f)), FVector(L + 60.f, Br.W * 100.f, Th), Deck, Rot, true)->ComponentTags.Add(FName("SurfConcrete"));
		// жиек (бордюр) және темір қоршау
		for (int32 s = -1; s <= 1; s += 2)
		{
			const FVector Side = Rt * (s * (Br.W * 50.f - 18.f));
			KAPVis::Add(this, RootComponent, EKAPShape::Cube, (A + B) * 0.5f + Side + FVector(0, 0, 8.f), FVector(L + 40.f, 30.f, 22.f), Deck, Rot, true);
			const int32 N = FMath::Max(2, FMath::RoundToInt32(L / 250.f));
			for (int32 i = 0; i <= N; i++)
			{
				const FVector P = A + D * (i / (float)N) + Side + FVector(0, 0, 18.f + 50.f);
				KAPVis::Add(this, RootComponent, EKAPShape::CylLow, P, FVector(9.f, 9.f, 100.f), Rail, FRotator::ZeroRotator, true);
			}
			for (const float Hh : { 115.f, 70.f })
				KAPVis::Add(this, RootComponent, EKAPShape::CubeLow, (A + B) * 0.5f + Side + FVector(0, 0, 18.f + Hh), FVector(L + 10.f, 7.f, 9.f), Rail, Rot, true);
		}
		// тіректер: әр 9 м, арна түбіне дейін
		const int32 Np = FMath::Max(1, FMath::FloorToInt32(L / 900.f));
		for (int32 i = 1; i <= Np; i++)
		{
			const float T = i / (float)(Np + 1);
			const FVector Top = A + D * T - FVector(0, 0, Th);
			const float G = KAPVis::GroundZ(GetWorld(), Top.X, Top.Y, Top.Z - 300.f) - 60.f;
			const float H = FMath::Max(60.f, Top.Z - G);
			for (int32 s = -1; s <= 1; s += 2)
				KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Top + Rt * (s * Br.W * 28.f) - FVector(0, 0, H * 0.5f), FVector(70.f, 70.f, H), Pier, FRotator::ZeroRotator, true);
			KAPVis::Add(this, RootComponent, EKAPShape::Cube, Top - FVector(0, 0, 25.f), FVector(80.f, Br.W * 100.f - 40.f, 50.f), Pier, FRotator(0, Rot.Yaw, 0), true);
		}
		// жағадағы тіреу блоктары
		for (const FVector& E : { A, B })
			KAPVis::Add(this, RootComponent, EKAPShape::Cube, E - FVector(0, 0, 120.f) - Fw * (E == A ? -40.f : 40.f), FVector(140.f, Br.W * 100.f + 60.f, 200.f), Pier, FRotator(0, Rot.Yaw, 0), true);
	}
}

// ---------------------------------------------------------------- су
void AKAPNature::BuildWater()
{
	UMaterialInterface* WM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Water.M_KAP_Water"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	auto NewPM = [&]()
	{
		UProceduralMeshComponent* Pm = NewObject<UProceduralMeshComponent>(this);
		Pm->SetupAttachment(RootComponent);
		Pm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pm->SetCastShadow(false);
		Pm->bAffectDistanceFieldLighting = false;
		Pm->RegisterComponent();
		if (WM) Pm->SetMaterial(0, WM);
		Pm->ComponentTags.Add(FName("SurfWater"));
		Water.Add(Pm);
		return Pm;
	};
	auto Finish = [](UProceduralMeshComponent* Pm, int32 Sec, TArray<FVector>& P, TArray<int32>& Tri)
	{
		// үшбұрыштар жоғары қарасын (жол таспасымен бірдей ереже: Cross(Pc−Pa, Pb−Pa).Z > 0)
		for (int32 t = 0; t + 2 < Tri.Num(); t += 3)
			if (FVector::CrossProduct(P[Tri[t + 2]] - P[Tri[t]], P[Tri[t + 1]] - P[Tri[t]]).Z < 0) Swap(Tri[t + 1], Tri[t + 2]);
		TArray<FVector> N; N.Init(FVector::UpVector, P.Num());
		TArray<FVector2D> UV; UV.SetNum(P.Num());
		for (int32 i = 0; i < P.Num(); i++) UV[i] = FVector2D(P[i].X, P[i].Y) / 1000.0;
		TArray<FLinearColor> VC; VC.Init(FLinearColor::White, P.Num());
		TArray<FProcMeshTangent> Tg; Tg.Init(FProcMeshTangent(1, 0, 0), P.Num());
		Pm->CreateMeshSection_LinearColor(Sec, P, Tri, N, UV, VC, Tg, false);
	};
	// таспа: орта сызығы (м) + жарты ені (м) + су деңгейі (м)
	auto Ribbon = [&](UProceduralMeshComponent* Pm, int32 Sec, const TArray<FVector>& C, const TArray<float>& Hw)
	{
		TArray<FVector> P; TArray<int32> Tri;
		const int32 Nr = C.Num(), Nc = 7;
		for (int32 r = 0; r < Nr; r++)
		{
			FVector2D Dir(C[FMath::Min(r + 1, Nr - 1)] - C[FMath::Max(r - 1, 0)]); Dir.Normalize();
			const FVector2D Lft(Dir.Y, -Dir.X);
			for (int32 i = 0; i < Nc; i++)
			{
				const float O = -1.f + 2.f * i / (Nc - 1);
				const FVector2D Q = FVector2D(C[r]) + Lft * (O * Hw[r]);
				P.Add(FVector(Q.X * 100.0, Q.Y * 100.0, C[r].Z * 100.0));
			}
		}
		for (int32 r = 0; r + 1 < Nr; r++) for (int32 i = 0; i + 1 < Nc; i++)
		{
			const int32 V0 = r * Nc + i, V1 = V0 + 1, V2 = V0 + Nc, V3 = V2 + 1;
			Tri.Append({ V0, V2, V1, V1, V2, V3 });
		}
		Finish(Pm, Sec, P, Tri);
	};
	auto Disc = [&](UProceduralMeshComponent* Pm, int32 Sec, FVector2D C, float R, float Zm, TFunctionRef<float(float)> Wob)
	{
		TArray<FVector> P; TArray<int32> Tri;
		const int32 Na = 72, Nk = 6;
		P.Add(FVector(C.X * 100, C.Y * 100, Zm * 100));
		for (int32 k = 1; k <= Nk; k++) for (int32 a = 0; a < Na; a++)
		{
			const float An = a * UE_TWO_PI / Na, Rr = (R + Wob(An)) * k / Nk;
			P.Add(FVector((C.X + FMath::Cos(An) * Rr) * 100, (C.Y + FMath::Sin(An) * Rr) * 100, Zm * 100));
		}
		for (int32 a = 0; a < Na; a++) Tri.Append({ 0, 1 + a, 1 + (a + 1) % Na });
		for (int32 k = 1; k < Nk; k++) for (int32 a = 0; a < Na; a++)
		{
			const int32 i0 = 1 + (k - 1) * Na + a, i1 = 1 + (k - 1) * Na + (a + 1) % Na, o0 = i0 + Na, o1 = i1 + Na;
			Tri.Append({ i0, o0, i1, i1, o0, o1 });
		}
		Finish(Pm, Sec, P, Tri);
	};
	using namespace KAPTerrain;
	UProceduralMeshComponent* Pm = NewPM();
	int32 Sec = 0;
	// басты арна: шатқалдан (таулар ішінен) атырапқа дейін
	{
		TArray<FVector> C; TArray<float> Hw;
		for (float X = MapHalf + 2600.f; X > MapHalf; X -= 6.f) { C.Add(FVector(X, GorgeY(X), GorgeWaterZ(X))); Hw.Add(22.f); }
		for (float X = MapHalf; X >= DeltaX0 - 22.f; X -= 3.f)
		{
			C.Add(FVector(X, RiverY(X), WaterZ(X)));
			Hw.Add(17.f * FMath::Clamp((X - (DeltaX0 - 24.f)) / 22.f, 0.05f, 1.f));
		}
		Ribbon(Pm, Sec++, C, Hw);
	}
	// атырап тармақтары: көлге құяды (бір-бірімен қабаттасқан жерде z-fighting болмасын — сәл төмен)
	for (int32 K = 0; K < 3; K++)
	{
		TArray<FVector> C; TArray<float> Hw;
		for (float X = DeltaX0 + 8.f; X > LakeX - LakeR; X -= 3.f)
		{
			const float Y = BranchY(K, X);
			if (LakeDist(X, Y) < LakeR - 6.f) break;
			C.Add(FVector(X, Y, WaterZ(X) - 0.02f * (K + 1)));
			Hw.Add((K == 1 ? 9.f : BranchHalfW) + 1.f);
		}
		if (C.Num() > 2) Ribbon(Pm, Sec++, C, Hw);
	}
	Disc(Pm, Sec++, FVector2D(LakeX, LakeY), LakeR + 5.f, LakeZ() - 0.01f, [](float A) { return KAPTerrain::LakeWobble(A); });
	const float Ho = 2.5f + 6.f * (Fbm(OasisX / 700.f + 3.1f, OasisY / 700.f - 1.7f, 4) - 0.5f);
	Disc(Pm, Sec++, FVector2D(OasisX, OasisY), OasisR + 3.f, Ho - 1.2f, [](float A) { return KAPTerrain::OasisWobble(A); });
	if (WM) for (int32 s = 0; s < Sec; s++) Pm->SetMaterial(s, WM); // әр бөлікке (атырап, көл, оазис) — бұрын тек біріншісінде еді
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE water sections=%d"), Sec);
}

// ---------------------------------------------------------------- таулар, шекара
void AKAPNature::BuildMountains()
{
	if (UStaticMesh* M = LoadSM(TEXT("SM_KAP_Mountains")))
	{
		Mountains = NewObject<UStaticMeshComponent>(this);
		Mountains->SetStaticMesh(M);
		Mountains->SetupAttachment(RootComponent);
		Mountains->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mountains->SetCastShadow(true);
		Mountains->bCastFarShadow = true;
		Mountains->SetCachedMaxDrawDistance(0.f);
		Mountains->RegisterComponent();
	}
	// көрінбейтін шекара: бөктерге шығып кетпеу үшін (таудың өзіне жету мүмкін емес)
	const float B = 115000.f;
	const FVector Ctr[4] = { FVector(B + 100, 0, 0), FVector(-B - 100, 0, 0), FVector(0, B + 100, 0), FVector(0, -B - 100, 0) };
	const FVector Ext[4] = { FVector(100, B + 300, 300000), FVector(100, B + 300, 300000), FVector(B + 300, 100, 300000), FVector(B + 300, 100, 300000) };
	for (int32 i = 0; i < 4; i++)
	{
		UBoxComponent* Bx = NewObject<UBoxComponent>(this);
		Bx->SetupAttachment(RootComponent);
		Bx->SetBoxExtent(Ext[i]);
		Bx->SetRelativeLocation(Ctr[i]);
		Bx->SetCollisionProfileName(TEXT("BlockAll"));
		Bx->SetHiddenInGame(true);
		Bx->SetCanEverAffectNavigation(false);
		Bx->RegisterComponent();
		Walls.Add(Bx);
	}
}

UHierarchicalInstancedStaticMeshComponent* AKAPNature::MakeH(UStaticMesh* M, UMaterialInterface* Mat, bool bCol, float Cull, bool bShadow)
{
	UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
	H->SetStaticMesh(M);
	H->SetupAttachment(RootComponent);
	if (Mat) H->SetMaterial(0, Mat);
	H->SetCollisionEnabled(bCol ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	H->SetCullDistances(0, (int32)Cull);
	H->SetCastShadow(bShadow);
	H->bAffectDistanceFieldLighting = false;
	H->RegisterComponent();
	return H;
}

// ---------------------------------------------------------------- ағаштар
void AKAPNature::BuildTrees()
{
	using namespace KAPTerrain;
	struct FSp { const TCHAR* Name; FLinearColor Bark; };
	const FSp Sp[4] = { { TEXT("Poplar"), FLinearColor(0.4f, 0.38f, 0.33f) }, { TEXT("Willow"), FLinearColor(0.3f, 0.26f, 0.2f) }, { TEXT("Elm"), FLinearColor(0.24f, 0.2f, 0.16f) }, { TEXT("Tamarisk"), FLinearColor(0.36f, 0.22f, 0.16f) } };
	UHierarchicalInstancedStaticMeshComponent* Trunk[4][2] = {}; UHierarchicalInstancedStaticMeshComponent* Leaf[4][2] = {};
	for (int32 s = 0; s < 4; s++) for (int32 v = 0; v < 2; v++)
	{
		UStaticMesh* T = LoadSM(FString::Printf(TEXT("SM_KAP_%sTrunk_%d"), Sp[s].Name, v));
		UStaticMesh* L = LoadSM(FString::Printf(TEXT("SM_KAP_%sLeaf_%d"), Sp[s].Name, v));
		if (!T || !L) continue;
		FKAPMat Bm = FKAPMat::Paint(Sp[s].Bark, 0.95f); Bm.Grime = 0.6f; Bm.VarAmt = 0.2f; Bm.VarScale = 0.02f; Bm.DustTop = 0.3f; Bm.Surface = 2;
		Trunk[s][v] = MakeH(T, KAPVis::SharedMaterial(Bm), s != 3, 70000, true);
		Leaf[s][v] = MakeH(L, nullptr, false, 70000, true);
		Trunk[s][v]->ComponentTags.Add(FName("SurfWood"));
	}
	FRandomStream R(9031);
	int32 N = 0;
	auto Plant = [&](int32 s, FVector2D P, float Scale)
	{
		const int32 v = R.RandRange(0, 1);
		if (!Trunk[s][v]) return;
		const float Z = KAPVis::GroundZ(GetWorld(), P.X * 100, P.Y * 100, 0.f);
		const FTransform X(FRotator(R.FRandRange(-3.f, 3.f), R.FRandRange(0.f, 360.f), R.FRandRange(-3.f, 3.f)), FVector(P.X * 100, P.Y * 100, Z - 15.f), FVector(Scale));
		Trunk[s][v]->AddInstance(X); Leaf[s][v]->AddInstance(X);
		N++;
	};
	auto Ok = [&](FVector2D P, float Water) { return !NearWater(P.X, P.Y, Water) && FieldMask(P.X, P.Y) <= 0.f && Clear(P, 8.f, 6.f) && FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y)) < 1140.f; };
	// оазис: бұлақ айналасында терек, қарағаш, жыңғыл
	for (int32 i = 0; i < 60; i++)
	{
		const float A = R.FRandRange(0.f, UE_TWO_PI), Rr = R.FRandRange(OasisR + 6.f, OasisR + 55.f);
		const FVector2D P(OasisX + FMath::Cos(A) * Rr, OasisY + FMath::Sin(A) * Rr);
		if (!Ok(P, 1.f)) continue;
		const float T = R.FRand();
		Plant(T < 0.55f ? 0 : T < 0.75f ? 2 : 3, P, R.FRandRange(0.8f, 1.2f));
	}
	// өзен жағасы мен атырап: тал, терек, жыңғыл тоғайлары
	for (float X = DeltaX1 - 60.f; X < MapHalf - 40.f; X += 6.f)
		for (int32 s = -1; s <= 1; s += 2)
		{
			float Y = 0.f, Hw = MainHalfW;
			if (X > DeltaX0) Y = RiverY(X) + s * (MainHalfW * 1.5f + R.FRandRange(3.f, 26.f));
			else { const int32 K = R.RandRange(0, 2); Hw = K == 1 ? 9.f : BranchHalfW; Y = BranchY(K, X) + s * (Hw * 1.5f + R.FRandRange(2.f, 22.f)); }
			const float Cl = Smooth(0.42f, 0.62f, Fbm(X / 120.f + 3.f, Y / 120.f - 1.f, 3));
			if (R.FRand() > Cl * 0.75f) continue;
			const FVector2D P(X + R.FRandRange(-3.f, 3.f), Y);
			if (!Ok(P, 1.5f)) continue;
			const float T = R.FRand();
			Plant(X < DeltaX0 ? (T < 0.45f ? 3 : T < 0.85f ? 1 : 0) : (T < 0.5f ? 1 : T < 0.82f ? 0 : 3), P, R.FRandRange(0.75f, 1.2f));
		}
	// ауылдар маңы: қарағаш (ескі ауыл ағаштары)
	if (UKAPDataSubsystem* D = Data.Get())
		D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L)
		{
			if (L.Type != FName("village")) return;
			for (int32 i = 0; i < 26; i++)
			{
				const float A = R.FRandRange(0.f, UE_TWO_PI), Rr = R.FRandRange(L.R - 25.f, L.R + 30.f);
				const FVector2D P(L.X + FMath::Cos(A) * Rr, L.Y + FMath::Sin(A) * Rr);
				if (!NearWater(P.X, P.Y, 1.f) && FieldMask(P.X, P.Y) <= 0.f && RoadDist(P) > 7.f && Clear(P, 7.f, -L.R + 2.f)) Plant(R.FRand() < 0.8f ? 2 : 0, P, R.FRandRange(0.8f, 1.15f));
			}
		});
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE trees=%d"), N);
}

// ---------------------------------------------------------------- тастар (бөктер)
void AKAPNature::BuildRocks()
{
	using namespace KAPTerrain;
	FKAPMat RockM = FKAPMat::Concrete(FLinearColor(0.4f, 0.37f, 0.33f)); RockM.Panel = 0.f; RockM.DustTop = 0.7f; RockM.VarAmt = 0.2f;
	UHierarchicalInstancedStaticMeshComponent* H = MakeH(KAPVis::Mesh(EKAPShape::Rock), KAPVis::SharedMaterial(RockM), true, 90000, true);
	FRandomStream R(551);
	int32 N = 0;
	for (int32 t = 0; t < 12000 && N < 1100; t++)
	{
		const FVector2D P(R.FRandRange(-1206.f, 1206.f), R.FRandRange(-1206.f, 1206.f));
		const float Fm = FoothillMask(P.X, P.Y);
		if (Fm < 0.05f || R.FRand() > Fm) continue;
		if (NearWater(P.X, P.Y, 2.f)) continue;
		const float S = R.FRandRange(0.8f, 2.5f) * (0.7f + Fm * 1.6f);
		const float Z = KAPVis::GroundZ(GetWorld(), P.X * 100, P.Y * 100, 0.f);
		H->AddInstance(FTransform(FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 360.f), R.FRandRange(-25.f, 25.f)), FVector(P.X * 100, P.Y * 100, Z - S * 30.f), FVector(S * R.FRandRange(0.9f, 1.6f), S, S * 0.8f)));
		N++;
	}
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE foothill rocks=%d"), N);
}

// ---------------------------------------------------------------- сексеуіл
void AKAPNature::BuildSaxaul()
{
	using namespace KAPTerrain;
	FKAPMat Bark = FKAPMat::Paint(FLinearColor(0.37f, 0.33f, 0.27f), 0.94f); Bark.Grime = 0.75f; Bark.VarAmt = 0.22f; Bark.VarScale = 0.03f; Bark.DustTop = 0.5f; Bark.Surface = 2;
	BarkMat = KAPVis::SharedMaterial(Bark);
	for (int32 v = 0; v < 3; v++)
	{
		UStaticMesh* W = LoadSM(FString::Printf(TEXT("SM_KAP_SaxaulWood_%d"), v));
		UStaticMesh* T = LoadSM(FString::Printf(TEXT("SM_KAP_SaxaulTwig_%d"), v));
		if (!W || !T) return;
		SaxWood.Add(MakeH(W, BarkMat, false, 45000, true));
		SaxTwig.Add(MakeH(T, nullptr, false, 32000, true));
	}
	// соқтығыс: діңнің ортасы (адам өте алмайды; көлік — VehicleSweep арқылы сындырады)
	SaxCore = MakeH(KAPVis::Mesh(EKAPShape::CylLow), nullptr, true, 8000, false);
	SaxCore->SetHiddenInGame(true);
	SaxCore->SetCollisionObjectType(ECC_WorldStatic);
	SaxCore->SetCollisionResponseToAllChannels(ECR_Block);
	SaxCore->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	SaxCore->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SaxCore->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	SaxCore->ComponentTags.Add(FName("SurfWood"));
	FRandomStream R(7321);
	int32 Tries = 0;
	while (Sax.Num() < 4200 && Tries++ < 60000)
	{
		const FVector2D P(R.FRandRange(-1140.f, 1140.f), R.FRandRange(-1140.f, 1140.f));
		const float Dm = DuneMask(P.X, P.Y);
		const float Cl = Fbm(P.X / 160.f + 5.f, P.Y / 160.f - 2.f, 3);
		// құмда — тоғай болып көп; жазық далада — шоғыр-шоғыр; ылғалды жерде, егінде, бөктерде — жоқ
		const float Wt = FMath::Max(Dm * 0.95f * Smooth(0.3f, 0.55f, Cl), (1.f - Dm) * 0.55f * Smooth(0.4f, 0.62f, Cl));
		if (R.FRand() > Wt) continue;
		if (NearWater(P.X, P.Y, 6.f) || WetMask(P.X, P.Y) > 0.2f || FieldMask(P.X, P.Y) > 0.f || FoothillMask(P.X, P.Y) > 0.25f) continue;
		if (!Clear(P, 6.f, 5.f)) continue;
		FSax S;
		S.V = (uint8)R.RandRange(0, 2);
		S.S = R.FRandRange(0.55f, 1.0f) * (Dm > 0.5f ? 1.1f : 1.f);
		S.Yaw = R.FRandRange(0.f, 360.f);
		S.P = FVector(P.X * 100, P.Y * 100, KAPVis::GroundZ(GetWorld(), P.X * 100, P.Y * 100, 0.f) - 8.f);
		S.Hp = 100.f * S.S; S.bDead = false; S.DeadT = 0.f;
		const FTransform X(FRotator(R.FRandRange(-4.f, 4.f), S.Yaw, R.FRandRange(-4.f, 4.f)), S.P, FVector(S.S));
		S.Inst = SaxWood[S.V]->AddInstance(X);
		SaxTwig[S.V]->AddInstance(X);
		SaxCore->AddInstance(FTransform(FRotator::ZeroRotator, S.P + FVector(0, 0, 75.f * S.S), FVector(0.42f * S.S, 0.42f * S.S, 1.5f * S.S)));
		SaxGrid.FindOrAdd(Cell(S.P)).Add(Sax.Num());
		Sax.Add(S);
	}
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE saxaul=%d"), Sax.Num());
}

bool AKAPNature::NearestSaxaul(const FVector& From, FVector& Out) const
{
	float Best = 1e12f;
	for (const FSax& S : Sax) if (!S.bDead) { const float D = FVector::DistSquared2D(S.P, From); if (D < Best) { Best = D; Out = S.P; } }
	return Best < 1e12f;
}

void AKAPNature::BreakSaxaul(int32 I, const FVector& Push)
{
	FSax& S = Sax[I];
	if (S.bDead) return;
	S.bDead = true; S.DeadT = 0.f;
	// SaxCore инстанс реті Sax ретімен бірдей; Wood/Twig — әр нұсқаның өз реті (S.Inst)
	const FTransform Gone(FRotator::ZeroRotator, S.P - FVector(0, 0, 5000.f), FVector(0.001f));
	SaxWood[S.V]->UpdateInstanceTransform(S.Inst, Gone, false, true, true);
	SaxTwig[S.V]->UpdateInstanceTransform(S.Inst, Gone, false, true, true);
	SaxCore->UpdateInstanceTransform(I, Gone, false, true, true);
	FVector2D Pd(Push.X, Push.Y);
	if (Pd.SizeSquared() < 1.f) { const float A = FMath::FRandRange(0.f, UE_TWO_PI); Pd = FVector2D(FMath::Cos(A), FMath::Sin(A)); }
	Pd.Normalize();
	// құлау: түбінен сынып, итерілген жаққа жатады
	FFelled F;
	F.Base = FRotator(0, S.Yaw, 0).Quaternion();
	F.Axis = FVector::CrossProduct(FVector::UpVector, FVector(Pd.X, Pd.Y, 0)).GetSafeNormal();
	auto Comp = [&](UStaticMesh* M, UMaterialInterface* Mat)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(M);
		if (Mat) C->SetMaterial(0, Mat);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetAbsolute(true, true, true);
		C->RegisterComponent();
		C->SetWorldTransform(FTransform(F.Base, S.P + FVector(0, 0, 12.f), FVector(S.S)));
		return C;
	};
	F.Wood = Comp(SaxWood[S.V]->GetStaticMesh(), BarkMat);
	F.Twig = Comp(SaxTwig[S.V]->GetStaticMesh(), nullptr);
	// отын үйіндісі (жинауға болады)
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector PileAt = S.P + FVector(Pd.X, Pd.Y, 0) * (110.f * S.S) + FVector(Pd.Y, -Pd.X, 0) * 60.f;
	if (AKAPWoodPile* Pile = GetWorld()->SpawnActorDeferred<AKAPWoodPile>(AKAPWoodPile::StaticClass(), FTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), FVector(PileAt.X, PileAt.Y, KAPVis::GroundZ(GetWorld(), PileAt.X, PileAt.Y, PileAt.Z)))))
	{
		Pile->Amount = 2 + S.V;
		Pile->FinishSpawning(FTransform(FRotator(0, FMath::FRandRange(0.f, 360.f), 0), FVector(PileAt.X, PileAt.Y, KAPVis::GroundZ(GetWorld(), PileAt.X, PileAt.Y, PileAt.Z))));
		F.Pile = Pile; F.bHadPile = true;
	}
	Felled.Add(F);
	// ұшқан бөлшектер (физика)
	if (UStaticMesh* Log = LoadSM(TEXT("SM_KAP_SaxaulLog")))
		for (int32 k = 0; k < 2 + S.V; k++)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
			C->SetStaticMesh(Log);
			C->SetMaterial(0, BarkMat);
			C->SetAbsolute(true, true, true);
			C->RegisterComponent();
			C->SetWorldTransform(FTransform(FRotator(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)), S.P + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(40.f, 160.f) * S.S), FVector(FMath::FRandRange(0.6f, 1.2f) * S.S)));
			C->SetCollisionProfileName(TEXT("PhysicsActor"));
			C->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			C->SetSimulatePhysics(true);
			C->SetMassOverrideInKg(NAME_None, 5.f, true);
			C->SetLinearDamping(0.2f); C->SetAngularDamping(0.6f);
			C->AddImpulse(Push * FMath::FRandRange(0.5f, 1.f) + FVector(FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(-180.f, 180.f), FMath::FRandRange(200.f, 450.f)), NAME_None, true);
			C->AddAngularImpulseInDegrees(FVector(FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-300.f, 300.f)), NAME_None, true);
			Debris.Add({ C, 0.f });
		}
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld()))
	{
		FX->Spawn(EKAPFX::Dust, S.P + FVector(0, 0, 40.f), 1.2f * S.S, Push * 0.1f);
		for (int32 k = 0; k < 6; k++) FX->Spawn(EKAPFX::Wood, S.P + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(30.f, 120.f)), 0.6f, Push * 0.4f);
	}
	if (UKAPAudio* Au = UKAPAudio::Get(this)) { Au->Impact(S.P + FVector(0, 0, 60.f), 2, 1.f); Au->Impact(S.P + FVector(0, 0, 60.f), 3, 0.5f); }
}

bool AKAPNature::MeleeHit(const FVector& From, const FVector& Dir, float Range, float Damage, bool bAxe)
{
	if (!Sax.Num()) return false;
	const FVector To = From + Dir * (Range + 40.f);
	int32 Best = -1; float BestT = 1e9f;
	const FIntPoint C0 = Cell(From.ComponentMin(To) - FVector(400)), C1 = Cell(From.ComponentMax(To) + FVector(400));
	for (int32 cx = C0.X; cx <= C1.X; cx++) for (int32 cy = C0.Y; cy <= C1.Y; cy++)
		if (const TArray<int32>* L = SaxGrid.Find(FIntPoint(cx, cy)))
			for (int32 i : *L)
			{
				const FSax& S = Sax[i];
				if (S.bDead) continue;
				const FVector Q(S.P.X, S.P.Y, FMath::Clamp(From.Z, S.P.Z, S.P.Z + 260.f * S.S));
				const FVector Cl = FMath::ClosestPointOnSegment(Q, From, To);
				if (FVector::Dist(Cl, Q) < 60.f * S.S + 25.f)
				{
					const float T = FVector::Dist(Cl, From);
					if (T < BestT) { BestT = T; Best = i; }
				}
			}
	if (Best < 0) return false;
	FSax& S = Sax[Best];
	S.Hp -= Damage * (bAxe ? 1.f : 0.3f);
	const FVector Hit = From + Dir * BestT;
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Impact(Hit, 2, bAxe ? 1.f : 0.7f);
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) for (int32 k = 0; k < (bAxe ? 4 : 2); k++) FX->Spawn(EKAPFX::Wood, Hit, 0.45f, -Dir * 120.f);
	if (S.Hp <= 0.f) BreakSaxaul(Best, Dir * 350.f);
	return true;
}

int32 AKAPNature::VehicleSweep(const FVector& Pos, const FVector& Vel, float Radius)
{
	const float Spd = Vel.Size2D();
	if (Spd < 150.f || !Sax.Num()) return 0;
	int32 N = 0;
	const FIntPoint C0 = Cell(Pos - FVector(Radius + 300.f)), C1 = Cell(Pos + FVector(Radius + 300.f));
	for (int32 cx = C0.X; cx <= C1.X; cx++) for (int32 cy = C0.Y; cy <= C1.Y; cy++)
		if (const TArray<int32>* L = SaxGrid.Find(FIntPoint(cx, cy)))
			for (int32 i : *L)
			{
				const FSax& S = Sax[i];
				if (S.bDead || FVector::Dist2D(S.P, Pos) > Radius + 45.f * S.S) continue;
				if (FMath::Abs(S.P.Z - Pos.Z) > 400.f) continue;
				BreakSaxaul(i, Vel * 0.55f + FVector(0, 0, 150.f));
				N++;
			}
	return N;
}

// ---------------------------------------------------------------- жалпы
void AKAPNature::Build(AKAPWorldBuilder* WB)
{
	Builder = WB;
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	Data = D;
	// егістік (редактордағы биом текстурасымен бірдей): жолдар — графтан, нысандар — кестелерден
	KAPTerrain::BuildFields([&](float X, float Y) { return RoadDist(FVector2D(X, Y)); },
		[&](float X, float Y, float Margin)
		{
			bool bOk = true;
			if (!D) return true;
			D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L) { if (FMath::Square(X - L.X) + FMath::Square(Y - L.Y) < FMath::Square(L.R + Margin)) bOk = false; });
			D->Wells->ForeachRow<FKAPWellRow>(TEXT("KAP"), [&](const FName&, const FKAPWellRow& W) { if (FMath::Square(X - W.X) + FMath::Square(Y - W.Y) < FMath::Square(9.f + Margin)) bOk = false; });
			D->Vehicles->ForeachRow<FKAPVehicleRow>(TEXT("KAP"), [&](const FName&, const FKAPVehicleRow& V) { if (FMath::Square(X - V.X) + FMath::Square(Y - V.Y) < FMath::Square(6.f + Margin)) bOk = false; });
			return bOk;
		});
	UE_LOG(LogTemp, Display, TEXT("KAPNATURE fields=%d"), KAPTerrain::FieldStore().Num());
	BuildWater();
	BuildMountains();
	BuildBridges();
	BuildTrees();
	BuildRocks();
	BuildSaxaul();
}

bool AKAPNature::WaterAt(const UWorld* W, const FVector& P, float& OutZ, float& OutDepth)
{
	const float X = P.X / 100.f, Y = P.Y / 100.f;
	float Zm = 0.f;
	bool b = false;
	if (X > KAPTerrain::MapHalf - 2.f && FMath::Abs(Y - KAPTerrain::GorgeY(X)) < 24.f) { Zm = KAPTerrain::GorgeWaterZ(X); b = true; }
	if (!b) { float Dep; b = KAPTerrain::WaterAt(X, Y, Zm, &Dep); if (!b) { float Hw; if (KAPTerrain::ChannelDist(X, Y, Hw) < Hw + 2.f) { Zm = KAPTerrain::WaterZ(X); b = true; } } }
	if (!b) return false;
	OutZ = Zm * 100.f;
	const float G = KAPVis::GroundZ(W, P.X, P.Y, OutZ - 100.f);
	OutDepth = FMath::Max(0.f, (OutZ - G) / 100.f);
	return OutDepth > 0.03f;
}

float AKAPNature::MudAt(const UWorld* W, FVector2D Pm)
{
	const AWeatherManager* WX = AWeatherManager::Get(const_cast<UWorld*>(W));
	const float Rain = WX ? WX->Env.Wet : 0.f;
	const float Dm = KAPTerrain::DuneMask(Pm.X, Pm.Y);
	// құм суды тез сіңіреді (батпақ болмайды), дала топырағы — лай, жаға — әрқашан ылғал
	const float M = Rain * (1.f - 0.75f * Dm) + KAPTerrain::WetMask(Pm.X, Pm.Y) * 0.75f;
	return FMath::Clamp(M, 0.f, 1.f);
}

void AKAPNature::Tick(float Dt)
{
	Super::Tick(Dt);
	// құлап жатқан сексеуіл
	for (int32 i = Felled.Num() - 1; i >= 0; i--)
	{
		FFelled& F = Felled[i];
		F.T += Dt;
		const float A = FMath::Clamp(F.T / 0.9f, 0.f, 1.f);
		const float Ang = FMath::DegreesToRadians(82.f) * A * A + (A >= 1.f ? FMath::Sin(FMath::Min(F.T - 0.9f, 0.6f) * 18.f) * FMath::Max(0.f, 0.6f - (F.T - 0.9f)) * 0.05f : 0.f);
		const FQuat Q = FQuat(F.Axis, Ang) * F.Base;
		if (F.Wood) F.Wood->SetWorldRotation(Q);
		if (F.Twig) F.Twig->SetWorldRotation(Q);
		if (F.T > 0.85f && F.T - Dt <= 0.85f) if (UKAPAudio* Au = UKAPAudio::Get(this)) if (F.Wood) Au->Impact(F.Wood->GetComponentLocation() + F.Axis.RotateAngleAxis(90.f, FVector::UpVector) * 120.f, 3, 0.8f);
		const bool bTaken = F.bHadPile && !F.Pile.IsValid();
		if (bTaken || F.T > 300.f)
		{
			if (F.Wood) F.Wood->DestroyComponent();
			if (F.Twig) F.Twig->DestroyComponent();
			if (F.Pile.IsValid()) F.Pile->Destroy();
			Felled.RemoveAtSwap(i);
		}
	}
	for (int32 i = Debris.Num() - 1; i >= 0; i--)
	{
		Debris[i].T += Dt;
		if (Debris[i].T > 30.f) { if (Debris[i].C) Debris[i].C->DestroyComponent(); Debris.RemoveAtSwap(i); }
	}
	// қайта өсу: ойыншы алыста болса, 15 минуттан соң
	RegrowAcc += Dt;
	for (FSax& S : Sax) if (S.bDead) S.DeadT += Dt;
	if (RegrowAcc > 5.f)
	{
		RegrowAcc = 0.f;
		const AKAPCharacter* P = AKAPCharacter::Get(this);
		for (int32 i = 0; i < Sax.Num(); i++)
		{
			FSax& S = Sax[i];
			if (!S.bDead || S.DeadT < 900.f || (P && FVector::Dist2D(P->GetActorLocation(), S.P) < 15000.f)) continue;
			S.bDead = false; S.Hp = 100.f * S.S;
			const FTransform X(FRotator(0, S.Yaw, 0), S.P, FVector(S.S));
			SaxWood[S.V]->UpdateInstanceTransform(S.Inst, X, false, true, true);
			SaxTwig[S.V]->UpdateInstanceTransform(S.Inst, X, false, true, true);
			SaxCore->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, S.P + FVector(0, 0, 75.f * S.S), FVector(0.42f * S.S, 0.42f * S.S, 1.5f * S.S)), false, true, true);
		}
	}
	// көліктер сексеуілді сындырады
	if (AKAPWorldBuilder* WB = Builder.Get())
		for (const TWeakObjectPtr<class AKAPVehicleHandle>& V : WB->Vehicles)
			if (V.IsValid()) if (AActor* Pw = V->Pawn.Get())
				if (VehicleSweep(Pw->GetActorLocation(), Pw->GetVelocity(), 230.f) > 0)
					if (UPrimitiveComponent* Pc = Cast<UPrimitiveComponent>(Pw->GetRootComponent())) if (Pc->IsSimulatingPhysics()) Pc->SetPhysicsLinearVelocity(Pc->GetPhysicsLinearVelocity() * 0.94f);
}
