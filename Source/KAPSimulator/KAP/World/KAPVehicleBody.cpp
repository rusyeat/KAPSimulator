// Көлік корпустары: шаблон Chaos көлігінің рамасы жасырылып, оның орнына нақты дөңгелек орындарына сай
// кеңестік/қазақстандық техника құрастырылады — УАЗ-469, УАЗ-452 «Буханка», Нива, КамАЗ, су тасушы, БТР.
// Дөңгелектер мен суспензия — шын физикалық (айналады, серпіледі); корпус — пішіндерден, индустриялық материалмен.
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAPVisuals.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	struct FBody
	{
		AActor* Owner = nullptr;
		USceneComponent* Parent = nullptr;
		/** Шекаралары бойынша қорап (X0..X1, Y0..Y1, Z0..Z1) */
		UStaticMeshComponent* Box(float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FKAPMat& M, FRotator R = FRotator::ZeroRotator) const
		{
			UStaticMeshComponent* C = KAPVis::Add(Owner, Parent, EKAPShape::Cube, FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f), FVector(FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0), FMath::Abs(Z1 - Z0)), M, R, false);
			C->bCastDynamicShadow = true;
			return C;
		}
		/** Симметриялы жұп (±Y) */
		void Pair(float X0, float X1, float Yin, float Yout, float Z0, float Z1, const FKAPMat& M, FRotator R = FRotator::ZeroRotator) const
		{
			Box(X0, X1, Yin, Yout, Z0, Z1, M, R);
			Box(X0, X1, -Yout, -Yin, Z0, Z1, M, FRotator(R.Pitch, -R.Yaw, -R.Roll));
		}
		/** Цилиндр: осі X бойынша (фара, бөшке) немесе Y бойынша (дөңгелек) */
		UStaticMeshComponent* CylX(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, C, FVector(Dia, Dia, Len), M, FRotator(90, 0, 0), false); }
		UStaticMeshComponent* CylY(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, C, FVector(Dia, Dia, Len), M, FRotator(0, 0, 90), false); }
		UStaticMeshComponent* CylZ(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, C, FVector(Dia, Dia, Len), M, FRotator::ZeroRotator, false); }
	};
}

void AKAPVehicleHandle::BuildBody()
{
	bBodyBuilt = true;
	USkeletalMeshComponent* M = Pawn ? Pawn->GetMesh() : nullptr;
	UChaosWheeledVehicleMovementComponent* Mov = Pawn ? Cast<UChaosWheeledVehicleMovementComponent>(Pawn->GetChaosVehicleMovement()) : nullptr;
	if (!M || !Mov || Mov->WheelSetups.Num() < 4) { bBodyBuilt = false; return; }
	// ---- нақты дөңгелек геометриясы ----
	float Xf = -1e9f, Xr = 1e9f, Yw = 0.f, Zc = 0.f, R = 0.f;
	for (int32 i = 0; i < Mov->WheelSetups.Num(); i++)
	{
		const FVector P = M->GetBoneLocation(Mov->WheelSetups[i].BoneName, EBoneSpaces::ComponentSpace);
		Xf = FMath::Max(Xf, (float)P.X); Xr = FMath::Min(Xr, (float)P.X);
		Yw = FMath::Max(Yw, FMath::Abs((float)P.Y)); Zc += (float)P.Z / Mov->WheelSetups.Num();
		if (Mov->Wheels.IsValidIndex(i) && Mov->Wheels[i]) R = FMath::Max(R, Mov->Wheels[i]->WheelRadius);
	}
	if (R < 10.f) R = 40.f;
	UE_LOG(LogTemp, Display, TEXT("KAP body %s: Xf=%.0f Xr=%.0f Yw=%.0f Zc=%.0f R=%.0f"), *Id.ToString(), Xf, Xr, Yw, Zc, R);
	// ---- шаблон көлігі толық жасырылады (рама, суспензия, баггидің жалпақ дөңгелектері);
	//      физика сол күйі қалады, ал көрінетін дөңгелектер 23% ішке жылжып, нақты көлік еніне келеді ----
	UMaterialInterface* Inv = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Invisible.M_KAP_Invisible"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Inv) for (int32 s = 0; s < M->GetNumMaterials(); s++) M->SetMaterial(s, Inv);
	M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	{
		// шаблон blueprint-індегі бөлек мештер (доңғалақтар, қозғалтқыш) — біздің бөлшектер әлі жасалмаған, сондықтан бәрі шаблондікі
		TArray<UStaticMeshComponent*> Old; Pawn->GetComponents<UStaticMeshComponent>(Old);
		for (UStaticMeshComponent* C : Old) { C->SetVisibility(false); C->SetCastShadow(false); } // Chassis (SM_Offroad_Body), 4× SM_Offroad_Tire
	}
	// тегіс корпус мештері бар болса — жаңа құрастыру
	if (BuildShellBody(M, Xf, Xr, Yw, Zc, R)) return;
	const float Rv = R * 0.82f;
	VisK = 0.56f; VisDrop = R - Rv; // нақты УАЗ: дөңгелек аралығы ≈1.44 м, корпус ені ≈1.94 м
	{
		FKAPMat Tire = FKAPMat::Paint(FLinearColor(0.035f, 0.033f, 0.03f), 0.92f); Tire.DustTop = 0.9f; Tire.Grime = 0.8f;
		FKAPMat Rim = FKAPMat::Paint(Def.Color * 0.8f, 0.55f); Rim.Metallic = 0.4f; Rim.Rust = 0.35f; Rim.Grime = 0.8f;
		const FKAPMat Hub = FKAPMat::RustySteel(FLinearColor(0.3f, 0.3f, 0.28f), 0.4f);
		for (int32 i = 0; i < Mov->WheelSetups.Num(); i++)
		{
			const FName Bone = Mov->WheelSetups[i].BoneName;
			const FTransform T = M->GetSocketTransform(Bone, RTS_Component);
			const float Out = T.GetLocation().Y >= 0 ? 1.f : -1.f;
			USceneComponent* Pv = NewObject<USceneComponent>(Pawn);
			Pv->SetupAttachment(M); Pv->RegisterComponent();
			KAPVis::Add(Pawn, Pv, EKAPShape::Cylinder, FVector::ZeroVector, FVector(2 * Rv, 2 * Rv, 25), Tire, FRotator(0, 0, 90), false);
			for (int32 k = 0; k < 14; k++)
			{
				// протектор блоктары — айналу анық көрінеді
				const float A = k / 14.f * UE_TWO_PI;
				KAPVis::Add(Pawn, Pv, EKAPShape::Cube, FVector(FMath::Cos(A) * (Rv - 0.5f), 0, FMath::Sin(A) * (Rv - 0.5f)), FVector(7, 26, 3.5f), Tire, FRotator(-FMath::RadiansToDegrees(A) + 90.f, 0, 0), false);
			}
			KAPVis::Add(Pawn, Pv, EKAPShape::Cylinder, FVector(0, Out * 11.5f, 0), FVector(Rv * 1.15f, Rv * 1.15f, 4), Rim, FRotator(0, 0, 90), false);
			KAPVis::Add(Pawn, Pv, EKAPShape::Cylinder, FVector(0, Out * 14.f, 0), FVector(Rv * 0.38f, Rv * 0.38f, 3), Hub, FRotator(0, 0, 90), false);
			for (int32 k = 0; k < 5; k++)
			{
				const float A = k / 5.f * UE_TWO_PI;
				KAPVis::Add(Pawn, Pv, EKAPShape::Cube, FVector(FMath::Cos(A) * Rv * 0.28f, Out * 15.6f, FMath::Sin(A) * Rv * 0.28f), FVector(2.6f, 2, 2.6f), Hub, FRotator::ZeroRotator, false);
			}
			WheelBones.Add(Bone); WheelRest.Add(T.GetRotation()); WheelPivots.Add(Pv);
		}
		UpdateVisualWheels();
	}
	// корпус енді көрінетін геометрия бойынша құрастырылады
	Yw *= VisK; Zc -= VisDrop; R = Rv;


	FBody B; B.Owner = Pawn; B.Parent = M;
	const float WB = Xf - Xr;                 // база
	const float W = 2.f * Yw + 36.f;          // корпус ені: дөңгелектер қанат астында
	const float Hw = W * 0.5f;
	const float Z0 = Zc + R * 0.1f;           // еден
	const float Zb = Zc + R * 1.3f;           // белдеу сызығы
	const FLinearColor Col = Def.Color;
	FKAPMat Paint = FKAPMat::Paint(Col, 0.62f); Paint.Grime = 0.35f; Paint.Rust = 0.12f; Paint.DustTop = 0.9f; Paint.Panel = 0.4f; Paint.PanelW = 90.f; Paint.Metallic = 0.25f;
	FKAPMat Dark = FKAPMat::Paint(FLinearColor(0.025f, 0.025f, 0.022f), 0.8f); Dark.Grime = 0.6f;
	FKAPMat Rubber = FKAPMat::Paint(FLinearColor(0.03f, 0.028f, 0.025f), 0.95f); Rubber.DustTop = 0.8f;
	FKAPMat Chrome = FKAPMat::Metal(FLinearColor(0.7f, 0.7f, 0.68f), 0.25f); Chrome.Rust = 0.15f;
	FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.2f, 0.2f, 0.19f), 0.45f);
	FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.05f, 0.06f, 0.065f), 0.05f); Glass.DustTop = 0.5f; Glass.Grime = 0.15f;
	FKAPMat Canvas = FKAPMat::Paint(Col * 0.7f, 0.97f); Canvas.Ribs = 0.25f; Canvas.RibPeriod = 35.f; Canvas.Grime = 0.9f; Canvas.DustTop = 1.f;
	const FKAPMat LensH = FKAPMat::Glow(FLinearColor(1.f, 0.92f, 0.75f), 0);
	const FKAPMat LensT = FKAPMat::Glow(FLinearColor(1.f, 0.05f, 0.02f), 0);
	auto Fenders = [&](float Zt, const FKAPMat& Mt)
	{
		// доңғалақ қаптамалары: доға пішіні үш қораппен
		for (float Xa : { Xf, Xr })
		{
			B.Pair(Xa - R * 1.05f, Xa + R * 1.05f, Hw - 30.f, Hw + 6.f, Zt - 6.f, Zt, Mt);
			B.Pair(Xa + R * 0.95f, Xa + R * 1.25f, Hw - 30.f, Hw + 4.f, Zt - R * 0.6f, Zt, Mt, FRotator(-30, 0, 0));
			B.Pair(Xa - R * 1.25f, Xa - R * 0.95f, Hw - 30.f, Hw + 4.f, Zt - R * 0.6f, Zt, Mt, FRotator(30, 0, 0));
		}
	};
	auto RoundLights = [&](float Xx, float Yy, float Zz, float Dia)
	{
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			B.CylX(FVector(Xx, Sg * Yy, Zz), Dia + 6.f, 6.f, Chrome);
			UStaticMeshComponent* L = B.CylX(FVector(Xx + 3.5f, Sg * Yy, Zz), Dia, 3.f, LensH);
			L->SetCastShadow(false);
			HeadLens.Add(L); HeadLocal[s] = FVector(Xx + 4.f, Sg * Yy, Zz);
		}
	};
	auto TailLights = [&](float Xx, float Yy, float Zz)
	{
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			UStaticMeshComponent* L = B.Box(Xx - 2.f, Xx, Sg * Yy - 8.f, Sg * Yy + 8.f, Zz - 6.f, Zz + 6.f, LensT);
			L->SetCastShadow(false);
			TailLens.Add(L); TailLocal[s] = FVector(Xx - 2.f, Sg * Yy, Zz);
		}
	};
	auto Bumper = [&](float Xx, float Zz, bool bFront)
	{
		B.Box(Xx - 7.f, Xx + 7.f, -Hw - 8.f, Hw + 8.f, Zz - 8.f, Zz + 8.f, Steel);
		if (bFront) { B.Pair(Xx - 2.f, Xx + 10.f, Hw * 0.55f, Hw * 0.55f + 8.f, Zz + 4.f, Zz + 14.f, Steel); }
	};
	auto Mirrors = [&](float Xx, float Zz)
	{
		B.Pair(Xx - 2.f, Xx + 2.f, Hw, Hw + 18.f, Zz, Zz + 2.f, Steel);
		B.Pair(Xx - 3.f, Xx + 1.f, Hw + 14.f, Hw + 26.f, Zz - 2.f, Zz + 14.f, Dark);
	};
	auto Plate = [&](float Xx, float Zz, bool bFront)
	{
		B.Box(Xx - 1.f, Xx + 1.f, -26.f, 26.f, Zz - 6.f, Zz + 6.f, FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.8f), 0.5f));
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
		T->SetupAttachment(M); T->SetRelativeLocation(FVector(Xx + (bFront ? 1.5f : -1.5f), 0, Zz - 5.f)); T->SetRelativeRotation(FRotator(0, bFront ? 0.f : 180.f, 0));
		T->SetText(FText::FromString(FString::Printf(TEXT("%03d KZ"), (GetTypeHash(Id) % 900) + 100))); T->SetHorizontalAlignment(EHTA_Center);
		T->SetWorldSize(9.f); T->SetTextRenderColor(FColor(20, 20, 20)); T->RegisterComponent();
	};

	const FString K = Id.ToString();
	if (K == TEXT("uaz469"))
	{
		// УАЗ-469: тік бұрышты «козлик», брезент шатыр, дөңгелек фаралар, тік саңылаулы радиатор
		const float Xfr = Xf + R * 1.55f, Xbk = Xr - R * 1.45f, Xw = Xf - R * 1.25f, Zh = Zb + 6.f, Zr = Zb + 88.f;
		B.Box(Xbk, Xfr - 8.f, -Hw + 6.f, Hw - 6.f, Z0, Zb, Paint);                 // корпус
		B.Box(Xw, Xfr, -Hw + 14.f, Hw - 14.f, Zb - 4.f, Zh, Paint, FRotator(-1.5f, 0, 0)); // капот
		Fenders(Zb - 2.f, Paint);
		B.Box(Xfr - 6.f, Xfr, -Hw + 22.f, Hw - 22.f, Z0 + 8.f, Zh, Paint);          // бет панелі
		for (int32 i = 0; i < 7; i++) { const float Y = -42.f + i * 14.f; B.Box(Xfr - 1.f, Xfr + 1.f, Y - 3.f, Y + 3.f, Zb - 32.f, Zh - 6.f, Dark); }
		RoundLights(Xfr, Hw - 36.f, Zb - 18.f, 20.f);
		B.Pair(Xfr - 1.f, Xfr + 1.f, Hw - 22.f, Hw - 16.f, Zb - 4.f, Zb + 2.f, FKAPMat::Glow(FLinearColor(1.f, 0.5f, 0.05f), 2.f)); // бұрылыс шамдары
		Bumper(Xfr + 10.f, Z0 + 4.f, true);
		Bumper(Xbk - 8.f, Z0 + 4.f, false);
		Plate(Xfr + 18.f, Z0 + 4.f, true);
		// әйнек жақтауы мен әйнек
		const FRotator Lean(12, 0, 0); // әйнек артқа қарай көлбеу
		B.Box(Xw - 3.f, Xw + 3.f, -Hw + 10.f, Hw - 10.f, Zh, Zh + 4.f, Paint);
		B.Pair(Xw - 3.f, Xw + 3.f, Hw - 14.f, Hw - 10.f, Zh, Zh + 56.f, Paint, Lean);
		B.Box(Xw - 6.f, Xw, -Hw + 14.f, Hw - 14.f, Zh + 52.f, Zh + 58.f, Paint, Lean);
		B.Box(Xw - 2.f, Xw + 1.f, -Hw + 14.f, Hw - 14.f, Zh + 4.f, Zh + 52.f, Glass, Lean);
		B.Box(Xw + 2.f, Xw + 4.f, -30.f, 30.f, Zh + 50.f, Zh + 52.f, Dark, Lean); // тазалағыштар
		// брезент шатыр (қабырғалары, артқы және бүйір терезелері)
		B.Box(Xbk + 6.f, Xw - 8.f, -Hw + 4.f, Hw - 4.f, Zr - 6.f, Zr, Canvas);
		B.Pair(Xbk + 6.f, Xw - 8.f, Hw - 8.f, Hw - 4.f, Zb, Zr - 4.f, Canvas);
		B.Box(Xbk + 4.f, Xbk + 8.f, -Hw + 6.f, Hw - 6.f, Zb, Zr - 4.f, Canvas);
		B.Pair(Xw - 70.f, Xw - 14.f, Hw - 4.f, Hw - 2.f, Zb + 16.f, Zr - 16.f, Glass);
		B.Pair(Xbk + 30.f, Xw - 90.f, Hw - 4.f, Hw - 2.f, Zb + 16.f, Zr - 16.f, Glass);
		B.Box(Xbk + 2.f, Xbk + 4.f, -32.f, 32.f, Zb + 22.f, Zr - 18.f, Glass);
		// есік жіктері, тұтқалар, баспалдақ
		for (float Xd : { Xw - 4.f, Xw - 82.f, Xbk + 26.f }) B.Pair(Xd - 1.f, Xd + 1.f, Hw - 6.f, Hw - 5.f, Z0 + 6.f, Zb - 2.f, Dark);
		B.Pair(Xw - 22.f, Xw - 10.f, Hw - 6.f, Hw - 3.f, Zb - 14.f, Zb - 10.f, Chrome);
		B.Pair(Xr + R * 1.2f, Xf - R * 1.2f, Hw - 10.f, Hw + 2.f, Z0 - 6.f, Z0 - 2.f, Steel);
		Mirrors(Xw - 4.f, Zh + 30.f);
		// артта қосалқы дөңгелек және канистр
		B.CylX(FVector(Xbk - 14.f, 0, Zb + 4.f), R * 1.85f, 24.f, Rubber);
		B.CylX(FVector(Xbk - 27.f, 0, Zb + 4.f), R * 0.9f, 4.f, Steel);
		B.Box(Xbk + 20.f, Xbk + 40.f, Hw - 4.f, Hw + 12.f, Z0 + 8.f, Z0 + 52.f, FKAPMat::Paint(FLinearColor(0.18f, 0.22f, 0.12f), 0.7f));
		TailLights(Xbk - 1.f, Hw - 14.f, Zb - 10.f);
		B.Box(Xr - 10.f, Xr + 10.f, -60.f, 60.f, Z0 - R * 0.35f, Z0 - 6.f, Steel); // артқы көпір
	}
	else if (K == TEXT("bukhanka"))
	{
		// УАЗ-452 «Буханка» (4.36×1.94×2.06 м): жоспарда дөңгеленген «нан» пішіні, ішке қарай сәл көлбеген жоғарғы бөлік,
		// екі бөлікті алдыңғы әйнек, жоғары орналасқан дөңгелек фаралар, доғалы доңғалақ ойықтары, екі түсті бояу
		const float Gz = Zc - R;                                    // жер деңгейі
		const float Xfr = Xf + R * 1.3f, Xbk = Xr - R * 1.55f;
		const float Zl = Zc + R * 0.15f, Zm = Gz + 112.f, Zr = Gz + 204.f;
		auto RBox = [&](float X0, float X1, float Y0, float Y1, float Za, float Zb2, float Rr, const FKAPMat& Mt)
		{
			B.Box(X0 + Rr, X1 - Rr, Y0, Y1, Za, Zb2, Mt);
			B.Box(X0, X1, Y0 + Rr, Y1 - Rr, Za, Zb2, Mt);
			for (float Cx : { X0 + Rr, X1 - Rr }) for (float Cy : { Y0 + Rr, Y1 - Rr }) B.CylZ(FVector(Cx, Cy, (Za + Zb2) * 0.5f), Rr * 2.f, Zb2 - Za, Mt);
		};
		FKAPMat Top = FKAPMat::Paint(FLinearColor(0.78f, 0.76f, 0.68f), 0.6f); Top.Grime = 0.5f; Top.Rust = 0.12f; Top.DustTop = 0.95f; Top.Metallic = 0.25f;
		// төменгі бөлік (бояу), жоғарғы бөлік (ақшыл) — классикалық екі түс
		RBox(Xbk, Xfr, -Hw, Hw, Zl, Zm, 26.f, Paint);
		RBox(Xbk + 4.f, Xfr - 8.f, -Hw + 4.f, Hw - 4.f, Zm, Zr - 18.f, 28.f, Top);
		RBox(Xbk + 16.f, Xfr - 24.f, -Hw + 18.f, Hw - 18.f, Zr - 20.f, Zr, 30.f, Top);
		for (float Sg : { -1.f, 1.f }) KAPVis::Add(Pawn, M, EKAPShape::Cylinder, FVector((Xbk + Xfr) * 0.5f - 4.f, Sg * (Hw - 18.f), Zr - 20.f), FVector(28.f, 28.f, Xfr - Xbk - 60.f), Top, FRotator(90, 0, 0), false);
		for (float Xe : { Xbk + 16.f, Xfr - 24.f }) KAPVis::Add(Pawn, M, EKAPShape::Cylinder, FVector(Xe, 0, Zr - 20.f), FVector(28.f, 28.f, W - 60.f), Top, FRotator(0, 0, 90), false);
		B.Pair(Xbk + 6.f, Xfr - 6.f, Hw - 1.f, Hw + 1.5f, Zm - 3.f, Zm + 3.f, Steel);  // белдеу молдинг
		// доңғалақ ойықтары: бүйірдегі қараңғы доғалар
		for (float Xa : { Xf, Xr }) for (float Sg : { -1.f, 1.f }) B.CylY(FVector(Xa, Sg * (Hw + 0.6f), Zc + R * 0.12f), R * 2.25f, 1.2f, Dark);
		// алдыңғы бет: екі бөлікті әйнек, ортаңғы тірек, тазалағыштар
		for (float Sg : { -1.f, 1.f })
		{
			B.Box(Xfr - 15.f, Xfr - 11.f, Sg > 0 ? 4.f : -Hw + 30.f, Sg > 0 ? Hw - 30.f : -4.f, Zm + 8.f, Zr - 34.f, Glass, FRotator(-7, 0, 0));
			B.Box(Xfr - 10.f, Xfr - 9.f, Sg * 26.f - 14.f, Sg * 26.f + 14.f, Zm + 12.f, Zm + 14.f, Dark, FRotator(-30, 0, Sg * 20.f));
		}
		B.Box(Xfr - 13.f, Xfr - 9.f, -4.f, 4.f, Zm + 6.f, Zr - 30.f, Top, FRotator(-7, 0, 0));
		RoundLights(Xfr + 0.5f, Hw - 30.f, Zm - 24.f, 18.f);
		B.Pair(Xfr - 1.f, Xfr + 1.f, Hw - 30.f - 6.f, Hw - 30.f + 6.f, Zm - 6.f, Zm - 1.f, FKAPMat::Glow(FLinearColor(1.f, 0.5f, 0.05f), 1.5f));
		for (int32 i = 0; i < 5; i++) { const float Z = Zm - 54.f + i * 6.f; B.Box(Xfr - 1.f, Xfr + 1.5f, -30.f, 30.f, Z - 1.6f, Z + 1.6f, Dark); }
		B.Box(Xfr - 1.f, Xfr + 1.f, -38.f, 38.f, Zm - 58.f, Zm - 22.f, Paint);
		Bumper(Xfr + 8.f, Zl + 6.f, true);
		Bumper(Xbk - 6.f, Zl + 6.f, false);
		Plate(Xfr + 16.f, Zl + 6.f, true);
		Mirrors(Xfr - 30.f, Zm + 16.f);
		// бүйір терезелері (жақтаулы), есіктер, тұтқалар
		const float Wx0 = Xbk + 22.f, Wx1 = Xfr - 70.f;
		for (int32 i = 0; i < 3; i++)
		{
			const float A0 = FMath::Lerp(Wx0, Wx1, i / 3.f) + 6.f, A1 = FMath::Lerp(Wx0, Wx1, (i + 1) / 3.f) - 6.f;
			B.Pair(A0, A1, Hw - 4.5f, Hw - 2.f, Zm + 18.f, Zr - 36.f, Glass);
			B.Pair(A0 - 3.f, A1 + 3.f, Hw - 5.f, Hw - 3.f, Zm + 15.f, Zr - 33.f, Dark);
		}
		B.Pair(Xfr - 62.f, Xfr - 20.f, Hw - 4.5f, Hw - 2.f, Zm + 18.f, Zr - 36.f, Glass); // жүргізуші есігі
		for (float Xd : { Xfr - 66.f, Xfr - 16.f, Xbk + 120.f, Xbk + 20.f }) B.Pair(Xd - 0.8f, Xd + 0.8f, Hw - 1.f, Hw + 0.5f, Zl + 6.f, Zr - 24.f, Dark);
		B.Pair(Xfr - 30.f, Xfr - 22.f, Hw, Hw + 2.f, Zm - 8.f, Zm - 5.f, Chrome);
		B.Pair(Xbk + 110.f, Xbk + 118.f, Hw, Hw + 2.f, Zm - 8.f, Zm - 5.f, Chrome);
		// артқы есіктер: екі жарты, терезелер, тігінен шамдар, баспалдақ
		B.Box(Xbk - 1.5f, Xbk + 1.f, -1.f, 1.f, Zl + 6.f, Zr - 26.f, Dark);
		for (float Sg : { -1.f, 1.f }) B.Box(Xbk - 2.f, Xbk + 1.f, Sg > 0 ? 8.f : -Hw + 34.f, Sg > 0 ? Hw - 34.f : -8.f, Zm + 18.f, Zr - 38.f, Glass);
		TailLights(Xbk - 1.f, Hw - 18.f, Zm - 20.f);
		B.Box(Xbk - 12.f, Xbk - 2.f, -24.f, 24.f, Zl - 4.f, Zl + 2.f, Steel);
		// шатыр торы, антенна
		for (float Xr2 : { Xbk + 40.f, (Xbk + Xfr) * 0.5f, Xfr - 60.f }) B.Box(Xr2 - 2.f, Xr2 + 2.f, -Hw + 26.f, Hw - 26.f, Zr, Zr + 8.f, Steel);
		B.Pair(Xbk + 40.f, Xfr - 60.f, Hw - 28.f, Hw - 24.f, Zr + 8.f, Zr + 11.f, Steel);
		B.CylZ(FVector(Xfr - 40.f, Hw - 24.f, Zr + 40.f), 1.2f, 80.f, Steel);
		// табалдырық пен балшық қалқандары
		B.Pair(Xf - R * 1.15f, Xr + R * 1.15f, Hw - 12.f, Hw + 1.f, Zl - 4.f, Zl + 2.f, Steel);
		for (float Xa : { Xf - R * 1.2f, Xr - R * 1.2f }) B.Pair(Xa - 1.f, Xa + 1.f, Hw - 22.f, Hw - 2.f, Gz + 18.f, Zl, Dark);
	}
	else if (K == TEXT("niva"))
	{
		// ВАЗ-2121 Нива: төмен корпус, қисық әйнек, тікбұрышты фаралар
		const float Xfr = Xf + R * 1.35f, Xbk = Xr - R * 1.05f, Xw = Xf - R * 0.9f, Zr = Zb + 66.f;
		B.Box(Xbk, Xfr - 6.f, -Hw + 10.f, Hw - 10.f, Z0, Zb, Paint);
		B.Box(Xw, Xfr, -Hw + 14.f, Hw - 14.f, Zb - 6.f, Zb + 4.f, Paint, FRotator(-3, 0, 0));
		Fenders(Zb - 4.f, Paint);
		B.Box(Xbk + 4.f, Xw - 30.f, -Hw + 16.f, Hw - 16.f, Zr - 5.f, Zr, Paint);
		B.Box(Xw - 34.f, Xw + 6.f, -Hw + 18.f, Hw - 18.f, Zb + 2.f, Zr - 2.f, Glass, FRotator(-35, 0, 0));
		B.Pair(Xbk + 6.f, Xw - 36.f, Hw - 18.f, Hw - 14.f, Zb + 2.f, Zr - 4.f, Glass);
		for (float Xp : { Xbk + 6.f, (Xbk + Xw) * 0.5f - 14.f, Xw - 34.f }) B.Pair(Xp - 3.f, Xp + 3.f, Hw - 19.f, Hw - 13.f, Zb, Zr - 2.f, Paint);
		B.Box(Xbk, Xbk + 6.f, -Hw + 16.f, Hw - 16.f, Zb, Zr - 3.f, Glass, FRotator(12, 0, 0));
		B.Box(Xfr - 2.f, Xfr + 1.f, -38.f, 38.f, Zb - 26.f, Zb - 6.f, Dark);
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			UStaticMeshComponent* L = B.Box(Xfr, Xfr + 2.f, Sg * (Hw - 34.f) - 11.f, Sg * (Hw - 34.f) + 11.f, Zb - 24.f, Zb - 8.f, LensH);
			L->SetCastShadow(false); HeadLens.Add(L); HeadLocal[s] = FVector(Xfr + 3.f, Sg * (Hw - 34.f), Zb - 16.f);
		}
		Bumper(Xfr + 8.f, Z0 + 6.f, false); Bumper(Xbk - 6.f, Z0 + 6.f, false);
		Plate(Xfr + 16.f, Z0 + 6.f, true);
		Mirrors(Xw - 20.f, Zb + 14.f);
		TailLights(Xbk - 1.f, Hw - 22.f, Zb - 14.f);
		B.CylX(FVector(Xbk - 14.f, 0, Zb - 2.f), R * 1.7f, 20.f, Rubber);
	}
	else if (K == TEXT("kamaz") || K == TEXT("water"))
	{
		// КамАЗ-5320 / су тасушы: биік кабина, алдыңғы тор, рама + борт немесе цистерна
		const bool bTank = K == TEXT("water");
		const float Xfr = Xf + R * 1.45f, Xc = Xf - R * 0.75f, Xbk = Xr - R * 1.9f, Zf = Zc + R * 0.55f, Zcab = Zb + 165.f;
		FKAPMat CabP = Paint; CabP.Panel = 0.6f;
		B.Box(Xbk, Xfr - 20.f, -40.f, 40.f, Zf - 14.f, Zf, Steel);                              // рама
		B.Box(Xc, Xfr, -Hw + 4.f, Hw - 4.f, Zf, Zcab - 10.f, CabP);                               // кабина
		B.Box(Xc + 4.f, Xfr - 4.f, -Hw + 12.f, Hw - 12.f, Zcab - 12.f, Zcab, CabP);
		B.Box(Xfr - 2.f, Xfr + 2.f, -Hw + 18.f, Hw - 18.f, Zb + 70.f, Zcab - 22.f, Glass, FRotator(-4, 0, 0));
		B.Pair(Xc + 30.f, Xfr - 14.f, Hw - 5.f, Hw - 3.f, Zb + 70.f, Zcab - 26.f, Glass);
		for (int32 i = 0; i < 6; i++) { const float Z = Zf + 30.f + i * 9.f; B.Box(Xfr, Xfr + 2.f, -Hw + 40.f, Hw - 40.f, Z - 2.5f, Z + 2.5f, Dark); }
		B.Box(Xc - 2.f, Xc + 2.f, -Hw + 6.f, Hw - 6.f, Zf, Zcab - 10.f, Dark);
		Fenders(Zf + R * 0.75f, Steel);
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			UStaticMeshComponent* L = B.Box(Xfr + 2.f, Xfr + 4.f, Sg * (Hw - 26.f) - 12.f, Sg * (Hw - 26.f) + 12.f, Zf + 4.f, Zf + 18.f, LensH);
			L->SetCastShadow(false); HeadLens.Add(L); HeadLocal[s] = FVector(Xfr + 5.f, Sg * (Hw - 26.f), Zf + 11.f);
		}
		Bumper(Xfr + 10.f, Zf - 4.f, false);
		Plate(Xfr + 18.f, Zf - 4.f, true);
		Mirrors(Xfr - 10.f, Zb + 100.f);
		B.Pair(Xc - 6.f, Xc + 30.f, Hw - 10.f, Hw + 4.f, Zf - 20.f, Zf + 10.f, FKAPMat::RustySteel()); // жанармай багы
		B.Box(Xc - 30.f, Xc - 12.f, -Hw + 10.f, -Hw + 26.f, Zf, Zcab + 40.f, Steel);              // түтін құбыры
		if (bTank)
		{
			FKAPMat TankM = FKAPMat::Metal(FLinearColor(0.62f, 0.64f, 0.66f), 0.45f); TankM.Rust = 0.35f; TankM.Grime = 0.8f; TankM.DustTop = 0.9f;
			B.CylX(FVector((Xbk + Xc) * 0.5f - 6.f, 0, Zf + 78.f), W - 30.f, Xc - Xbk - 20.f, TankM);
			for (float Xr2 : { Xbk + 30.f, (Xbk + Xc) * 0.5f, Xc - 40.f }) B.CylX(FVector(Xr2, 0, Zf + 78.f), W - 22.f, 8.f, Steel);
			B.CylZ(FVector((Xbk + Xc) * 0.5f, 0, Zf + 78.f + (W - 30.f) * 0.5f), 50.f, 12.f, Steel);
			UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
			T->SetupAttachment(M); T->SetRelativeLocation(FVector((Xbk + Xc) * 0.5f, Hw - 12.f, Zf + 78.f)); T->SetRelativeRotation(FRotator(0, 90, 0));
			KAPVis::SignText(T); T->SetText(FText::FromString(TEXT("СУ / ВОДА"))); T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(32.f); T->SetTextRenderColor(FColor(40, 60, 140)); T->RegisterComponent();
		}
		else
		{
			FKAPMat Planks = FKAPMat::Paint(Col * 0.85f, 0.85f); Planks.Ribs = 0.6f; Planks.RibPeriod = 20.f; Planks.Panel = 1.f; Planks.PanelW = 60.f; Planks.Rust = 0.25f; Planks.Grime = 0.9f;
			B.Box(Xbk, Xc - 10.f, -Hw - 4.f, Hw + 4.f, Zf, Zf + 8.f, Steel);
			B.Pair(Xbk, Xc - 10.f, Hw, Hw + 4.f, Zf + 8.f, Zf + 70.f, Planks);
			B.Box(Xbk, Xbk + 4.f, -Hw, Hw, Zf + 8.f, Zf + 70.f, Planks);
			B.Box(Xc - 14.f, Xc - 10.f, -Hw, Hw, Zf + 8.f, Zf + 90.f, Planks);
			for (int32 i = 0; i < 3; i++) B.Box(Xbk + 30.f + i * 70.f, Xbk + 80.f + i * 70.f, -50.f, 30.f, Zf + 8.f, Zf + 48.f, FKAPMat::Paint(FLinearColor(0.32f, 0.22f, 0.12f), 0.85f)); // жәшіктер
		}
		TailLights(Xbk - 1.f, Hw - 14.f, Zf - 4.f);
		// артқы қосымша көпір: декоративті айналатын дөңгелектер
		const float Xa = Xr - R * 2.3f;
		FakeR = R;
		for (float Sg : { -1.f, 1.f })
		{
			UStaticMeshComponent* Wh = B.CylY(FVector(Xa, Sg * (Yw - 4.f), Zc), R * 2.f, 30.f, Rubber);
			for (int32 j = 0; j < 3; j++)
			{
				KAPVis::Add(Pawn, Wh, EKAPShape::Cube, FVector(0, 0, -52.f * Sg), FVector(80.f, 10.f, 6.f), Steel, FRotator(0, j * 60.f, 0), false);
			}
			FakeWheels.Add(Wh);
		}
		B.Pair(Xa - R * 1.1f, Xa + R * 1.1f, Hw - 30.f, Hw + 6.f, Zc + R + 4.f, Zc + R + 10.f, Steel);
	}
	else // btr
	{
		// БТР-80 тектес бронетранспортер: көлбеу сауыт тақталары, мұнара, пулемет
		FKAPMat Armor = Paint; Armor.Panel = 0.8f; Armor.PanelW = 70.f; Armor.Rust = 0.3f;
		const float Xfr = Xf + R * 1.9f, Xbk = Xr - R * 2.6f, Zt = Zb + 50.f;
		B.Box(Xbk + 30.f, Xfr - 60.f, -Hw - 6.f, Hw + 6.f, Z0, Zt, Armor);
		B.Box(Xfr - 90.f, Xfr, -Hw + 6.f, Hw - 6.f, Z0 + 10.f, Zt - 6.f, Armor, FRotator(-28, 0, 0));
		B.Box(Xfr - 30.f, Xfr + 14.f, -Hw + 10.f, Hw - 10.f, Z0 - 6.f, Z0 + 36.f, Armor, FRotator(38, 0, 0));
		B.Box(Xbk, Xbk + 50.f, -Hw + 4.f, Hw - 4.f, Z0 + 8.f, Zt - 4.f, Armor, FRotator(22, 0, 0));
		B.Pair(Xbk + 30.f, Xfr - 60.f, Hw + 2.f, Hw + 14.f, Zb, Zt - 4.f, Armor, FRotator(0, 0, 18));
		B.CylZ(FVector(Xf - R * 1.4f, 0, Zt + 14.f), 110.f, 30.f, Armor);
		B.CylZ(FVector(Xf - R * 1.4f, 0, Zt + 34.f), 80.f, 12.f, Armor);
		B.CylX(FVector(Xf - R * 1.4f + 120.f, 0, Zt + 22.f), 9.f, 170.f, Steel);
		B.CylX(FVector(Xf - R * 1.4f + 60.f, 18.f, Zt + 20.f), 6.f, 70.f, Steel);
		for (int32 i = 0; i < 3; i++) B.Pair(Xbk + 120.f + i * 70.f, Xbk + 150.f + i * 70.f, Hw + 8.f, Hw + 12.f, Zb + 10.f, Zb + 24.f, Dark); // атыс саңылаулары
		B.Box(Xfr - 70.f, Xfr - 40.f, -Hw + 30.f, Hw - 30.f, Zt - 10.f, Zt - 2.f, Glass, FRotator(-28, 0, 0));
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			B.Box(Xfr - 40.f, Xfr - 24.f, Sg * (Hw - 20.f) - 10.f, Sg * (Hw - 20.f) + 10.f, Zb, Zb + 14.f, Steel);
			UStaticMeshComponent* L = B.CylX(FVector(Xfr - 22.f, Sg * (Hw - 20.f), Zb + 7.f), 14.f, 3.f, LensH);
			L->SetCastShadow(false); HeadLens.Add(L); HeadLocal[s] = FVector(Xfr - 18.f, Sg * (Hw - 20.f), Zb + 7.f);
		}
		TailLights(Xbk + 2.f, Hw - 20.f, Zb + 4.f);
		// 8 доңғалақ: 4 шын + 4 декоративті (айналады)
		FakeR = R;
		for (float Xa : { Xr - R * 2.3f, Xf + R * 0.0f - WB * 0.5f })
			for (float Sg : { -1.f, 1.f })
			{
				UStaticMeshComponent* Wh = B.CylY(FVector(Xa, Sg * (Yw - 4.f), Zc), R * 2.f, 32.f, Rubber);
				for (int32 j = 0; j < 3; j++) KAPVis::Add(Pawn, Wh, EKAPShape::Cube, FVector(0, 0, -52.f * Sg), FVector(80.f, 10.f, 6.f), Steel, FRotator(0, j * 60.f, 0), false);
				FakeWheels.Add(Wh);
			}
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
		T->SetupAttachment(M); T->SetRelativeLocation(FVector((Xbk + Xfr) * 0.5f, Hw + 15.f, Zb + 30.f)); T->SetRelativeRotation(FRotator(0, 90, 0));
		T->SetText(FText::FromString(TEXT("217"))); T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(30.f); T->SetTextRenderColor(FColor(220, 220, 210)); T->RegisterComponent();
	}
	// декор бөлшектері көлеңке есептеуді ауырлатпасын: ұсақтарына Lumen/VSM көлеңкесі қажет емес
	TArray<USceneComponent*> Kids; M->GetChildrenComponents(true, Kids);
	for (USceneComponent* C : Kids)
		if (UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
		{
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (S->Bounds.SphereRadius < 25.f) S->SetCastShadow(false);
		}
}
