#include "KAPNPC.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPVisuals.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPFXManager.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/World/KAPProjectile.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAP/Core/KAPRegistry.h"
#include "Components/CapsuleComponent.h"
#include "KAP/Core/KAPRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "KAP/Core/KAPRegistry.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "KAP/Core/KAPRegistry.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SpotLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "KAP/Core/KAPRegistry.h"
#include "Engine/DamageEvents.h"
#include "KAP/Core/KAPRegistry.h"
#include "EngineUtils.h"
#include "KAP/Core/KAPRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "KAP/Core/KAPRegistry.h"

double AKAPNPC::TrainMs = 0;
static KAPAI::FQPolicy GPolicies[3];
static bool GTrained = false;

void AKAPNPC::TrainPolicies()
{
	if (GTrained) return;
	const double T0 = FPlatformTime::Seconds();
	GPolicies[0].Train(KAPAI::EQProfile::Militia, 25000, 11);
	GPolicies[1].Train(KAPAI::EQProfile::Bandit, 25000, 12);
	GPolicies[2].Train(KAPAI::EQProfile::Monster, 25000, 13);
	TrainMs = (FPlatformTime::Seconds() - T0) * 1000.0;
	GTrained = true;
}
const KAPAI::FQPolicy& AKAPNPC::GetPolicy(KAPAI::EQProfile P) { TrainPolicies(); return GPolicies[(int32)P]; }

AKAPNPC::AKAPNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 360, 0);
	GetCharacterMovement()->bRunPhysicsWithNoController = true; // контроллерсіз қозғалады (өз AI-іміз)
	bUseControllerRotationYaw = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void AKAPNPC::BeginPlay()
{
	Super::BeginPlay();
	LastPos = GetActorLocation();
	// көрінбейтін (артта, қабырға сыртында) NPC анимациясы есептелмейді; кішкентай көрінсе — сиректеу
	if (USkeletalMeshComponent* SK = GetMesh())
	{
		SK->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		SK->bEnableUpdateRateOptimizations = true;
		// тірі кезде қаңқа физикасы керек емес (оқ капсулаға тиеді); өлгенде Kill() рагдоллды қосады.
		// Әйтпесе 157 NPC × ~23 буын = 3600 физика буыны әр кадр сайын синхрондалады
		SK->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// жылжымалы қаңқа сәуле трассировкасына енбейді (әр кадр BLAS жаңарту — қымбат)
		SK->SetVisibleInRayTracing(false);
	}
	KAPReg::Add(KAPReg::NPCs(), this);
	KAPReg::Add(KAPReg::Interactables(), (AActor*)this);
}

void AKAPNPC::EndPlay(const EEndPlayReason::Type R)
{
	KAPReg::Remove(KAPReg::NPCs(), this);
	KAPReg::Remove(KAPReg::Interactables(), (AActor*)this);
	Super::EndPlay(R);
}

static FKAPMat ShirtOf(ENPCKind K)
{
	switch (K)
	{
	case ENPCKind::Worker: return FKAPMat::Paint(FLinearColor(0.8f, 0.3f, 0.03f));
	case ENPCKind::Militia: return FKAPMat::Paint(FLinearColor(0.2f, 0.25f, 0.12f));
	case ENPCKind::Guard: return FKAPMat::Paint(FLinearColor(0.08f, 0.12f, 0.25f));
	case ENPCKind::Bandit: return FKAPMat::Paint(FLinearColor(0.05f, 0.04f, 0.04f));
	case ENPCKind::Elder: return FKAPMat::Paint(FLinearColor(0.2f, 0.14f, 0.08f));
	case ENPCKind::Mother: return FKAPMat::Paint(FLinearColor(0.15f, 0.08f, 0.15f));
	default: return FKAPMat::Paint(FLinearColor(0.35f, 0.3f, 0.25f));
	}
}

void AKAPNPC::Setup(ENPCKind InKind, const FString& InName, float Scale, bool bInBoss)
{
	Kind = InKind; DisplayName = InName; bBoss = bInBoss;
	Home = KAP::CM2M(GetActorLocation());
	USkeletalMeshComponent* SK = GetMesh();
	bFemale = InKind == ENPCKind::Mother || InKind == ENPCKind::Girl || (InKind == ENPCKind::Villager && FMath::FRand() < 0.5f);
	if (IsChild())
	{
		Scale *= 0.67f;
		WalkSpeed = 150.f; RunSpeed = 260.f; WanderR = 4.f;
		bSick = InKind == ENPCKind::SickChild;
		if (bSick) WanderR = 0.f;
	}
	switch (InKind)
	{
	case ENPCKind::Worker: Faction = EKAPFaction::Worker; break;
	case ENPCKind::Militia: Faction = EKAPFaction::Militia; Policy = KAPAI::EQProfile::Militia; Hp = 100; Dmg = 10; RangeM = 55; break;
	case ENPCKind::Guard: Faction = EKAPFaction::Guard; Policy = KAPAI::EQProfile::Militia; Hp = 100; Dmg = 10; RangeM = 55; break;
	case ENPCKind::Police: Faction = EKAPFaction::Militia; Policy = KAPAI::EQProfile::Militia; Hp = 110; Dmg = 9; RangeM = 50; break;
	case ENPCKind::Soldier: Faction = EKAPFaction::Guard; Policy = KAPAI::EQProfile::Militia; Hp = 150; Dmg = 14; RangeM = 75; break;
	case ENPCKind::Bandit: Faction = EKAPFaction::Bandit; Policy = KAPAI::EQProfile::Bandit; Hp = 100; Dmg = 9; RangeM = 55; break;
	case ENPCKind::Irradiated: Faction = EKAPFaction::Monster; bMelee = true; Hp = 160; Dmg = 20; RangeM = 1.9f; WalkSpeed = 220; RunSpeed = 340; break;
	case ENPCKind::Wolf: Faction = EKAPFaction::Monster; bMelee = true; Hp = 45; Dmg = 8; RangeM = 1.7f; WalkSpeed = 400; RunSpeed = 1000; break;
	case ENPCKind::Worm: Faction = EKAPFaction::Monster; bMelee = true; Hp = 90; Dmg = 16; RangeM = 2.6f; WalkSpeed = 500; RunSpeed = 700; break;
	default: Faction = EKAPFaction::Civil; break;
	}
	if (bBoss) { Hp = 900; Dmg = 30; RangeM *= 1.9f; }
	MaxHp = Hp;
	SetActorScale3D(FVector(Scale));
	if (IsHuman() && SetupSmoothHuman(InKind, Scale)) {}
	else if (IsHuman())
	{
		if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, bFemale ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple") : TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
		{
			SK->SetSkeletalMesh(M);
			SK->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
			if (UClass* ABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"))) SK->SetAnimInstanceClass(ABP);
			// манекенге фракция түсіндегі комбинезон (бүкіл денеге PBR материалы)
			FLinearColor Suit = FLinearColor(0.42f, 0.36f, 0.28f);
			switch (InKind)
			{
			case ENPCKind::Worker: Suit = FLinearColor(0.75f, 0.28f, 0.04f); break;
			case ENPCKind::Militia: Suit = FLinearColor(0.2f, 0.23f, 0.12f); break;
			case ENPCKind::Guard: Suit = FLinearColor(0.08f, 0.11f, 0.2f); break;
			case ENPCKind::Bandit: Suit = FLinearColor(0.05f, 0.045f, 0.04f); break;
			case ENPCKind::Elder: Suit = FLinearColor(0.22f, 0.15f, 0.09f); break;
			case ENPCKind::Mother: Suit = FLinearColor(0.16f, 0.08f, 0.14f); break;
			case ENPCKind::Survivor: Suit = FLinearColor(0.3f, 0.28f, 0.24f); break;
			case ENPCKind::Irradiated: Suit = FLinearColor(0.2f, 0.3f, 0.08f); break;
			case ENPCKind::Villager: { const FLinearColor V[4] = { FLinearColor(0.35f, 0.2f, 0.4f), FLinearColor(0.15f, 0.3f, 0.42f), FLinearColor(0.45f, 0.15f, 0.12f), FLinearColor(0.3f, 0.36f, 0.18f) }; Suit = V[FMath::RandRange(0, 3)]; break; }
			default: break;
			}
			FKAPMat SM = FKAPMat::Paint(Suit, 0.85f); SM.Grime = 0.6f;
			UMaterialInterface* HumM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Human.M_KAP_Human"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (HumM)
			{
				// тері реңкі (қазақ даласының тұрғындары: ашық-қоңырдан қараторыға дейін), шаш, шалбар
				static const FLinearColor Skins[5] = { FLinearColor(0.62f, 0.42f, 0.3f), FLinearColor(0.55f, 0.36f, 0.24f), FLinearColor(0.47f, 0.3f, 0.19f), FLinearColor(0.68f, 0.5f, 0.38f), FLinearColor(0.4f, 0.25f, 0.16f) };
				const FLinearColor SkinC = Skins[FMath::RandRange(0, 4)];
				const bool bOld = InKind == ENPCKind::Elder || (InKind == ENPCKind::Villager && FMath::FRand() < 0.3f);
				const FLinearColor HairC = bOld ? FLinearColor(0.45f, 0.43f, 0.4f) : FMath::FRand() < 0.8f ? FLinearColor(0.015f, 0.012f, 0.01f) : FLinearColor(0.08f, 0.045f, 0.02f);
				FLinearColor PantsC = FLinearColor(0.1f, 0.1f, 0.11f);
				if (InKind == ENPCKind::Worker) PantsC = Suit * 0.8f;           // комбинезон
				else if (InKind == ENPCKind::Militia) PantsC = Suit * 0.9f;
				else if (InKind == ENPCKind::Guard) PantsC = FLinearColor(0.05f, 0.06f, 0.1f);
				else if (InKind == ENPCKind::Villager || InKind == ENPCKind::Mother) PantsC = FMath::FRand() < 0.5f ? FLinearColor(0.12f, 0.14f, 0.22f) : FLinearColor(0.2f, 0.16f, 0.12f);
				for (int32 Mi = 0; Mi < SK->GetNumMaterials(); Mi++)
				{
					UMaterialInstanceDynamic* H = UMaterialInstanceDynamic::Create(HumM, SK);
					H->SetVectorParameterValue("Jacket", Suit);
					H->SetVectorParameterValue("Pants", PantsC);
					H->SetVectorParameterValue("Skin", SkinC);
					H->SetVectorParameterValue("Hair", HairC);
					H->SetScalarParameterValue("InvScale", 1.f / FMath::Max(0.1f, Scale));
					H->SetScalarParameterValue("Gloves", InKind == ENPCKind::Worker || InKind == ENPCKind::Militia || InKind == ENPCKind::Bandit ? 1.f : 0.f);
					H->SetScalarParameterValue("HairAmt", InKind == ENPCKind::Mother ? 0.f : 1.f);
					H->SetScalarParameterValue("StripeAmt", InKind == ENPCKind::Worker || InKind == ENPCKind::Guard ? 1.f : 0.f);
					if (InKind == ENPCKind::Irradiated)
					{
						// сұрғылт-жасыл, күйік тері; жыртылған, қап-қара күйген комбинезон
						H->SetVectorParameterValue("Skin", FLinearColor(0.32f, 0.36f, 0.26f));
						H->SetVectorParameterValue("Jacket", FLinearColor(0.07f, 0.08f, 0.05f));
						H->SetVectorParameterValue("Pants", FLinearColor(0.05f, 0.05f, 0.04f));
						H->SetScalarParameterValue("HairAmt", 0.f);
						H->SetScalarParameterValue("Gloves", 0.f);
					}
					H->SetScalarParameterValue("StripeGlow", 0.5f);
					H->SetScalarParameterValue("Grime", InKind == ENPCKind::Bandit || InKind == ENPCKind::Survivor ? 0.9f : 0.55f);
					SK->SetMaterial(Mi, H);
				}
			}
			else for (int32 Mi = 0; Mi < SK->GetNumMaterials(); Mi++) SK->SetMaterial(Mi, KAPVis::MakeMID(SK, SM));
		}
		// Фракцияға сай киім-кешек: дулыға/орамал, кеудеше, қару
		auto Attach = [&](EKAPShape S, FName Socket, FVector P, FVector Sz, FKAPMat Mt, FRotator R = FRotator::ZeroRotator)
		{
			UStaticMeshComponent* C = KAPVis::Add(this, SK, S, P, Sz, Mt, R, false);
			C->AttachToComponent(SK, FAttachmentTransformRules::KeepRelativeTransform, Socket);
			Parts.Add(C);
		};
		// ---- Киім-жабдық (сүйек кеңістігі: X — жоғары, Y — алға, Z — бүйірге) ----
		const FKAPMat Sh = ShirtOf(InKind);
		FKAPMat Rubber = FKAPMat::Paint(FLinearColor(0.035f, 0.035f, 0.032f), 0.7f); Rubber.DustTop = 0.4f;
		FKAPMat Canvas = FKAPMat::Paint(FLinearColor(0.24f, 0.22f, 0.15f), 0.95f); Canvas.Ribs = 0.15f; Canvas.RibPeriod = 6.f; Canvas.Grime = 0.9f;
		FKAPMat Strap = FKAPMat::Paint(FLinearColor(0.08f, 0.07f, 0.05f), 0.9f);
		const FKAPMat Metal = FKAPMat::RustySteel(FLinearColor(0.35f, 0.35f, 0.33f), 0.25f);
		FKAPMat GlassM = FKAPMat::Metal(FLinearColor(0.05f, 0.06f, 0.06f), 0.05f); GlassM.Grime = 0.2f;
		auto Helmet = [&](const FKAPMat& M, bool bBrim, bool bLamp)
		{
			Attach(EKAPShape::Sphere, "head", FVector(13, 1, 0), FVector(17, 29, 27), M);                                  // күмбез
			if (bBrim) Attach(EKAPShape::Cylinder, "head", FVector(7, 3, 0), FVector(32, 35, 2), M, FRotator(90, 0, 0));    // жиек
			if (bLamp)
			{
				Attach(EKAPShape::Cylinder, "head", FVector(14, 15, 0), FVector(6, 6, 5), Metal, FRotator(0, 0, 90));
				Attach(EKAPShape::Cylinder, "head", FVector(14, 17.5f, 0), FVector(5, 5, 1), FKAPMat::Glow(FLinearColor(1.f, 0.9f, 0.7f), 8), FRotator(0, 0, 90));
			}
		};
		auto Respirator = [&](const FKAPMat& FilterM, bool bGoggles)
		{
			Attach(EKAPShape::Cube, "head", FVector(-4, 11, 0), FVector(9, 7, 12), Rubber);                                // бет бөлігі
			for (float S : { -1.f, 1.f })
			{
				Attach(EKAPShape::Cylinder, "head", FVector(-6, 12, S * 7.5f), FVector(7.5f, 7.5f, 6), FilterM, FRotator(0, S * 25.f, 90)); // сүзгілер
				Attach(EKAPShape::Cube, "head", FVector(-1, 2, S * 9.5f), FVector(2.5f, 18, 1.5f), Strap);                    // белдік
			}
			if (bGoggles)
			{
				Attach(EKAPShape::Cube, "head", FVector(5, 11, 0), FVector(5, 3, 15), GlassM);
				Attach(EKAPShape::Cube, "head", FVector(5, 3, 0), FVector(3, 16, 18.5f), Strap);
			}
		};
		auto Vest = [&](const FKAPMat& M, bool bStripes, bool bPlates)
		{
			Attach(EKAPShape::Cube, "spine_04", FVector(-2, 12, 0), FVector(bPlates ? 30 : 34, bPlates ? 6 : 3, 30), M);   // алдыңғы
			Attach(EKAPShape::Cube, "spine_04", FVector(-2, -12, 0), FVector(34, 3, 30), M);                               // артқы
			for (float S : { -1.f, 1.f }) Attach(EKAPShape::Cube, "spine_05", FVector(4, 0, S * 9), FVector(4, 26, 6), M);  // иық
			if (bStripes)
			{
				FKAPMat Refl = FKAPMat::Paint(FLinearColor(0.85f, 0.85f, 0.8f), 0.3f); Refl.Emissive = FLinearColor(0.25f, 0.25f, 0.24f);
				for (float X : { -10.f, 6.f }) { Attach(EKAPShape::Cube, "spine_04", FVector(X, 13.8f, 0), FVector(3, 1, 30), Refl); Attach(EKAPShape::Cube, "spine_04", FVector(X, -13.8f, 0), FVector(3, 1, 30), Refl); }
			}
			if (bPlates) for (int32 i = 0; i < 3; i++) Attach(EKAPShape::Cube, "spine_03", FVector(-6, 15, -10 + i * 10), FVector(10, 4, 8), Canvas); // қалталар
		};
		auto Backpack = [&](const FKAPMat& M, bool bBig)
		{
			const float H = bBig ? 48.f : 34.f;
			Attach(EKAPShape::Cube, "spine_03", FVector(4, -22, 0), FVector(H, bBig ? 20 : 15, 32), M);
			Attach(EKAPShape::Cube, "spine_03", FVector(-6, -33, 0), FVector(H * 0.5f, 6, 24), M);                          // сыртқы қалта
			if (bBig) Attach(EKAPShape::Cylinder, "spine_03", FVector(H * 0.5f + 10, -22, 0), FVector(14, 14, 36), FKAPMat::Paint(FLinearColor(0.12f, 0.2f, 0.12f), 0.95f)); // төсеніш
			for (float S : { -1.f, 1.f }) Attach(EKAPShape::Cube, "spine_04", FVector(6, -2, S * 9), FVector(26, 30, 3.5f), Strap);
		};
		auto Belt = [&](bool bTools)
		{
			for (float S : { -1.f, 1.f }) Attach(EKAPShape::Cube, "pelvis", FVector(4, 6, S * 17), FVector(10, 9, 6), Canvas, FRotator(0, 0, 0));
			if (bTools) Attach(EKAPShape::Cube, "pelvis", FVector(-2, -2, 20), FVector(16, 4, 3), Metal);
		};
		auto Dosimeter = [&]()
		{
			Attach(EKAPShape::Cube, "spine_04", FVector(4, 15, -8), FVector(9, 2.5f, 6), FKAPMat::Paint(FLinearColor(0.85f, 0.83f, 0.75f), 0.4f));
			Attach(EKAPShape::Sphere, "spine_04", FVector(7, 16.4f, -6), FVector(1.2f, 1.2f, 1.2f), FKAPMat::Glow(FLinearColor(0.2f, 1.f, 0.2f), 30));
		};
		FKAPMat HardHat = FKAPMat::Paint(FLinearColor(0.9f, 0.68f, 0.02f), 0.35f); HardHat.Grime = 0.6f; HardHat.Rust = 0.f; HardHat.DustTop = 0.8f;
		FKAPMat HiVis = FKAPMat::Paint(FLinearColor(0.85f, 0.42f, 0.03f), 0.75f); HiVis.Grime = 0.8f;
		FKAPMat Olive = FKAPMat::Paint(FLinearColor(0.17f, 0.2f, 0.11f), 0.85f); Olive.Grime = 0.7f;
		FKAPMat FilterY = FKAPMat::Paint(FLinearColor(0.75f, 0.6f, 0.08f), 0.5f);
		// ---- «мата» киім: сүйектің нақты ұзындығы мен бағыты бойынша жең, балақ, етік (ермексаз денені жабады) ----
		auto Limb = [&](FName Parent, FName Child, float Dia, float Extra, const FKAPMat& Mt)
		{
			const USkeletalMesh* SM = SK->GetSkeletalMeshAsset();
			if (!SM) return;
			const FReferenceSkeleton& RS = SM->GetRefSkeleton();
			const int32 Ci = RS.FindBoneIndex(Child);
			if (Ci == INDEX_NONE) return;
			const FVector Off = RS.GetRefBonePose()[Ci].GetTranslation();
			const float Len = Off.Size();
			if (Len < 1.f) return;
			Attach(EKAPShape::Cylinder, Parent, Off * 0.5f, FVector(Dia, Dia, Len + Extra), Mt, FRotationMatrix::MakeFromZ(Off / Len).Rotator());
		};
		auto Boot = [&](FName Foot, FName Ball, const FKAPMat& Mt)
		{
			const USkeletalMesh* SM = SK->GetSkeletalMeshAsset();
			if (!SM) return;
			const FReferenceSkeleton& RS = SM->GetRefSkeleton();
			const int32 Bi = RS.FindBoneIndex(Ball);
			if (Bi == INDEX_NONE) return;
			const FVector Off = RS.GetRefBonePose()[Bi].GetTranslation();
			Attach(EKAPShape::Cube, Foot, Off * 0.45f, FVector(Off.Size() + 12.f, 11.5f, 12.f), Mt, FRotationMatrix::MakeFromX(Off.GetSafeNormal()).Rotator());
			Attach(EKAPShape::Cylinder, Foot, FVector::ZeroVector, FVector(13.f, 13.f, 14.f), Mt, FRotationMatrix::MakeFromZ(-Off.GetSafeNormal()).Rotator());
		};
		auto Clothes = [&](const FKAPMat& Top, const FKAPMat& Bottom, const FKAPMat& Shoes, bool bLongSleeve)
		{
			for (const TCHAR* S : { TEXT("_l"), TEXT("_r") })
			{
				const FString Sx(S);
				Limb(FName(*(TEXT("upperarm") + Sx)), FName(*(TEXT("lowerarm") + Sx)), 13.f, 4.f, Top);
				if (bLongSleeve) Limb(FName(*(TEXT("lowerarm") + Sx)), FName(*(TEXT("hand") + Sx)), 11.f, -2.f, Top);
				Limb(FName(*(TEXT("thigh") + Sx)), FName(*(TEXT("calf") + Sx)), 19.5f, 6.f, Bottom);
				Limb(FName(*(TEXT("calf") + Sx)), FName(*(TEXT("foot") + Sx)), 15.f, -4.f, Bottom);
				Boot(FName(*(TEXT("foot") + Sx)), FName(*(TEXT("ball") + Sx)), Shoes);
			}
			Attach(EKAPShape::Cylinder, "neck_01", FVector(0, 0, 0), FVector(17, 17, 6), Top, FRotator(90, 0, 0)); // жаға
		};
		auto Fabric = [&](FLinearColor C, float Folds) { FKAPMat F = FKAPMat::Paint(C, 0.92f); F.Ribs = Folds; F.RibPeriod = 3.5f; F.Grime = 0.85f; F.Rust = 0.f; F.DustTop = 0.7f; F.VarAmt = 0.14f; F.VarScale = 0.05f; return F; };
		FKAPMat BootM = FKAPMat::Paint(FLinearColor(0.04f, 0.035f, 0.03f), 0.65f); BootM.Grime = 0.9f; BootM.DustTop = 0.9f;
		const FLinearColor SuitC = Sh.Color;
		switch (InKind)
		{
		case ENPCKind::Worker:
		{
			// қорғаныш костюмі (қатпарлы, кірлеген мата) + капюшон
			const FKAPMat Hz = Fabric(FLinearColor(0.78f, 0.36f, 0.05f), 0.35f);
			Clothes(Hz, Hz, FKAPMat::Paint(FLinearColor(0.06f, 0.06f, 0.05f), 0.6f), true);
			Attach(EKAPShape::Sphere, "head", FVector(3, -2, 0), FVector(27, 28, 27), Hz);
			break;
		}
		case ENPCKind::Militia: Clothes(Fabric(FLinearColor(0.18f, 0.21f, 0.12f), 0.3f), Fabric(FLinearColor(0.16f, 0.19f, 0.11f), 0.3f), BootM, true); break;
		case ENPCKind::Guard: Clothes(Fabric(FLinearColor(0.07f, 0.09f, 0.15f), 0.25f), Fabric(FLinearColor(0.05f, 0.06f, 0.1f), 0.25f), BootM, true); break;
		case ENPCKind::Bandit: Clothes(Fabric(FLinearColor(0.14f, 0.12f, 0.1f), 0.4f), Fabric(FLinearColor(0.1f, 0.1f, 0.12f), 0.4f), BootM, true); break;
		case ENPCKind::Survivor: Clothes(Fabric(FLinearColor(0.3f, 0.27f, 0.22f), 0.45f), Fabric(FLinearColor(0.2f, 0.18f, 0.15f), 0.45f), BootM, true); break;
		case ENPCKind::Irradiated: Clothes(Fabric(FLinearColor(0.08f, 0.08f, 0.05f), 0.5f), Fabric(FLinearColor(0.06f, 0.06f, 0.05f), 0.5f), BootM, FMath::FRand() < 0.5f); break;
		case ENPCKind::Villager:
		case ENPCKind::Elder:
		case ENPCKind::Mother:
		{
			Clothes(Fabric(SuitC, 0.3f), Fabric(InKind == ENPCKind::Mother ? SuitC * 0.9f : FLinearColor(0.12f, 0.13f, 0.18f), 0.3f), BootM, true);
			// бас: шаш/бөрік, қарттарда ақ сақал
			if (InKind != ENPCKind::Mother) Attach(EKAPShape::Sphere, "head", FVector(8, -3, 0), FVector(21, 24, 23), FKAPMat::Paint(InKind == ENPCKind::Elder ? FLinearColor(0.5f, 0.48f, 0.45f) : FLinearColor(0.02f, 0.015f, 0.01f), 0.8f));
			if (InKind == ENPCKind::Elder) Attach(EKAPShape::Sphere, "head", FVector(-7, 9, 0), FVector(10, 6, 9), FKAPMat::Paint(FLinearColor(0.8f, 0.78f, 0.74f), 0.9f));
			break;
		}
		default: break;
		}
		switch (InKind)
		{
		case ENPCKind::Worker:
			// жөндеуші-инженер: каска + фонарь, респиратор/көзілдірік, шағылыс жолақты кеудеше, рюкзак, дозиметр
			Helmet(HardHat, true, true);
			if (FMath::FRand() < 0.75f) Respirator(FilterY, FMath::FRand() < 0.5f);
			Vest(HiVis, true, false);
			if (FMath::FRand() < 0.6f) Backpack(Canvas, FMath::FRand() < 0.4f);
			Belt(true); Dosimeter();
			break;
		case ENPCKind::Guard:
			Helmet(FKAPMat::Paint(FLinearColor(0.06f, 0.08f, 0.14f), 0.6f), false, false);
			Vest(FKAPMat::Paint(FLinearColor(0.05f, 0.06f, 0.1f), 0.85f), false, true);
			Belt(false);
			break;
		case ENPCKind::Militia:
			// жасақ: дулыға, бронежилет қалталарымен, газтұтқыш, рюкзак
			Helmet(Olive, false, false);
			Vest(Olive, false, true);
			Respirator(FKAPMat::Paint(FLinearColor(0.12f, 0.14f, 0.08f), 0.6f), true);
			Backpack(Olive, false);
			Belt(false);
			break;
		case ENPCKind::Bandit:
			// бандит: бет орамал (балаклава), ескі рюкзак
			Attach(EKAPShape::Sphere, "head", FVector(4, 2, 0), FVector(26, 25, 24), FKAPMat::Paint(FLinearColor(0.04f, 0.04f, 0.04f), 0.95f));
			Attach(EKAPShape::Cylinder, "neck_01", FVector(0, 4, 0), FVector(22, 22, 12), FKAPMat::Paint(FLinearColor(0.35f, 0.03f, 0.03f), 0.9f), FRotator(0, 0, 90));
			Vest(FKAPMat::Paint(FLinearColor(0.12f, 0.1f, 0.08f), 0.9f), false, true);
			if (FMath::FRand() < 0.6f) Backpack(Canvas, FMath::FRand() < 0.5f);
			break;
		case ENPCKind::Survivor:
			Respirator(FKAPMat::Paint(FLinearColor(0.3f, 0.3f, 0.28f), 0.6f), false);
			Backpack(Canvas, true);
			Attach(EKAPShape::Cube, "spine_04", FVector(0, 6, 0), FVector(32, 28, 38), FKAPMat::Paint(FLinearColor(0.25f, 0.23f, 0.2f), 1.f), FRotator(0, 0, 90));
			break;
		case ENPCKind::Mother:
		case ENPCKind::Elder:
			Attach(EKAPShape::Sphere, "head", FVector(8, 0, 0), FVector(26, 27, 27), FKAPMat::Paint(InKind == ENPCKind::Mother ? FLinearColor(0.9f, 0.9f, 0.88f) : FLinearColor(0.05f, 0.05f, 0.05f), 0.8f));
			break;
		case ENPCKind::Villager:
			Attach(EKAPShape::Cube, "spine_04", FVector(-2, 0, 0), FVector(34, 26, 30), Sh); // жейде/күрте
			if (FMath::FRand() < 0.35f) Attach(EKAPShape::Cylinder, "head", FVector(11, 1, 0), FVector(24, 26, 7), FKAPMat::Paint(FLinearColor(0.12f, 0.1f, 0.08f), 0.9f), FRotator(90, 0, 0)); // бөрік/кепка
			break;
		case ENPCKind::Irradiated:
			// сәулеленген жұмысшы: жыртылған кеудеше қалдығы, салбыраған респиратор
			Attach(EKAPShape::Cube, "spine_04", FVector(-6, 12, 5), FVector(20, 3, 22), FKAPMat::Paint(FLinearColor(0.3f, 0.16f, 0.04f), 0.95f), FRotator(0, 10, 15));
			Attach(EKAPShape::Cube, "neck_01", FVector(-6, 10, 0), FVector(8, 7, 11), Rubber, FRotator(25, 0, 0));
			break;
		default: break;
		}
		if (InKind == ENPCKind::Militia || InKind == ENPCKind::Guard || InKind == ENPCKind::Bandit)
		{
			// автомат: қорап, ұңғы, магазин, дүмбі, тұтқа (hand_r кеңістігінде X — алға)
			FKAPMat Gun = FKAPMat::Metal(FLinearColor(0.045f, 0.045f, 0.045f), 0.45f); Gun.Rust = 0.15f;
			FKAPMat Wood2 = FKAPMat::Paint(FLinearColor(0.28f, 0.14f, 0.06f), 0.55f);
			Attach(EKAPShape::Cube, "hand_r", FVector(14, 0, 3), FVector(30, 5, 8), Gun);
			Attach(EKAPShape::Cylinder, "hand_r", FVector(42, 0, 5), FVector(2.4f, 2.4f, 30), Gun, FRotator(90, 0, 0));
			Attach(EKAPShape::Cube, "hand_r", FVector(30, 0, 2), FVector(16, 5.5f, 6), Wood2);
			Attach(EKAPShape::Cube, "hand_r", FVector(12, 0, -6), FVector(6, 3.5f, 14), Gun, FRotator(-15, 0, 0));
			Attach(EKAPShape::Cube, "hand_r", FVector(-8, 0, 0), FVector(22, 4.5f, 9), Wood2, FRotator(-8, 0, 0));
			Attach(EKAPShape::Cube, "hand_r", FVector(4, 0, -4), FVector(4, 3, 9), Gun);
		}
		if (InKind == ENPCKind::Irradiated)
		{
			// толық жасыл overlay жоқ: қараңғы силуэт, жарқыраған көздер мен ісіктер — қорқыныштырақ
			Attach(EKAPShape::Sphere, "head", FVector(12, 10, 4), FVector(5, 5, 5), FKAPMat::Glow(FLinearColor(0.5f, 1.f, 0.1f), 40));
			Attach(EKAPShape::Sphere, "head", FVector(12, 10, -4), FVector(5, 5, 5), FKAPMat::Glow(FLinearColor(0.5f, 1.f, 0.1f), 40));
			Attach(EKAPShape::Rock, "spine_03", FVector(0, -18, 0), FVector(30, 25, 30), FKAPMat::Paint(FLinearColor(0.25f, 0.35f, 0.1f), 0.3f));
			// ісіктер мен жарылған тері: иық, қол, бас, сан — дымқыл (кедір-бұдырлығы аз), іштей әлсіз жарқырайды
			FKAPMat Tumor = FKAPMat::Paint(FLinearColor(0.18f, 0.22f, 0.07f), 0.25f); Tumor.Emissive = FLinearColor(0.15f, 0.4f, 0.03f) * 0.6f; Tumor.Grime = 0.9f; Tumor.DustTop = 0.f;
			Attach(EKAPShape::Rock, "clavicle_l", FVector(0, 8, 6), FVector(24, 20, 22), Tumor);
			Attach(EKAPShape::Rock, "upperarm_r", FVector(10, 0, 4), FVector(18, 16, 16), Tumor);
			Attach(EKAPShape::Rock, "head", FVector(6, -8, 9), FVector(15, 13, 12), Tumor);
			Attach(EKAPShape::Rock, "thigh_l", FVector(12, 6, 0), FVector(16, 14, 14), Tumor);
			Attach(EKAPShape::Sphere, "spine_02", FVector(4, 14, 8), FVector(4, 4, 4), FKAPMat::Glow(FLinearColor(0.4f, 1.f, 0.1f), 15));
			Attach(EKAPShape::Sphere, "upperarm_l", FVector(12, 4, 5), FVector(3, 3, 3), FKAPMat::Glow(FLinearColor(0.4f, 1.f, 0.1f), 15));
		}
	}
	else
	{
		SK->SetVisibility(false);
		const FKAPMat Fur = FKAPMat::Paint(FLinearColor(0.16f, 0.13f, 0.1f), 0.9f);
		if (InKind == ENPCKind::Wolf)
		{
			GetCapsuleComponent()->SetCapsuleSize(40.f, 45.f);
			Parts.Add(KAPVis::Add(this, GetCapsuleComponent(), EKAPShape::Wolf, FVector(0, 0, -45), FVector(100, 100, 100), Fur, FRotator::ZeroRotator, false));
			Parts.Add(KAPVis::Add(this, GetCapsuleComponent(), EKAPShape::Sphere, FVector(70, -7, 12), FVector(5, 5, 5), FKAPMat::Glow(FLinearColor(1, 0.1f, 0.f), 60), FRotator::ZeroRotator, false));
			Parts.Add(KAPVis::Add(this, GetCapsuleComponent(), EKAPShape::Sphere, FVector(70, 7, 12), FVector(5, 5, 5), FKAPMat::Glow(FLinearColor(1, 0.1f, 0.f), 60), FRotator::ZeroRotator, false));
		}
		else
		{
			GetCapsuleComponent()->SetCapsuleSize(50.f, 60.f);
			for (int32 i = 0; i < 7; i++)
				Parts.Add(KAPVis::Add(this, GetCapsuleComponent(), EKAPShape::WormSeg, FVector(-i * 65.f, 0, 0), FVector(110 - i * 9, 110 - i * 9, 110 - i * 9), FKAPMat::Paint(FLinearColor(0.32f, 0.22f, 0.13f), 0.7f), FRotator::ZeroRotator, false));
			Rise = 0.f;
		}
	}
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

bool AKAPNPC::IsHostileToPlayer() const
{
	if (bDead) return false;
	if (Faction == EKAPFaction::Monster || Faction == EKAPFaction::Bandit) return true;
	// сарбаздар: әскери аймаққа рұқсатсыз кірген ойыншыға ескертуден кейін оқ атады
	if (Kind == ENPCKind::Soldier) if (const AKAPGameMode* GM0 = AKAPGameMode::Get(this)) if (GM0->ArmyHostile > 0.f) return true;
	if (Faction == EKAPFaction::Militia || Faction == EKAPFaction::Guard)
	{
		const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
		const AKAPGameMode* GM = AKAPGameMode::Get(this);
		return (GI && GI->S.RepMilitia <= -30) || (GM && GM->Provoked > 0);
	}
	return false;
}

bool AKAPNPC::HasLineOfSightTo(const AActor* Other) const
{
	if (!Other) return false;
	FHitResult H;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPLOS), false, this);
	Q.AddIgnoredActor(Other);
	return !GetWorld()->LineTraceSingleByChannel(H, GetActorLocation() + FVector(0, 0, 60), Other->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, Q);
}

void AKAPNPC::Alert(FVector2D AtM, float Seconds) { Alerted = Seconds; LastHeard = AtM; }

FString AKAPNPC::GetInteractText(AKAPCharacter*) const
{
	if (TalkId.IsNone()) return FString();
	if (bDead) return WorkerIndex >= 0 ? DisplayName + TEXT(" — қаза тапқан") : FString();
	if (IsHostileToPlayer()) return FString();
	return TEXT("Сөйлесу: ") + DisplayName;
}

void AKAPNPC::Interact(AKAPCharacter*) { if (!bDead && !TalkId.IsNone()) if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Talk(this); }

float AKAPNPC::TakeDamage(float Damage, FDamageEvent const& Event, AController* Inst, AActor* Causer)
{
	if (bDead) return 0;
	if (Kind == ENPCKind::Worm && Rise < 0.3f) Damage *= 0.2f;
	Hp -= Damage;
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(Faction == EKAPFaction::Monster ? EKAPFX::Green : EKAPFX::Blood, GetActorLocation() + FVector(0, 0, 40));
	const bool bByPlayer = Causer && Causer->IsA(AKAPCharacter::StaticClass());
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (bByPlayer && GM)
	{
		Aggro = 30;
		GM->OnNPCAttackedByPlayer(this);
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (*It != this && It->Faction == Faction && !It->bDead && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 5000.f) It->Aggro = 30;
	}
	if (Hp <= 0) Kill(bByPlayer);
	return Damage;
}

void AKAPNPC::Kill(bool bByPlayer)
{
	if (bDead) return;
	bDead = true; DeadT = 0;
	// адамның өлім айқайы (әйелге — жоғарырақ)
	if (IsHuman()) if (UKAPAudio* A = UKAPAudio::Get(this)) if (!A->PlayBank("Scream", GetActorLocation() + FVector(0, 0, 60), 1.f, bFemale ? 1.3f : FMath::FRandRange(0.92f, 1.04f))) A->Creature(GetActorLocation() + FVector(0, 0, 60), 1, 0.6f);
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (IsHuman())
	{
		GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
		GetMesh()->SetSimulatePhysics(true);
	}
	else for (UStaticMeshComponent* P : Parts) P->AddLocalRotation(FRotator(0, 0, 80));
	if (Carrying.IsValid()) Carrying = nullptr;
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->OnActorKilled(this, bByPlayer);
}

void AKAPNPC::SteerTo(FVector2D T, float Speed)
{
	const FVector2D P = KAP::CM2M(GetActorLocation());
	FVector2D D = T - P;
	const double L = D.Size();
	if (L < 0.3) return;
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	FVector Dir(D.X / L, D.Y / L, 0);
	// кедергіден бұрылу: тұрып қалса бүйірге
	if (StuckT > 1.f) Dir = FVector(-Dir.Y, Dir.X, 0) * (FMath::Sin(GetWorld()->GetTimeSeconds() + GetUniqueID()) > 0 ? 1 : -1);
	AddMovementInput(Dir, 1.f);
}

void AKAPNPC::Tick(float Dt)
{
	Super::Tick(Dt);
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!PC || !GM) return;
	const float DistP = FVector::Dist2D(GetActorLocation(), PC->GetActorLocation()) / 100.f;
	if (bDead)
	{
		DeadT += Dt;
		if (DeadT > 40 && !bPersistent && WorkerIndex < 0) Destroy();
		return;
	}
	if (DistP > 450 && !bPersistent) { Destroy(); return; }
	UpdateLod(DistP);
	if (LodTier >= 3) return; // өте алыс: AI тоқтап тұрады, тек қашықтық тексеріледі
	if (GM->bCinematic) return;
	if (bStalker) { StalkerTick(Dt, PC, DistP); return; }
	SoundTick(Dt, DistP);
	// сәулеленген: «сынған» қозғалыс — анимация жылдамдығы кенет өзгереді, бас тартылып бұрылады
	if (Kind == ENPCKind::Irradiated && GetMesh())
	{
		TwitchT -= Dt;
		if (TwitchT <= 0)
		{
			const bool bSpasm = FMath::FRand() < 0.3f;
			GetMesh()->GlobalAnimRateScale = bSpasm ? FMath::FRandRange(1.8f, 3.f) : FMath::FRandRange(0.45f, 1.f);
			HeadJit = bSpasm ? FMath::FRandRange(-14.f, 14.f) : FMath::FRandRange(-4.f, 4.f);
			TwitchT = bSpasm ? FMath::FRandRange(0.08f, 0.25f) : FMath::FRandRange(0.3f, 0.9f);
		}
		GetMesh()->SetRelativeRotation(FRotator(HeadJit * 0.3f, -90.f + HeadJit, HeadJit * 0.5f));
	}
	// тұрып қалуды анықтау
	const float Moved = FVector::Dist2D(GetActorLocation(), LastPos);
	StuckT = (GetLastMovementInputVector().SizeSquared() > 0.1f && Moved < 1.f) ? StuckT + Dt : FMath::Max(0.f, StuckT - Dt * 2);
	LastPos = GetActorLocation();
	Cd -= Dt; Aggro -= Dt; Alerted -= Dt; HelpCd -= Dt;
	if (Faction == EKAPFaction::Monster || Faction == EKAPFaction::Bandit || Faction == EKAPFaction::Militia || Faction == EKAPFaction::Guard) CombatAI(Dt);
	else CivilAI(Dt);
	// жануарлар мен құрттың процедуралық анимациясы
	AnimT += Dt * (2.f + GetVelocity().Size2D() / 80.f);
	if (Kind == ENPCKind::Wolf && Parts.Num()) Parts[0]->SetRelativeRotation(FRotator(FMath::Sin(AnimT * 2) * 4.f, 0, FMath::Sin(AnimT) * 3.f));
	if (Kind == ENPCKind::Worm)
		for (int32 i = 0; i < Parts.Num(); i++)
			Parts[i]->SetRelativeLocation(FVector(-i * 65.f, FMath::Sin(AnimT * 0.7f - i) * 20.f, -140.f * (1 - Rise) + Rise * (110 - i * 22) + FMath::Sin(AnimT - i * 0.8f) * 18.f));
	if (Kind == ENPCKind::Worm && Rise < 0.5f && GetVelocity().Size2D() > 100 && FMath::FRand() < Dt * 8)
		if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(EKAPFX::Dust, GetActorLocation() - FVector(0, 0, 50), 1.5f);
}

void AKAPNPC::UpdateLod(float DistP)
{
	// қашықтық деңгейі; шекарада 8 м гистерезис (жыпылықтамасын)
	auto TierAt = [](float D) { return D < 60.f ? 0 : D < 150.f ? 1 : D < 260.f ? 2 : 3; };
	int32 Want = TierAt(DistP);
	if (LodTier >= 0)
	{
		if (Want > LodTier) Want = FMath::Max((int32)LodTier, TierAt(DistP - 8.f));
		else if (Want < LodTier) Want = FMath::Min((int32)LodTier, TierAt(DistP + 8.f));
	}
	// тапсырмадағы, ұрыстағы, ерекше NPC тоқтамасын
	if (GoTo.IsSet() || Task != ENPCTask::None || bFollowPlayer || bBoss) Want = FMath::Min(Want, 2);
	if (bStalker || Aggro > 0 || Alerted > 0 || Target.IsValid()) Want = FMath::Min(Want, 1);
	if (Want == LodTier) return;
	if (LodTier < 0)
	{
		// бір рет: киім/құрал бөлшектері сәуле трассировкасына, қашықтық өрісі жарығына қатыспайды (қозғалады — жаңарту қымбат)
		TArray<UStaticMeshComponent*> Sm; GetComponents(Sm);
		for (UStaticMeshComponent* C : Sm) { C->SetVisibleInRayTracing(false); C->SetAffectDistanceFieldLighting(false); }
	}
	// көлеңке: дене — 150 м дейін, ұсақ бөлшектер — 60 м дейін (VSM кэші әр қозғалыста бұзылады)
	if (USkeletalMeshComponent* SK = GetMesh()) if (SK->CastShadow != (Want <= 1)) SK->SetCastShadow(Want <= 1);
	{
		TArray<UStaticMeshComponent*> Sm; GetComponents(Sm);
		for (UStaticMeshComponent* C : Sm) if (C->GetAttachParent() == GetMesh() && C->Bounds.SphereRadius > 6.f) { const bool bSh = Want == 0 && C->Bounds.SphereRadius > 10.f; if (C->CastShadow != bSh) C->SetCastShadow(bSh); }
	}
	LodTier = (int8)Want;
	static const float ActorIv[4] = { 0.f, 0.1f, 0.25f, 0.5f };
	static const float MoveIv[4] = { 0.f, 0.033f, 0.2f, 0.f };
	static const float MeshIv[4] = { 0.f, 0.05f, 0.2f, 0.f };
	SetActorTickInterval(ActorIv[Want]);
	if (UCharacterMovementComponent* M = GetCharacterMovement())
	{
		M->SetComponentTickEnabled(Want < 3);
		M->SetComponentTickInterval(MoveIv[Want]);
		M->MaxSimulationTimeStep = Want >= 2 ? 0.2f : 0.05f;
		M->MaxSimulationIterations = Want >= 2 ? 2 : 8;
	}
	if (USkeletalMeshComponent* SK = GetMesh())
	{
		SK->SetComponentTickEnabled(Want < 3);
		SK->SetComponentTickInterval(MeshIv[Want]);
	}
}

void AKAPNPC::CivilAI(float Dt)
{
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	const FVector2D P = KAP::CM2M(GetActorLocation()), Pp = KAP::CM2M(PC->GetActorLocation());
	if (bFollowPlayer) { const double D = FVector2D::Distance(P, Pp); if (D > 4) SteerTo(Pp, D > 12 ? 500.f : 300.f); return; }
	if (GoTo.IsSet())
	{
		SteerTo(*GoTo, 450.f);
		if (FVector2D::Distance(P, *GoTo) < 2.5) { GoTo.Reset(); if (bVanishOnArrive) Destroy(); }
		return;
	}
	if (TaskTick(Dt)) return; // жөндеу, жүк тасу, от басы, қашу
	if (bSick) return;
	WanderT -= Dt;
	if (WanderT <= 0)
	{
		WanderT = 3 + FMath::FRand() * 6;
		if (FMath::FRand() < 0.5f) Wander.Reset(); else Wander = Home + FVector2D(FMath::FRandRange(-WanderR, WanderR), FMath::FRandRange(-WanderR, WanderR));
	}
	if (Wander.IsSet()) { SteerTo(*Wander, 130.f); if (FVector2D::Distance(P, *Wander) < 0.7) Wander.Reset(); }
	else if (FVector2D::Distance(P, Pp) < 6) SetActorRotation(FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(Pp.Y - P.Y, Pp.X - P.X)), 0.0));
}

AActor* AKAPNPC::FindCover(const FVector2D& FromM, AActor* Threat, FVector2D& OutM)
{
	AActor* Best = nullptr; double Bs = 1e9;
	for (const FVector2D& C : AKAPStructure::Covers(GetWorld()))
	{
		const double D = FVector2D::Distance(C, FromM);
		if (D > 30) continue;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPCover), false, this);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(H, FVector(C.X * 100, C.Y * 100, GetActorLocation().Z), Threat->GetActorLocation(), ECC_Visibility, Q) && H.GetActor() != Threat;
		const double Sc = D + (bBlocked ? 0 : 25);
		if (Sc < Bs) { Bs = Sc; OutM = C; Best = Threat; }
	}
	return Best;
}

void AKAPNPC::Shoot(AActor* T, float DistM)
{
	if (Cd > 0 || Ammo <= 0) return;
	Cd = 0.7f + FMath::FRand() * 0.6f;
	Ammo -= 1;
	const FVector Dir = (T->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	SetActorRotation(Dir.Rotation());
	if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(EKAPFX::Flash, GetActorLocation() + Dir * 80 + FVector(0, 0, 50), 0.8f);
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	const float DPl = FVector::Dist(GetActorLocation(), PC->GetActorLocation()) / 100.f;
	if (DPl < 160) if (UKAPAudio* A = UKAPAudio::Get(this)) { if (A->HasBank("GunAK")) A->ShotAt(Faction == EKAPFaction::Bandit && (GetUniqueID() % 3) == 0 ? FName("shotgun") : FName("ak"), GetActorLocation() + FVector(0, 0, 50)); else A->Play(EKAPSound::ShotFar, FMath::Clamp(1.f - DPl / 160.f, 0.1f, 0.9f)); }
	PC->Noise = FMath::Max(PC->Noise, 0.4f);
	const FKAPEnv* Env = AWeatherManager::Get(GetWorld()) ? &AWeatherManager::Get(GetWorld())->Env : nullptr;
	float P = 0.55f * (1.f - DistM / (RangeM * 1.3f));
	if (Env && Env->Storm > 0.5f) P *= 0.4f;
	if (T == PC) { if (PC->bMoving) P *= 0.7f; if (PC->bIndoor) P *= 0.5f; }
	if (FMath::FRand() < FMath::Max(0.05f, P)) UGameplayStatics::ApplyDamage(T, Dmg, nullptr, this, nullptr);
}

void AKAPNPC::CombatAI(float Dt)
{
	AKAPCharacter* PC = AKAPCharacter::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const FKAPEnv Env = WX ? WX->Env : FKAPEnv();
	const FVector2D P = KAP::CM2M(GetActorLocation());
	const float LookR = Env.Storm > 0.5f ? 10.f : Env.bNight ? 28.f : Env.Fog > 0.5f ? 25.f : 48.f;
	// ---- нысананы көру/есту арқылы табу ----
	ScanT -= Dt;
	if (ScanT <= 0)
	{
		ScanT = 0.5f;
		AActor* Best = nullptr; double Bd = 1e9;
		const float Sight = Faction == EKAPFaction::Monster ? LookR : LookR * 1.4f;
		auto Enemy = [&](EKAPFaction F)
		{
			switch (Faction)
			{
			case EKAPFaction::Monster: return F == EKAPFaction::Militia || F == EKAPFaction::Guard || F == EKAPFaction::Bandit;
			case EKAPFaction::Bandit: return F == EKAPFaction::Militia || F == EKAPFaction::Guard || F == EKAPFaction::Monster;
			default: return F == EKAPFaction::Monster || F == EKAPFaction::Bandit;
			}
		};
		if (IsHostileToPlayer() && !PC->bDead && !GM->bCinematic && !PC->IsHidden())
		{
			const double D = FVector2D::Distance(P, KAP::CM2M(PC->GetActorLocation()));
			bool bSeen = D < Sight && !(PC->bIndoor && D > 3);
			// дауыл шуы естуді бәсеңдетеді (шу маскасы), түнгі фонарь алыстан көрінеді
			const double HearR = (12 + PC->Noise * 130) * (1.f - Env.Storm * 0.55f);
			if (Env.bNight && PC->Flashlight && PC->Flashlight->Intensity > 0 && !bSeen && D < Sight * 1.8f && !PC->bIndoor) bSeen = true;
			if (!bSeen && D < HearR)
			{
				const double J = Env.Storm > 0.5f ? 12 : 4;
				LastHeard = KAP::CM2M(PC->GetActorLocation()) + FVector2D(FMath::FRandRange(-J, J), FMath::FRandRange(-J, J));
			}
			if ((Aggro > 0 || Alerted > 0) && D < 120) bSeen = true;
			if (bSeen) { Best = PC; Bd = D; }
		}
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
		{
			if (*It == this || It->bDead || !Enemy(It->Faction)) continue;
			const double D = FVector2D::Distance(P, KAP::CM2M(It->GetActorLocation()));
			if (D < Sight && D < Bd) { Bd = D; Best = *It; }
		}
		Target = Best;
		if (Best) LastHeard = KAP::CM2M(Best->GetActorLocation());
		// сигнал шамы монстрларды алдайды
		if (Faction == EKAPFaction::Monster)
		{
			FVector2D F;
			if (AKAPProjectile::NearestFlare(GetWorld(), P, 70, F) && (!Best || FVector2D::Distance(F, P) < Bd)) { Target = nullptr; LastHeard = F; }
		}
	}
	AActor* T = Target.Get();
	if (AKAPNPC* TN = Cast<AKAPNPC>(T)) if (TN->bDead) { Target = nullptr; T = nullptr; }
	// жасақ тактикасы: жаралыны алып кету
	if (bCarry)
	{
		const FVector2D Away = T ? P + (P - KAP::CM2M(T->GetActorLocation())) : Home;
		SteerTo(Away, 400.f);
		if (AKAPNPC* C = Carrying.Get()) C->SetActorLocation(GetActorLocation() + GetActorRightVector() * 60 + FVector(0, 0, 40));
		CarryT -= Dt;
		if (CarryT <= 0) { bCarry = false; if (AKAPNPC* C = Carrying.Get()) { C->Hp = C->MaxHp * 0.5f; C->GetCharacterMovement()->SetMovementMode(MOVE_Walking); } Carrying = nullptr; }
		return;
	}
	if (!T)
	{
		if (LastHeard.IsSet())
		{
			SteerTo(*LastHeard, Faction == EKAPFaction::Monster ? RunSpeed * 0.7f : WalkSpeed * 1.3f);
			if (FVector2D::Distance(P, *LastHeard) < 3) LastHeard.Reset();
			if (Kind == ENPCKind::Worm) Rise = FMath::Max(0.f, Rise - Dt);
			return;
		}
		if ((Faction == EKAPFaction::Militia || Faction == EKAPFaction::Guard) && !bCampMember && TaskTick(Dt)) return; // күзет бағыты
		WanderT -= Dt;
		if (WanderT <= 0 || !Wander.IsSet())
		{
			WanderT = 4 + FMath::FRand() * 8;
			const float R = Faction == EKAPFaction::Monster ? 60.f : PatrolR;
			Wander = Home + FVector2D(FMath::FRandRange(-R, R), FMath::FRandRange(-R, R));
		}
		SteerTo(*Wander, WalkSpeed * 0.6f);
		if (Kind == ENPCKind::Worm) Rise = FMath::Max(0.f, Rise - Dt);
		return;
	}
	const FVector2D Tp = KAP::CM2M(T->GetActorLocation());
	const float D = (float)FVector2D::Distance(P, Tp);
	// ---- MDP күйі → Q-кесте бойынша әрекет ----
	DecT -= Dt;
	if (DecT <= 0)
	{
		DecT = 0.7f + FMath::FRand() * 0.3f;
		const float Frac = Hp / MaxHp;
		const int32 HpB = Frac > 0.66f ? 2 : Frac > 0.3f ? 1 : 0;
		const float Close = bMelee ? 4.f : 12.f, Mid = bMelee ? 25.f : 45.f;
		const int32 DistB = D < Close ? 0 : D < Mid ? 1 : 2;
		const int32 AmmoB = bMelee ? 2 : Ammo > 15 ? 2 : Ammo > 0 ? 1 : 0;
		int32 Allies = 0;
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (*It != this && !It->bDead && It->Faction == Faction && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 3000.f) Allies++;
		LastState = KAPAI::EncodeState(HpB, DistB, AmmoB, Allies > 0 ? 1 : 0);
		LastAction = GetPolicy(Policy).Best(LastState);
		if (bBoss) LastAction = (HpB == 0 && FMath::FRand() < 0.3f) ? 2 : 0;
		if (LastAction == 3 && HelpCd <= 0)
		{
			HelpCd = 15;
			if (Kind == ENPCKind::Wolf) UKAPAudio::Get(this)->Play(EKAPSound::Howl, 0.6f);
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It) if (It->Faction == Faction && !It->bDead && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 12000.f) It->Alert(Tp, 20);
			if (Faction == EKAPFaction::Bandit && FMath::FRand() < 0.5f) GM->SpawnReinforcement(this);
		}
		if (LastAction == 1 && !bMelee) { FVector2D C; if (FindCover(P, T, C)) CoverPt = C; else CoverPt.Reset(); }
		if (Faction == EKAPFaction::Militia && HpB > 0)
			for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
				if (*It != this && It->Faction == EKAPFaction::Militia && !It->bDead && It->Hp < It->MaxHp * 0.25f && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 2500.f) { Rescue = *It; break; }
	}
	if (AKAPNPC* R = Rescue.Get())
	{
		SteerTo(KAP::CM2M(R->GetActorLocation()), RunSpeed);
		if (R->bDead) Rescue = nullptr;
		else if (FVector::Dist2D(R->GetActorLocation(), GetActorLocation()) < 150) { Carrying = R; R->GetCharacterMovement()->DisableMovement(); bCarry = true; CarryT = 8; Rescue = nullptr; }
		return;
	}
	const int32 Act = LastAction < 0 ? 0 : LastAction;
	if (Kind == ENPCKind::Worm)
	{
		if (Act == 1 || Act == 2 || D > 7) Rise = FMath::Max(0.f, Rise - Dt * 1.5f);
		else Rise = FMath::Min(1.f, Rise + Dt * 2.f);
	}
	if (Act == 0 || bBoss)
	{
		if (bMelee)
		{
			if (D > RangeM * 0.9f) SteerTo(Tp, D < 30 ? RunSpeed : WalkSpeed * 1.5f);
			else if (Cd <= 0 && !(Kind == ENPCKind::Worm && Rise < 0.8f))
			{
				Cd = bBoss ? 1.4f : 1.1f;
				if (T == PC) PC->DamagePlayer(Dmg * (PC->GetController() && PC->IsPlayerControlled() ? 1.f : 0.5f), Kind == ENPCKind::Irradiated ? 6.f : 0.f);
				else UGameplayStatics::ApplyDamage(T, Dmg, nullptr, this, nullptr);
				if (FMath::FRand() < 0.5f) UKAPAudio::Get(this)->Play(EKAPSound::Growl, FMath::Clamp(1.f - D / 60.f, 0.2f, 1.f));
			}
		}
		else
		{
			const bool bLOS = HasLineOfSightTo(T);
			if (D > RangeM * 0.8f || !bLOS) SteerTo(Tp, RunSpeed * 0.8f);
			else Shoot(T, D);
		}
	}
	else if (Act == 1)
	{
		if (bMelee)
		{
			const double A = FMath::Atan2(P.Y - Tp.Y, P.X - Tp.X) + 0.9;
			SteerTo(Tp + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 12.0, RunSpeed * 0.8f);
		}
		else if (CoverPt.IsSet())
		{
			SteerTo(*CoverPt, RunSpeed);
			if (FVector2D::Distance(P, *CoverPt) < 1.2) { if (Ammo < 30) Ammo += Dt * 3; if (HasLineOfSightTo(T) && FMath::FRand() < 0.3f) Shoot(T, D); }
		}
		else Shoot(T, D);
	}
	else if (Act == 2)
	{
		SteerTo(P + (P - Tp), RunSpeed);
		if (D > 90) { Hp = FMath::Min(MaxHp, Hp + Dt * 4); if (!bMelee) Ammo = FMath::Min(30.f, Ammo + Dt * 2); }
		if (!bMelee && D < RangeM && FMath::FRand() < 0.02f) Shoot(T, D);
	}
}

// ---------------- Дауыс, сөз, қадам ----------------
static double GLastBark = -100;
void AKAPNPC::Bark(float DistP)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (!GM || !GI) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - GLastBark < 6.0) return;
	TArray<const TCHAR*> L;
	bool bLeak = false;
	if (WorkerIndex >= 0 && GI->S.Workers.IsValidIndex(WorkerIndex))
	{
		const int32 W = GI->S.Workers[WorkerIndex].WellId - 1;
		bLeak = GI->S.Wells.IsValidIndex(W) && GI->S.Wells[W].State != EWellState::Ok;
	}
	switch (Kind)
	{
	case ENPCKind::Worker:
		if (bSick) L = { TEXT("Басым айналып тұр... Су бар ма?"), TEXT("Демім жетпей жатыр..."), TEXT("Дәрігер шақырыңызшы...") };
		else if (bLeak) L = { TEXT("Инженер! Ұңғыма істен шықты, тез жөндеңіз!"), TEXT("Қысым түсіп кетті, сорғы тұрып қалды!"), TEXT("Ағып кетсе бәріміз сәулеленеміз!"), TEXT("Пульт анау жақта, көмектесіңіз!") };
		else L = { TEXT("Сәлем, инженер. Бәрі қалыпты."), TEXT("Сорғы тынбай жұмыс істеп тұр."), TEXT("Түнде даладан біртүрлі дыбыстар шығады..."), TEXT("Ауылға қашан қайтамыз, білмеймін."), TEXT("Сіз келгелі сәл жеңілдеді."), TEXT("Дозиметрге қарап жүріңіз.") };
		break;
	case ENPCKind::Villager: L = { TEXT("Ассалаумағалейкум!"), TEXT("Су таусылып барады..."), TEXT("Балалар үйден шықпайды, қорқады."), TEXT("Кеніштегілерге сәлем айтыңыз."), TEXT("Түнде қасқырлар ұлиды...") }; break;
	case ENPCKind::Elder: L = { TEXT("Келші, балам, сөйлесейік."), TEXT("Бұл далада бұрын тыныш еді..."), TEXT("Жігіттерге көмек керек.") }; break;
	case ENPCKind::Militia: L = IsHostileToPlayer() ? TArray<const TCHAR*>{ TEXT("Тоқта! Атамын!") } : TArray<const TCHAR*>{ TEXT("Ауылда тәртіп сақта."), TEXT("Мутанттарды көрдің бе?"), TEXT("Түнгі күзет — ең ауыры.") }; break;
	case ENPCKind::Guard: L = GI->S.bPermitWeapon ? TArray<const TCHAR*>{ TEXT("Жолың болсын, инженер."), TEXT("Абай бол, дала қауіпті.") } : TArray<const TCHAR*>{ TEXT("Кеніш аумағы. Құжатың бар ма?"), TEXT("Рұқсатсыз қару ұстама.") }; break;
	case ENPCKind::Survivor: L = { TEXT("Көмектесіңіз... біз мұнда үш күннен бері..."), TEXT("Ол түнде келеді... алып...") }; break;
	case ENPCKind::Police: L = { TEXT("Ақсай полициясы. Тәртіп сақтаңыз."), TEXT("Бандиттерді көрсеңіз — бізге хабарлаңыз."), TEXT("Капитанда сізге жұмыс бар."), TEXT("Түнде жолға жалғыз шықпаңыз.") }; break;
	case ENPCKind::Soldier: L = { TEXT("Әскери аймақ. Құжатыңызды дайындаңыз."), TEXT("Майор жұмыс береді — қақпаның жанында."), TEXT("Түнде ұядан бір нәрсе шығады..."), TEXT("Рұқсатсыз ішке кірмеңіз.") }; break;
	case ENPCKind::Trader: L = { TEXT("Келіңіз, тауар таза, баға әділ!"), TEXT("Оқ-дәрі керек пе? Менде бар."), TEXT("Сынық металл әкелсеңіз, сатып аламын.") }; break;
	default: return;
	}
	if (!L.Num()) return;
	GLastBark = Now;
	BarkCd = 25.f + FMath::FRand() * 20.f;
	const FString Line = L[FMath::RandRange(0, L.Num() - 1)];
	GM->Subtitle(Line, FString(), DisplayName, 3.5f);
	if (UKAPAudio* A = UKAPAudio::Get(this)) if (!A->Speak(Line, GetActorLocation() + FVector(0, 0, 60), bFemale, 0, 1.f, FMath::FRandRange(0.95f, 1.05f))) A->Voice(GetActorLocation() + FVector(0, 0, 60), FMath::Clamp(Line.Len() * 0.055f, 0.8f, 3.2f), bFemale, 0.9f, GetUniqueID() + (int32)Now);
	// сөйлегенде ойыншыға бұрылады
	if (AKAPCharacter* P = AKAPCharacter::Get(this)) SetActorRotation(FRotator(0, (P->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));
}

void AKAPNPC::SoundTick(float Dt, float DistP)
{
	UKAPAudio* A = UKAPAudio::Get(this);
	if (!A || bDead) return;
	BarkCd -= Dt;
	if (IsHuman() && DistP < 7.f && BarkCd <= 0 && !IsHostileToPlayer()) Bark(DistP);
	// бандит ойыншыны алғаш көргенде айқайлайды
	if (Faction == EKAPFaction::Bandit && !bShouted && Target.IsValid() && Target.Get() == AKAPCharacter::Get(this) && DistP < 60.f)
	{
		bShouted = true;
		static const TCHAR* Sh[] = { TEXT("Ей, инженер! Тоқта!"), TEXT("Бөлшектерді бер, тірі қаласың!"), TEXT("Жігіттер, мұнда!") };
		const FString Line = Sh[FMath::RandRange(0, 2)];
		if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Subtitle(Line, FString(), DisplayName, 3.f);
		if (!A->Speak(Line, GetActorLocation() + FVector(0, 0, 60), false, 0, 1.3f, 0.97f)) A->Voice(GetActorLocation() + FVector(0, 0, 60), 1.2f, false, 1.2f);
	}
	// монстр дауыстары: ыңырсу, ырыл; ойыншыны алғаш көргенде — шыңғырық + стингер
	if (Faction == EKAPFaction::Monster && Kind != ENPCKind::Worm)
	{
		GroanCd -= Dt;
		const bool bHunting = Target.IsValid() && Target.Get() == AKAPCharacter::Get(this);
		if (bHunting && !bShouted && DistP < 45.f)
		{
			bShouted = true;
			A->Creature(GetActorLocation() + FVector(0, 0, 60), Kind == ENPCKind::Wolf ? 2 : 1, 1.2f);
			A->Stinger(true);
		}
		else if (GroanCd <= 0 && DistP < 35.f)
		{
			GroanCd = FMath::FRandRange(3.5f, 9.f) * (bHunting ? 0.6f : 1.f);
			A->Creature(GetActorLocation() + FVector(0, 0, 60), Kind == ENPCKind::Wolf ? 2 : 0, Kind == ENPCKind::Wolf ? 0.8f : 1.f);
		}
	}
	// жөтел
	if (bSick && DistP < 25.f && FMath::FRand() < Dt * 0.12f) A->PlayAt(EKAPSound::Cough, GetActorLocation() + FVector(0, 0, 60), 0.8f);
	// қадам дыбысы
	const float V = GetVelocity().Size2D();
	if (DistP < 22.f && V > 60.f && Kind != ENPCKind::Worm)
	{
		StepAcc += V * Dt;
		const float Stride = Kind == ENPCKind::Wolf ? 45.f : 75.f;
		if (StepAcc > Stride) { StepAcc = 0; A->Footstep(GetActorLocation() - FVector(0, 0, 80), Kind == ENPCKind::Wolf ? 4 : 0, Kind == ENPCKind::Wolf ? 0.3f : 0.45f); }
	}
}

void AKAPNPC::StalkerTick(float Dt, AKAPCharacter* PC, float DistP)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	StalkLife += Dt;
	GetCharacterMovement()->StopMovementImmediately();
	const FVector To = PC->GetActorLocation() - GetActorLocation();
	SetActorRotation(FRotator(0, To.Rotation().Yaw, 0));
	if (GetMesh()) { GetMesh()->GlobalAnimRateScale = 0.2f; GetMesh()->SetRelativeRotation(FRotator(0, -90.f + FMath::Sin(StalkLife * 0.7f) * 6.f, 8.f)); }
	// ойыншы қарап тұр ма? (камера бағыты + көрінетіндік; фонарь сәулесі — тезірек)
	bool bWatched = false;
	if (APlayerController* C = Cast<APlayerController>(PC->GetController()))
	{
		FVector CamP; FRotator CamR; C->GetPlayerViewPoint(CamP, CamR);
		const FVector Dir = (GetActorLocation() + FVector(0, 0, 60) - CamP).GetSafeNormal();
		const float Dot = FVector::DotProduct(CamR.Vector(), Dir);
		const bool bLamp = PC->Flashlight && PC->Flashlight->Intensity > 0 && Dot > 0.9f && DistP < 40.f;
		bWatched = (Dot > 0.975f || bLamp) && DistP < 70.f && HasLineOfSightTo(PC);
		WatchT = bWatched ? WatchT + Dt * (bLamp ? 2.f : 1.f) : FMath::Max(0.f, WatchT - Dt * 0.5f);
	}
	if (WatchT > 1.1f)
	{
		// көзге түсті — жоғалады: шаң, сыбыр, стингер
		if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(EKAPFX::Dust, GetActorLocation() - FVector(0, 0, 60), 0.8f);
		if (UKAPAudio* A = UKAPAudio::Get(this)) { A->Creature(GetActorLocation() + FVector(0, 0, 60), 3, 0.6f); A->Stinger(true); }
		Destroy();
		return;
	}
	UnseenT = bWatched ? 0.f : UnseenT + Dt;
	if (UnseenT > 9.f && DistP > 10.f)
	{
		// қарамаған кезде жақындайды (көрінбейтін жерге «секіреді»)
		UnseenT = 0.f;
		const FVector P = GetActorLocation() + To.GetSafeNormal2D() * FMath::Min(700.f, (DistP - 8.f) * 100.f);
		SetActorLocation(FVector(P.X, P.Y, KAPVis::GroundZ(GetWorld(), P.X, P.Y, P.Z) + 95.f), false, nullptr, ETeleportType::TeleportPhysics);
		if (UKAPAudio* A = UKAPAudio::Get(this)) A->Creature(GetActorLocation() + FVector(0, 0, 60), 3, 0.35f);
	}
	if (DistP < 9.f)
	{
		// тым жақын — шабуылға шығады
		bStalker = false; Aggro = 30.f; Target = PC;
		if (GetMesh()) GetMesh()->GlobalAnimRateScale = 1.f;
		if (UKAPAudio* A = UKAPAudio::Get(this)) { A->Creature(GetActorLocation() + FVector(0, 0, 60), 1, 1.3f); A->Stinger(true); }
		return;
	}
	if (StalkLife > 100.f || (GM && GM->Weather && !GM->Weather->Env.bNight)) Destroy();
}

// Әскери аймақ: рұқсатсыз кірген ойыншыға ескерту (субтитр + жазылған дауыс)
void AKAPNPC::ArmyWarn(int32 Stage)
{
	static const TCHAR* Kk[3] = { TEXT("Тоқта! Бұл әскери аймақ! Кері қайт!"), TEXT("Соңғы ескерту! Аймақтан шық, әйтпесе атамыз!"), TEXT("Атыңдар!") };
	static const TCHAR* Ru[3] = { TEXT("Стой! Это военная зона! Назад!"), TEXT("Последнее предупреждение! Покинь зону, или открываем огонь!"), TEXT("Огонь!") };
	const int32 S = FMath::Clamp(Stage, 0, 2);
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Subtitle(Kk[S], Ru[S], DisplayName, 4.f);
	if (UKAPAudio* A = UKAPAudio::Get(this)) if (!A->Speak(Kk[S], GetActorLocation() + FVector(0, 0, 60), false, 0, 1.6f, 0.95f)) A->Voice(GetActorLocation() + FVector(0, 0, 60), 1.4f, false, 1.4f);
	if (AKAPCharacter* P = AKAPCharacter::Get(this)) SetActorRotation(FRotator(0, (P->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0));
}
