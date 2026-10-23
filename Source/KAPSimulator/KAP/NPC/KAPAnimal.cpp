#include "KAPAnimal.h"
#include "KAPNPC.h"
#include "KAP/World/KAPTerrain.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/World/KAPNature.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/World/KAPFXManager.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"

namespace
{
	struct FKindDef
	{
		const TCHAR* Name; float Walk, Trot, Gallop; float Len, Height; float Hp; float FearR; float Turn;
		FLinearColor A[3], B[3]; // жүн түсі нұсқалары: негізгі / екінші
	};
	// жылдамдықтар — м/с; Len/Height — м; FearR — адамнан қорқу қашықтығы (м)
	const FKindDef Defs[5] = {
		{ TEXT("Horse"), 1.2f, 3.2f, 8.f, 2.3f, 1.45f, 160.f, 22.f, 140.f,
			{ FLinearColor(0.2f, 0.1f, 0.05f), FLinearColor(0.32f, 0.2f, 0.1f), FLinearColor(0.07f, 0.05f, 0.04f) },
			{ FLinearColor(0.04f, 0.03f, 0.025f), FLinearColor(0.06f, 0.04f, 0.03f), FLinearColor(0.03f, 0.025f, 0.02f) } },
		{ TEXT("Cow"), 0.8f, 2.f, 4.5f, 2.3f, 1.3f, 180.f, 10.f, 80.f,
			{ FLinearColor(0.28f, 0.1f, 0.04f), FLinearColor(0.22f, 0.08f, 0.035f), FLinearColor(0.32f, 0.16f, 0.07f) },
			{ FLinearColor(0.75f, 0.72f, 0.64f), FLinearColor(0.7f, 0.67f, 0.6f), FLinearColor(0.8f, 0.77f, 0.7f) } },
		{ TEXT("Goat"), 0.7f, 1.8f, 4.5f, 1.15f, 0.7f, 60.f, 14.f, 160.f,
			{ FLinearColor(0.06f, 0.05f, 0.045f), FLinearColor(0.62f, 0.58f, 0.52f), FLinearColor(0.25f, 0.15f, 0.08f) },
			{ FLinearColor(0.03f, 0.025f, 0.02f), FLinearColor(0.35f, 0.3f, 0.25f), FLinearColor(0.1f, 0.06f, 0.04f) } },
		{ TEXT("Sheep"), 0.6f, 1.6f, 3.8f, 1.2f, 0.8f, 55.f, 16.f, 120.f,
			{ FLinearColor(0.62f, 0.56f, 0.45f), FLinearColor(0.55f, 0.48f, 0.38f), FLinearColor(0.5f, 0.36f, 0.24f) },
			{ FLinearColor(0.05f, 0.035f, 0.03f), FLinearColor(0.08f, 0.05f, 0.035f), FLinearColor(0.04f, 0.03f, 0.025f) } },
		{ TEXT("Fox"), 0.7f, 2.2f, 6.5f, 1.1f, 0.4f, 35.f, 40.f, 220.f,
			{ FLinearColor(0.45f, 0.18f, 0.05f), FLinearColor(0.5f, 0.24f, 0.08f), FLinearColor(0.38f, 0.26f, 0.15f) },
			{ FLinearColor(0.75f, 0.72f, 0.66f), FLinearColor(0.7f, 0.66f, 0.6f), FLinearColor(0.72f, 0.7f, 0.65f) } },
	};
	const TCHAR* AnimName[5] = { TEXT("Idle"), TEXT("Walk"), TEXT("Trot"), TEXT("Gallop"), TEXT("Graze") };
	// Blender-дегі табиғи жылдамдық (м/с, адым ұзындығы бойынша; animals.json)
	const float Native[5][5] = {
		{ 0.f, 0.864f, 2.548f, 5.381f, 0.f }, { 0.f, 0.582f, 1.761f, 3.676f, 0.f }, { 0.f, 0.533f, 1.534f, 3.333f, 0.f },
		{ 0.f, 0.448f, 1.293f, 2.641f, 0.f }, { 0.f, 0.447f, 1.344f, 2.766f, 0.f } };
}

AKAPAnimal::AKAPAnimal()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
	Body = CreateDefaultSubobject<UCapsuleComponent>("Body");
	Body->SetupAttachment(RootComponent);
	Body->SetRelativeRotation(FRotator(90.f, 0, 0));
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Body->SetCanEverAffectNavigation(false);
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->bEnableUpdateRateOptimizations = true;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Mesh->SetCastShadow(true);
}

void AKAPAnimal::Setup(EAnimalKind InKind, int32 InHerd, FVector2D HomeM, float InHomeR, uint32 Seed)
{
	Kind = InKind; Herd = InHerd; Home = HomeM; HomeR = InHomeR;
	Rng.Initialize(Seed);
	const FKindDef& D = Defs[(int32)Kind];
	Hp = D.Hp;
	const FString Base = FString::Printf(TEXT("/Game/KAP/Animals/V1/SKM_KAP_%s.SKM_KAP_%s"), D.Name, D.Name);
	if (USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, *Base, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Mesh->SetSkeletalMesh(SK);
		// модельдің алды қай осьте екенін басы мен жамбасынан анықтаймыз (FBX осьтері)
		const int32 Hi = SK->GetRefSkeleton().FindBoneIndex(FName("head")), Pi = SK->GetRefSkeleton().FindBoneIndex(FName("pelvis"));
		if (Hi != INDEX_NONE && Pi != INDEX_NONE)
		{
			auto CS = [&](int32 I) { FTransform T = FTransform::Identity; for (int32 b = I; b != INDEX_NONE; b = SK->GetRefSkeleton().GetParentIndex(b)) T = T * SK->GetRefSkeleton().GetRefBonePose()[b]; return T.GetLocation(); };
			const FVector Fw = (CS(Hi) - CS(Pi)).GetSafeNormal2D();
			YawOff = -FMath::RadiansToDegrees(FMath::Atan2(Fw.Y, Fw.X));
		}
		Mesh->SetRelativeRotation(FRotator(0, YawOff, 0));
		// жеке түс (бір табында да әртүрлі)
		UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(0);
		const int32 V = Rng.RandRange(0, 2);
		const float Jit = Rng.FRandRange(0.85f, 1.15f);
		if (MID) { MID->SetVectorParameterValue("CoatA", D.A[V] * Jit); MID->SetVectorParameterValue("CoatB", D.B[V]); }
	}
	for (int32 a = 0; a < 5; a++)
	{
		const FString P = FString::Printf(TEXT("/Game/KAP/Animals/V1/AN_KAP_%s_%s.AN_KAP_%s_%s"), D.Name, AnimName[a], D.Name, AnimName[a]);
		Anim[a].Seq = LoadObject<UAnimSequence>(nullptr, *P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		Anim[a].Native = Native[(int32)Kind][a];
		if (Anim[a].Seq) AnimKeep.Add(Anim[a].Seq);
	}
	if (!Anim[4].Seq) Anim[4] = Anim[0];
	const float S = Rng.FRandRange(0.92f, 1.08f);
	SetActorScale3D(FVector(S));
	Body->SetCapsuleSize(D.Height * 28.f, D.Len * 42.f);
	Body->SetRelativeLocation(FVector(0, 0, D.Height * 62.f));
	Yaw = Rng.FRandRange(0.f, 360.f);
	State = EAnimalState::Graze; StateDur = Rng.FRandRange(4.f, 15.f); StateT = Rng.FRandRange(0.f, StateDur);
	Thirst = Rng.FRandRange(0.f, 0.7f);
	Play(4, 1.f);
}

void AKAPAnimal::Play(int32 A, float Rate)
{
	if (!Anim[A].Seq) A = 0;
	if (!Anim[A].Seq) return;
	if (A != CurAnim)
	{
		CurAnim = A;
		Mesh->PlayAnimation(Anim[A].Seq, true);
		Mesh->SetPosition(Rng.FRandRange(0.f, Anim[A].Seq->GetPlayLength()), false);
	}
	Mesh->SetPlayRate(Rate);
}

bool AKAPAnimal::PickPoint(FVector2D C, float R, FVector2D& Out, bool bWater) const
{
	FRandomStream& Rn = const_cast<FRandomStream&>(Rng);
	for (int32 i = 0; i < 12; i++)
	{
		const float A = Rn.FRandRange(0.f, UE_TWO_PI), Rr = FMath::Sqrt(Rn.FRand()) * R;
		const FVector2D P = C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
		if (FMath::Max(FMath::Abs(P.X), FMath::Abs(P.Y)) > 1130.f) continue;
		float Hw; const float Dc = KAPTerrain::ChannelDist(P.X, P.Y, Hw);
		const bool bWet = Dc < Hw * 1.45f + 1.f || KAPTerrain::LakeDist(P.X, P.Y) < KAPTerrain::LakeR + 4.f || KAPTerrain::OasisDist(P.X, P.Y) < KAPTerrain::OasisR + 2.f;
		if (bWet) continue;
		if (Kind != EAnimalKind::Goat && Kind != EAnimalKind::Fox && KAPTerrain::FoothillMask(P.X, P.Y) > 0.35f) continue;
		Out = P;
		return true;
	}
	return false;
}

void AKAPAnimal::Scare(const FVector& From, float Strength, bool bSpread)
{
	if (State == EAnimalState::Dead) return;
	Fear = FMath::Max(Fear, Strength);
	FleeFrom = FVector2D(From);
	if (State != EAnimalState::Flee)
	{
		State = EAnimalState::Flee; StateT = 0.f; StateDur = 5.f + Strength * 8.f;
		if (VoiceCd <= 0.f) { VoiceCd = Rng.FRandRange(6.f, 14.f); if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Creature(GetActorLocation() + FVector(0, 0, 100), Kind == EAnimalKind::Fox ? 1 : 0, 0.45f); }
	}
	// үрей көршіге жұғады (табын бірге қашады)
	if (bSpread && Fauna.IsValid())
		for (const TWeakObjectPtr<AKAPAnimal>& O : Fauna->Animals)
			if (O.IsValid() && O.Get() != this && O->Herd == Herd && O->State != EAnimalState::Flee && FVector::Dist2D(O->GetActorLocation(), GetActorLocation()) < 3500.f)
				O->Scare(From, Strength * 0.8f, false);
}

float AKAPAnimal::TakeDamage(float Damage, FDamageEvent const& Event, AController* Inst, AActor* Causer)
{
	if (State == EAnimalState::Dead) return 0.f;
	Hp -= Damage;
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(EKAPFX::Blood, GetActorLocation() + FVector(0, 0, Defs[(int32)Kind].Height * 60.f), 1.f);
	const FVector From = Causer ? Causer->GetActorLocation() : GetActorLocation() - GetActorForwardVector() * 500.f;
	if (Hp <= 0.f) Die((GetActorLocation() - From).GetSafeNormal() * 600.f + FVector(0, 0, 200.f));
	else Scare(From, 1.f);
	return Damage;
}

void AKAPAnimal::Die(const FVector& Impulse)
{
	if (State == EAnimalState::Dead) return;
	State = EAnimalState::Dead; DeadT = 0.f; Speed = 0.f;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Fauna.IsValid()) Fauna->Killed++;
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Creature(GetActorLocation() + FVector(0, 0, 80), 1, 0.6f);
	// рагдолл: импортта жасалған физика ассеті
	if (Mesh->GetPhysicsAsset())
	{
		Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->SetSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
		Mesh->AddImpulseToAllBodiesBelow(Impulse, NAME_None, true, true);
	}
	else
	{
		Mesh->Stop();
		Mesh->SetRelativeRotation(FRotator(0, YawOff, 90.f));
	}
	if (Fauna.IsValid()) Fauna->Alarm(GetActorLocation(), 2500.f, 0.8f);
}

void AKAPAnimal::Think()
{
	const FKindDef& D = Defs[(int32)Kind];
	const FVector P = GetActorLocation();
	const FVector2D Pm = FVector2D(P) / 100.0;
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	// ---- қауіптер: ойыншы (жүгірсе/атса — алыстан), көлік, қасқыр ----
	float Threat = 0.f; FVector ThreatAt = P;
	if (const AKAPCharacter* Pl = AKAPCharacter::Get(this))
	{
		const float Dd = FVector::Dist2D(Pl->GetActorLocation(), P) / 100.f;
		const float R = D.FearR * (Pl->GetVelocity().Size2D() > 500.f ? 1.8f : 1.f) * (Pl->bIndoor ? 0.f : 1.f);
		if (Dd < R) { Threat = FMath::Max(Threat, 0.4f + 0.6f * (1.f - Dd / R)); ThreatAt = Pl->GetActorLocation(); }
	}
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) if (GM->Builder)
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : GM->Builder->Vehicles)
			if (V.IsValid() && V->Pawn)
			{
				const float Dd = FVector::Dist2D(V->Pawn->GetActorLocation(), P) / 100.f, Sp = V->Pawn->GetVelocity().Size2D() / 100.f;
				if (Sp > 3.f && Dd < 18.f + Sp * 2.5f) { Threat = FMath::Max(Threat, 0.9f); ThreatAt = V->Pawn->GetActorLocation(); }
			}
	if (Kind != EAnimalKind::Fox)
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (It->Kind == ENPCKind::Wolf && !It->bDead)
			{
				const float Dd = FVector::Dist2D(It->GetActorLocation(), P) / 100.f;
				if (Dd < 70.f) { Threat = 1.f; ThreatAt = It->GetActorLocation(); }
				if (Dd < 1.6f && Kind != EAnimalKind::Horse && Kind != EAnimalKind::Cow) { Die((P - It->GetActorLocation()).GetSafeNormal() * 300.f); return; } // қасқыр ұстап алды
			}
	if (Threat > 0.35f) Scare(ThreatAt, Threat);
	if (State == EAnimalState::Flee) return;
	// ---- қалыпты өмір: жайылу, жүру, суға бару (шөлдесе), жаңбырда топтасу ----
	const float Rain = WX ? WX->Env.Rain : 0.f;
	if (StateT < StateDur) return;
	StateT = 0.f;
	float R = Rain > 0.4f ? HomeR * 0.35f : HomeR;
	if (Thirst > 1.f && Kind != EAnimalKind::Fox)
	{
		// ең жақын су жағасы: өзен бойымен немесе көл/оазис
		FVector2D Best = Pm; float BestD = 1e9f;
		for (float X = Pm.X - 300.f; X <= Pm.X + 300.f; X += 15.f)
		{
			const float Xc = FMath::Clamp(X, KAPTerrain::DeltaX0, KAPTerrain::MapHalf - 30.f);
			for (int32 s = -1; s <= 1; s += 2)
			{
				const FVector2D C(Xc, KAPTerrain::RiverY(Xc) + s * (KAPTerrain::MainHalfW * 1.45f + 2.f));
				const float Dd = FVector2D::Distance(C, Pm);
				if (Dd < BestD) { BestD = Dd; Best = C; }
			}
		}
		for (const FVector2D& W : { FVector2D(KAPTerrain::LakeX, KAPTerrain::LakeY), FVector2D(KAPTerrain::OasisX, KAPTerrain::OasisY) })
		{
			const float Rw = W.X < -500.f ? KAPTerrain::LakeR + 6.f : KAPTerrain::OasisR + 4.f;
			const FVector2D E = W + (Pm - W).GetSafeNormal() * Rw;
			if (FVector2D::Distance(E, Pm) < BestD) { BestD = FVector2D::Distance(E, Pm); Best = E; }
		}
		if (BestD < 700.f) { Target = Best; State = EAnimalState::Walk; StateDur = 120.f; bLod = false; return; }
		Thirst = 0.f;
	}
	const float T = Rng.FRand();
	if (Kind == EAnimalKind::Fox)
	{
		if (T < 0.55f && PickPoint(Home, HomeR, Target, false)) { State = EAnimalState::Walk; StateDur = 60.f; }
		else { State = EAnimalState::Idle; StateDur = Rng.FRandRange(3.f, 9.f); }
		return;
	}
	if (T < 0.55f) { State = EAnimalState::Graze; StateDur = Rng.FRandRange(6.f, 22.f); }
	else if (T < 0.68f) { State = EAnimalState::Idle; StateDur = Rng.FRandRange(3.f, 8.f); }
	else
	{
		// табын ортасына жақын жүру (жайылым біртіндеп ауысады)
		FVector2D C = Home;
		if (Fauna.IsValid()) { FVector2D Sum(0, 0); int32 N = 0; for (const auto& O : Fauna->Animals) if (O.IsValid() && O->Herd == Herd && !O->IsDead()) { Sum += FVector2D(O->GetActorLocation()) / 100.0; N++; } if (N) C = FMath::Lerp(Home, Sum / N, 0.7); }
		if (PickPoint(C, FMath::Min(R, 35.f), Target, false)) { State = EAnimalState::Walk; StateDur = 40.f; }
		else { State = EAnimalState::Graze; StateDur = 8.f; }
	}
}

void AKAPAnimal::Move(float Dt)
{
	const FKindDef& D = Defs[(int32)Kind];
	FVector P = GetActorLocation();
	const FVector2D Pm = FVector2D(P) / 100.0;
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	float Want = 0.f;
	int32 A = 0;
	switch (State)
	{
	case EAnimalState::Flee:
	{
		FVector2D Away = (Pm - FleeFrom / 100.0).GetSafeNormal();
		if (Away.IsNearlyZero()) Away = Dir;
		// шекараға, суға қарай қашпайды — бұрылады
		const FVector2D Ahead = Pm + Away * 25.f;
		if (FMath::Max(FMath::Abs(Ahead.X), FMath::Abs(Ahead.Y)) > 1120.f) Away = (-Pm).GetSafeNormal();
		float Hw; if (KAPTerrain::ChannelDist(Ahead.X, Ahead.Y, Hw) < Hw * 1.5f) Away = FVector2D(-Away.Y, Away.X);
		Target = Pm + Away * 30.f;
		Want = D.Gallop * (0.75f + 0.25f * Fear);
		if (StateT > StateDur) { State = EAnimalState::Idle; StateT = 0.f; StateDur = Rng.FRandRange(2.f, 5.f); Fear = 0.f; }
		break;
	}
	case EAnimalState::Play:
		Want = D.Gallop * 0.85f;
		if (FVector2D::Distance(Pm, Target) < 6.f || StateT > StateDur) { State = EAnimalState::Idle; StateT = 0.f; StateDur = Rng.FRandRange(3.f, 6.f); }
		break;
	case EAnimalState::Walk:
	{
		const float Dd = FVector2D::Distance(Pm, Target);
		Want = Dd > 40.f ? D.Trot : D.Walk;
		if (Dd < 2.f)
		{
			// су жағасына жетсе — ішеді
			float Hw; const bool bShore = KAPTerrain::ChannelDist(Pm.X, Pm.Y, Hw) < Hw * 2.2f + 4.f || KAPTerrain::LakeDist(Pm.X, Pm.Y) < KAPTerrain::LakeR + 12.f || KAPTerrain::OasisDist(Pm.X, Pm.Y) < KAPTerrain::OasisR + 10.f;
			if (Thirst > 1.f && bShore) { State = EAnimalState::Drink; StateT = 0.f; StateDur = Rng.FRandRange(8.f, 14.f); Thirst = 0.f; }
			else { State = EAnimalState::Graze; StateT = 0.f; StateDur = Rng.FRandRange(5.f, 15.f); }
		}
		break;
	}
	case EAnimalState::Graze: case EAnimalState::Drink: A = 4; break;
	default: A = 0; break;
	}
	// бағыт: нысанаға қарай + табындастардан ара қашықтық
	if (Want > 0.f)
	{
		FVector2D To = (Target - Pm).GetSafeNormal();
		FVector2D Sep(0, 0);
		if (Fauna.IsValid())
			for (const auto& O : Fauna->Animals)
				if (O.IsValid() && O.Get() != this && !O->IsDead())
				{
					const FVector2D Dv = Pm - FVector2D(O->GetActorLocation()) / 100.0;
					const float Dd = Dv.Size(), Min = (D.Len + Defs[(int32)O->Kind].Len) * 0.45f;
					if (Dd < Min && Dd > 0.01f) Sep += Dv / Dd * (1.f - Dd / Min);
				}
		To = (To + Sep * 1.5f).GetSafeNormal();
		const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
		Yaw = FMath::FixedTurn(Yaw, WantYaw, D.Turn * Dt * (State == EAnimalState::Flee ? 1.6f : 1.f));
	}
	else if (WX && WX->Env.WindMs > 9.f && (Kind == EAnimalKind::Horse || Kind == EAnimalKind::Cow || Kind == EAnimalKind::Sheep))
	{
		// қатты желде арқасын желге береді (бас жел жаққа емес)
		Yaw = FMath::FixedTurn(Yaw, WX->Env.WindYaw, 12.f * Dt);
	}
	WantSpeed = Want * 100.f;
	Speed = FMath::FInterpTo(Speed, WantSpeed, Dt, Want > Speed / 100.f ? 1.6f : 2.5f);
	const FVector2D Fw(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	P.X += Fw.X * Speed * Dt; P.Y += Fw.Y * Speed * Dt;
	// жер бедеріне жабысу + еңіске қарай иілу
	const float Hl = D.Len * 40.f * GetActorScale3D().X;
	const float Zc = KAPVis::GroundZ(GetWorld(), P.X, P.Y, P.Z);
	if (!bLod)
	{
		const float Zf = KAPVis::GroundZ(GetWorld(), P.X + Fw.X * Hl, P.Y + Fw.Y * Hl, Zc), Zb = KAPVis::GroundZ(GetWorld(), P.X - Fw.X * Hl, P.Y - Fw.Y * Hl, Zc);
		Pitch = FMath::FInterpTo(Pitch, FMath::RadiansToDegrees(FMath::Atan2(Zf - Zb, 2.f * Hl)), Dt, 6.f);
	}
	// суда — бойы батады
	float Wz, Wd; const float Sink = AKAPNature::WaterAt(GetWorld(), P, Wz, Wd) ? FMath::Min(Wd, 0.6f) * 100.f * 0.4f : 0.f;
	P.Z = Zc - Sink;
	SetActorLocationAndRotation(P, FRotator(Pitch, Yaw, 0));
	// анимация: жылдамдыққа қарай жүріс түрі, адымы табиғи жылдамдыққа сай
	const float Ms = Speed / 100.f;
	if (Ms > 0.15f) A = Ms > (D.Trot + D.Gallop) * 0.5f ? 3 : Ms > (D.Walk + D.Trot) * 0.5f ? 2 : 1;
	const float Rate = A >= 1 && A <= 3 ? FMath::Clamp(Ms / FMath::Max(0.1f, Anim[A].Native), 0.6f, 2.2f) : 1.f;
	Play(A, Rate);
}

void AKAPAnimal::Tick(float Dt)
{
	Super::Tick(Dt);
	if (State == EAnimalState::Dead)
	{
		DeadT += Dt;
		if (DeadT > 180.f) Destroy();
		return;
	}
	// өнімділік: алыстағылар сирек ойлайды, өте алыстағылар жасырылады
	const AKAPCharacter* Pl = AKAPCharacter::Get(this);
	const float Dp = Pl ? FVector::Dist2D(Pl->GetActorLocation(), GetActorLocation()) : 0.f;
	bLod = Dp > 15000.f;
	Mesh->SetVisibility(Dp < 40000.f);
	StateT += Dt; VoiceCd -= Dt;
	Thirst += Dt / (Kind == EAnimalKind::Fox ? 1e9f : (Kind == EAnimalKind::Cow ? 240.f : 360.f));
	ThinkT -= Dt;
	if (ThinkT <= 0.f) { ThinkT = bLod ? 1.5f : 0.35f; Think(); if (State == EAnimalState::Dead) return; }
	Move(Dt);
	// көлік соғып кетсе
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) if (GM->Builder && !bLod)
		for (const TWeakObjectPtr<AKAPVehicleHandle>& V : GM->Builder->Vehicles)
			if (V.IsValid() && V->Pawn)
			{
				const FVector Vv = V->Pawn->GetVelocity();
				if (Vv.Size2D() > 400.f && FVector::Dist2D(V->Pawn->GetActorLocation(), GetActorLocation()) < 230.f + Defs[(int32)Kind].Len * 35.f)
				{
					Hp -= Vv.Size2D() / 100.f * 12.f;
					if (Hp <= 0.f) Die(Vv * 0.6f + FVector(0, 0, 300.f)); else Scare(V->Pawn->GetActorLocation(), 1.f);
					if (UPrimitiveComponent* Pc = Cast<UPrimitiveComponent>(V->Pawn->GetRootComponent())) if (Pc->IsSimulatingPhysics()) Pc->SetPhysicsLinearVelocity(Pc->GetPhysicsLinearVelocity() * (Kind == EAnimalKind::Horse || Kind == EAnimalKind::Cow ? 0.6f : 0.9f));
					break;
				}
			}
	// дауыс: сирек, тыныш кезде
	if (VoiceCd <= 0.f && !bLod && Dp < 6000.f)
	{
		VoiceCd = Rng.FRandRange(18.f, 45.f);
		if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Creature(GetActorLocation() + FVector(0, 0, 100), Kind == EAnimalKind::Fox ? 3 : 0, Kind == EAnimalKind::Cow ? 0.35f : 0.2f);
	}
}

// ================================================================ табындар
AKAPFauna::AKAPFauna()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

AKAPFauna* AKAPFauna::Get(const UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AKAPFauna> It(const_cast<UWorld*>(W)); It; ++It) return *It;
	return nullptr;
}

void AKAPFauna::BeginPlay()
{
	Super::BeginPlay();
	Rng.Initialize(20261023);
	SpawnAll();
}

void AKAPFauna::SpawnAll()
{
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	auto Loc = [&](FName Id) { const FKAPLocationRow* R = D ? D->Loc(Id) : nullptr; return R ? FVector2D(R->X, R->Y) : FVector2D::ZeroVector; };
	const FVector2D Aksai = Loc("aksai"), Josaly = Loc("josaly");
	// табындар: жылқылар — ашық далада, сиырлар — ауыл маңы мен өзен жайылмасы, қой мен ешкі — ауыл мен бөктер, түлкі — бөктер мен құм
	Herds = {
		{ EAnimalKind::Horse, FVector2D(-150, 520), 160.f, 11 },
		{ EAnimalKind::Horse, FVector2D(420, -620), 170.f, 9 },
		{ EAnimalKind::Cow, Aksai + FVector2D(140, -170), 80.f, 7 },
		{ EAnimalKind::Cow, FVector2D(150, KAPTerrain::RiverY(150.f) - 80.f), 70.f, 6 },
		{ EAnimalKind::Sheep, Josaly + FVector2D(-160, -120), 70.f, 16 },
		{ EAnimalKind::Sheep, Aksai + FVector2D(-120, -200), 60.f, 14 },
		{ EAnimalKind::Goat, FVector2D(-1000, -400), 70.f, 8 },
		{ EAnimalKind::Goat, FVector2D(600, -1020), 60.f, 7 },
		{ EAnimalKind::Fox, FVector2D(-850, -1000), 160.f, 1 }, { EAnimalKind::Fox, FVector2D(1020, 600), 160.f, 1 },
		{ EAnimalKind::Fox, FVector2D(150, -350), 200.f, 1 }, { EAnimalKind::Fox, FVector2D(-300, 1050), 150.f, 1 },
		{ EAnimalKind::Fox, FVector2D(700, 1000), 150.f, 1 },
	};
	FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 h = 0; h < Herds.Num(); h++)
	{
		const FHerdInfo& Hd = Herds[h];
		for (int32 i = 0; i < Hd.Count; i++)
		{
			// қой отарына бірнеше ешкі араласады (қазақ отарындағыдай)
			const EAnimalKind K = (Hd.Kind == EAnimalKind::Sheep && i % 6 == 5) ? EAnimalKind::Goat : Hd.Kind;
			FVector2D P;
			int32 Tries = 0;
			do { const float A = Rng.FRandRange(0.f, UE_TWO_PI), R = FMath::Sqrt(Rng.FRand()) * FMath::Min(Hd.R, 30.f); P = Hd.Home + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R; Tries++; }
			while (Tries < 10 && [&]() { float Hw; return KAPTerrain::ChannelDist(P.X, P.Y, Hw) < Hw * 1.6f; }());
			const FVector At(P.X * 100, P.Y * 100, KAPVis::GroundZ(GetWorld(), P.X * 100, P.Y * 100, 0.f));
			AKAPAnimal* An = GetWorld()->SpawnActor<AKAPAnimal>(AKAPAnimal::StaticClass(), At, FRotator::ZeroRotator, SP);
			if (!An) continue;
			An->Fauna = this;
			An->Setup(K, h, Hd.Home, Hd.R, Rng.RandRange(1, 1 << 28));
			Animals.Add(An);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("KAPFAUNA animals=%d herds=%d"), Animals.Num(), Herds.Num());
}

void AKAPFauna::Alarm(const FVector& At, float RadiusCm, float Strength)
{
	for (const auto& A : Animals)
		if (A.IsValid() && !A->IsDead())
		{
			const float D = FVector::Dist2D(A->GetActorLocation(), At);
			if (D < RadiusCm) A->Scare(At, Strength * (1.f - 0.5f * D / RadiusCm), true);
		}
}

void AKAPFauna::Stampede(int32 HerdIdx)
{
	if (!Herds.IsValidIndex(HerdIdx)) return;
	const FHerdInfo& H = Herds[HerdIdx];
	const float A = Rng.FRandRange(0.f, UE_TWO_PI);
	const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
	for (const auto& An : Animals)
		if (An.IsValid() && An->Herd == HerdIdx && !An->IsDead())
		{
			const FVector2D P = FVector2D(An->GetActorLocation()) / 100.0;
			FVector2D T = H.Home + Dir * (H.R * 0.9f) + (P - H.Home) * 0.3;
			T.X = FMath::Clamp(T.X, -1100.f, 1100.f); T.Y = FMath::Clamp(T.Y, -1100.f, 1100.f);
			An->StartPlay(T, 25.f);
		}
	UE_LOG(LogTemp, Display, TEXT("KAPFAUNA stampede herd %d"), HerdIdx);
}

void AKAPFauna::Tick(float Dt)
{
	Super::Tick(Dt);
	for (int32 i = Animals.Num() - 1; i >= 0; i--) if (!Animals[i].IsValid()) Animals.RemoveAtSwap(i);
	// атыс — ойыншы шулы (Noise): 150 м ішіндегі жануарлар қашады
	if (const AKAPCharacter* P = AKAPCharacter::Get(this))
	{
		if (P->Noise > 0.9f && LastNoise <= 0.9f) Alarm(P->GetActorLocation(), 15000.f, 1.f);
		LastNoise = P->Noise;
	}
	// найзағай — жақын түссе үрейленеді
	if (const AWeatherManager* WX = AWeatherManager::Get(GetWorld()))
	{
		if (WX->Env.StrikeSeq != LastStrike) { LastStrike = WX->Env.StrikeSeq; Alarm(WX->Env.StrikeAt, 40000.f, 0.8f); }
		// түнде тау етегінен қасқырлар шығады (2 топ × 3)
		WolfT -= Dt;
		if (WolfT <= 0.f)
		{
			WolfT = 30.f;
			int32 Alive = 0; for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->Kind == ENPCKind::Wolf && !It->bDead) Alive++;
			if (WX->Env.bNight && Alive < 6 && WolvesSpawned < 60)
			{
				static const FVector2D Dens[3] = { FVector2D(-1080, -700), FVector2D(1060, 900), FVector2D(-700, 1080) };
				const FVector2D C = Dens[Rng.RandRange(0, 2)];
				FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				for (int32 k = 0; k < 3; k++)
				{
					const FVector2D Pm = C + FVector2D(Rng.FRandRange(-12.f, 12.f), Rng.FRandRange(-12.f, 12.f));
					const FVector At(Pm.X * 100, Pm.Y * 100, KAPVis::GroundZ(GetWorld(), Pm.X * 100, Pm.Y * 100, 0.f) + 100.f);
					if (AKAPNPC* N = GetWorld()->SpawnActor<AKAPNPC>(At, FRotator(0, Rng.FRandRange(0.f, 360.f), 0), SP)) { N->Setup(ENPCKind::Wolf, TEXT("Қасқыр")); WolvesSpawned++; }
				}
				UE_LOG(LogTemp, Display, TEXT("KAPFAUNA wolves at (%.0f, %.0f)"), C.X, C.Y);
			}
		}
	}
	// жылқылар кейде өздігінен шабады (ойнақ) — бір табын, 2–4 минутта бір
	PlayT -= Dt;
	if (PlayT <= 0.f)
	{
		PlayT = Rng.FRandRange(120.f, 240.f);
		TArray<int32> HorseHerds; for (int32 h = 0; h < Herds.Num(); h++) if (Herds[h].Kind == EAnimalKind::Horse) HorseHerds.Add(h);
		if (HorseHerds.Num()) Stampede(HorseHerds[Rng.RandRange(0, HorseHerds.Num() - 1)]);
	}
}
