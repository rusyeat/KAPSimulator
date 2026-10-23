// Көлік есіктері мен бұзылу көрінісі.
//  • Корпус тұтас меш (KAPVehicleGen.inl), сондықтан жүргізуші есігі — бөлек «топса» компоненті: ашылғанда есік
//    сыртқа бұрылады, ал корпустағы есік орны қараңғы ойыққа (кабина іші, орындық) ауысады; жабылғанда бәрі жасырылады.
//  • Бұзылу: радиатор торынан ақ бу бұрқырайды, алдыңғы жағы астынан антифриз шалшығы жайылады, апат шамдары жыпылықтайды.
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAPVisuals.h"
#include "KAPFXManager.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
	/** Есік (корпус жүйесінде, сол жақ −Y): Xh — топса (алдыңғы жиек), Xr — артқы жиек; Zb — белдеу (одан жоғары — терезе бөлігі ішке көлбейді) */
	struct FDoorDef
	{
		const TCHAR* Id;
		float Xh, Xr, Z0, Zb, Z1, Y, Yup, Lean;
		float Gx0, Gx1, Gz0, Gz1;     // терезе
		float Hx0, Hx1, Hz;           // тұтқа
		float SeatX0, SeatX1;         // орындық (ойықтан көрінеді)
		float MaxAng;
		bool bTopSecond;              // жоғарғы бөлік екінші түспен (Буханка — ақшыл, УАЗ — брезент)
		FVector Grille, Puddle;       // бу шығатын тор, шалшық
		FVector HazF, HazR, HazSz;    // апат шамдары (оң жақтағысы — Y таңбасы ауысады)
	};
	static const FDoorDef GDoorDefs[] = {
		{ TEXT("bukhanka"), 194.f, 122.f, 41.f, 112.f, 197.f, 97.f, 97.f, 0.075f, 128.f, 188.f, 124.f, 190.f, 126.f, 136.f, 103.5f, 138.f, 178.f, 64.f, true,
			FVector(219.f, 0.f, 80.f), FVector(185.f, 0.f, 0.6f), FVector(217.6f, 78.f, 85.f), FVector(-223.4f, 82.f, 97.f), FVector(1.2f, 8.f, 6.f) },
		{ TEXT("uaz469"), 47.f, -3.f, 50.f, 117.f, 188.f, 85.f, 85.f, 0.1f, 6.f, 44.f, 132.f, 182.f, 1.f, 15.f, 107.5f, -2.f, 36.f, 70.f, true,
			FVector(195.f, 0.f, 92.f), FVector(160.f, 0.f, 0.6f), FVector(194.2f, 60.f, 108.f), FVector(-196.f, 70.f, 106.f), FVector(1.2f, 6.f, 4.f) },
		{ TEXT("niva"), 54.f, -60.f, 34.f, 102.f, 158.f, 84.f, 81.f, 0.164f, -54.f, 50.f, 106.f, 156.f, -42.f, -30.f, 95.5f, -40.f, 10.f, 60.f, false,
			FVector(187.f, 0.f, 76.f), FVector(150.f, 0.f, 0.6f), FVector(186.f, 60.f, 63.5f), FVector(-191.f, 64.f, 96.f), FVector(1.2f, 14.f, 5.f) },
	};
	const FDoorDef* FindDoorDef(FName Id) { for (const FDoorDef& D : GDoorDefs) if (Id == FName(D.Id)) return &D; return nullptr; }
}

FVector AKAPVehicleHandle::ShellToWorld(const FVector& L) const
{
	const USkeletalMeshComponent* M = Pawn ? Pawn->GetMesh() : nullptr;
	return M ? M->GetComponentTransform().TransformPosition(ShellO + L) : GetActorLocation() + L;
}

FVector AKAPVehicleHandle::WorldToShell(const FVector& W) const
{
	const USkeletalMeshComponent* M = Pawn ? Pawn->GetMesh() : nullptr;
	return M ? M->GetComponentTransform().InverseTransformPosition(W) - ShellO : W - GetActorLocation();
}

void AKAPVehicleHandle::BuildDoors(USkeletalMeshComponent* M, const FVector& O)
{
	ShellO = O;
	const FDoorDef* D = FindDoorDef(Id);
	if (!D || !Pawn) return;
	GrilleLocal = D->Grille;
	const FLinearColor Col = Def.Color;
	FKAPMat Paint = FKAPMat::Paint(Col, 0.48f); Paint.Grime = 0.22f; Paint.Rust = 0.08f; Paint.DustTop = 0.85f; Paint.Metallic = 0.3f; Paint.VarAmt = 0.07f;
	FKAPMat Top = Paint;
	if (Id == FName("bukhanka")) { Top = FKAPMat::Paint(FLinearColor(0.78f, 0.76f, 0.68f), 0.5f); Top.Grime = 0.3f; Top.Rust = 0.08f; Top.DustTop = 0.9f; Top.Metallic = 0.3f; }
	if (Id == FName("uaz469")) { Top = FKAPMat::Paint(Col * 0.72f, 0.96f); Top.Ribs = 0.12f; Top.RibPeriod = 40.f; Top.Grime = 0.9f; Top.DustTop = 1.f; }
	FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.035f, 0.045f, 0.05f), 0.04f); Glass.DustTop = 0.12f; Glass.Grime = 0.06f; Glass.VarAmt = 0.f;
	FKAPMat Chrome = FKAPMat::Metal(FLinearColor(0.72f, 0.72f, 0.7f), 0.22f); Chrome.Rust = 0.12f;
	FKAPMat Trim = FKAPMat::Paint(FLinearColor(0.07f, 0.068f, 0.062f), 0.8f); Trim.Grime = 0.5f; Trim.DustTop = 0.3f;       // есіктің ішкі қаптамасы
	FKAPMat Hole = FKAPMat::Paint(FLinearColor(0.018f, 0.017f, 0.016f), 0.95f); Hole.Grime = 0.f; Hole.DustTop = 0.f; Hole.VarAmt = 0.f; // кабина іші
	FKAPMat Seat = FKAPMat::Paint(FLinearColor(0.07f, 0.045f, 0.03f), 0.7f); Seat.Grime = 0.3f; Seat.DustTop = 0.f;            // қоңыр дерматин орындық
	const FKAPMat Top2 = D->bTopSecond ? Top : Paint;
	for (int32 Side = 0; Side < 2; Side++)
	{
		const float Sg = Side == 0 ? -1.f : 1.f;
		FDoorRt Dr; Dr.Sign = Sg; Dr.MaxAng = D->MaxAng;
		USceneComponent* H = NewObject<USceneComponent>(Pawn);
		H->SetupAttachment(M); H->SetRelativeLocation(O + FVector(D->Xh, Sg * (D->Y + 0.6f), 0.f)); H->RegisterComponent();
		Dr.Hinge = H; DoorParts.Add(H);
		const float LeanDeg = FMath::RadiansToDegrees(FMath::Atan(D->Lean));
		// бөлшек: X0..X1, Z0..Z1 (корпус жүйесі), Out — сыртқа қарай ығысу (см), Th — қалыңдығы; жоғарғы бөлік ішке көлбейді
		auto Piece = [&](USceneComponent* Parent, FVector Base, float X0, float X1, float Za, float Zb2, float Out, float Th, const FKAPMat& Mt, bool bLean)
		{
			const float Zm = (Za + Zb2) * 0.5f;
			const float In = bLean ? (D->Y - D->Yup) + D->Lean * (Zm - D->Zb) : 0.f;
			UStaticMeshComponent* C = KAPVis::Add(Pawn, Parent, EKAPShape::Cube, Base + FVector((X0 + X1) * 0.5f, Sg * (Out - In), Zm), FVector(FMath::Abs(X1 - X0), Th, FMath::Abs(Zb2 - Za)), Mt,
				FRotator(0, 0, bLean ? Sg * LeanDeg : 0.f), false);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetVisibility(false);
			C->SetCastShadow(Th > 1.f);
			Dr.Show.Add(C);
			return C;
		};
		// ---- есік (топсаға қатысты: x = X − Xh) ----
		const FVector Hb(-D->Xh, 0.f, 0.f);
		Piece(H, Hb, D->Xr, D->Xh, D->Z0, D->Zb, 1.0f, 2.8f, Paint, false);                                        // төменгі панель
		Piece(H, Hb, D->Xr + 4.f, D->Xh - 4.f, D->Z0 + 6.f, D->Zb - 4.f, -0.8f, 1.2f, Trim, false);                // ішкі қаптама
		Piece(H, Hb, D->Xr, D->Xh, D->Zb, D->Gz0, 1.0f, 2.8f, Top2, true);                                          // терезе асты
		Piece(H, Hb, D->Xr, D->Xh, D->Gz1, D->Z1, 1.0f, 2.8f, Top2, true);                                          // терезе үсті
		Piece(H, Hb, D->Xr, D->Gx0, D->Gz0, D->Gz1, 1.0f, 2.8f, Top2, true);                                        // артқы тірек
		Piece(H, Hb, D->Gx1, D->Xh, D->Gz0, D->Gz1, 1.0f, 2.8f, Top2, true);                                        // алдыңғы тірек
		Piece(H, Hb, D->Gx0, D->Gx1, D->Gz0, D->Gz1, 0.4f, 0.8f, Glass, true);                                      // әйнек
		Piece(H, Hb, D->Hx0, D->Hx1, D->Hz - 1.5f, D->Hz + 1.5f, 3.2f, 1.6f, Chrome, false);                        // тұтқа
		// ---- корпустағы есік орны: қараңғы ойық, ішінде орындық (есік жабық кезде жасырын) ----
		const FVector Bb(0.f, 0.f, 0.f);
		USceneComponent* Body = M;
		auto Gap = [&](float X0, float X1, float Za, float Zb2, float Out, const FKAPMat& Mt, bool bLean)
		{
			UStaticMeshComponent* C = Piece(Body, O + FVector(0.f, Sg * D->Y, 0.f), X0, X1, Za, Zb2, Out, 0.5f, Mt, bLean);
			C->SetCastShadow(false);
		};
		Gap(D->Xr + 1.f, D->Xh - 1.f, D->Z0 + 1.f, D->Zb, 0.3f, Hole, false);
		Gap(D->Xr + 1.f, D->Xh - 1.f, D->Zb, D->Z1 - 1.f, 0.3f, Hole, true);
		Gap(D->SeatX0, D->SeatX1, D->Z0 + 18.f, D->Z0 + 32.f, 0.6f, Seat, false);                                  // отырғыш
		Gap(D->SeatX0 - 4.f, D->SeatX0 + 9.f, D->Z0 + 30.f, FMath::Min(D->Z1 - 20.f, D->Z0 + 105.f), 0.6f, Seat, D->Z0 + 105.f > D->Zb); // арқалық
		Gap(D->Xh - 22.f, D->Xh - 18.f, D->Zb - 2.f, D->Zb + 30.f, 0.6f, Trim, true);                              // руль бағанасы
		Doors.Add(Dr);
		// ---- апат шамдары (алдыңғы, артқы) ----
		for (int32 k = 0; k < 2; k++)
		{
			FVector P = k == 0 ? D->HazF : D->HazR; P.Y *= Sg;
			UStaticMeshComponent* L = KAPVis::Add(Pawn, M, EKAPShape::Cube, O + P, D->HazSz, FKAPMat::Glow(FLinearColor(1.f, 0.42f, 0.03f), 0.f), FRotator::ZeroRotator, false);
			L->SetCollisionEnabled(ECollisionEnabled::NoCollision); L->SetCastShadow(false);
			HazardLens.Add(L);
		}
	}
	for (int32 k = 0; k < 2; k++)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Pawn);
		L->SetupAttachment(M); L->SetRelativeLocation(O + (k == 0 ? D->HazF + FVector(30.f, -D->HazF.Y, 0.f) : D->HazR - FVector(30.f, D->HazR.Y, 0.f)));
		L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f); L->SetAttenuationRadius(420.f);
		L->SetLightColor(FLinearColor(1.f, 0.45f, 0.05f)); L->SetCastShadows(false); L->SetVisibility(false);
		L->RegisterComponent();
		HazardL.Add(L);
	}
	// антифриз шалшығы (жасыл-көк, жылтыр)
	FKAPMat Wet = FKAPMat::Paint(FLinearColor(0.012f, 0.03f, 0.028f), 0.05f); Wet.Grime = 0.f; Wet.DustTop = 0.f; Wet.VarAmt = 0.05f;
	Coolant = KAPVis::Add(Pawn, M, EKAPShape::Cylinder, O + D->Puddle, FVector(10.f, 10.f, 0.6f), Wet, FRotator::ZeroRotator, false);
	Coolant->SetCollisionEnabled(ECollisionEnabled::NoCollision); Coolant->SetCastShadow(false); Coolant->SetVisibility(false);
}

void AKAPVehicleHandle::SwingDoor(int32 Side, float HoldSec)
{
	if (!Doors.IsValidIndex(Side)) return;
	FDoorRt& D = Doors[Side];
	if (D.Target <= 0.f && D.Ang <= 1.f)
		if (UKAPAudio* A = UKAPAudio::Get(this)) A->PlayBank("DoorOpen", ShellToWorld(FVector(160.f, D.Sign * 100.f, 100.f)), 0.75f, FMath::FRandRange(0.95f, 1.05f));
	D.Target = D.MaxAng; D.bStay = HoldSec < 0.f; D.Hold = FMath::Max(0.f, HoldSec);
	for (UPrimitiveComponent* C : D.Show) if (C) C->SetVisibility(true);
}

void AKAPVehicleHandle::CloseDoor(int32 Side)
{
	if (!Doors.IsValidIndex(Side)) return;
	Doors[Side].Target = 0.f; Doors[Side].bStay = false; Doors[Side].Hold = 0.f;
}

void AKAPVehicleHandle::DoorTick(float Dt)
{
	for (FDoorRt& D : Doors)
	{
		if (!D.Hinge || (D.Ang <= 0.f && D.Target <= 0.f)) continue;
		if (!D.bStay && D.Target > 0.f && D.Ang >= D.Target - 0.5f) { D.Hold -= Dt; if (D.Hold <= 0.f) D.Target = 0.f; }
		const float Prev = D.Ang;
		// ашылу: тез басталып баяулайды (≈0.6 с); жабылу: үдемелі, соңында тарс етеді
		if (D.Ang < D.Target) D.Ang = FMath::Min(D.Target, D.Ang + Dt * FMath::Max(30.f, (D.Target - D.Ang) * 5.5f));
		else if (D.Ang > D.Target) D.Ang = FMath::Max(D.Target, D.Ang - Dt * (80.f + (D.MaxAng - D.Ang) * 4.f));
		D.Hinge->SetRelativeRotation(FRotator(0.f, -D.Sign * D.Ang, 0.f));
		if (Prev > 0.f && D.Ang <= 0.f)
		{
			for (UPrimitiveComponent* C : D.Show) if (C) C->SetVisibility(false);
			if (UKAPAudio* A = UKAPAudio::Get(this)) A->PlayBank("DoorClose", ShellToWorld(FVector(160.f, D.Sign * 100.f, 100.f)), 0.8f, FMath::FRandRange(0.95f, 1.05f));
		}
	}
	// ---- апат шамдары: 1.4 Гц ----
	if (HazardLens.Num())
	{
		HazardT = bHazard ? HazardT + Dt : 0.f;
		const bool bOn = bHazard && FMath::Frac(HazardT * 1.4f) < 0.5f;
		// материал тек күй ауысқанда жаңарады
		const bool bWas = HazardL.Num() && HazardL[0] && HazardL[0]->IsVisible();
		if (bOn != bWas)
		{
			for (UStaticMeshComponent* L : HazardLens) if (L) if (UMaterialInstanceDynamic* MI = KAPVis::UniqueMID(L)) MI->SetVectorParameterValue("Emissive", bOn ? FLinearColor(1.f, 0.42f, 0.03f) * 90.f : FLinearColor::Black);
			for (UPointLightComponent* L : HazardL) if (L) { L->SetVisibility(bOn); L->SetIntensity(bOn ? 5.f : 0.f); }
		}
	}
	// ---- бу мен антифриз ----
	if (SteamFade > 0.f && SteamK > 0.f) SteamK = FMath::Max(0.f, SteamK - Dt / SteamFade);
	if (SteamK <= 0.01f && CoolantK <= 0.f) return;
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FVector Gw = ShellToWorld(GrilleLocal);
	if (SteamK > 0.3f) CoolantK = FMath::Min(1.f, CoolantK + Dt / 14.f);
	if (Coolant)
	{
		Coolant->SetVisibility(CoolantK > 0.01f);
		const float Rr = 20.f + 120.f * FMath::Sqrt(CoolantK);
		Coolant->SetRelativeScale3D(FVector(Rr / 100.f, Rr * 0.8f / 100.f, 0.006f));
	}
	if (SteamK <= 0.01f || FVector::Dist(Cam, Gw) > 9000.f) return;
	AKAPFXManager* FX = AKAPFXManager::Get(GetWorld());
	if (!FX) return;
	const FVector Fw = Pawn->GetActorForwardVector(), Rt = Pawn->GetActorRightVector();
	SteamAcc += Dt * 26.f * SteamK;
	while (SteamAcc >= 1.f)
	{
		SteamAcc -= 1.f;
		// тордан алға-жоғары бұрқырайды
		const FVector P = ShellToWorld(GrilleLocal + FVector(FMath::FRandRange(-2.f, 4.f), FMath::FRandRange(-28.f, 28.f), FMath::FRandRange(-16.f, 14.f)));
		FX->Spawn(EKAPFX::Vapor, P, FMath::FRandRange(0.6f, 1.05f) * (0.6f + 0.4f * SteamK), Fw * FMath::FRandRange(40.f, 110.f) * SteamK + FVector(0, 0, FMath::FRandRange(20.f, 70.f)));
		// алдыңғы доңғалақ ойықтары мен астынан — жерге жайылады
		if (FMath::FRand() < 0.18f)
		{
			const float S = FMath::FRand() < 0.5f ? -1.f : 1.f;
			const FVector Q = ShellToWorld(FVector(GrilleLocal.X - 70.f, S * FMath::FRandRange(40.f, 85.f), 28.f));
			FX->Spawn(EKAPFX::Vapor, Q, FMath::FRandRange(0.35f, 0.55f), Rt * S * FMath::FRandRange(30.f, 70.f) + FVector(0, 0, -40.f));
		}
	}
	HissCd -= Dt;
	if (HissCd <= 0.f && SteamK > 0.4f && FVector::Dist(Cam, Gw) < 3500.f)
	{
		HissCd = FMath::FRandRange(1.8f, 2.6f);
		if (UKAPAudio* A = UKAPAudio::Get(this)) A->PlayAt(EKAPSound::Steam, Gw, 0.45f * SteamK);
	}
}
