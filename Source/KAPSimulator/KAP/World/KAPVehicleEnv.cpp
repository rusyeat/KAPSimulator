// Көлік пен табиғат: жел (аэродинамика — салыстырмалы ауа ағыны), батпақ (дөңгелек ұстасуы, батып қалу, лай шашырауы),
// су (кедергі, шашырау), жаңбырдан кейінгі тайғақ асфальт. AKAPVehicleHandle::Tick-тен шақырылады.
#include "KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAPNature.h"
#include "KAPVisuals.h"
#include "KAPFXManager.h"
#include "WeatherManager.h"
#include "KAP/Audio/KAPAudio.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/SkeletalMeshComponent.h"

void AKAPVehicleHandle::EnvTick(float Dt)
{
	USkeletalMeshComponent* M = Pawn ? Pawn->GetMesh() : nullptr;
	if (!M || !M->IsSimulatingPhysics()) return;
	UChaosWheeledVehicleMovementComponent* Mov = Cast<UChaosWheeledVehicleMovementComponent>(Pawn->GetChaosVehicleMovement());
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const FVector Pos = Pawn->GetActorLocation();
	const FVector Vel = Pawn->GetVelocity();
	const FVector V2(Vel.X / 100.f, Vel.Y / 100.f, 0.f);  // м/с
	const float Spd = V2.Size();
	const float Mass = FMath::Max(300.f, M->GetMass());
	// ---- жел: Chaos ішкі кедергісі тыныш ауаға қарай есептеледі; біз айырмасын қосамыз:
	//      F = ½ρ·Cd·A·k·(|Vr|·Vr + |V|·V), Vr = Wind − V. Қарсы жел — кедергі өседі (жылдамдық желдей азаяды),
	//      артқы жел — итереді, бүйір жел — бұрып әкетеді. k = 15 (ойын үшін: 14 м/с қарсы жел жылдамдықты ондаған км/сағ-қа азайтады), тұрған көлікті итермейді.
	if (WX && (bDriven || bAutoPilot || Spd > 1.f))
	{
		const FVector W(WX->Env.Wind.X / 100.f, WX->Env.Wind.Y / 100.f, 0.f);
		const float Area = Mov ? FMath::Max(0.5f, Mov->ChassisWidth * Mov->ChassisHeight / 10000.f) : 2.5f;
		const float Cd = Mov ? FMath::Max(0.2f, Mov->DragCoefficient) : 0.35f;
		const float K = 0.5f * 1.225f * Cd * Area * 28.f * FMath::Clamp(Spd / 6.f, 0.f, 1.f);
		const FVector Vr = W - V2;
		FVector F = (Vr * Vr.Size() + V2 * Spd) * K; // Н
		// қозғалтқыш қарсы желді жартылай жеңеді, ал артқы жел көлікті тым үдетеді — ойын сезімі үшін екеуін теңестіреміз:
		// 14 м/с жел ≈ ±25–35 км/сағ (қарсы — баяулату, артқы — үдету)
		F *= FVector::DotProduct(F, V2) < 0.f ? 2.1f : 0.32f;
		// бүйір жел биік көлікті күштірек итереді (Буханка, КамАЗ)
		const FVector Rt = Pawn->GetActorRightVector().GetSafeNormal2D();
		F += Rt * FVector::DotProduct(F, Rt) * 0.6f;
		M->AddForce(F * 100.f, NAME_None, false); // Н → кг·см/с²
		WindForceN = F.Size();
	}
	// ---- жер беті: асфальт (тег), су, батпақ ----
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPVehSurf), false, Pawn);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(H, Pos + FVector(0, 0, 50), Pos - FVector(0, 0, 300), ECC_Visibility, Q);
	const bool bAsph = bHit && H.GetComponent() && (H.GetComponent()->ComponentHasTag("SurfAsphalt") || H.GetComponent()->ComponentHasTag("SurfConcrete"));
	float Wz = 0.f, Wd = 0.f;
	const bool bWater = !bAsph && AKAPNature::WaterAt(GetWorld(), Pos, Wz, Wd);
	const float Mud = bAsph ? 0.f : AKAPNature::MudAt(GetWorld(), FVector2D(Pos.X, Pos.Y) / 100.0);
	MudK = FMath::FInterpTo(MudK, bWater ? 1.f : Mud, Dt, 2.f);
	// дөңгелек ұстасуы: батпақта тайғанайды, жаңбырдан кейін асфальт та тайғақ
	if (Mov)
	{
		const int32 Nw = Mov->Wheels.Num();
		if (BaseFriction.Num() != Nw) { BaseFriction.Reset(); for (UChaosVehicleWheel* Wh : Mov->Wheels) BaseFriction.Add(Wh ? Wh->FrictionForceMultiplier : 2.f); }
		const float Wet = WX ? WX->Env.Wet : 0.f;
		const float Grip = bAsph ? FMath::Lerp(1.f, 0.82f, Wet) : FMath::Lerp(1.f, 0.42f, FMath::Clamp((MudK - 0.15f) / 0.75f, 0.f, 1.f));
		if (FMath::Abs(Grip - GripApplied) > 0.01f)
		{
			GripApplied = Grip;
			for (int32 i = 0; i < Nw; i++) Mov->SetWheelFrictionMultiplier(i, BaseFriction[i] * Grip);
		}
	}
	// батпаққа бату (дөңгелек кедергісі) және судың кедергісі
	if (Spd > 0.3f)
	{
		const float Sink = bWater ? FMath::Clamp(Wd, 0.f, 1.2f) * 0.45f : FMath::Clamp((MudK - 0.3f) / 0.7f, 0.f, 1.f) * 0.2f;
		if (Sink > 0.f) M->AddForce(-V2.GetSafeNormal() * (Sink * Mass * 9.8f) * 100.f, NAME_None, false);
	}
	// ---- шашырау: дөңгелектің артынан лай (батпақта) немесе су ----
	AKAPFXManager* FX = AKAPFXManager::Get(GetWorld());
	if (FX && Spd > 2.f && (bWater || MudK > 0.35f))
	{
		SplashAcc += Dt * Spd * (bWater ? 3.f : 1.6f * MudK);
		const FVector Fw = Pawn->GetActorForwardVector(), Rt = Pawn->GetActorRightVector();
		const bool bFwd = FVector::DotProduct(Vel, Fw) >= 0.f;
		while (SplashAcc > 1.f)
		{
			SplashAcc -= 1.f;
			const float Side = FMath::RandBool() ? 1.f : -1.f;
			const FVector At = Pos + Rt * (Side * 80.f) + Fw * (bFwd ? -120.f : 120.f) + FVector(0, 0, bWater ? Wz - Pos.Z + 15.f : -40.f);
			const FVector Out = -Vel.GetSafeNormal2D() * FMath::FRandRange(150.f, 420.f) + Rt * Side * FMath::FRandRange(60.f, 260.f);
			FX->Spawn(bWater ? EKAPFX::Splash : EKAPFX::Mud, At, bWater ? FMath::FRandRange(0.8f, 1.6f) : FMath::FRandRange(0.6f, 1.3f), Out + Vel * 0.25f);
		}
		SplashSndCd -= Dt;
		if (SplashSndCd <= 0.f) { SplashSndCd = bWater ? 0.35f : 0.6f; if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Impact(Pos - FVector(0, 0, 40), 3, FMath::Clamp(Spd / 15.f, 0.2f, 0.8f)); }
	}
}
