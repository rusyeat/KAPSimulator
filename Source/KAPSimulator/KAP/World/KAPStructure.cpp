#include "KAPStructure.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPVisuals.h"
#include "KAP/Core/KAPGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"

TArray<FVector2D> AKAPStructure::CoverCache;
uint64 AKAPStructure::CoverCacheFrame = 0;

AKAPStructure::AKAPStructure()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.08f;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

UStaticMeshComponent* AKAPStructure::Box(FVector Pos, FVector Size, const FKAPMat& M, FRotator R, bool bCol)
{
	return KAPVis::Add(this, RootComponent, EKAPShape::Cube, Pos, Size, M, R, bCol);
}

void AKAPStructure::Build(EStructType InType, FVector S, FLinearColor Color, int32 Seed, bool bBright)
{
	Type = InType;
	KAPReg::Add(KAPReg::Structures(), this);
	FRandomStream R(Seed);
	const FTransform Xf = GetActorTransform();
	auto ToW = [&](FVector L) { return Xf.TransformPosition(L); };
	auto Cover = [&](FVector L) { const FVector W = ToW(L); CoverPts.Add(FVector2D(W.X, W.Y) / 100.0); };
	const FKAPMat Wall = FKAPMat::Sheet(Color, 0.3f + R.FRand() * 0.25f);
	const FKAPMat Dark = FKAPMat::Paint(FLinearColor(0.18f, 0.14f, 0.1f), 0.9f);
	FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.06f, 0.075f, 0.085f), 0.06f); Glass.Grime = 0.25f; Glass.DustTop = 0.4f;
	FKAPMat Wood = FKAPMat::Paint(FLinearColor(0.3f, 0.2f, 0.11f), 0.85f); Wood.Ribs = 0.35f; Wood.RibPeriod = 14.f; Wood.Panel = 1.f; Wood.PanelW = 14.f; Wood.Grime = 0.8f;
	const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.42f, 0.43f, 0.45f), 0.45f);
	const FKAPMat Frame = FKAPMat::Paint(FLinearColor(0.82f, 0.8f, 0.74f), 0.7f);
	// айналадағы «тірі» заттар: бөшке, газ баллоны, паллет, дөңгелек үйіндісі
	auto Barrel = [&](FVector P, FLinearColor C)
	{
		FKAPMat B = FKAPMat::Sheet(C, 0.5f + R.FRand() * 0.4f, 0.f); B.Metallic = 0.5f;
		const bool bFallen = R.FRand() < 0.2f;
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, P + FVector(0, 0, bFallen ? 30 : 45), FVector(58, 58, 88), B, bFallen ? FRotator(90, R.FRandRange(0.f, 180.f), 0) : FRotator::ZeroRotator, true);
		if (!bFallen) for (float Z : { 22.f, 68.f }) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, P + FVector(0, 0, Z), FVector(61, 61, 3), Steel, FRotator::ZeroRotator, false);
	};
	auto Props = [&](FVector Around, float Spread, int32 N)
	{
		static const FLinearColor BarrelC[4] = { FLinearColor(0.08f, 0.18f, 0.4f), FLinearColor(0.45f, 0.08f, 0.05f), FLinearColor(0.25f, 0.3f, 0.15f), FLinearColor(0.55f, 0.45f, 0.08f) };
		for (int32 i = 0; i < N; i++)
		{
			const FVector P = Around + FVector(R.FRandRange(-Spread, Spread), R.FRandRange(-Spread * 0.5f, Spread * 0.5f), 0);
			const float T = R.FRand();
			if (T < 0.45f) { Barrel(P, BarrelC[R.RandRange(0, 3)]); if (R.FRand() < 0.5f) Barrel(P + FVector(62, R.FRandRange(-10.f, 10.f), 0), BarrelC[R.RandRange(0, 3)]); }
			else if (T < 0.6f) { KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, P + FVector(0, 0, 65), FVector(30, 30, 130), FKAPMat::Paint(FLinearColor(0.6f, 0.07f, 0.04f), 0.5f), FRotator(0, 0, R.FRandRange(-4.f, 4.f)), true); KAPVis::Add(this, RootComponent, EKAPShape::Sphere, P + FVector(0, 0, 132), FVector(30, 30, 20), FKAPMat::Paint(FLinearColor(0.6f, 0.07f, 0.04f), 0.5f), FRotator::ZeroRotator, false); }
			else if (T < 0.8f) { for (int32 j = 0; j < 3; j++) Box(P + FVector(0, 0, 6 + j * 14), FVector(120, 100, 4), Wood, FRotator(0, R.FRandRange(-6.f, 6.f), 0), j == 0); Box(P + FVector(0, 0, 13), FVector(120, 10, 10), Wood, FRotator::ZeroRotator, false); }
			else { for (int32 j = 0; j < R.RandRange(1, 4); j++) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, P + FVector(R.FRandRange(-6.f, 6.f), R.FRandRange(-6.f, 6.f), 12 + j * 24), FVector(80, 80, 24), FKAPMat::Paint(FLinearColor(0.03f, 0.03f, 0.03f), 0.95f), FRotator::ZeroRotator, j == 0); }
		}
	};
	switch (Type)
	{
	case EStructType::Wagon:
	{
		// 8×3×2.8 м вагон-үй, есігі +Y жақта
		const float L = 800, D = 300, H = 280, T = 10;
		Box(FVector(0, 0, 45), FVector(L, D, 30), Wood);
		Box(FVector(0, 0, H + 62), FVector(L + 20, D + 30, 15), FKAPMat::Sheet(FLinearColor(0.36f, 0.35f, 0.33f), 0.6f));
		// шаналар (сүйретпе рама), баспалдақ, терезе жақтаулары, кондиционер, антенна, жазу
		for (float Y : { -D * 0.35f, D * 0.35f }) Box(FVector(0, Y, 16), FVector(L + 60, 22, 32), Steel);
		for (float X : { -L / 2 - 20, L / 2 + 20 }) Box(FVector(X, 0, 22), FVector(20, D - 20, 16), Steel, FRotator(0, 0, 0), false);
		Box(FVector(-240, D / 2 + 4, 210), FVector(112, 4, 6), Frame, FRotator::ZeroRotator, false);
		Box(FVector(-240, D / 2 + 4, 172), FVector(112, 10, 6), Frame, FRotator::ZeroRotator, false);
		Box(FVector(-240, D / 2 + 4, 248), FVector(112, 4, 6), Frame, FRotator::ZeroRotator, false);
		Box(FVector(-240, D / 2 + 4, 210), FVector(4, 5, 76), Frame, FRotator::ZeroRotator, false);
		Box(FVector(240, -D / 2 - 2, 210), FVector(100, 4, 70), Glass, FRotator::ZeroRotator, false);
		Box(FVector(240, -D / 2 - 4, 210), FVector(112, 4, 82), Frame, FRotator::ZeroRotator, false);
		Box(FVector(-L / 2 - 25, 60, 250), FVector(40, 80, 55), FKAPMat::Paint(FLinearColor(0.78f, 0.77f, 0.72f), 0.6f));
		for (int32 i = 0; i < 6; i++) Box(FVector(-L / 2 - 46, 60, 232 + i * 7), FVector(2, 70, 3), Dark, FRotator::ZeroRotator, false);
		Box(FVector(56, D / 2 + 58, 165), FVector(4, 110, 230), FKAPMat::Sheet(FLinearColor(0.3f, 0.32f, 0.3f), 0.5f), FRotator(0, -8, 0), false); // ашық есік
		Box(FVector(60, D / 2 + 104, 160), FVector(6, 6, 18), Steel, FRotator::ZeroRotator, false);
		Box(FVector(300, -80, H + 140), FVector(5, 5, 160), Steel, FRotator::ZeroRotator, false);
		Box(FVector(300, -80, H + 200), FVector(60, 4, 4), Steel, FRotator::ZeroRotator, false);
		{
			UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
			Tx->SetupAttachment(RootComponent); Tx->SetRelativeLocation(FVector(-120, -D / 2 - 8, 270)); Tx->SetRelativeRotation(FRotator(0, -90, 0));
			Tx->SetText(FText::FromString(TEXT("QazUran · ВАГОН-ҮЙ №") + FString::FromInt(Seed % 40 + 1))); Tx->SetHorizontalAlignment(EHTA_Center); Tx->SetWorldSize(22);
			Tx->SetTextRenderColor(FColor(230, 225, 210)); Tx->RegisterComponent();
		}
		Props(FVector(-100, -D / 2 - 170, 0), 320, 3);
		Props(FVector(L / 2 + 160, 0, 0), 80, 1);
		Box(FVector(0, -D / 2, H / 2 + 60), FVector(L, T, H), Wall);
		Box(FVector(-L / 2, 0, H / 2 + 60), FVector(T, D, H), Wall);
		Box(FVector(L / 2, 0, H / 2 + 60), FVector(T, D, H), Wall);
		Box(FVector(-170, D / 2, H / 2 + 60), FVector(460, T, H), Wall);
		Box(FVector(290, D / 2, H / 2 + 60), FVector(220, T, H), Wall);
		Box(FVector(120, D / 2, H + 30), FVector(120, T, 60), Wall);
		Box(FVector(-240, D / 2 + 2, 210), FVector(100, 4, 70), Glass, FRotator::ZeroRotator, false);
		Box(FVector(120, D / 2 + 60, 30), FVector(120, 80, 30), Wood);
		// ішкі жиһаз
		Box(FVector(-260, -90, 140), FVector(160, 90, 8), Wood);
		Box(FVector(-330, -120, 100), FVector(8, 8, 80), Wood); Box(FVector(-190, -120, 100), FVector(8, 8, 80), Wood);
		Box(FVector(260, -90, 85), FVector(200, 90, 40), FKAPMat::Paint(FLinearColor(0.2f, 0.25f, 0.32f), 0.95f));
		Box(FVector(330, -90, 110), FVector(60, 70, 15), FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.82f), 0.95f), FRotator::ZeroRotator, false);
		Blinker = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(120, D / 2 + 25, H + 30), FVector(30, 30, 30), FKAPMat::Glow(FLinearColor(1, 0.8f, 0.45f), bBright ? 60 : 25), FRotator::ZeroRotator, false);
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent); Light->SetRelativeLocation(FVector(120, D / 2 + 70, H + 10));
		Light->SetIntensity(bBright ? 25000 : 8000); Light->SetAttenuationRadius(bBright ? 6000 : 2200); Light->SetLightColor(FLinearColor(1, 0.78f, 0.45f));
		Light->RegisterComponent();
		UPointLightComponent* In = NewObject<UPointLightComponent>(this);
		In->SetupAttachment(RootComponent); In->SetRelativeLocation(FVector(0, 0, H + 20)); In->SetIntensity(3000); In->SetAttenuationRadius(700); In->SetLightColor(FLinearColor(1, 0.85f, 0.6f)); In->RegisterComponent();
		{
			// төбе шамы (кейбір вагондарда ақаулы — жыпылықтайды) және ішкі заттар: сөре, радио, орындық, қағаздар, күнтізбе
			UStaticMeshComponent* Bulb = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(0, 0, H + 40), FVector(14, 14, 14), FKAPMat::Glow(FLinearColor(1, 0.85f, 0.6f), 0), FRotator::ZeroRotator, false);
			Bulb->SetCastShadow(false);
			FFlick F; F.L = In; F.Bulb = Bulb; F.Base = 3000; F.bBad = R.FRand() < 0.4f; F.Col = FLinearColor(1, 0.85f, 0.6f); F.Next = R.FRandRange(1.f, 5.f); Flicks.Add(F);
			for (int32 z = 0; z < 3; z++) Box(FVector(-L / 2 + 30, 40, 120 + z * 50), FVector(40, 160, 4), Wood, FRotator::ZeroRotator, false);
			Box(FVector(-L / 2 + 30, 10, 182), FVector(26, 36, 20), FKAPMat::Paint(FLinearColor(0.15f, 0.14f, 0.1f), 0.6f), FRotator::ZeroRotator, false);
			KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(-L / 2 + 44, 0, 186), FVector(3, 3, 3), FKAPMat::Glow(FLinearColor(1.f, 0.4f, 0.05f), 30), FRotator::ZeroRotator, false);
			for (int32 k = 0; k < 4; k++) Box(FVector(-L / 2 + 30, 70 + k * 20, 130 + (k % 2) * 50), FVector(30, 12, 26), FKAPMat::Paint(FLinearColor(0.4f + k * 0.1f, 0.3f, 0.2f), 0.8f), FRotator::ZeroRotator, false);
			Box(FVector(-200, 20, 50), FVector(45, 45, 6), Wood, FRotator(0, 15, 0), false);
			for (float Sx : { -1.f, 1.f }) for (float Sy : { -1.f, 1.f }) Box(FVector(-200 + Sx * 18, 20 + Sy * 18, 24), FVector(4, 4, 48), Steel, FRotator(0, 15, 0), false);
			Box(FVector(-220, -100, 146), FVector(34, 26, 1), FKAPMat::Paint(FLinearColor(0.9f, 0.88f, 0.8f), 0.9f), FRotator(0, 22, 0), false);
			Box(FVector(-280, -80, 146), FVector(30, 22, 1), FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.75f), 0.9f), FRotator(0, -10, 0), false);
			Box(FVector(0, -D / 2 + 8, 200), FVector(40, 2, 55), FKAPMat::Paint(FLinearColor(0.85f, 0.8f, 0.7f), 0.9f), FRotator::ZeroRotator, false);
			// «әлемдік әңгіме»: суыған шай кесесі мен табақша, қабырғадағы отбасы суреті, ілулі күрте
			KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(-240, -70, 148), FVector(9, 9, 1), FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.8f), 0.4f), FRotator::ZeroRotator, false);
			KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(-240, -70, 152), FVector(7, 7, 7), FKAPMat::Paint(FLinearColor(0.9f, 0.9f, 0.86f), 0.3f), FRotator::ZeroRotator, false);
			KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(-240, -70, 155.4f), FVector(6, 6, 0.3f), FKAPMat::Paint(FLinearColor(0.22f, 0.08f, 0.02f), 0.05f), FRotator::ZeroRotator, false);
			Box(FVector(180, -D / 2 + 7, 200), FVector(26, 2, 20), Wood, FRotator::ZeroRotator, false);
			Box(FVector(180, -D / 2 + 8.2f, 200), FVector(22, 1, 16), FKAPMat::Paint(FLinearColor(0.55f, 0.42f, 0.3f), 0.6f), FRotator::ZeroRotator, false);
			for (int32 k = 0; k < 3; k++) Box(FVector(174 + k * 6, -D / 2 + 8.8f, 197 + (k % 2) * 2), FVector(3.5f, 0.5f, 8), FKAPMat::Paint(FLinearColor(0.15f + k * 0.1f, 0.1f, 0.08f), 0.7f), FRotator::ZeroRotator, false);
			Box(FVector(330, -D / 2 + 14, 210), FVector(40, 10, 70), FKAPMat::Paint(FLinearColor(0.7f, 0.33f, 0.05f), 0.95f), FRotator(0, 0, 4), false);
		}
		const FVector A = ToW(FVector(-L / 2, -D / 2, 0)), B = ToW(FVector(L / 2, D / 2, 0));
		IndoorBoxes.Add(FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y))));
		TablePos = ToW(FVector(-260, -90, 150)); BedPos = ToW(FVector(260, -90, 100)); CratePos = ToW(FVector(-80, -100, 40)); DoorPos = ToW(FVector(120, D / 2 + 120, 0));
		ShadeR = 500;
		break;
	}
	case EStructType::House:
	case EStructType::Ruin:
	{
		const bool bRuin = Type == EStructType::Ruin;
		const float W = S.X, D = S.Y, H = 320;
		FKAPMat M = bRuin ? FKAPMat::Concrete(FLinearColor(0.16f, 0.14f, 0.12f)) : FKAPMat::Concrete(Color * 1.1f);
		if (bRuin) M.Rust = 0.f;
		if (!bRuin)
		{
			Box(FVector(0, 0, H / 2), FVector(W, D, H), M);
			Box(FVector(0, 0, H + 12), FVector(W + 40, D + 40, 25), FKAPMat::Sheet(FLinearColor(0.3f, 0.26f, 0.22f), 0.55f));
			Box(FVector(0, 0, H + 30), FVector(W * 0.6f, D * 0.6f, 12), FKAPMat::Sheet(FLinearColor(0.28f, 0.25f, 0.21f), 0.6f));
			// есік: жақтау + ағаш полотно + баспалдақ
			Box(FVector(0, D / 2 + 3, 100), FVector(100, 6, 200), Wood, FRotator::ZeroRotator, false);
			Box(FVector(0, D / 2 + 6, 205), FVector(116, 8, 10), Frame, FRotator::ZeroRotator, false);
			for (float X : { -54.f, 54.f }) Box(FVector(X, D / 2 + 6, 102), FVector(8, 8, 206), Frame, FRotator::ZeroRotator, false);
			Box(FVector(0, D / 2 + 40, 8), FVector(140, 70, 16), FKAPMat::Concrete());
			// терезелер: әйнек, жақтау, темір тор, жапқыштар
			for (float Sx : { -1.f, 1.f })
			{
				const float X = Sx * W / 3;
				Box(FVector(X, D / 2 + 3, 180), FVector(90, 6, 80), Glass, FRotator::ZeroRotator, false);
				Box(FVector(X, D / 2 + 7, 136), FVector(104, 14, 8), Frame, FRotator::ZeroRotator, false);
				Box(FVector(X, D / 2 + 6, 224), FVector(104, 6, 8), Frame, FRotator::ZeroRotator, false);
				for (int32 b = -2; b <= 2; b++) Box(FVector(X + b * 18, D / 2 + 9, 180), FVector(2.5f, 2.5f, 82), Steel, FRotator::ZeroRotator, false);
				if (R.FRand() < 0.6f) for (float Sh : { -1.f, 1.f }) Box(FVector(X + Sh * 62, D / 2 + 8, 180), FVector(30, 4, 86), FKAPMat::Paint(FLinearColor(0.12f, 0.25f, 0.35f) * R.FRandRange(0.6f, 1.2f), 0.8f), FRotator::ZeroRotator, false);
			}
			// науа, түтін мұржасы, кондиционер, антенна
			Box(FVector(W / 2 + 8, D / 2 + 8, H / 2), FVector(10, 10, H), FKAPMat::RustySteel(), FRotator::ZeroRotator, false);
			Box(FVector(W * 0.25f, -D * 0.2f, H + 80), FVector(50, 50, 110), FKAPMat::Concrete(FLinearColor(0.35f, 0.3f, 0.26f)), FRotator::ZeroRotator, false);
			if (R.FRand() < 0.5f) { Box(FVector(-W / 2 - 22, 0, 230), FVector(40, 70, 50), FKAPMat::Paint(FLinearColor(0.8f, 0.8f, 0.76f), 0.6f), FRotator::ZeroRotator, false); }
			if (R.FRand() < 0.5f) KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(-W * 0.3f, D * 0.2f, H + 70), FVector(90, 90, 20), FKAPMat::Paint(FLinearColor(0.75f, 0.75f, 0.72f), 0.5f), FRotator(35, 0, 0), false);
			Box(FVector(0, 0, 6), FVector(W + 60, D + 60, 12), FKAPMat::Concrete());
			Props(FVector(W / 2 + 150, D / 4, 0), 120, 2);
		}
		else
		{
			Box(FVector(0, -D / 2, H * 0.4f), FVector(W, 30, H * 0.8f), M);
			Box(FVector(-W / 2, 0, H * 0.25f), FVector(30, D, H * 0.5f), M);
			Box(FVector(W * 0.3f, D / 2, H * 0.15f), FVector(W * 0.4f, 30, H * 0.3f), M);
			for (int32 i = 0; i < 5; i++) KAPVis::Add(this, RootComponent, EKAPShape::Rock, FVector(R.FRandRange(-W / 2, W / 2), R.FRandRange(-D / 2, D / 2), 10), FVector(R.FRandRange(60, 140), R.FRandRange(60, 140), 40), FKAPMat::Paint(FLinearColor(0.08f, 0.07f, 0.06f), 1.f));
			// қираған ауылдың әңгімесі: киіз үйдің бос қаңқасы, желмен тербелетін шелек, құмға көмілген велосипед
			const FKAPMat Pole = FKAPMat::Paint(FLinearColor(0.28f, 0.18f, 0.1f), 0.85f);
			const FVector Yc(W / 2 + 450, 0, 0);
			for (int32 k = 0; k < 16; k++) { const float A = k / 16.f * UE_TWO_PI; KAPVis::Add(this, RootComponent, EKAPShape::CylLow, Yc + FVector(FMath::Cos(A) * 290, FMath::Sin(A) * 290, 85), FVector(4, 4, 170), Pole, FRotator(FMath::Sin(A) * 8.f, 0, FMath::Cos(A) * 8.f), false); }
			for (int32 k = 0; k < 10; k++) { const float A = k / 10.f * UE_TWO_PI; if (k % 3 == 1) continue; KAPVis::Add(this, RootComponent, EKAPShape::CylLow, Yc + FVector(FMath::Cos(A) * 150, FMath::Sin(A) * 150, 225), FVector(3, 3, 300), Pole, FRotationMatrix::MakeFromZ(FVector(-FMath::Cos(A), -FMath::Sin(A), 0.45f)).Rotator(), false); }
			KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Yc + FVector(0, 0, 290), FVector(60, 60, 8), Pole, FRotator::ZeroRotator, false);
			Box(FVector(-W / 2 - 120, D / 2 + 60, 110), FVector(8, 8, 220), Pole);
			Box(FVector(-W / 2 - 90, D / 2 + 60, 214), FVector(60, 6, 6), Pole, FRotator::ZeroRotator, false);
			SwingBucket = KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(-W / 2 - 65, D / 2 + 60, 170), FVector(26, 26, 30), FKAPMat::RustySteel(FLinearColor(0.4f, 0.4f, 0.38f), 0.6f), FRotator::ZeroRotator, false);
			const FVector Bk(-W / 3, -D / 2 - 220, 6);
			for (float X : { -55.f, 55.f }) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Bk + FVector(X, 0, 18), FVector(64, 64, 3), FKAPMat::RustySteel(FLinearColor(0.2f, 0.2f, 0.2f), 0.8f), FRotator(0, 0, 80), false);
			Box(Bk + FVector(0, 0, 40), FVector(100, 4, 4), FKAPMat::RustySteel(FLinearColor(0.35f, 0.08f, 0.05f), 0.7f), FRotator(-10, 0, 80), false);
			Box(Bk + FVector(-30, 0, 45), FVector(4, 4, 50), FKAPMat::RustySteel(FLinearColor(0.35f, 0.08f, 0.05f), 0.7f), FRotator(20, 0, 80), false);
			KAPVis::Add(this, RootComponent, EKAPShape::Rock, Bk + FVector(20, 10, 0), FVector(120, 80, 30), FKAPMat::Concrete(FLinearColor(0.6f, 0.47f, 0.32f)), FRotator::ZeroRotator, false);
		}
		Cover(FVector(W / 2 + 120, 0, 0)); Cover(FVector(-W / 2 - 120, 0, 0)); Cover(FVector(0, -D / 2 - 120, 0));
		ShadeR = FMath::Max(W, D) * 0.8f;
		break;
	}
	case EStructType::Yurt:
	{
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 110), FVector(600, 600, 220), FKAPMat::Paint(FLinearColor(0.85f, 0.8f, 0.7f), 0.95f));
		KAPVis::Add(this, RootComponent, EKAPShape::Cone, FVector(0, 0, 300), FVector(640, 640, 160), FKAPMat::Paint(FLinearColor(0.75f, 0.7f, 0.6f), 0.95f));
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 380), FVector(70, 70, 10), Wood);
		Box(FVector(0, 300, 80), FVector(90, 10, 160), FKAPMat::Paint(FLinearColor(0.55f, 0.08f, 0.05f), 0.7f), FRotator::ZeroRotator, false);
		ShadeR = 400;
		break;
	}
	case EStructType::Campfire:
	{
		for (int32 i = 0; i < 6; i++) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 10), FVector(16, 16, 100), Dark, FRotator(90, i * 30.f, 0), false);
		for (int32 i = 0; i < 8; i++) KAPVis::Add(this, RootComponent, EKAPShape::Rock, FVector(FMath::Cos(i * 0.785f) * 70, FMath::Sin(i * 0.785f) * 70, 5), FVector(30, 30, 20), FKAPMat::Paint(FLinearColor(0.2f, 0.18f, 0.16f)), FRotator::ZeroRotator, false);
		for (int32 i = 0; i < 3; i++) Flames.Add(KAPVis::Add(this, RootComponent, EKAPShape::Cone, FVector(R.FRandRange(-15, 15), R.FRandRange(-15, 15), 50), FVector(60, 60, 100), FKAPMat::Glow(FLinearColor(1, 0.45f, 0.05f), 30), FRotator::ZeroRotator, false));
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(RootComponent); Light->SetRelativeLocation(FVector(0, 0, 120)); Light->SetIntensity(20000); Light->SetAttenuationRadius(1800); Light->SetLightColor(FLinearColor(1, 0.55f, 0.2f));
		Light->RegisterComponent();
		bFire = true;
		break;
	}
	case EStructType::Tower:
	{
		const float H = 3200;
		for (int32 i = 0; i < 4; i++) { const float X = (i & 1) ? 90 : -90, Y = (i & 2) ? 90 : -90; Box(FVector(X, Y, H / 2), FVector(20, 20, H), Steel); }
		for (float Z = 300; Z < H; Z += 400) { Box(FVector(0, 90, Z), FVector(200, 8, 8), Steel, FRotator::ZeroRotator, false); Box(FVector(90, 0, Z), FVector(8, 200, 8), Steel, FRotator::ZeroRotator, false); Box(FVector(0, 0, Z + 200), FVector(8, 260, 8), Steel, FRotator(0, 45, 35), false); }
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 120, H - 400), FVector(160, 160, 15), FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.85f)), FRotator(90, 0, 0), false);
		Blinker = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(0, 0, H + 40), FVector(70, 70, 70), FKAPMat::Glow(FLinearColor(1, 0, 0), 80), FRotator::ZeroRotator, false);
		Box(FVector(240, 0, 70), FVector(100, 60, 140), FKAPMat::Paint(FLinearColor(0.25f, 0.25f, 0.25f)));
		break;
	}
	case EStructType::Tank:
	{
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 400), FVector(2000, 2000, 800), FKAPMat::Sheet(FLinearColor(0.58f, 0.6f, 0.6f), 0.45f, 0.6f));
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 810), FVector(2040, 2040, 40), Steel);
		Box(FVector(1400, 0, 150), FVector(500, 400, 300), FKAPMat::Paint(FLinearColor(0.6f, 0.55f, 0.45f)));
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(1150, 0, 120), FVector(60, 60, 600), Steel, FRotator(90, 0, 0));
		ShadeR = 1400;
		break;
	}
	case EStructType::Tent:
		KAPVis::Add(this, RootComponent, EKAPShape::Cone, FVector(0, 0, 130), FVector(480, 480, 260), FKAPMat::Paint(Color, 0.95f), FRotator(0, R.FRandRange(0, 90), 0));
		Cover(FVector(260, 0, 0)); Cover(FVector(-260, 0, 0));
		break;
	case EStructType::Barrier:
		Box(FVector(0, 0, 50), FVector(S.X, 60, 100), FKAPMat::Concrete());
		Box(FVector(0, 0, 104), FVector(S.X, 62, 12), FKAPMat::Hazard(), FRotator::ZeroRotator, false);
		Cover(FVector(0, 120, 0)); Cover(FVector(0, -120, 0));
		break;
	case EStructType::GuardPost:
	{
		Box(FVector(0, 0, 130), FVector(220, 220, 260), FKAPMat::Sheet(FLinearColor(0.27f, 0.33f, 0.22f), 0.4f));
		Box(FVector(0, 0, 270), FVector(240, 240, 15), Dark);
		Box(FVector(0, 112, 170), FVector(140, 4, 60), Glass, FRotator::ZeroRotator, false);
		Box(FVector(400, 200, 100), FVector(600, 12, 12), FKAPMat::Hazard(), FRotator::ZeroRotator, false);
		Box(FVector(100, 200, 50), FVector(30, 30, 100), FKAPMat::Hazard());
		UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
		Tx->SetupAttachment(RootComponent); Tx->SetRelativeLocation(FVector(0, 125, 320)); Tx->SetRelativeRotation(FRotator(0, 90, 0));
		Tx->SetText(FText::FromString(TEXT("KUZET"))); Tx->SetHorizontalAlignment(EHTA_Center); Tx->SetWorldSize(50); Tx->SetTextRenderColor(FColor(255, 140, 100)); Tx->RegisterComponent();
		Cover(FVector(160, 0, 0)); Cover(FVector(-160, 0, 0));
		break;
	}
	case EStructType::HQOffice:
	{
		// ашық алдыңғы жағы бар кеңсе (10×8 м)
		const FKAPMat M = FKAPMat::Concrete(FLinearColor(0.62f, 0.62f, 0.6f));
		Box(FVector(0, -400, 160), FVector(1000, 30, 320), M);
		Box(FVector(-500, 0, 160), FVector(30, 800, 320), M);
		Box(FVector(500, 0, 160), FVector(30, 800, 320), M);
		Box(FVector(0, 0, 330), FVector(1060, 860, 25), FKAPMat::Paint(FLinearColor(0.35f, 0.35f, 0.35f)));
		Box(FVector(-200, -250, 45), FVector(240, 100, 90), Wood);
		Box(FVector(300, -320, 60), FVector(140, 80, 120), FKAPMat::Paint(FLinearColor(0.12f, 0.16f, 0.2f)));
		KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(300, -275, 130), FVector(10, 10, 10), FKAPMat::Glow(FLinearColor(0, 1, 0), 40), FRotator::ZeroRotator, false);
		Box(FVector(-200, -250, 95), FVector(40, 30, 6), FKAPMat::Paint(FLinearColor(0.5f, 0.1f, 0.08f)), FRotator(0, 15, 0), false);
		const FVector A = ToW(FVector(-500, -400, 0)), B = ToW(FVector(500, 400, 0));
		IndoorBoxes.Add(FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y))));
		TablePos = ToW(FVector(-200, -250, 100)); BedPos = ToW(FVector(300, -320, 120));
		break;
	}
	case EStructType::Fence:
		Box(FVector(0, 0, 120), FVector(S.X, 8, 240), FKAPMat::Sheet(FLinearColor(0.42f, 0.43f, 0.42f), 0.6f));
		Box(FVector(0, 0, 236), FVector(S.X, 14, 8), Steel, FRotator::ZeroRotator, false);
		for (float X = -S.X / 2; X <= S.X / 2; X += 300) Box(FVector(X, 0, 130), FVector(14, 14, 260), Steel, FRotator::ZeroRotator, false);
		break;
	case EStructType::Gate:
		Box(FVector(0, 0, 130), FVector(800, 20, 260), FKAPMat::Sheet(FLinearColor(0.5f, 0.08f, 0.05f), 0.5f));
		Box(FVector(0, 0, 20), FVector(800, 24, 40), FKAPMat::Hazard(), FRotator::ZeroRotator, false);
		for (float X : { -410.f, 410.f }) Box(FVector(X, 0, 150), FVector(26, 26, 300), Steel);
		break;
	case EStructType::Wreck:
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vehicles/OffroadCar/SM_Offroad_Body.SM_Offroad_Body")));
		C->SetupAttachment(RootComponent); C->SetRelativeRotation(FRotator(4, 0, 9)); C->SetRelativeLocation(FVector(0, 0, 20));
		const int32 NM = C->GetNumMaterials();
		for (int32 i = 0; i < NM; i++) C->SetMaterial(i, KAPVis::MakeMID(C, FKAPMat::RustySteel(FLinearColor(0.12f, 0.08f, 0.05f), 0.95f)));
		C->RegisterComponent();
		AddInstanceComponent(C);
		break;
	}
	case EStructType::Crate:
		{ FKAPMat Cw = Wood; Cw.Color = Color; Box(FVector(0, 0, 40), FVector(100, 70, 80), Cw); }
		Box(FVector(0, 0, 82), FVector(104, 74, 6), Dark, FRotator::ZeroRotator, false);
		break;
	case EStructType::Silo:
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 250), FVector(320, 320, 500), FKAPMat::Sheet(FLinearColor(0.72f, 0.72f, 0.7f), 0.5f, 0.7f));
		break;
	case EStructType::Workshop:
		BuildWorkshop(R, S.X > 2000.f);
		break;
	case EStructType::Nest:
	case EStructType::Bones:
		for (int32 i = 0; i < 14; i++) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(R.FRandRange(-1500, 1500), R.FRandRange(-1500, 1500), 8), FVector(10, 10, R.FRandRange(40, 90)), FKAPMat::Paint(FLinearColor(0.8f, 0.78f, 0.7f), 0.7f), FRotator(90, R.FRandRange(0, 180), 0), false);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, 0, 2), FVector(3200, 3200, 2), FKAPMat::Paint(FLinearColor(0.08f, 0.09f, 0.03f), 1.f), FRotator::ZeroRotator, false);
		break;
	}
}

void AKAPStructure::Tick(float Dt)
{
	Super::Tick(Dt);
	const float T = GetWorld()->GetTimeSeconds();
	if (bFire && Light)
	{
		Light->SetIntensity(17000 + FMath::FRand() * 7000);
		for (int32 i = 0; i < Flames.Num(); i++) Flames[i]->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.8f + 0.35f * FMath::Sin(T * 11 + i * 2.1f)));
	}
	if (Type == EStructType::Tower && Blinker) Blinker->SetVisibility(FMath::Fmod(T, 1.5f) < 0.75f);
	if (Flicks.Num() || Leds.Num() || BeaconL) TickLights(Dt);
	if (SwingBucket) SwingBucket->SetRelativeRotation(FRotator(0, 0, FMath::Sin(T * 1.7f) * 10.f + FMath::Sin(T * 0.6f) * 6.f));
}

bool AKAPStructure::IsIndoor(UWorld* W, const FVector& P)
{
	for (KAPReg::TIter<AKAPStructure> It(W, KAPReg::Structures()); It; ++It) for (const FBox2D& B : It->IndoorBoxes) if (B.IsInside(FVector2D(P.X, P.Y))) return true;
	return false;
}
bool AKAPStructure::IsShaded(UWorld* W, const FVector& P)
{
	for (KAPReg::TIter<AKAPStructure> It(W, KAPReg::Structures()); It; ++It) if (It->ShadeR > 0 && FVector::Dist2D(It->GetActorLocation(), P) < It->ShadeR * 0.6f) return true;
	return false;
}
bool AKAPStructure::IsNearFire(UWorld* W, const FVector& P)
{
	for (KAPReg::TIter<AKAPStructure> It(W, KAPReg::Structures()); It; ++It) if (It->bFire && FVector::Dist2D(It->GetActorLocation(), P) < 500.f) return true;
	return false;
}
const TArray<FVector2D>& AKAPStructure::Covers(UWorld* W)
{
	if (CoverCacheFrame == 0 || GFrameCounter - CoverCacheFrame > 600)
	{
		CoverCache.Reset();
		for (KAPReg::TIter<AKAPStructure> It(W, KAPReg::Structures()); It; ++It) CoverCache.Append(It->CoverPts);
		CoverCacheFrame = GFrameCounter;
	}
	return CoverCache;
}

// ---------------- AKAPInteractSpot ----------------
AKAPInteractSpot::AKAPInteractSpot()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}
void AKAPInteractSpot::BeginPlay() { Super::BeginPlay(); KAPReg::Add(KAPReg::Interactables(), (AActor*)this); }
FString AKAPInteractSpot::GetInteractText(AKAPCharacter*) const
{
	const AKAPGameMode* GM = AKAPGameMode::Get(this);
	return GM ? GM->SpotText(this) : FString();
}
void AKAPInteractSpot::Interact(AKAPCharacter*)
{
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->SpotUse(this);
}
