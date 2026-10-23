#include "KAPLife.h"
#include "KAPNPC.h"
#include "KAPAnimal.h"
#include "KAP/World/KAPTerrain.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Audio/KAPAudio.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"

// ================================================================ физикалық реакция
UKAPHitReactComponent::UKAPHitReactComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UKAPHitReactComponent::BeginPlay()
{
	Super::BeginPlay();
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C || !C->GetMesh() || !C->GetMesh()->GetPhysicsAsset()) { SetComponentTickEnabled(false); return; }
	Mesh = C->GetMesh();
	PhysAnim = NewObject<UPhysicalAnimationComponent>(GetOwner());
	PhysAnim->RegisterComponent();
	PhysAnim->SetSkeletalMeshComponent(C->GetMesh());
	// AKAPNPC::TakeDamage оқиға таратпайды — соңғы жарақат уақыты (HurtTime) мен Hp өзгерісін бақылаймыз
	if (const AKAPNPC* N = Cast<AKAPNPC>(GetOwner())) { LastHurt = N->HurtTime; LastHp = N->Hp; }
}

FName UKAPHitReactComponent::ChainOf(FName Bone) const
{
	const FString B = Bone.ToString().ToLower();
	if (B.Contains(TEXT("head")) || B.Contains(TEXT("neck"))) return FName("spine_03");
	if (B.EndsWith(TEXT("_l")) && (B.Contains(TEXT("arm")) || B.Contains(TEXT("hand")) || B.Contains(TEXT("clavicle")))) return FName("clavicle_l");
	if (B.EndsWith(TEXT("_r")) && (B.Contains(TEXT("arm")) || B.Contains(TEXT("hand")) || B.Contains(TEXT("clavicle")))) return FName("clavicle_r");
	if (B.EndsWith(TEXT("_l")) && (B.Contains(TEXT("thigh")) || B.Contains(TEXT("calf")) || B.Contains(TEXT("foot")))) return FName("thigh_l");
	if (B.EndsWith(TEXT("_r")) && (B.Contains(TEXT("thigh")) || B.Contains(TEXT("calf")) || B.Contains(TEXT("foot")))) return FName("thigh_r");
	return FName("spine_01");
}

void UKAPHitReactComponent::OnDamage(AActor* Damaged, float Damage, const UDamageType* Type, AController* Inst, AActor* Causer)
{
	const AKAPNPC* N = Cast<AKAPNPC>(GetOwner());
	if (!N || N->bDead || N->bKnockedOut || !Mesh.IsValid()) return;
	// қай сүйекке тиді: ойыншы атса — көздеу сызығы бойынша дене физикасына сәуле
	FName Bone = FName("spine_02");
	FVector Dir = Causer ? (GetOwner()->GetActorLocation() - Causer->GetActorLocation()).GetSafeNormal() : -GetOwner()->GetActorForwardVector();
	if (const AKAPCharacter* P = Cast<AKAPCharacter>(Causer))
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
			if (PC->PlayerCameraManager)
			{
				const FVector O = PC->PlayerCameraManager->GetCameraLocation();
				Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
				FHitResult H;
				if (Mesh->LineTraceComponent(H, O, O + Dir * 30000.f, FCollisionQueryParams(SCENE_QUERY_STAT(KAPHitBone), false))) Bone = H.BoneName;
			}
	React(Bone, Dir * FMath::Clamp(Damage * 45.f, 400.f, 2600.f), FMath::Clamp(Damage / 30.f, 0.4f, 1.f));
}

void UKAPHitReactComponent::React(FName Bone, const FVector& Impulse, float Strength)
{
	USkeletalMeshComponent* M = Mesh.Get();
	const AKAPNPC* N = Cast<AKAPNPC>(GetOwner());
	if (!M || !PhysAnim || !N || N->bDead || N->bKnockedOut || M->IsSimulatingPhysics(FName("pelvis"))) return;
	if (M->GetBodyInstance(Bone) == nullptr) Bone = FName("spine_02");
	const FName NewChain = ChainOf(Bone);
	if (Chain != NAME_None && Chain != NewChain) EndReact();
	Chain = NewChain;
	// бұлшықеттер: соққыдан кейін сүйектерді анимация позасына қайта тартады (күшті — тез қалпына келеді)
	FPhysicalAnimationData D;
	D.bIsLocalSimulation = false;
	D.OrientationStrength = 1600.f * (1.2f - Strength * 0.6f);
	D.AngularVelocityStrength = 160.f;
	D.PositionStrength = 1200.f * (1.2f - Strength * 0.6f);
	D.VelocityStrength = 120.f;
	PhysAnim->ApplyPhysicalAnimationSettingsBelow(Chain, D, true);
	M->SetAllBodiesBelowSimulatePhysics(Chain, true, true);
	M->SetAllBodiesBelowPhysicsBlendWeight(Chain, 1.f, false, true);
	M->AddImpulse(Impulse, Bone, true);
	T = 0.f; Hold = 0.12f + 0.2f * Strength; Fade = 0.45f + 0.5f * Strength;
	Reactions++;
}

void UKAPHitReactComponent::EndReact()
{
	if (USkeletalMeshComponent* M = Mesh.Get())
		if (Chain != NAME_None)
		{
			const AKAPNPC* N = Cast<AKAPNPC>(GetOwner());
			// өлсе/талса — NPC-дің толық рагдоллы өзі басқарады, біз физиканы өшірмейміз
			if (!N || (!N->bDead && !N->bKnockedOut)) { M->SetAllBodiesBelowPhysicsBlendWeight(Chain, 0.f, false, true); M->SetAllBodiesBelowSimulatePhysics(Chain, false, true); }
		}
	Chain = NAME_None; T = -1.f;
}

void UKAPHitReactComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	const AKAPNPC* N = Cast<AKAPNPC>(GetOwner());
	if (!N) return;
	if ((N->bDead || N->bKnockedOut) && T >= 0.f) { Chain = NAME_None; T = -1.f; return; }
	if (T >= 0.f && Mesh.IsValid())
	{
		T += Dt;
		const float W = T < Hold ? 1.f : FMath::Clamp(1.f - (T - Hold) / Fade, 0.f, 1.f);
		Mesh->SetAllBodiesBelowPhysicsBlendWeight(Chain, W, false, true);
		if (W <= 0.f) EndReact();
	}
	// жаңа жарақат: тиген сүйекті ойыншының көздеу сызығынан табамыз
	if (N->HurtTime > LastHurt + 0.001f)
	{
		LastHurt = N->HurtTime;
		const float Dmg = FMath::Max(5.f, LastHp - N->Hp);
		const AKAPCharacter* Pl = AKAPCharacter::Get(this);
		OnDamage(GetOwner(), Dmg, nullptr, nullptr, Pl && FVector::Dist(Pl->GetActorLocation(), N->GetActorLocation()) < 30000.f && Pl->Noise > 0.5f ? (AActor*)Pl : nullptr);
	}
	LastHp = N->Hp;
	// ойыншы жүгіріп келіп соғылса — сүрінеді
	PushCd -= Dt;
	if (PushCd <= 0.f && !N->bDead)
		if (const AKAPCharacter* P = AKAPCharacter::Get(this))
		{
			const FVector V = P->GetVelocity();
			if (V.Size2D() > 450.f && FVector::Dist(P->GetActorLocation(), N->GetActorLocation()) < 85.f)
			{
				PushCd = 1.2f;
				React(FName("spine_02"), V * 0.55f + FVector(0, 0, 80.f), 0.7f);
				if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Impact(N->GetActorLocation(), 0, 0.4f);
			}
		}
}

// ================================================================ күн тәртібі
AKAPLife::AKAPLife()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

AKAPLife* AKAPLife::Get(const UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AKAPLife> It(const_cast<UWorld*>(W)); It; ++It) return *It;
	return nullptr;
}

void AKAPLife::Scan()
{
	// жаңа NPC-лер: барлық адамға физикалық реакция; ауыл тұрғындарына — күн тәртібі
	TArray<FVector2D> Villages, Fires;
	if (UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this))
		D->Locations->ForeachRow<FKAPLocationRow>(TEXT("KAP"), [&](const FName&, const FKAPLocationRow& L) { if (L.Type == FName("village")) Villages.Add(FVector2D(L.X, L.Y)); });
	for (KAPReg::TIter<AKAPStructure> It(GetWorld(), KAPReg::Structures()); It; ++It) if (It->bFire) Fires.Add(FVector2D(It->GetActorLocation()) / 100.0);
	for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
	{
		AKAPNPC* N = *It;
		if (!N->IsHuman() || N->bDead) continue;
		if (!N->FindComponentByClass<UKAPHitReactComponent>())
		{
			UKAPHitReactComponent* C = NewObject<UKAPHitReactComponent>(N);
			C->RegisterComponent();
		}
		const bool bCivil = N->Kind == ENPCKind::Villager || N->Kind == ENPCKind::Boy || N->Kind == ENPCKind::Girl;
		const bool bGeneric = N->TalkId.IsNone() || N->TalkId == FName("villager") || N->TalkId == FName("child_boy") || N->TalkId == FName("child_girl");
		if (!bCivil || !bGeneric) continue; // сюжет кейіпкерлері өз орнында қалады
		bool bKnown = false; for (const FPerson& P : People) if (P.N.Get() == N) bKnown = true;
		if (bKnown) continue;
		FPerson P; P.N = N; P.OrigHome = N->Home; P.OrigR = N->WanderR; P.HomeSpot = N->Home;
		const FVector2D Pm = FVector2D(N->GetActorLocation()) / 100.0;
		float Best = 1e9f; for (const FVector2D& V : Villages) { const float Dd = FVector2D::Distance(V, Pm); if (Dd < Best) { Best = Dd; P.Village = V; } }
		if (Best > 250.f) continue; // ауылдан алыс адамдар (жол бойы, ұңғыма) — өз ісімен
		// ең жақын егістік — жұмыс орны; от басы — ауылдағы от
		float Bf = 1e9f; for (const KAPTerrain::FField& F : KAPTerrain::FieldStore()) { const FVector2D C((F.X0 + F.X1) * 0.5f, (F.Y0 + F.Y1) * 0.5f); const float Dd = FVector2D::Distance(C, P.Village) + FMath::FRandRange(0.f, 60.f); if (Dd < Bf) { Bf = Dd; P.Field = C; } }
		P.Fire = P.Village; for (const FVector2D& F : Fires) if (FVector2D::Distance(F, P.Village) < 120.f) P.Fire = F;
		// әр ауылдың бір ересек тұрғыны — шопан (жақын қой отарына)
		if (N->Kind == ENPCKind::Villager)
			if (AKAPFauna* Fa = AKAPFauna::Get(GetWorld()))
				for (int32 h = 0; h < Fa->Herds.Num(); h++)
					if (Fa->Herds[h].Kind == EAnimalKind::Sheep && FVector2D::Distance(Fa->Herds[h].Home, P.Village) < 300.f)
					{
						bool bTaken = false; for (const FPerson& O : People) if (O.Flock == h) bTaken = true;
						if (!bTaken) { P.Flock = h; break; }
					}
		People.Add(P);
	}
}

void AKAPLife::Plan(FPerson& P, double Hour, float Rain, float Thunder)
{
	AKAPNPC* N = P.N.Get();
	if (!N || N->bDead || N->bKnockedOut || N->Task != ENPCTask::None || N->GoTo.IsSet() || N->bFollowPlayer) return;
	const bool bKid = N->Kind == ENPCKind::Boy || N->Kind == ENPCKind::Girl;
	EGoal G = EGoal::Home;
	if (Rain > 0.45f || Thunder > 0.3f) G = EGoal::Shelter;                       // жаңбыр, найзағай — баспанаға
	else if (Hour >= 6.5 && Hour < 12.5) G = P.Flock >= 0 ? EGoal::Herd : bKid ? EGoal::Play : EGoal::Field; // таң — жұмыс
	else if (Hour >= 12.5 && Hour < 14.0) G = EGoal::Home;                       // түскі ас
	else if (Hour >= 14.0 && Hour < 18.5) G = P.Flock >= 0 ? EGoal::Herd : bKid ? EGoal::Play : EGoal::Field;
	else if (Hour >= 18.5 && Hour < 22.0) G = EGoal::Fire;                        // кешкі от басы
	else G = EGoal::Home;                                                         // түн
	FVector2D Spot = P.HomeSpot; float R = 4.f;
	switch (G)
	{
	case EGoal::Field: Spot = P.Field; R = 22.f; break;
	case EGoal::Herd:
		if (AKAPFauna* Fa = AKAPFauna::Get(GetWorld()))
		{
			FVector2D Sum(0, 0); int32 Cn = 0;
			for (const auto& A : Fa->Animals) if (A.IsValid() && A->Herd == P.Flock && !A->IsDead()) { Sum += FVector2D(A->GetActorLocation()) / 100.0; Cn++; }
			if (Cn) { Spot = Sum / Cn + FVector2D(-6, 4); R = 12.f; }
			// шопан қасқырды көрсе — айғайлайды, отар қашады
			P.ShoutCd -= 0.5f;
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
				if (It->Kind == ENPCKind::Wolf && !It->bDead && FVector::Dist2D(It->GetActorLocation(), N->GetActorLocation()) < 6000.f && P.ShoutCd <= 0.f)
				{
					P.ShoutCd = 12.f;
					if (UKAPAudio* Au = UKAPAudio::Get(this)) Au->Yell(N->GetActorLocation() + FVector(0, 0, 160), false);
					Fa->Alarm(It->GetActorLocation(), 5000.f, 1.f);
					break;
				}
		}
		break;
	case EGoal::Fire: Spot = P.Fire + FVector2D(FMath::FRandRange(-3.f, 3.f), FMath::FRandRange(-3.f, 3.f)); R = 3.f; break;
	case EGoal::Play: Spot = P.Village + FVector2D(20, -15); R = 18.f; break;
	case EGoal::Shelter: Spot = P.HomeSpot; R = 2.f; break;
	default: break;
	}
	if (G != P.Goal) UE_LOG(LogTemp, Verbose, TEXT("KAPLIFE %s → %d"), *N->DisplayName, (int32)G);
	P.Goal = G;
	N->Home = Spot; N->WanderR = R;
}

void AKAPLife::Tick(float Dt)
{
	Super::Tick(Dt);
	ScanT -= Dt;
	if (ScanT <= 0.f) { ScanT = 3.f; Scan(); }
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	if (!GI) return;
	const double Hour = FMath::Fmod(GI->S.Time, 24.0);
	for (int32 i = People.Num() - 1; i >= 0; i--)
	{
		if (!People[i].N.IsValid()) { People.RemoveAtSwap(i); continue; }
		Plan(People[i], Hour, WX ? WX->Env.Rain : 0.f, WX ? WX->Env.Thunder : 0.f);
	}
}

void AKAPLife::Dump() const
{
	int32 Cnt[8] = {}; int32 React = 0, Comp = 0;
	for (const FPerson& P : People) Cnt[(int32)P.Goal]++;
	for (TActorIterator<AKAPNPC> It(GetWorld()); It; ++It) if (const UKAPHitReactComponent* C = It->FindComponentByClass<UKAPHitReactComponent>()) { Comp++; React += C->Reactions; }
	UE_LOG(LogTemp, Display, TEXT("KAPLIFE people=%d field=%d herd=%d home=%d fire=%d shelter=%d play=%d | hitreact comps=%d reactions=%d"), People.Num(), Cnt[1], Cnt[2], Cnt[3], Cnt[4], Cnt[5], Cnt[6], Comp, React);
}
