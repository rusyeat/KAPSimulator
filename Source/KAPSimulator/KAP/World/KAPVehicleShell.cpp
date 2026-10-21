// Тегіс көлік корпустары: редактор генераторы жасаған SM_KAP_Veh_<id>_<аймақ> мештері (SDF → Nanite),
// нақты пропорциядағы дөңгелектер (шина + диск мештері) және ұсақ бөлшектер (фара, бампер, айна, рама).
// Барлық координата — корпус мешінің жүйесінде: X=0 — база ортасы, Z=0 — жер (KAPVehicleSpec.h).
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAPVisuals.h"
#include "KAPVehicleSpec.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "ChaosWheeledVehicleMovementComponent.h"

namespace
{
	UStaticMesh* LoadSM(const FString& N)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/KAP/Meshes/%s.%s"), *N, *N), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	/** Корпус жүйесіндегі бөлшектер (O — мештің компоненттегі орны) */
	struct FPart
	{
		AActor* Owner = nullptr;
		USceneComponent* Parent = nullptr;
		FVector O = FVector::ZeroVector;
		UStaticMeshComponent* Box(float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FKAPMat& M, FRotator R = FRotator::ZeroRotator) const
		{
			return KAPVis::Add(Owner, Parent, EKAPShape::Cube, O + FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, (Z0 + Z1) * 0.5f), FVector(FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0), FMath::Abs(Z1 - Z0)), M, R, false);
		}
		/** ±Y жұбы (Y0..Y1 — оң жақ) */
		void Pair(float X0, float X1, float Y0, float Y1, float Z0, float Z1, const FKAPMat& M, FRotator R = FRotator::ZeroRotator) const
		{
			Box(X0, X1, Y0, Y1, Z0, Z1, M, R);
			Box(X0, X1, -Y1, -Y0, Z0, Z1, M, FRotator(R.Pitch, -R.Yaw, -R.Roll));
		}
		UStaticMeshComponent* CylX(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, O + C, FVector(Dia, Dia, Len), M, FRotator(90, 0, 0), false); }
		UStaticMeshComponent* CylY(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, O + C, FVector(Dia, Dia, Len), M, FRotator(0, 0, 90), false); }
		UStaticMeshComponent* CylZ(FVector C, float Dia, float Len, const FKAPMat& M) const { return KAPVis::Add(Owner, Parent, EKAPShape::Cylinder, O + C, FVector(Dia, Dia, Len), M, FRotator::ZeroRotator, false); }
	};
}

bool AKAPVehicleHandle::BuildShellBody(USkeletalMeshComponent* M, float Xf, float Xr, float Yw, float Zc, float Rphys)
{
	const FString K = Id.ToString();
	const FKAPVehSpec* Sp = KAPVehSpec::Find(K);
	if (!Sp) return false;
	const FString ShellId = K == TEXT("water") ? FString(TEXT("kamaz")) : K;
	UStaticMesh* Sh[KAPVehSpec::NumRegions] = {};
	bool bAny = false;
	for (int32 r = 0; r < KAPVehSpec::NumRegions; r++) { Sh[r] = LoadSM(FString::Printf(TEXT("SM_KAP_Veh_%s_%d"), *ShellId, r)); bAny |= Sh[r] != nullptr; }
	UStaticMesh* TireM = LoadSM(TEXT("SM_KAP_Tire"));
	UStaticMesh* RimM = LoadSM(TEXT("SM_KAP_Rim"));
	if (!bAny || !TireM || !RimM) return false;

	// ---- көрінетін геометрия: нақты көліктің базасы, ізі, дөңгелек радиусы ----
	const float Xm = (Xf + Xr) * 0.5f;
	const float Gz = Zc - Rphys;                 // жер деңгейі (компонент кеңістігінде)
	const float R = Sp->R, Tw = Sp->TireW, Tr = Sp->Track;
	VisKX = Sp->WB / FMath::Max(1.f, Xf - Xr); VisXc = Xm;
	VisK = Tr / FMath::Max(1.f, Yw);
	VisDrop = Zc - (Gz + R);
	FakeR = R;
	FPart B; B.Owner = Pawn; B.Parent = M; B.O = FVector(Xm, 0, Gz);
	UE_LOG(LogTemp, Display, TEXT("KAP shell %s: WB %.0f R %.0f track %.0f (phys WB %.0f)"), *K, Sp->WB, R, Tr, Xf - Xr);

	// ---- материалдар ----
	const FLinearColor Col = Def.Color;
	FKAPMat Paint = FKAPMat::Paint(Col, 0.48f); Paint.Grime = 0.22f; Paint.Rust = 0.08f; Paint.DustTop = 0.85f; Paint.Metallic = 0.3f; Paint.VarAmt = 0.07f;
	FKAPMat Dark = FKAPMat::Paint(FLinearColor(0.022f, 0.022f, 0.02f), 0.82f); Dark.Grime = 0.55f; Dark.DustTop = 0.5f;
	FKAPMat Rubber = FKAPMat::Paint(FLinearColor(0.03f, 0.028f, 0.025f), 0.93f); Rubber.DustTop = 0.85f; Rubber.Grime = 0.8f;
	FKAPMat Chrome = FKAPMat::Metal(FLinearColor(0.72f, 0.72f, 0.7f), 0.22f); Chrome.Rust = 0.12f; Chrome.Grime = 0.3f;
	FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.2f, 0.2f, 0.19f), 0.45f);
	FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.035f, 0.045f, 0.05f), 0.04f); Glass.DustTop = 0.12f; Glass.Grime = 0.06f; Glass.VarAmt = 0.f;
	const FKAPMat LensH = FKAPMat::Glow(FLinearColor(1.f, 0.92f, 0.75f), 0);
	const FKAPMat LensT = FKAPMat::Glow(FLinearColor(1.f, 0.05f, 0.02f), 0);
	const FKAPMat Amber = FKAPMat::Glow(FLinearColor(1.f, 0.45f, 0.04f), 1.5f);
	FKAPMat Second = Paint;
	if (K == TEXT("uaz469")) { Second = FKAPMat::Paint(Col * 0.72f, 0.96f); Second.Ribs = 0.12f; Second.RibPeriod = 40.f; Second.Grime = 0.9f; Second.DustTop = 1.f; }
	if (K == TEXT("bukhanka")) { Second = FKAPMat::Paint(FLinearColor(0.78f, 0.76f, 0.68f), 0.5f); Second.Grime = 0.3f; Second.Rust = 0.08f; Second.DustTop = 0.9f; Second.Metallic = 0.3f; }
	FKAPMat Trim = K == TEXT("bukhanka") ? Chrome : Steel;

	// ---- корпус ----
	const FKAPMat RegM[KAPVehSpec::NumRegions] = { Paint, Second, Glass, Dark, Trim };
	for (int32 r = 0; r < KAPVehSpec::NumRegions; r++)
		if (Sh[r])
		{
			UStaticMeshComponent* C = KAPVis::AddMesh(Pawn, M, Sh[r], B.O, FVector::OneVector, RegM[r]);
			C->bCastDynamicShadow = r != KAPVehSpec::Glass;
			C->SetCastShadow(r != KAPVehSpec::Glass && r != KAPVehSpec::Trim);
		}
	if (K == TEXT("water"))
	{
		FKAPMat TankM = FKAPMat::Metal(FLinearColor(0.6f, 0.62f, 0.63f), 0.52f); TankM.Rust = 0.22f; TankM.Grime = 0.35f; TankM.DustTop = 0.9f;
		for (int32 r = 0; r < KAPVehSpec::NumRegions; r++)
			if (UStaticMesh* T = LoadSM(FString::Printf(TEXT("SM_KAP_Veh_watertank_%d"), r)))
				KAPVis::AddMesh(Pawn, M, T, B.O, FVector::OneVector, r == KAPVehSpec::Trim ? Steel : TankM);
	}

	// ---- дөңгелектер: шина (протектор бұдырларымен) + диск, сол жақтағысы айналдырылған ----
	FKAPMat Tire = FKAPMat::Paint(FLinearColor(0.032f, 0.03f, 0.028f), 0.9f); Tire.DustTop = 0.9f; Tire.Grime = 0.85f; Tire.VarAmt = 0.04f;
	FKAPMat RimMat = K == TEXT("btr") || K == TEXT("kamaz") || K == TEXT("water") ? FKAPMat::Paint(Col * 0.85f, 0.55f) : FKAPMat::Paint(FLinearColor(0.16f, 0.17f, 0.15f), 0.5f);
	RimMat.Metallic = 0.45f; RimMat.Rust = 0.35f; RimMat.Grime = 0.8f;
	const FVector WScale(2.f * R / 100.f, Tw / 100.f, 2.f * R / 100.f);
	auto MakeWheel = [&](USceneComponent* Pv, float Out)
	{
		KAPVis::AddMesh(Pawn, Pv, TireM, FVector::ZeroVector, WScale, Tire)->bCastDynamicShadow = true;
		KAPVis::AddMesh(Pawn, Pv, RimM, FVector::ZeroVector, WScale, RimMat, FRotator(0, Out > 0 ? 0.f : 180.f, 0));
	};
	{
		UChaosWheeledVehicleMovementComponent* Mov = Cast<UChaosWheeledVehicleMovementComponent>(Pawn->GetChaosVehicleMovement());
		for (int32 i = 0; Mov && i < Mov->WheelSetups.Num(); i++)
		{
			const FName Bone = Mov->WheelSetups[i].BoneName;
			const FTransform T = M->GetSocketTransform(Bone, RTS_Component);
			USceneComponent* Pv = NewObject<USceneComponent>(Pawn);
			Pv->SetupAttachment(M); Pv->RegisterComponent();
			MakeWheel(Pv, T.GetLocation().Y >= 0 ? 1.f : -1.f);
			WheelBones.Add(Bone); WheelRest.Add(T.GetRotation()); WheelPivots.Add(Pv);
		}
		UpdateVisualWheels();
	}
	auto FakeWheel = [&](float X)
	{
		for (float Sg : { -1.f, 1.f })
		{
			USceneComponent* Pv = NewObject<USceneComponent>(Pawn);
			Pv->SetupAttachment(M); Pv->SetRelativeLocation(B.O + FVector(X, Sg * Tr, R)); Pv->RegisterComponent();
			MakeWheel(Pv, Sg);
			FakePivots.Add(Pv);
		}
	};

	// ---- ортақ бөлшектер ----
	auto Head = [&](int32 s, FVector P, float Dia, bool bRound)
	{
		if (bRound) { B.CylX(P - FVector(1.5f, 0, 0), Dia + 5.f, 4.f, Chrome); }
		UStaticMeshComponent* L = bRound ? B.CylX(P + FVector(0.8f, 0, 0), Dia, 2.f, LensH) : B.Box(P.X - 1.f, P.X + 1.f, P.Y - Dia * 0.5f, P.Y + Dia * 0.5f, P.Z - Dia * 0.32f, P.Z + Dia * 0.32f, LensH);
		L->SetCastShadow(false);
		HeadLens.Add(L); HeadLocal[s] = B.O + P + FVector(2.f, 0, 0);
	};
	auto Tail = [&](float X, float Y, float Z, float W, float H)
	{
		for (int32 s = 0; s < 2; s++)
		{
			const float Sg = s == 0 ? -1.f : 1.f;
			UStaticMeshComponent* L = B.Box(X - 1.5f, X, Sg * Y - W * 0.5f, Sg * Y + W * 0.5f, Z - H * 0.5f, Z + H * 0.5f, LensT);
			L->SetCastShadow(false);
			TailLens.Add(L); TailLocal[s] = B.O + FVector(X - 1.5f, Sg * Y, Z);
		}
	};
	auto Plate = [&](float X, float Z, bool bFront)
	{
		B.Box(X - 0.8f, X + 0.8f, -26.f, 26.f, Z - 6.f, Z + 6.f, FKAPMat::Paint(FLinearColor(0.82f, 0.82f, 0.78f), 0.5f));
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
		T->SetupAttachment(M); T->SetRelativeLocation(B.O + FVector(X + (bFront ? 1.2f : -1.2f), 0, Z - 5.f)); T->SetRelativeRotation(FRotator(0, bFront ? 0.f : 180.f, 0));
		T->SetText(FText::FromString(FString::Printf(TEXT("%03d KZ"), (GetTypeHash(Id) % 900) + 100))); T->SetHorizontalAlignment(EHTA_Center);
		T->SetWorldSize(9.f); T->SetTextRenderColor(FColor(20, 20, 20)); T->RegisterComponent();
	};
	auto Mirror = [&](float X, float Yb, float Z, float Arm, float Hh)
	{
		B.Pair(X - 1.f, X + 1.f, Yb, Yb + Arm, Z - 1.f, Z + 1.f, Steel);
		B.Pair(X - 3.f, X + 0.5f, Yb + Arm - 5.f, Yb + Arm + 7.f, Z - Hh * 0.5f, Z + Hh * 0.5f, Dark);
	};
	auto Underbody = [&](float X0, float X1, float Zr, float Hw)
	{
		B.Pair(X0, X1, Hw - 6.f, Hw + 6.f, Zr - 6.f, Zr + 6.f, Steel);                                      // рама
		const float Xa = Sp->WB * 0.5f;
		for (float Xx : { Xa, -Xa }) B.Box(Xx - 6.f, Xx + 6.f, -Tr + Tw * 0.5f + 2.f, Tr - Tw * 0.5f - 2.f, R - 6.f, R + 6.f, Steel); // көпірлер
		for (float Xx : { Xa, -Xa }) B.CylY(FVector(Xx, 0, R), 24.f, 24.f, Steel);                         // редуктор
	};

	if (K == TEXT("uaz469"))
	{
		for (int32 s = 0; s < 2; s++) Head(s, FVector(192.5f, (s ? 1.f : -1.f) * 52.f, 94.f), 17.f, true);
		B.Pair(191.5f, 193.5f, 57.f, 63.f, 106.f, 110.f, Amber);                                                  // габарит/бұрылыс
		B.Box(194, 204, -88, 88, 52, 64, Steel);                                                                    // бампер
		B.Pair(200, 210, 40, 46, 56, 60, Steel);                                                                    // сүйрегіш ілмектер
		B.Box(-205, -197, -86, 86, 50, 62, Steel);
		Plate(204.8f, 58.f, true);
		Mirror(52.f, 80.f, 150.f, 14.f, 13.f);
		B.Pair(57.5f, 59.5f, 10.f, 46.f, 124.f, 126.f, Dark, FRotator(0, 0, 8));                                    // тазалағыштар
		for (float Xh : { 8.f, -56.f }) B.Pair(Xh - 7.f, Xh + 7.f, 85.f, 86.5f, 106.f, 109.f, Chrome);              // тұтқалар
		B.Pair(-92, 46, 80, 90, 43, 46, Steel);                                                                     // баспалдақ
		// артта қосалқы дөңгелек пен канистр
		KAPVis::AddMesh(Pawn, M, TireM, B.O + FVector(-210.f, -26.f, 102.f), WScale, Tire, FRotator(0, 90, 0));
		KAPVis::AddMesh(Pawn, M, RimM, B.O + FVector(-210.f, -26.f, 102.f), WScale, RimMat, FRotator(0, -90, 0));
		B.Box(-206, -197, 32, 60, 66, 112, FKAPMat::Paint(FLinearColor(0.16f, 0.2f, 0.11f), 0.7f));
		Tail(-195.3f, 70.f, 96.f, 10.f, 12.f);
		Underbody(-185, 180, 46, 42);
		B.Box(80, 170, -38, 38, 50, 68, Dark);                                                                      // қозғалтқыш асты
		B.CylZ(FVector(-185.f, 74.f, 238.f), 1.2f, 75.f, Steel);                                                    // антенна
	}
	else if (K == TEXT("bukhanka"))
	{
		for (int32 s = 0; s < 2; s++) Head(s, FVector(215.6f, (s ? 1.f : -1.f) * 70.f, 98.f), 17.f, true);
		B.Pair(215.f, 217.f, 74.f, 82.f, 82.f, 88.f, Amber);
		B.Box(214, 224, -96, 96, 46, 58, Steel);
		B.Box(-230, -222, -94, 94, 46, 58, Steel);
		Plate(224.8f, 52.f, true);
		Mirror(192.f, 94.f, 150.f, 16.f, 16.f);
		for (float Y : { -26.f, 26.f }) B.Box(205.f, 207.f, Y - 20.f, Y + 20.f, 126.f, 128.f, Dark, FRotator(-9, 0, 0));
		B.Pair(126, 136, 97, 98.6f, 102, 105, Chrome);
		B.Box(14, 24, 97, 98.6f, 102, 105, Chrome);
		B.Box(-224.5f, -222.f, 6.f, 14.f, 100.f, 104.f, Chrome);
		Tail(-222.3f, 82.f, 80.f, 7.f, 22.f);
		for (float Xr2 : { -170.f, -40.f, 90.f }) B.Box(Xr2 - 2.f, Xr2 + 2.f, -72, 72, 205.5f, 211.f, Steel);       // шатыр торы
		B.Pair(-170, 90, 70, 74, 211, 214, Steel);
		B.CylZ(FVector(180.f, 72.f, 238.f), 1.2f, 70.f, Steel);
		B.Pair(140, 186, 92, 99, 44, 47, Steel);                                                                    // баспалдақ
		Underbody(-200, 200, 48, 44);
		B.CylX(FVector(-150.f, -50.f, 38.f), 7.f, 120.f, Steel);                                                    // түтін құбыры
	}
	else if (K == TEXT("niva"))
	{
		for (int32 s = 0; s < 2; s++) { const float Sg = s ? 1.f : -1.f; B.Box(183.f, 184.5f, Sg * 60.f - 13.f, Sg * 60.f + 13.f, 70.f, 86.f, Chrome); Head(s, FVector(185.f, Sg * 60.f, 78.f), 22.f, false); }
		B.Pair(184.f, 185.6f, 52.f, 68.f, 61.f, 66.f, Amber);
		B.Box(184, 194, -86, 86, 44, 56, Chrome);
		B.Box(-200, -190, -86, 86, 44, 56, Chrome);
		Plate(194.8f, 50.f, true);
		Mirror(50.f, 79.f, 112.f, 10.f, 11.f);
		for (float Y : { -24.f, 24.f }) B.Box(54.f, 56.f, Y - 18.f, Y + 18.f, 104.f, 106.f, Dark, FRotator(-38, 0, 0));
		B.Pair(-42, -30, 84, 85.5f, 94, 97, Chrome);
		Tail(-190.4f, 64.f, 88.f, 18.f, 12.f);
		Underbody(-170, 170, 36, 40);
		B.CylX(FVector(-150.f, -40.f, 26.f), 6.f, 80.f, Steel);
	}
	else if (K == TEXT("kamaz") || K == TEXT("water"))
	{
		const bool bTank = K == TEXT("water");
		const float Xa = Sp->WB * 0.5f, Xb = -Xa - 132.f;                                                           // артқы арба: екі көпір
		FakeWheel(Xb);
		B.Pair(bTank ? -352.f : -410.f, 272.f, 32.f, 44.f, 78.f, 100.f, Steel);                                                                  // рама
		B.Box(276, 296, -124, 124, 70, 106, Steel);                                                                 // бампер
		for (int32 s = 0; s < 2; s++) { const float Sg = s ? 1.f : -1.f; B.Box(295.f, 296.5f, Sg * 95.f - 16.f, Sg * 95.f + 16.f, 80.f, 96.f, Dark); Head(s, FVector(297.f, Sg * 95.f, 88.f), 26.f, false); }
		Plate(296.8f, 88.f, true);
		B.Pair(196, 252, 118, 130, 100, 104, Steel); B.Pair(196, 252, 118, 130, 74, 78, Steel);                    // баспалдақтар
		Mirror(272.f, 124.f, 252.f, 22.f, 44.f);
		B.Box(282, 298, -112, 112, 300, 304, Paint, FRotator(-6, 0, 0));                                            // күн қалқаны
		for (float Y : { -24.f, 0.f, 24.f }) B.Box(278.f, 284.f, Y - 5.f, Y + 5.f, 312.f, 317.f, Amber);
		for (float Y : { -50.f, 50.f }) B.Box(281.f, 283.f, Y - 30.f, Y + 30.f, 212.f, 214.f, Dark, FRotator(0, 0, 6));
		B.Pair(10, 84, 92, 124, 88, 130, FKAPMat::RustySteel());                                                    // жанармай багы
		B.CylZ(FVector(78.f, -112.f, 252.f), 12.f, 200.f, Steel);                                                   // түтін құбыры
		B.CylZ(FVector(78.f, 112.f, 200.f), 32.f, 80.f, Dark);                                                      // ауа сүзгісі
		B.Pair(-318, -118, 88, 122, 112, 116, Steel);                                                               // артқы қанат
		B.Pair(-346, -343, 86, 120, 34, 102, Dark);                                                                 // балшық қалқаны
		const float Xe = bTank ? -356.f : -414.f;
		B.Box(Xe, Xe + 8.f, -112, 112, 90, 104, Steel);
		Tail(Xe - 0.2f, 96.f, 97.f, 16.f, 10.f);
		for (float Xx : { Xa, -Xa, Xb }) B.Box(Xx - 7.f, Xx + 7.f, -Tr + Tw * 0.5f + 2.f, Tr - Tw * 0.5f - 2.f, R - 7.f, R + 7.f, Steel);
		if (bTank)
		{
			for (float Xs : { -262.f, -130.f, 2.f }) B.Box(Xs - 8.f, Xs + 8.f, -88, 88, 100, 136, Steel);           // ер-тоқым тіректер
			B.CylX(FVector(-340.f, 0, 150.f), 12.f, 20.f, Steel);
			for (float Sg : { -1.f, 1.f })
			{
				UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
				T->SetupAttachment(M); T->SetRelativeLocation(B.O + FVector(-130.f, Sg * 109.f, 196.f)); T->SetRelativeRotation(FRotator(0, Sg * 90.f, 0));
				T->SetText(FText::FromString(TEXT("СУ / ВОДА"))); T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(32.f); T->SetTextRenderColor(FColor(40, 60, 140)); T->RegisterComponent();
			}
		}
		else
		{
			FKAPMat Planks = FKAPMat::Paint(Col * 0.85f, 0.85f); Planks.Ribs = 0.6f; Planks.RibPeriod = 20.f; Planks.Rust = 0.25f; Planks.Grime = 0.9f;
			B.Box(-410, 72, -124, 124, 104, 112, Steel);
			B.Pair(-410, 72, 120, 124, 112, 176, Planks);
			B.Box(-410, -406, -120, 120, 112, 176, Planks);
			B.Box(66, 72, -120, 120, 112, 196, Planks);
			for (int32 i = 0; i < 3; i++) B.Box(-380.f + i * 80.f, -322.f + i * 80.f, -60.f, 30.f, 112.f, 156.f, FKAPMat::Paint(FLinearColor(0.32f, 0.22f, 0.12f), 0.85f));
		}
	}
	else // btr
	{
		const float A1 = Sp->WB * 0.5f, A3 = -A1;
		FakeWheel(A1 - 135.f); FakeWheel(A3 - 135.f);
		B.CylX(FVector(225.f, 0, 222.f), 9.f, 170.f, Steel);                                                        // КПВТ
		B.Box(128, 150, -18, 18, 208, 236, Paint);                                                                  // маска
		B.CylX(FVector(170.f, 14.f, 218.f), 5.f, 60.f, Steel);
		for (float Sg : { -1.f, 1.f }) for (int32 i = 0; i < 3; i++) B.CylY(FVector(52.f - i * 9.f, Sg * 52.f, 230.f), 7.f, 18.f, Dark); // түтін гранаталары
		for (int32 s = 0; s < 2; s++) { const float Sg = s ? 1.f : -1.f; B.Box(218.f, 232.f, Sg * 100.f - 10.f, Sg * 100.f + 10.f, 180.f, 196.f, Paint); Head(s, FVector(232.5f, Sg * 100.f, 188.f), 13.f, true); }
		Tail(-440.f, 108.f, 168.f, 10.f, 8.f);
		for (int32 i = 0; i < 6; i++) B.Box(-420.f + i * 20.f, -408.f + i * 20.f, -88, 88, 205.f, 207.5f, Dark);   // мотор торлары
		B.Pair(300, 314, 28, 40, 98, 110, Steel);                                                                   // сүйрегіш ілмектер
		for (float Sg : { -1.f, 1.f }) B.CylZ(FVector(-300.f, Sg * 88.f, 268.f), 1.4f, 125.f, Steel);              // антенналар
		B.Pair(-150, -110, 146, 149, 150, 153, Steel);                                                              // тұтқалар
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(Pawn);
		T->SetupAttachment(M); T->SetRelativeLocation(B.O + FVector(-210.f, 141.f, 170.f)); T->SetRelativeRotation(FRotator(-20, 90, 0));
		T->SetText(FText::FromString(TEXT("217"))); T->SetHorizontalAlignment(EHTA_Center); T->SetWorldSize(30.f); T->SetTextRenderColor(FColor(220, 220, 210)); T->RegisterComponent();
	}

	// ұсақ бөлшектер соқтығыспайды, ең ұсақтары көлеңке түсірмейді
	TArray<USceneComponent*> Kids; M->GetChildrenComponents(true, Kids);
	for (USceneComponent* C : Kids)
		if (UStaticMeshComponent* S = Cast<UStaticMeshComponent>(C))
		{
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (S->Bounds.SphereRadius < 25.f) S->SetCastShadow(false);
		}
	return true;
}
