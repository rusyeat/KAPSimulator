// Ұңғыма цехы (кіруге болатын индустриялық ангар) + жыпылықтайтын шамдар + апат маягы мен сирена.
// Құрылымы: гофрленген қабырғалар, үлкен ашық қақпа, болат фермалар, құбыр тіректері, резервуарлар,
// сорғылар, басқару пульті (жыпылықтайтын индикаторлар), көпірше мен баспалдақ, ілулі шамдар.
#include "KAPStructure.h"
#include "KAPVisuals.h"
#include "WellManagerSubsystem.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void AKAPStructure::AddFlickLamp(FVector Pos, float Cd, bool bBad, bool bShadow, FLinearColor Col)
{
	const FKAPMat Shade = FKAPMat::RustySteel(FLinearColor(0.2f, 0.22f, 0.2f), 0.4f);
	KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Pos + FVector(0, 0, 60), FVector(2, 2, 120), Shade, FRotator::ZeroRotator, false);  // ілгіш
	KAPVis::Add(this, RootComponent, EKAPShape::Cone, Pos + FVector(0, 0, 4), FVector(60, 60, 26), Shade, FRotator::ZeroRotator, false);       // қалпақ
	UStaticMeshComponent* Bulb = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, Pos - FVector(0, 0, 8), FVector(16, 16, 12), FKAPMat::Glow(Col, 0), FRotator::ZeroRotator, false);
	Bulb->SetCastShadow(false);
	USpotLightComponent* L = NewObject<USpotLightComponent>(this);
	L->SetupAttachment(RootComponent); L->SetRelativeLocation(Pos - FVector(0, 0, 12)); L->SetRelativeRotation(FRotator(-90, 0, 0));
	L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(Cd);
	L->SetOuterConeAngle(62.f); L->SetInnerConeAngle(28.f); L->SetAttenuationRadius(1400.f);
	L->SetLightColor(Col); L->SetVolumetricScatteringIntensity(2.2f);
	L->SetCastShadows(bShadow); L->bCastVolumetricShadow = bShadow;
	L->RegisterComponent();
	FFlick F; F.L = L; F.Bulb = Bulb; F.Base = Cd; F.bBad = bBad; F.Col = Col; F.Next = FMath::FRandRange(0.5f, 4.f);
	Flicks.Add(F);
}

void AKAPStructure::AddAlarm(FVector Pos, float Yaw)
{
	KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Pos - FVector(0, 0, 10), FVector(26, 26, 10), FKAPMat::RustySteel(), FRotator::ZeroRotator, false);
	BeaconM = KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, Pos + FVector(0, 0, 8), FVector(20, 20, 26), FKAPMat::Glow(FLinearColor(1.f, 0.08f, 0.02f), 0), FRotator::ZeroRotator, false);
	BeaconM->SetCastShadow(false);
	BeaconL = NewObject<USpotLightComponent>(this);
	BeaconL->SetupAttachment(RootComponent); BeaconL->SetRelativeLocation(Pos + FVector(0, 0, 8)); BeaconL->SetRelativeRotation(FRotator(-10, Yaw, 0));
	BeaconL->SetIntensityUnits(ELightUnits::Candelas); BeaconL->SetIntensity(0.f);
	BeaconL->SetOuterConeAngle(30.f); BeaconL->SetInnerConeAngle(10.f); BeaconL->SetAttenuationRadius(3500.f);
	BeaconL->SetLightColor(FLinearColor(1.f, 0.06f, 0.02f)); BeaconL->SetVolumetricScatteringIntensity(3.f); BeaconL->SetCastShadows(false);
	BeaconL->RegisterComponent();
	if (UKAPAudio* Au = UKAPAudio::Get(this)) SirenW = Au->AttachSiren(BeaconM);
}

void AKAPStructure::BuildWorkshop(FRandomStream& R, bool bBig)
{
	const float L = bBig ? 2200.f : 1800.f, D = 1200.f, H = 700.f, T = 12.f;
	const FKAPMat Wall = FKAPMat::Sheet(FLinearColor(0.52f, 0.54f, 0.5f), 0.5f);
	const FKAPMat Roof = FKAPMat::Sheet(FLinearColor(0.38f, 0.37f, 0.35f), 0.6f);
	const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.28f, 0.3f, 0.32f), 0.45f);
	FKAPMat Yellow = FKAPMat::Paint(FLinearColor(0.75f, 0.55f, 0.05f), 0.6f); Yellow.Rust = 0.3f; Yellow.Grime = 0.7f;
	const FKAPMat Teal = FKAPMat::Sheet(FLinearColor(0.12f, 0.3f, 0.28f), 0.35f, 0.f);
	FKAPMat Floor = FKAPMat::Concrete(FLinearColor(0.2f, 0.195f, 0.185f)); Floor.DustTop = 0.25f; // майлы, күңгірт бетон
	const float Door = 650.f, DoorH = 520.f;
	// еден, қабырғалар (алдыңғы жақта үлкен қақпа), шатыр
	Box(FVector(0, 0, 6), FVector(L + 40, D + 40, 12), Floor);
	Box(FVector(0, -D / 2, H / 2), FVector(L, T, H), Wall);
	Box(FVector(-L / 2, 0, H / 2), FVector(T, D, H), Wall);
	Box(FVector(L / 2, 0, H / 2), FVector(T, D, H), Wall);
	const float SideW = (L - Door) / 2;
	Box(FVector(-L / 2 + SideW / 2, D / 2, H / 2), FVector(SideW, T, H), Wall);
	Box(FVector(L / 2 - SideW / 2, D / 2, H / 2), FVector(SideW, T, H), Wall);
	Box(FVector(0, D / 2, DoorH + (H - DoorH) / 2), FVector(Door, T, H - DoorH), Wall);
	Box(FVector(0, D / 2 + 20, DoorH + 30), FVector(Door + 40, 30, 50), Steel, FRotator::ZeroRotator, false);       // жиналмалы қақпа қорабы
	Box(FVector(0, D / 2 + 4, DoorH - 40), FVector(Door - 20, 6, 70), FKAPMat::Sheet(FLinearColor(0.45f, 0.1f, 0.06f), 0.6f), FRotator::ZeroRotator, false); // жартылай түскен қақпа
	for (float S : { -1.f, 1.f }) Box(FVector(S * (Door / 2 + 14), D / 2 + 10, DoorH / 2), FVector(24, 24, DoorH), FKAPMat::Hazard());
	Box(FVector(0, 0, H + 10), FVector(L + 60, D + 60, 20), Roof);
	// фермалар мен бағаналар
	for (float X = -L / 2 + 300; X < L / 2; X += 400)
	{
		Box(FVector(X, 0, H - 30), FVector(20, D - 20, 30), Steel, FRotator::ZeroRotator, false);
		for (float S : { -1.f, 1.f }) if (!(S > 0 && FMath::Abs(X) < Door / 2 + 30)) Box(FVector(X, S * (D / 2 - 20), H / 2), FVector(24, 24, H), Steel, FRotator::ZeroRotator, false); // қақпа ойығында бағана жоқ
		Box(FVector(X, -D / 4, H - 90), FVector(8, D / 2, 8), Steel, FRotator(0, 0, 12), false);
		Box(FVector(X, D / 4, H - 90), FVector(8, D / 2, 8), Steel, FRotator(0, 0, -12), false);
	}
	// жоғарғы терезелер жолағы (лас шыны)
	FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.06f, 0.07f, 0.07f), 0.15f); Glass.Grime = 0.3f;
	for (float X = -L / 2 + 200; X < L / 2 - 100; X += 300) Box(FVector(X, -D / 2 - 2, H - 140), FVector(200, 6, 90), Glass, FRotator::ZeroRotator, false);
	// құбыр тіректері артқы қабырға бойымен: үш құбыр, тіреулер, ысырмалар
	for (int32 k = 0; k < 3; k++)
	{
		const float Z = 160.f + k * 85.f, Dia = 34.f - k * 6.f;
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(0, -D / 2 + 70 + k * 6, Z), FVector(Dia, Dia, L - 120), k == 1 ? Yellow : Steel, FRotator(90, 0, 0), false);
		for (float X = -L / 2 + 250; X < L / 2 - 150; X += 500) KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(X, -D / 2 + 70 + k * 6, Z), FVector(Dia + 14, Dia + 14, 10), Steel, FRotator(90, 0, 0), false);
	}
	for (float X = -L / 2 + 150; X < L / 2; X += 500) Box(FVector(X, -D / 2 + 70, 210), FVector(10, 60, 420), Steel, FRotator::ZeroRotator, false);
	for (float X : { -L * 0.2f, L * 0.15f })
	{
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(X, -D / 2 + 120, 245), FVector(10, 10, 60), Steel, FRotator(0, 0, 90), false);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector(X, -D / 2 + 150, 245), FVector(56, 56, 6), FKAPMat::Paint(FLinearColor(0.6f, 0.05f, 0.03f), 0.5f), FRotator(0, 0, 90), false);
	}
	// тік резервуарлар (сатысымен)
	for (int32 i = 0; i < 2; i++)
	{
		const FVector C(-L / 2 + 260 + i * 330, -D / 2 + 330, 0);
		FKAPMat Tk = FKAPMat::Sheet(i ? FLinearColor(0.68f, 0.66f, 0.6f) : FLinearColor(0.3f, 0.36f, 0.42f), 0.45f, 0.f);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, C + FVector(0, 0, 250), FVector(260, 260, 460), Tk);
		KAPVis::Add(this, RootComponent, EKAPShape::Sphere, C + FVector(0, 0, 480), FVector(260, 260, 70), Tk, FRotator::ZeroRotator, false);
		for (float Z = 40; Z < 460; Z += 35) Box(C + FVector(0, 138, Z), FVector(40, 4, 4), Steel, FRotator::ZeroRotator, false);
		for (float S : { -1.f, 1.f }) Box(C + FVector(S * 20, 138, 250), FVector(4, 4, 460), Steel, FRotator::ZeroRotator, false);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, C + FVector(0, -150, 120), FVector(16, 16, 240), Steel, FRotator::ZeroRotator, false);
	}
	// сорғы агрегаттары
	for (int32 i = 0; i < 3; i++)
	{
		const FVector C(L * 0.05f + i * 260, -D / 2 + 300, 0);
		Box(C + FVector(0, 0, 25), FVector(200, 110, 30), Steel);
		Box(C + FVector(-40, 0, 85), FVector(100, 90, 90), Teal);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, C + FVector(55, 0, 80), FVector(70, 70, 90), FKAPMat::Paint(FLinearColor(0.15f, 0.2f, 0.32f), 0.5f), FRotator(90, 0, 0), false);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, C + FVector(-40, -70, 150), FVector(14, 14, 200), Steel, FRotator::ZeroRotator, false);
	}
	// басқару пульті: экран, батырмалар, жыпылықтайтын индикаторлар
	{
		const FVector C(L / 2 - 320, D / 2 - 220, 0);
		Box(C + FVector(0, 0, 55), FVector(320, 90, 110), FKAPMat::Sheet(FLinearColor(0.75f, 0.74f, 0.68f), 0.2f, 0.f));
		Box(C + FVector(0, -10, 125), FVector(320, 70, 30), FKAPMat::Paint(FLinearColor(0.7f, 0.69f, 0.64f), 0.5f), FRotator(0, 0, -25), false);
		Box(C + FVector(0, -40, 175), FVector(300, 12, 90), FKAPMat::Sheet(FLinearColor(0.72f, 0.71f, 0.66f), 0.2f, 0.f), FRotator::ZeroRotator, false);
		for (int32 s = 0; s < 3; s++) Box(C + FVector(-100 + s * 100, -32, 180), FVector(70, 3, 46), FKAPMat::Glow(FLinearColor(0.15f, 0.9f, 0.4f), 1.2f), FRotator::ZeroRotator, false);
		static const FLinearColor LedC[3] = { FLinearColor(1, 0.1f, 0.05f), FLinearColor(0.1f, 1, 0.2f), FLinearColor(1, 0.7f, 0.05f) };
		for (int32 i = 0; i < 18; i++)
		{
			UStaticMeshComponent* Led = KAPVis::Add(this, RootComponent, EKAPShape::Sphere, C + FVector(-140 + (i % 9) * 35, -20 + (i / 9) * 14, 132 + (i / 9) * 7), FVector(5, 5, 3), FKAPMat::Glow(LedC[i % 3], 25), FRotator::ZeroRotator, false);
			Led->SetCastShadow(false); Leds.Add(Led);
		}
		UTextRenderComponent* Tx = NewObject<UTextRenderComponent>(this);
		Tx->SetupAttachment(RootComponent); Tx->SetRelativeLocation(C + FVector(0, -48, 235)); Tx->SetRelativeRotation(FRotator(0, 90, 0));
		KAPVis::SignText(Tx); Tx->SetText(FText::FromString(TEXT("БАСҚАРУ ПУЛЬТІ · ҰҢҒЫМА ҚЫСЫМЫ"))); Tx->SetHorizontalAlignment(EHTA_Center); Tx->SetWorldSize(14); Tx->SetTextRenderColor(FColor(30, 30, 28)); Tx->RegisterComponent();
	}
	// көпірше (бүйір қабырға бойымен, 3 м биіктікте) + баспалдақ
	{
		const float Zc = 300.f, Xc = L / 2 - 160;
		Box(FVector(Xc, -D * 0.1f, Zc), FVector(200, D * 0.7f, 10), Steel);
		for (float Y : { -D * 0.45f, D * 0.25f }) for (float S : { -1.f, 1.f }) Box(FVector(Xc + S * 90, Y, Zc / 2), FVector(10, 10, Zc), Steel, FRotator::ZeroRotator, false);
		Box(FVector(Xc - 100, -D * 0.1f, Zc + 55), FVector(4, D * 0.7f, 4), Yellow, FRotator::ZeroRotator, false);
		Box(FVector(Xc - 100, -D * 0.1f, Zc + 100), FVector(5, D * 0.7f, 5), Yellow, FRotator::ZeroRotator, false);
		for (float Y = -D * 0.45f; Y <= D * 0.25f; Y += 120) Box(FVector(Xc - 100, Y, Zc + 50), FVector(4, 4, 100), Yellow, FRotator::ZeroRotator, false);
		// баспалдақ (14 саты, жоғарғысы көпірше деңгейінде)
		const float Ys = D * 0.2f;
		for (int32 s = 0; s < 14; s++) Box(FVector(Xc - 115 - (13 - s) * 28.f, Ys, 12 + s * 21.f), FVector(30, 110, 8), Steel, FRotator::ZeroRotator, true); // 21 см сатылар — кейіпкер адымдап көтеріледі
	}
	// жұмыс үстелі, жәшіктер, бөшкелер, сөрелер
	Box(FVector(-L / 2 + 160, D / 2 - 180, 85), FVector(220, 90, 8), FKAPMat::RustySteel(FLinearColor(0.3f, 0.3f, 0.28f), 0.3f));
	for (float S : { -1.f, 1.f }) Box(FVector(-L / 2 + 160 + S * 100, D / 2 - 180, 42), FVector(8, 80, 84), Steel, FRotator::ZeroRotator, false);
	Box(FVector(-L / 2 + 120, D / 2 - 190, 100), FVector(40, 24, 22), FKAPMat::Paint(FLinearColor(0.6f, 0.08f, 0.05f), 0.5f), FRotator(0, 20, 0), false);
	for (int32 i = 0; i < 2; i++)
	{
		const FVector C(-L / 2 + 80, -100 + i * 230, 0);
		for (int32 z = 0; z < 4; z++) Box(C + FVector(0, 0, 30 + z * 60), FVector(70, 200, 5), Steel, FRotator::ZeroRotator, false);
		for (float S : { -1.f, 1.f }) Box(C + FVector(0, S * 98, 110), FVector(6, 6, 220), Steel, FRotator::ZeroRotator, false);
		for (int32 z = 0; z < 4; z++) if (R.FRand() < 0.8f) Box(C + FVector(0, R.FRandRange(-60.f, 60.f), 50 + z * 60), FVector(50, 70, 36), FKAPMat::Paint(FLinearColor(0.35f, 0.26f, 0.15f), 0.85f), FRotator(0, R.FRandRange(-8.f, 8.f), 0), false);
	}
	for (int32 i = 0; i < 6; i++)
	{
		const FVector P(R.FRandRange(-L * 0.2f, L * 0.25f), D / 2 - R.FRandRange(150.f, 300.f), 0);
		FKAPMat B = FKAPMat::Sheet(i % 2 ? FLinearColor(0.1f, 0.2f, 0.42f) : FLinearColor(0.42f, 0.38f, 0.1f), 0.6f, 0.f);
		KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, P + FVector(0, 0, 45), FVector(58, 58, 88), B);
	}
	// еденнің сары қауіп сызықтары
	for (float S : { -1.f, 1.f }) Box(FVector(S * (Door / 2 + 40), D / 4, 13), FVector(14, D / 2, 2), FKAPMat::Hazard(), FRotator::ZeroRotator, false);
	Box(FVector(0, -D / 2 + 210, 13), FVector(L - 300, 14, 2), FKAPMat::Hazard(), FRotator::ZeroRotator, false);
	// жазулар
	for (const TPair<FVector, FString>& Tx : TArray<TPair<FVector, FString>>{ { FVector(0, D / 2 + 30, DoorH + 110), FString::Printf(TEXT("QazUran · ЦЕХ №%d"), R.RandRange(1, 9)) }, { FVector(-L * 0.3f, D / 2 + 8, 330), FString(TEXT("ҚАУІПСІЗДІК ТЕХНИКАСЫН САҚТАҢЫЗ")) } })
	{
		UTextRenderComponent* T2 = NewObject<UTextRenderComponent>(this);
		T2->SetupAttachment(RootComponent); T2->SetRelativeLocation(Tx.Key); T2->SetRelativeRotation(FRotator(0, 90, 0));
		T2->SetText(FText::FromString(Tx.Value)); T2->SetHorizontalAlignment(EHTA_Center); T2->SetWorldSize(Tx.Key.Z > 500 ? 60 : 26); T2->SetTextRenderColor(FColor(225, 215, 190)); T2->RegisterComponent();
	}
	// ілулі шамдар: екеуі ғана көлеңкелі (perf), біреуі «ақаулы» — жыпылықтайды
	int32 n = 0;
	for (float X = -L / 2 + 300; X < L / 2 - 100; X += 500)
		for (float Y : { -D * 0.18f, D * 0.2f })
		{
			const bool bBad = (n == 2) || R.FRand() < 0.12f;
			AddFlickLamp(FVector(X, Y, H - 140), 900.f, bBad, n < 2, FLinearColor(1.f, 0.86f, 0.62f));
			n++;
		}
	// апат маягы: қақпаның үстінде және ішкі қабырғада
	AddAlarm(FVector(Door / 2 + 80, D / 2 + 30, DoorH + 60), 90.f);
	// кіре берістегі сыртқы прожектор
	AddFlickLamp(FVector(-Door / 2 - 100, D / 2 + 90, DoorH + 40), 2500.f, false, false, FLinearColor(1.f, 0.75f, 0.45f));
	const FTransform Xf = GetActorTransform();
	const FVector A = Xf.TransformPosition(FVector(-L / 2, -D / 2, 0)), B = Xf.TransformPosition(FVector(L / 2, D / 2, 0));
	IndoorBoxes.Add(FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y))));
	CoverPts.Add(FVector2D(Xf.TransformPosition(FVector(0, -D / 2 + 300, 0))) / 100.0);
	ShadeR = L * 0.5f;
	SetActorTickInterval(0.f);
}

/** Жыпылықтау, индикаторлар, апат маягы мен сирена (Tick-тен) */
void AKAPStructure::TickLights(float Dt)
{
	const float T = GetWorld()->GetTimeSeconds();
	for (FFlick& F : Flicks)
	{
		float K = 1.f;
		if (F.bBad)
		{
			// ақаулы шам: кейде бірнеше рет сөніп-жанады, кейде ұзақ сөнеді
			F.Next -= Dt;
			if (F.Next <= 0) { F.Burst = FMath::FRandRange(0.15f, 1.2f); F.Next = FMath::FRandRange(1.5f, 7.f); }
			if (F.Burst > 0) { F.Burst -= Dt; K = FMath::FRand() < 0.5f ? 0.05f : FMath::FRandRange(0.4f, 1.f); }
		}
		else K = 0.97f + 0.03f * FMath::Sin(T * 314.f); // 50 Гц әлсіз діріл
		if (F.L) F.L->SetIntensity(F.Base * K);
		if (F.Bulb) if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(F.Bulb->GetMaterial(0))) M->SetVectorParameterValue("Emissive", F.Col * 30.f * K);
	}
	// пульт индикаторлары: әрқайсысы өз ырғағымен
	for (int32 i = 0; i < Leds.Num(); i++) Leds[i]->SetVisibility(FMath::Fmod(T * (0.7f + (i % 5) * 0.37f) + i * 0.61f, 1.f) < (bAlarm && i % 3 == 0 ? 0.5f : 0.75f));
	// апат: жақындағы ұңғымада ақау/өрт/ағу → маяк айналады, сирена
	AlarmCheck -= Dt;
	if (AlarmCheck <= 0)
	{
		AlarmCheck = 1.f;
		bool bA = false;
		if (UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>())
			if (const UKAPGameInstance* GI = UKAPGameInstance::Get(this))
				for (int32 i = 0; i < WM->Wells.Num() && i < GI->S.Wells.Num(); i++)
				{
					const EWellState S = GI->S.Wells[i].State;
					if ((S == EWellState::Fault || S == EWellState::Fire || S == EWellState::Leak) && FVector2D::Distance(WM->Wells[i].PosM(), FVector2D(GetActorLocation()) / 100.0) < AlarmRadiusM) bA = true;
				}
		bAlarm = bA;
	}
	if (BeaconL)
	{
		BeaconYaw = FMath::Fmod(BeaconYaw + Dt * 300.f, 360.f);
		BeaconL->SetRelativeRotation(FRotator(-12, BeaconYaw, 0));
		BeaconL->SetIntensity(bAlarm ? 9000.f : 0.f);
		if (UMaterialInstanceDynamic* M = Cast<UMaterialInstanceDynamic>(BeaconM->GetMaterial(0))) M->SetVectorParameterValue("Emissive", FLinearColor(1.f, 0.08f, 0.02f) * (bAlarm ? 20.f + 30.f * FMath::Max(0.f, FMath::Cos(FMath::DegreesToRadians(BeaconYaw - 90.f))) : 0.3f));
	}
	if (SirenW) SirenW->TargetGain = bAlarm ? 0.22f : 0.f;
}
