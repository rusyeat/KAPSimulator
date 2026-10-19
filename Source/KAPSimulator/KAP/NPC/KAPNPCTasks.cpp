// NPC тапсырмалары: жұмысшылар ұңғыманы жөндейді және жүк тасиды, жасақ күзетте жүреді,
// тұрғындар түнде от басына жиналады, монстр жақындаса бейбіт адамдар айқайлап қашады.
#include "KAPNPC.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPFXManager.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

static bool IsCivil(ENPCKind K) { return K == ENPCKind::Worker || K == ENPCKind::Villager || K == ENPCKind::Elder || K == ENPCKind::Mother || K == ENPCKind::Survivor; }

void AKAPNPC::SetCarry(bool bOn)
{
	if (bOn && !Carried && GetMesh())
	{
		FKAPMat Box = FKAPMat::Paint(FLinearColor(0.34f, 0.24f, 0.13f), 0.85f); Box.Ribs = 0.3f; Box.RibPeriod = 12.f; Box.Panel = 1.f; Box.PanelW = 12.f;
		Carried = KAPVis::Add(this, GetMesh(), EKAPShape::Cube, FVector::ZeroVector, FVector(36, 30, 26), Box, FRotator::ZeroRotator, false);
		Carried->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, "spine_03");
		Carried->SetRelativeLocation(FVector(-2, 34, 0));
	}
	if (Carried) Carried->SetVisibility(bOn);
	GetCharacterMovement()->MaxWalkSpeed = bOn ? WalkSpeed * 0.75f : WalkSpeed;
}

void AKAPNPC::SetTool(bool bOn)
{
	if (bOn && !Tool && GetMesh())
	{
		Tool = KAPVis::Add(this, GetMesh(), EKAPShape::Cube, FVector::ZeroVector, FVector(28, 3, 2.5f), FKAPMat::Metal(FLinearColor(0.7f, 0.7f, 0.72f), 0.25f), FRotator::ZeroRotator, false);
		Tool->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, "hand_r");
		Tool->SetRelativeLocation(FVector(10, 0, 0));
	}
	if (Tool) Tool->SetVisibility(bOn);
}

void AKAPNPC::FaceTo(const FVector2D& Pm)
{
	const FVector2D P = KAP::CM2M(GetActorLocation());
	SetActorRotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Pm.Y - P.Y, Pm.X - P.X)), 0.0));
}

void AKAPNPC::SayTask(const TCHAR* Kk)
{
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	if (!PC || TaskSayCd > 0) return;
	if (FVector::Dist(PC->GetActorLocation(), GetActorLocation()) > 1200.f) return;
	TaskSayCd = FMath::FRandRange(25.f, 45.f);
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Subtitle(Kk, FString(), DisplayName, 3.f);
	if (UKAPAudio* A = UKAPAudio::Get(this)) A->Voice(GetActorLocation() + FVector(0, 0, 60), FMath::Clamp(FCString::Strlen(Kk) * 0.06f, 0.8f, 2.6f), bFemale, 0.9f);
}

void AKAPNPC::PickTask()
{
	TaskT = 0; TaskStep = 0;
	const FVector2D P = KAP::CM2M(GetActorLocation());
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const bool bNight = WX && WX->Env.bNight;
	if (Kind == ENPCKind::Militia || Kind == ENPCKind::Guard)
	{
		// күзет бағыты: үйінің айналасында 4 нүкте
		PatrolPts.Reset();
		const float R = FMath::Max(8.f, PatrolR * 0.6f);
		const float A0 = FMath::FRandRange(0.f, UE_TWO_PI);
		for (int32 i = 0; i < 4; i++) { const float A = A0 + i * UE_HALF_PI; PatrolPts.Add(Home + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R * FMath::FRandRange(0.7f, 1.f)); }
		PatrolI = 0; Task = ENPCTask::Patrol;
		return;
	}
	if (!IsCivil(Kind) || bSick) { Task = ENPCTask::None; return; }
	// түнде — ең жақын от басына
	if (bNight && Kind != ENPCKind::Worker)
	{
		double Bd = 150.0;
		for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It)
			if (It->bFire) { const FVector2D F = KAP::CM2M(It->GetActorLocation()); const double D = FVector2D::Distance(F, P); if (D < Bd) { Bd = D; TaskPos2 = F; Task = ENPCTask::Campfire; } }
		if (Task == ENPCTask::Campfire) { const float A = FMath::FRandRange(0.f, UE_TWO_PI); TaskPos = TaskPos2 + FVector2D(FMath::Cos(A), FMath::Sin(A)) * FMath::FRandRange(2.2f, 3.2f); return; }
	}
	if (Kind == ENPCKind::Worker)
	{
		// ең жақын ұңғыма: жөндеу немесе ұңғымадан вагонға жәшік тасу
		double Bd = 160.0; bool bWell = false;
		if (const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>())
			for (int32 i = 0; i < WM->Wells.Num(); i++) { const FVector2D W = WM->Wells[i].PosM(); const double D = FVector2D::Distance(W, Home); if (D < Bd) { Bd = D; TaskPos2 = W; bWell = true; } }
		if (!bWell) { Task = ENPCTask::None; return; }
		if (FMath::FRand() < 0.6f)
		{
			Task = ENPCTask::Repair;
			const float A = FMath::FRandRange(0.f, UE_TWO_PI);
			TaskPos = TaskPos2 + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 1.6f;
			TaskDur = FMath::FRandRange(20.f, 40.f);
		}
		else
		{
			Task = ENPCTask::Carry;
			// межелі нүкте: ең жақын вагон/құрылым, болмаса үйі
			FVector2D Dst = Home + FVector2D(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f));
			double Bs = 250.0;
			for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It)
				if (It->Type == EStructType::Wagon || It->Type == EStructType::Workshop) { const FVector2D S = KAP::CM2M(It->GetActorLocation()); const double D = FVector2D::Distance(S, TaskPos2); if (D < Bs && D > 15.0) { Bs = D; Dst = S + FVector2D(4, 4); } }
			TaskPos = TaskPos2 + FVector2D(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-3.f, 3.f)); // жүкті алатын жер
			TaskPos3 = Dst;
		}
		return;
	}
	Task = ENPCTask::None;
}

bool AKAPNPC::TaskTick(float Dt)
{
	TaskSayCd -= Dt;
	const FVector2D P = KAP::CM2M(GetActorLocation());
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	// бейбіт адамдар: монстр жақындаса — айқайлап, үйіне/алысқа қашады
	if (IsCivil(Kind) && Task != ENPCTask::Flee)
	{
		FleeScan -= Dt;
		if (FleeScan <= 0)
		{
			FleeScan = 0.6f;
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
				if (It->Faction == EKAPFaction::Monster && !It->bDead && !It->bStalker && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 2600.f)
				{
					const FVector2D M = KAP::CM2M(It->GetActorLocation());
					Task = ENPCTask::Flee; TaskT = 0;
					TaskPos = P + (P - M).GetSafeNormal() * 45.f;
					SetCarry(false); SetTool(false);
					static const TCHAR* Sh[] = { TEXT("Қашыңдар! Мұнда бір нәрсе бар!"), TEXT("Көмектесіңдер!"), TEXT("Құдай-ау, анау не?!") };
					TaskSayCd = 0; SayTask(Sh[FMath::RandRange(0, 2)]);
					if (UKAPAudio* A = UKAPAudio::Get(this)) A->Creature(GetActorLocation() + FVector(0, 0, 60), 1, 0.5f);
					break;
				}
		}
	}
	if (Task == ENPCTask::None) { TaskT += Dt; if (TaskT > 6.f) PickTask(); return Task != ENPCTask::None && Task != ENPCTask::Patrol; }
	TaskT += Dt;
	switch (Task)
	{
	case ENPCTask::Flee:
		SteerTo(TaskPos, RunSpeed);
		if (FVector2D::Distance(P, TaskPos) < 3.0 || TaskT > 20.f) { Task = ENPCTask::None; TaskT = 0; }
		return true;
	case ENPCTask::Repair:
		if (TaskStep == 0)
		{
			SetTool(true);
			SteerTo(TaskPos, WalkSpeed * 0.7f);
			if (FVector2D::Distance(P, TaskPos) < 1.0) { TaskStep = 1; TaskT = 0; }
			if (TaskT > 60.f) Task = ENPCTask::None;
			return true;
		}
		// ұңғыма басында жұмыс: бұрылып тұрады, кілтпен соғады — ұшқын мен металл дыбысы
		FaceTo(TaskPos2);
		WorkFxT -= Dt;
		if (WorkFxT <= 0)
		{
			WorkFxT = FMath::FRandRange(0.8f, 1.6f);
			const FVector Hit = GetActorLocation() + GetActorForwardVector() * 70.f + FVector(0, 0, 10);
			if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(FMath::FRand() < 0.5f ? EKAPFX::Flash : EKAPFX::Steam, Hit, 0.25f);
			if (UKAPAudio* A = UKAPAudio::Get(this)) A->Impact(Hit, 1, 0.5f);
			if (GetMesh()) GetMesh()->SetRelativeRotation(FRotator(FMath::FRandRange(-4.f, 4.f), -90.f, 0));
		}
		if (TaskT > 3.f && FMath::FRand() < Dt * 0.05f) { static const TCHAR* L[] = { TEXT("Клапанды тартып жатырмын, сәл күте тұрыңыз."), TEXT("Қысым тағы да секірді..."), TEXT("Мына бұранда тот басып қалған.") }; SayTask(L[FMath::RandRange(0, 2)]); }
		if (TaskT > TaskDur) { SetTool(false); Task = ENPCTask::None; TaskT = 0; }
		return true;
	case ENPCTask::Carry:
		if (TaskStep == 0) { SteerTo(TaskPos, WalkSpeed * 0.8f); if (FVector2D::Distance(P, TaskPos) < 1.2 || TaskT > 60.f) { TaskStep = 1; TaskT = 0; SetCarry(true); } return true; }
		if (TaskStep == 1)
		{
			SteerTo(TaskPos3, WalkSpeed);
			if (FMath::FRand() < Dt * 0.03f) SayTask(TEXT("Жәшікті вагонға апарып тастайын."));
			if (FVector2D::Distance(P, TaskPos3) < 2.0 || TaskT > 90.f) { TaskStep = 2; TaskT = 0; SetCarry(false); if (UKAPAudio* A = UKAPAudio::Get(this)) A->Impact(GetActorLocation() + GetActorForwardVector() * 50.f - FVector(0, 0, 70), 2, 0.6f); }
			return true;
		}
		if (TaskT > 4.f) { Task = ENPCTask::None; TaskT = 0; }
		return true;
	case ENPCTask::Campfire:
	{
		const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
		if (WX && !WX->Env.bNight) { Task = ENPCTask::None; return false; }
		if (FVector2D::Distance(P, TaskPos) > 0.8) SteerTo(TaskPos, WalkSpeed * 0.7f);
		else { FaceTo(TaskPos2); if (FMath::FRand() < Dt * 0.02f) { static const TCHAR* L[] = { TEXT("Түн ұзақ болады..."), TEXT("Отты өшірмеңдер, ол жарықтан қорқады."), TEXT("Атам айтушы еді: далада жалғыз жүрме деп.") }; SayTask(L[FMath::RandRange(0, 2)]); } }
		return true;
	}
	case ENPCTask::Patrol:
	{
		// күзет: нүктеден нүктеге, әр нүктеде тоқтап айналаға қарайды, анда-санда рациямен сөйлеседі
		if (!PatrolPts.Num()) { Task = ENPCTask::None; return false; }
		const FVector2D Tg = PatrolPts[PatrolI % PatrolPts.Num()];
		if (TaskStep == 0)
		{
			SteerTo(Tg, WalkSpeed * 0.55f);
			if (FVector2D::Distance(P, Tg) < 1.5 || TaskT > 40.f) { TaskStep = 1; TaskT = 0; TaskDur = FMath::FRandRange(3.f, 7.f); LookYaw = GetActorRotation().Yaw; }
			return true;
		}
		SetActorRotation(FRotator(0, LookYaw + FMath::Sin(TaskT * 0.9f) * 70.f, 0));
		if (FMath::FRand() < Dt * 0.04f) { static const TCHAR* L[] = { TEXT("Бекет, мұнда тыныш. Қабылдадым."), TEXT("Шығыс жақта бірдеңе жылтырады..."), TEXT("Бір сағаттан кейін ауысамыз.") }; SayTask(L[FMath::RandRange(0, 2)]); if (UKAPAudio* A = UKAPAudio::Get(this)) A->Impact(GetActorLocation() + FVector(0, 0, 40), 4, 0.25f); }
		if (TaskT > TaskDur) { TaskStep = 0; TaskT = 0; PatrolI++; }
		return true;
	}
	default: return false;
	}
}
