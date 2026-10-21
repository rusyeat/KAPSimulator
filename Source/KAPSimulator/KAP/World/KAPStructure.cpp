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
	BuildSize = S;
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
	case EStructType::Shop:
	case EStructType::Ruin:
	{
		const bool bRuin = Type == EStructType::Ruin;
		const bool bShop = Type == EStructType::Shop;
		const float W = S.X, D = S.Y, H = 320;
		FKAPMat M = bRuin ? FKAPMat::Concrete(FLinearColor(0.16f, 0.14f, 0.12f)) : FKAPMat::Concrete(Color * 1.1f);
		if (bRuin) M.Rust = 0.f;
		if (!bRuin)
		{
			// Ауыл үйі: сыланған/ақталған қабырға, қоңыр цоколь, қос беткейлі гофр шатыр (жотасы ұзын қабырға бойымен,
			// маңдайша үшбұрыштары қабырға түстес), көк/ақ жақтаулы терезелер, есік алдында бетон баспалдақ пен күнқағар.
			// bBright = ауласы бар (штакетник қоршау, қақпа, сарай) — ауыл үйлеріне
			static const FLinearColor RoofCols[6] = { FLinearColor(0.12f, 0.24f, 0.36f), FLinearColor(0.16f, 0.27f, 0.18f), FLinearColor(0.34f, 0.13f, 0.08f), FLinearColor(0.42f, 0.43f, 0.44f), FLinearColor(0.1f, 0.3f, 0.32f), FLinearColor(0.3f, 0.29f, 0.27f) };
			static const FLinearColor TrimCols[4] = { FLinearColor(0.08f, 0.22f, 0.5f), FLinearColor(0.82f, 0.82f, 0.78f), FLinearColor(0.1f, 0.35f, 0.3f), FLinearColor(0.45f, 0.18f, 0.08f) };
			RoofColor = RoofCols[R.RandRange(0, 5)];
			const FLinearColor Trim = TrimCols[R.RandRange(0, 3)];
			FKAPMat Plaster = FKAPMat::Concrete(Color * 1.12f); Plaster.Panel = 0.f; Plaster.VarAmt = 0.14f; Plaster.VarScale = 0.012f; Plaster.Grime = 0.65f; Plaster.Rust = 0.f;
			FKAPMat Plinth = FKAPMat::Concrete(FLinearColor(0.32f, 0.27f, 0.22f)); Plinth.Panel = 0.f;
			FKAPMat RoofM = FKAPMat::Sheet(RoofColor, 0.08f + R.FRand() * 0.15f, 1.f); RoofM.RibPeriod = 16.f; RoofM.Panel = 0.f; RoofM.DustTop = 0.7f; RoofM.VarAmt = 0.05f; RoofM.VarScale = 0.002f;
			const FKAPMat TrimM = FKAPMat::Paint(Trim, 0.6f);
			const float Pitch = FMath::DegreesToRadians(26.f), Hr = D * 0.5f * FMath::Tan(Pitch);
			Box(FVector(0, 0, H / 2), FVector(W, D, H), Plaster);
			Box(FVector(0, 0, 25), FVector(W + 10, D + 10, 50), Plinth, FRotator::ZeroRotator, false);
			// маңдайша (шатыр астын толтыратын екі көлбеу блок; төменгі бөлігі қабырға ішінде жасырын)
			const float Tk = Hr * 1.2f + 10.f;
			for (float Sg : { -1.f, 1.f })
			{
				const FVector Yax(0, FMath::Cos(Pitch), -Sg * FMath::Sin(Pitch)); // беткей бойымен (жотадан шетке +Sg)
				const FVector Up(0, Sg * FMath::Sin(Pitch), FMath::Cos(Pitch));     // беткейдің сыртқы нормалі
				const FRotator Rr = FRotationMatrix::MakeFromXY(FVector(1, 0, 0), Yax * Sg).Rotator();
				const FVector TopMid(0, Sg * D * 0.25f, H + Hr * 0.5f);
				Box(TopMid - Up * (Tk * 0.5f), FVector(W - 2.f, D * 0.5f / FMath::Cos(Pitch), Tk), Plaster, Rr, false);
				// шатыр табағы: шеттерінен 45 см шығып тұрады (күнқағар), маңдайшадан 35 см
				const FVector Down = FVector(0, Sg * FMath::Cos(Pitch), -FMath::Sin(Pitch));
				Box(TopMid + Down * 22.f + Up * 3.f, FVector(W + 70.f, D * 0.5f / FMath::Cos(Pitch) + 48.f, 6.f), RoofM, Rr, true);
			}
			Box(FVector(0, 0, H + Hr + 5), FVector(W + 74, 28, 9), FKAPMat::Sheet(RoofColor * 0.8f, 0.4f, 0.f), FRotator::ZeroRotator, false);
			// терезе: жақтау тақтасы + ішке батқан әйнек + терезе алды тақтасы; кейбірінде жапқыш
			auto Win = [&](FVector At, FVector Nrm, float Ww, float Wh)
			{
				const FVector Side = FVector::CrossProduct(FVector::UpVector, Nrm); // терезе ені бағыты
				const bool bAlongX = FMath::Abs(Side.X) > 0.5f;
				auto Sz = [&](float A, float T, float Hh) { return bAlongX ? FVector(A, T, Hh) : FVector(T, A, Hh); };
				Box(At + Nrm * 2.f, Sz(Ww + 18.f, 4.f, Wh + 18.f), TrimM, FRotator::ZeroRotator, false);
				Box(At + Nrm * 3.f, Sz(Ww, 4.f, Wh), Glass, FRotator::ZeroRotator, false);
				Box(At + Nrm * 8.f - FVector(0, 0, Wh * 0.5f + 12.f), Sz(Ww + 26.f, 14.f, 5.f), Frame, FRotator::ZeroRotator, false);
				if (R.FRand() < 0.45f) for (float Sh : { -1.f, 1.f }) Box(At + Nrm * 6.f + Side * Sh * (Ww * 0.5f + 26.f), Sz(30.f, 4.f, Wh + 8.f), TrimM, FRotator::ZeroRotator, false);
			};
			const float Wy = H * 0.58f;
			const float DoorX = bShop ? W * 0.28f : 0.f;
			if (bShop) Win(FVector(-W * 0.14f, D / 2, Wy - 10.f), FVector(0, 1, 0), 260.f, 140.f); // витрина
			else for (float Sx : { -1.f, 1.f }) Win(FVector(Sx * W * 0.3f, D / 2, Wy), FVector(0, 1, 0), 100.f, 110.f);
			for (float Sx : { -1.f, 1.f }) Win(FVector(Sx * W * 0.28f, -D / 2, Wy), FVector(0, -1, 0), 90.f, 100.f);
			for (float Sx : { -1.f, 1.f }) Win(FVector(Sx * W / 2, R.FRandRange(-D * 0.15f, D * 0.15f), Wy), FVector(Sx, 0, 0), 80.f, 95.f);
			// есік: жақтау, ағаш полотно, бетон баспалдақ, күнқағар
			Box(FVector(DoorX, D / 2 + 2, 104), FVector(118, 4, 218), TrimM, FRotator::ZeroRotator, false);
			Box(FVector(DoorX, D / 2 + 4, 100), FVector(96, 5, 200), Wood, FRotator::ZeroRotator, false);
			Box(FVector(DoorX + 36, D / 2 + 8, 100), FVector(6, 4, 4), Steel, FRotator::ZeroRotator, false);
			Box(FVector(DoorX, D / 2 + 45, 10), FVector(160, 80, 20), FKAPMat::Concrete(), FRotator::ZeroRotator, true);
			if (bShop) Box(FVector(0, D / 2 + 70, 236), FVector(W + 40, 150, 6), RoofM, FRotator(0, 0, 0), false); // дүкеннің ұзын күнқағары
			else Box(FVector(0, D / 2 + 55, 238), FVector(170, 110, 6), RoofM, FRotator(0, 0, 0), false);
			if (bShop) { Box(FVector(0, D / 2 + 4, 278), FVector(W * 0.85f, 6, 64), FKAPMat::Paint(FLinearColor(0.07f, 0.2f, 0.45f), 0.5f), FRotator::ZeroRotator, false); for (float Sx : { -1.f, 1.f }) Box(FVector(Sx * (W / 2 + 10), D / 2 + 140, 118), FVector(8, 8, 236), Steel, FRotator::ZeroRotator, false); }
			// мұржа, науа, кейде спутник тарелкасы
			Box(FVector(W * 0.22f, -D * 0.12f, H + Hr * 0.75f + 50.f), FVector(48, 48, Hr * 0.6f + 130.f), FKAPMat::Concrete(FLinearColor(0.42f, 0.36f, 0.3f)), FRotator::ZeroRotator, false);
			Box(FVector(W / 2 + 6, D / 2 + 6, H / 2), FVector(9, 9, H), FKAPMat::RustySteel(), FRotator::ZeroRotator, false);
			if (R.FRand() < 0.4f) KAPVis::Add(this, RootComponent, EKAPShape::Sphere, FVector(-W * 0.3f, D / 2 + 20, H - 30), FVector(80, 80, 18), FKAPMat::Paint(FLinearColor(0.78f, 0.78f, 0.75f), 0.5f), FRotator(70, 90, 0), false);
			if (bBright)
			{
				// аула: алдында 4 м, артында 9 м, бүйірінде 3 м; алдыңғы қоршауда есік (1.4 м) және көлік қақпасы (3 м)
				FKAPMat Picket = FKAPMat::Paint(R.FRand() < 0.5f ? FLinearColor(0.35f, 0.25f, 0.15f) : Trim * 0.9f, 0.85f); Picket.Ribs = 0.9f; Picket.RibPeriod = 11.f; Picket.Grime = 0.9f;
				const float Fx = W / 2 + 300.f, Yf = D / 2 + 400.f, Yb = -D / 2 - 900.f, Fh = 130.f;
				auto Panel = [&](FVector A, FVector B)
				{
					const FVector Mid = (A + B) * 0.5f, Dl = B - A;
					const bool bX = FMath::Abs(Dl.X) > FMath::Abs(Dl.Y);
					UStaticMeshComponent* C = Box(Mid + FVector(0, 0, Fh / 2), bX ? FVector(Dl.Size(), 4.f, Fh) : FVector(4.f, Dl.Size(), Fh), Picket, FRotator::ZeroRotator, true);
					// қоршау ойыншы мен көлікке кедергі, NPC-лер өтіп кетеді (жол табуы қоршауды білмейді)
					if (C) C->SetCollisionObjectType(ECC_Destructible);
				};
				Panel(FVector(-Fx, Yb, 0), FVector(Fx, Yb, 0));
				Panel(FVector(-Fx, Yb, 0), FVector(-Fx, Yf, 0));
				Panel(FVector(Fx, Yb, 0), FVector(Fx, Yf, 0));
				Panel(FVector(-Fx, Yf, 0), FVector(-70.f, Yf, 0));
				Panel(FVector(70.f, Yf, 0), FVector(Fx - 340.f, Yf, 0));
				for (float X : { -Fx, -70.f, 70.f, Fx - 340.f, Fx }) Box(FVector(X, Yf, Fh / 2 + 10), FVector(10, 10, Fh + 20), Wood, FRotator::ZeroRotator, false);
				// сарай: бір беткейлі шатыр
				const FVector Sh(-Fx + 230.f, Yb + 200.f, 0);
				Box(Sh + FVector(0, 0, 110), FVector(320, 260, 220), Wood);
				Box(Sh + FVector(0, 0, 232), FVector(350, 300, 6), FKAPMat::Sheet(FLinearColor(0.35f, 0.33f, 0.3f), 0.7f), FRotator(0, 0, 8), false);
				Props(FVector(Fx - 200.f, Yb + 260.f, 0), 90, 1);
				// жіп (кір кептіретін) — екі бағана мен сым
				Box(FVector(Fx - 120.f, -D / 2 - 300.f, 100), FVector(6, 6, 200), Steel, FRotator::ZeroRotator, false);
				Box(FVector(Fx - 520.f, -D / 2 - 300.f, 100), FVector(6, 6, 200), Steel, FRotator::ZeroRotator, false);
				Box(FVector(Fx - 320.f, -D / 2 - 300.f, 190), FVector(400, 1.5f, 1.5f), Steel, FRotator::ZeroRotator, false);
				Box(FVector(Fx - 260.f, -D / 2 - 300.f, 165), FVector(60, 2, 50), FKAPMat::Paint(Trim * 1.4f, 0.9f), FRotator::ZeroRotator, false);
			}
			else Props(FVector(W / 2 + 150, D / 4, 0), 120, 2);
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

FVector2D AKAPStructure::FootprintHalf() const
{
	const FVector& S = BuildSize;
	switch (Type)
	{
	case EStructType::Wagon: return FVector2D(440, 170);
	case EStructType::House: case EStructType::Ruin: case EStructType::Shop: return FVector2D(S.X * 0.5f + 30, S.Y * 0.5f + 30);
	case EStructType::Workshop: return FVector2D((S.X > 2000.f ? 2200.f : 1800.f) * 0.5f + 20, 620);
	case EStructType::HQOffice: return FVector2D(530, 430);
	case EStructType::Yurt: return FVector2D(320, 320);
	case EStructType::Tent: return FVector2D(240, 240);
	case EStructType::Campfire: return FVector2D(100, 100);
	case EStructType::Tower: return FVector2D(130, 130);
	case EStructType::Tank: return FVector2D(1650, 1020);
	case EStructType::Barrier: return FVector2D(S.X * 0.5f, 30);
	case EStructType::GuardPost: return FVector2D(120, 120);
	case EStructType::Fence: return FVector2D(S.X * 0.5f, 12);
	case EStructType::Gate: return FVector2D(420, 15);
	case EStructType::Wreck: return FVector2D(200, 110);
	case EStructType::Crate: return FVector2D(55, 40);
	case EStructType::Silo: return FVector2D(160, 160);
	default: return FVector2D::ZeroVector; // ұя, сүйектер — жалпақ, жолға кедергі емес
	}
}
FLinearColor AKAPStructure::MapColor() const
{
	switch (Type)
	{
	case EStructType::House: case EStructType::Shop: return RoofColor * 1.25f + FLinearColor(0.06f, 0.06f, 0.06f, 0.f);
	case EStructType::Ruin: return FLinearColor(0.3f, 0.26f, 0.22f);
	case EStructType::Wagon: return FLinearColor(0.5f, 0.48f, 0.44f);
	case EStructType::Workshop: return FLinearColor(0.55f, 0.56f, 0.58f);
	case EStructType::HQOffice: return FLinearColor(0.62f, 0.62f, 0.64f);
	case EStructType::GuardPost: return FLinearColor(0.5f, 0.45f, 0.35f);
	case EStructType::Yurt: return FLinearColor(0.9f, 0.86f, 0.76f);
	case EStructType::Silo: return FLinearColor(0.7f, 0.7f, 0.72f);
	case EStructType::Tent: return FLinearColor(0.35f, 0.37f, 0.24f);
	default: return FLinearColor(0.5f, 0.5f, 0.5f);
	}
}
