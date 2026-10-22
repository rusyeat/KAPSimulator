#include "KAPDoor.h"
#include "KAPStructure.h"
#include "KAPVisuals.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/CapsuleComponent.h"

namespace { float DoorEase(float A) { return A * A * (3.f - 2.f * A); } }

AKAPDoor::AKAPDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
	Hinge = CreateDefaultSubobject<USceneComponent>("Hinge");
	Hinge->SetupAttachment(RootComponent);
}

void AKAPDoor::Build(float Width, float Height, float InOpenYaw, int32 Sound, const FKAPMat& Panel, const FKAPMat& Handle)
{
	W = Width; H = Height; OpenYaw = InOpenYaw; SoundType = Sound;
	// полотно: ойыншыны да, NPC-ді де, көлікті де өткізбейді (WorldDynamic)
	Leaf = KAPVis::Add(this, Hinge, EKAPShape::Cube, FVector(W * 0.5f, 0, H * 0.5f), FVector(W - 4.f, 5.f, H - 2.f), Panel, FRotator::ZeroRotator, true);
	Leaf->SetCollisionObjectType(ECC_WorldDynamic);
	Leaf->CanCharacterStepUpOn = ECB_No;
	// көлденең жолақтар (полотно тегіс болмасын), екі жақта тұтқа, топса ілмектері
	for (float Z : { H * 0.3f, H * 0.72f }) for (float S : { -1.f, 1.f })
		KAPVis::Add(this, Hinge, EKAPShape::Cube, FVector(W * 0.5f, S * 3.f, Z), FVector(W - 16.f, 2.f, 10.f), Panel, FRotator::ZeroRotator, false);
	for (float S : { -1.f, 1.f })
	{
		KAPVis::Add(this, Hinge, EKAPShape::Cube, FVector(W - 13.f, S * 3.5f, 100.f), FVector(5.f, 2.f, 15.f), Handle, FRotator::ZeroRotator, false);
		KAPVis::Add(this, Hinge, EKAPShape::Cube, FVector(W - 19.f, S * 7.f, 104.f), FVector(15.f, 3.f, 3.f), Handle, FRotator::ZeroRotator, false);
	}
	for (float Z : { 28.f, H - 28.f }) KAPVis::Add(this, Hinge, EKAPShape::Cylinder, FVector(0.f, 0, Z), FVector(5.f, 5.f, 16.f), Handle, FRotator::ZeroRotator, false);
}

void AKAPDoor::BeginPlay()
{
	Super::BeginPlay();
	KAPReg::Add(KAPReg::Interactables(), (AActor*)this);
}

void AKAPDoor::EndPlay(const EEndPlayReason::Type Reason)
{
	KAPReg::Remove(KAPReg::Interactables(), (AActor*)this);
	Super::EndPlay(Reason);
}

FVector AKAPDoor::Mid() const
{
	return GetActorTransform().TransformPosition(FVector(W * 0.5f, 0.f, 100.f));
}

FString AKAPDoor::GetInteractText(AKAPCharacter*) const
{
	return bWantOpen ? TEXT("Есікті жабу") : TEXT("Есікті ашу");
}

void AKAPDoor::Interact(AKAPCharacter* By)
{
	bNpcOpened = false;
	SetOpen(!bWantOpen, By ? By->GetActorLocation() : Mid(), TEXT("player"));
}

void AKAPDoor::SetOpen(bool bOpen, const FVector& From, const FString& Who)
{
	if (bOpen == bWantOpen) return;
	bWantOpen = bOpen;
	UE_LOG(LogTemp, Display, TEXT("KAPDOOR %s %s by %s"), *GetName(), bOpen ? TEXT("open") : TEXT("close"), *Who);
	// жабық тұрған есік ашқан адамнан әрі қарай ашылады (ішке не сыртқа) — полотно оны қақпайды
	if (bOpen && Ang <= 0.001f) Dir = FVector::DotProduct(From - GetActorLocation(), GetActorRightVector()) > 0.f ? -1.f : 1.f;
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->DoorSfx(Mid(), SoundType, bOpen ? 0 : 1);
	SetActorTickInterval(0.f);
}

bool AKAPDoor::Blocked(float A) const
{
	const FTransform Xf = GetActorTransform();
	const FVector P0 = Xf.GetLocation();
	const FVector Ax = Xf.TransformVectorNoScale(FRotator(0, Dir * OpenYaw * DoorEase(A), 0).Vector());
	const FVector2D S0(P0.X, P0.Y), S1 = S0 + FVector2D(Ax.X, Ax.Y) * W;
	auto Hit = [&](const ACharacter* C)
	{
		if (!C || C->IsHidden()) return false;
		const UCapsuleComponent* Cp = C->GetCapsuleComponent();
		const FVector L = C->GetActorLocation();
		const float Hh = Cp->GetScaledCapsuleHalfHeight();
		if (L.Z - Hh > P0.Z + H || L.Z + Hh < P0.Z) return false;
		const FVector2D Q(L.X, L.Y);
		return FVector2D::Distance(Q, FMath::ClosestPointOnSegment2D(Q, S0, S1)) < Cp->GetScaledCapsuleRadius() + 4.f;
	};
	if (Hit(AKAPCharacter::Get(this))) return true;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
		if (!It->bDead && FVector::DistSquared2D(It->GetActorLocation(), P0) < FMath::Square(W + 120.f) && Hit(*It)) return true;
	return false;
}

void AKAPDoor::ApplyAngle()
{
	float Yaw = Dir * OpenYaw * DoorEase(Ang);
	// құбыжық соққанда жабық есік жақтауында дірілдейді
	if (ShakeT > 0.f && Ang < 0.05f) Yaw += FMath::Sin(GetWorld()->GetTimeSeconds() * 70.f) * 2.2f * (ShakeT / 0.25f);
	Hinge->SetRelativeRotation(FRotator(0, Yaw, 0));
}

void AKAPDoor::CheckNPCs()
{
	const FVector M = Mid();
	const AKAPNPC* Human = nullptr; const AKAPNPC* Beast = nullptr;
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
	{
		const AKAPNPC* N = *It;
		if (N->bDead || N->bKnockedOut || N->IsHidden()) continue;
		const FVector L = N->GetActorLocation();
		if (FMath::Abs(L.Z - M.Z) > 250.f) continue;
		const float D = FVector::Dist2D(L, M);
		if (D > 280.f) continue;
		const bool bBeast = N->bStalker || N->Kind == ENPCKind::Irradiated || N->Kind == ENPCKind::Wolf || N->Kind == ENPCKind::Worm;
		if (bBeast) { if (D < 170.f) Beast = N; continue; }
		// адам: есікке қарай жүріп келе жатса ғана ашады (жанынан өтіп бара жатқан не тұрған адам ашпайды)
		const FVector V = N->GetVelocity();
		if (V.Size2D() > 40.f && FVector::DotProduct(V.GetSafeNormal2D(), (M - L).GetSafeNormal2D()) > 0.35f) Human = N;
		else if (bNpcOpened && D < 100.f) NpcAway = 0.f; // ойықта тұр — жабылмайды
	}
	if (Human)
	{
		NpcAway = 0.f;
		if (!bWantOpen) { SetOpen(true, Human->GetActorLocation(), Human->DisplayName); bNpcOpened = true; }
	}
	else if (bNpcOpened && bWantOpen)
	{
		NpcAway += 0.25f;
		if (NpcAway > 1.5f) { bNpcOpened = false; SetOpen(false, M, TEXT("auto")); }
	}
	// құбыжық жабық есікті ұрады: тарс, полотно дірілдейді
	if (Beast && Ang < 0.02f && KnockCd <= 0.f)
	{
		KnockCd = FMath::FRandRange(0.8f, 2.2f);
		Knock(FMath::FRandRange(0.75f, 1.f));
	}
}

void AKAPDoor::Knock(float Strength)
{
	ShakeT = 0.25f; Knocks++;
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Impact(Mid(), SoundType == 0 ? 1 : 2, Strength);
	SetActorTickInterval(0.f);
}

void AKAPDoor::Tick(float Dt)
{
	Super::Tick(Dt);
	Check -= Dt; KnockCd -= Dt;
	if (Check <= 0.f) { Check = 0.25f; CheckNPCs(); }
	const float Step = FMath::Min(Dt, 0.1f);
	const float Target = bWantOpen ? 1.f : 0.f;
	bool bMoved = false;
	if (Ang != Target)
	{
		// ашылу 0.8 с, жабылу 0.65 с; жолында адам тұрса — тоқтап күтеді
		const float Next = FMath::FInterpConstantTo(Ang, Target, Step, bWantOpen ? 1.f / 0.8f : 1.f / 0.65f);
		if (!Blocked(Next) || Blocked(Ang)) { Ang = Next; bMoved = true; }
		if (bMoved && Ang <= 0.f && !bWantOpen) if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->DoorSfx(Mid(), SoundType, 2); // ілгекке түсті
	}
	if (ShakeT > 0.f) { ShakeT = FMath::Max(0.f, ShakeT - Step); bMoved = true; }
	if (bMoved) ApplyAngle();
	const float Want = (Ang == Target && ShakeT <= 0.f) ? 0.25f : 0.f; // тыныш кезде сирек тексеріс
	if (PrimaryActorTick.TickInterval != Want) SetActorTickInterval(Want);
}

// ---- ғимаратқа есік қосу (вагон, кеңсе) ----
AKAPDoor* AKAPStructure::AddDoor(FVector LocalHinge, float LocalYaw, float Width, float Height, float OpenYaw, int32 Sound, const FKAPMat& Panel, const FKAPMat& Handle)
{
	UWorld* Wd = GetWorld();
	if (!Wd) return nullptr;
	const FTransform Xf = GetActorTransform();
	FActorSpawnParameters P; P.Owner = this; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AKAPDoor* D = Wd->SpawnActor<AKAPDoor>(Xf.TransformPosition(LocalHinge), (Xf.GetRotation() * FRotator(0, LocalYaw, 0).Quaternion()).Rotator(), P);
	if (!D) return nullptr;
	D->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	D->Build(Width, Height, OpenYaw, Sound, Panel, Handle);
	Doors.Add(D);
	return D;
}

void AKAPStructure::Destroyed()
{
	for (const TWeakObjectPtr<AKAPDoor>& D : Doors) if (D.IsValid()) D->Destroy();
	Super::Destroyed();
}
