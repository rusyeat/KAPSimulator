#include "KAPCharacter.h"
#include "KAP/Core/KAPRegistry.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "KAPSurvivalAttributeSet.h"
#include "NavigatorComponent.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Core/KAPInteractable.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/World/KAPVisuals.h"
#include "Engine/StaticMesh.h"
#include "KAP/World/KAPFXManager.h"
#include "KAP/World/WeatherManager.h"
#include "KAP/World/WellManagerSubsystem.h"
#include "KAP/World/KAPProjectile.h"
#include "KAP/World/KAPStructure.h"
#include "KAP/World/KAPWorldBuilder.h"
#include "KAP/Audio/KAPAudio.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "Engine/DamageEvents.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "KAP/World/KAPNature.h"
#include "KAP/NPC/KAPAnimal.h"

AKAPCharacter* AKAPCharacter::Get(const UObject* WorldContext)
{
	UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!W) return nullptr;
	for (TActorIterator<AKAPCharacter> It(W); It; ++It) return *It;
	return nullptr;
}

AKAPCharacter::AKAPCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0, 0, 72));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(80.f);
	Camera->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
	Flashlight = CreateDefaultSubobject<USpotLightComponent>("Flashlight");
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetRelativeLocation(FVector(10, 15, -10));
	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(0.f);
	Flashlight->SetVolumetricScatteringIntensity(2.2f); // шаң/тұманда көрінетін сәуле
	Flashlight->SetCastShadows(true);
	Flashlight->bCastVolumetricShadow = true;
	Flashlight->SetOuterConeAngle(31.f); Flashlight->SetInnerConeAngle(15.f); Flashlight->SetSourceRadius(2.f); Flashlight->SetSoftSourceRadius(4.f);
	Flashlight->SetAttenuationRadius(3500.f);
	Flashlight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
	HeldRoot = CreateDefaultSubobject<USceneComponent>("HeldRoot");
	HeldRoot->SetupAttachment(Camera);
	HeldRoot->SetRelativeLocation(FVector(45, 22, -22));
	HeldRoot->SetRelativeScale3D(FVector(0.55f));
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>("ASC");
	Attr = CreateDefaultSubobject<UKAPSurvivalAttributeSet>("Attr");
	Navigator = CreateDefaultSubobject<UNavigatorComponent>("Navigator");
	GetMesh()->SetOwnerNoSee(true);
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = 420.f; M->JumpZVelocity = 500.f; M->AirControl = 0.25f;
	M->bOrientRotationToMovement = false;
	bUseControllerRotationYaw = true;
}

void AKAPCharacter::BeginPlay()
{
	Super::BeginPlay();
	ASC->InitAbilityActorInfo(this, this);
	if (USkeletalMesh* SK = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		GetMesh()->SetSkeletalMesh(SK);
		GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -90), FRotator(0, -90, 0));
		if (UClass* ABP = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"))) GetMesh()->SetAnimInstanceClass(ABP);
	}
	if (UKAPGameInstance* GI = UKAPGameInstance::Get(this)) Equip(GI->S.Equipped);
}

float AKAPCharacter::GetHp() const { return Attr->GetHealth(); }
float AKAPCharacter::GetMaxHp() const { return Attr->GetMaxHealth(); }
float AKAPCharacter::GetWater() const { return Attr->GetWater(); }
float AKAPCharacter::GetTemp() const { return Attr->GetBodyTemp(); }
float AKAPCharacter::GetDose() const { return Attr->GetDose(); }
float AKAPCharacter::GetStamina() const { return Attr->GetStamina(); }
void AKAPCharacter::SetAttr(const FGameplayAttribute& A, float V) { ASC->SetNumericAttributeBase(A, V); }

void AKAPCharacter::MoveInput(FVector2D V) { MoveVec = V; }
void AKAPCharacter::LookInput(FVector2D V)
{
	if (bFrozen) return;
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const float S = (GI ? GI->S.MouseSens : 1.f) * (bScoped ? 0.3f : 1.f);
	AddControllerYawInput(V.X * S);
	AddControllerPitchInput(-V.Y * S);
}
void AKAPCharacter::DoJump()
{
	if (bFrozen || GetStamina() < 8) return;
	if (CanJump()) { Jump(); SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), GetStamina() - 8); }
}
void AKAPCharacter::ToggleFlashlight()
{
	Flashlight->SetIntensity(Flashlight->Intensity > 0 ? 0.f : 800.f);
	if (UKAPAudio* A = UKAPAudio::Get(this)) A->Foley(0);
	UKAPAudio::Get(this)->Beep(true);
}
void AKAPCharacter::SetAim(bool b)
{
	bScoped = b && Cast<UKAPGameInstance>(GetGameInstance())->S.Equipped == "sniper";
}

void AKAPCharacter::Equip(FName Id)
{
	if (UKAPGameInstance* GI = UKAPGameInstance::Get(this)) GI->S.Equipped = Id;
	bScoped = false; ReloadT = 0; SwingT = 0;
	BuildHeld(Id);
	if (UKAPAudio* A = UKAPAudio::Get(this)) A->Foley(1);
}

void AKAPCharacter::BuildHeld(FName Id)
{
	TArray<USceneComponent*> Kids; HeldRoot->GetChildrenComponents(false, Kids);
	for (USceneComponent* K : Kids) K->DestroyComponent();
	// HeldRoot кеңістігі: X — алға, Y — оңға, Z — жоғары; бастапқы нүкте — ұстайтын жер (см)
	auto B = [&](FVector P, FVector S, const FKAPMat& M, FRotator R = FRotator::ZeroRotator) { UStaticMeshComponent* C = KAPVis::Add(this, HeldRoot, EKAPShape::Cube, P, S, M, R, false); C->SetCastShadow(false); C->SetOnlyOwnerSee(true); C->SetCachedMaxDrawDistance(0.f); return C; };
	auto Cy = [&](FVector P, FVector S, const FKAPMat& M, FRotator R) { UStaticMeshComponent* C = KAPVis::Add(this, HeldRoot, EKAPShape::Cylinder, P, S, M, R, false); C->SetCastShadow(false); C->SetOnlyOwnerSee(true); C->SetCachedMaxDrawDistance(0.f); return C; };
	const FRotator AlongX(90, 0, 0), AlongY(0, 0, 90);
	FKAPMat Gun = FKAPMat::Metal(FLinearColor(0.045f, 0.045f, 0.048f), 0.38f); Gun.Rust = 0.12f; Gun.Grime = 0.5f; Gun.DustTop = 0.3f;
	FKAPMat Blued = FKAPMat::Metal(FLinearColor(0.03f, 0.035f, 0.045f), 0.25f); Blued.DustTop = 0.2f;
	FKAPMat Wood = FKAPMat::Paint(FLinearColor(0.32f, 0.15f, 0.06f), 0.45f); Wood.Ribs = 0.1f; Wood.RibPeriod = 1.5f; Wood.Grime = 0.5f; Wood.DustTop = 0.2f;
	FKAPMat Bakelite = FKAPMat::Paint(FLinearColor(0.22f, 0.07f, 0.03f), 0.5f); Bakelite.DustTop = 0.2f;
	FKAPMat Chrome = FKAPMat::Metal(FLinearColor(0.75f, 0.76f, 0.78f), 0.22f); Chrome.Rust = 0.1f; Chrome.DustTop = 0.2f;
	FKAPMat Rubber = FKAPMat::Paint(FLinearColor(0.03f, 0.03f, 0.03f), 0.8f); Rubber.Ribs = 0.4f; Rubber.RibPeriod = 0.8f; Rubber.DustTop = 0.2f;
	FKAPMat RedPaint = FKAPMat::Paint(FLinearColor(0.55f, 0.06f, 0.03f), 0.55f); RedPaint.Rust = 0.35f; RedPaint.Grime = 0.7f; RedPaint.DustTop = 0.3f;
	FKAPMat Orange = FKAPMat::Paint(FLinearColor(0.85f, 0.32f, 0.03f), 0.4f); Orange.DustTop = 0.2f;
	// ұстау нүктесі: мылтықтар — ортаға жақын (иыққа тіреледі), тапанша — алдыда, суық қару — оң жақта
	HeldBase = (Id == "ak" || Id == "sniper" || Id == "shotgun") ? FVector(24, 13, -16) : (Id == "pm" || Id == "flaregun") ? FVector(30, 11, -13) : FVector(40, 22, -24);
	// ---- бірінші тұлға қолдары (қолғап + жең) және ұңғы жарқылы ----
	{
		FKAPMat Glove = FKAPMat::Paint(FLinearColor(0.05f, 0.045f, 0.04f), 0.75f); Glove.Ribs = 0.2f; Glove.RibPeriod = 1.2f; Glove.DustTop = 0.3f; Glove.Grime = 0.7f;
		FKAPMat Sleeve = FKAPMat::Paint(FLinearColor(0.1f, 0.13f, 0.2f), 0.92f); Sleeve.Ribs = 0.35f; Sleeve.RibPeriod = 2.5f; Sleeve.Grime = 0.85f; Sleeve.DustTop = 0.6f;
		const bool bLong = Id == "ak" || Id == "sniper" || Id == "shotgun";
		const bool bPistol = Id == "pm" || Id == "flaregun";
		// тегіс жұдырық пен жең мештері (редактор генераторы); жоқ болса — ескі пішіндер
		static TWeakObjectPtr<UStaticMesh> FistM, SleeveM;
		if (!FistM.IsValid()) FistM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_Fist_0.SM_KAP_Fist_0"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!SleeveM.IsValid()) SleeveM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_Sleeve.SM_KAP_Sleeve"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		auto FP = [&](UStaticMesh* Ms, FVector P, FVector S, const FKAPMat& M, FRotator R)
		{
			UStaticMeshComponent* C = KAPVis::AddMesh(this, HeldRoot, Ms, P, S, M, R, false);
			C->SetCastShadow(false); C->SetOnlyOwnerSee(true); C->SetCachedMaxDrawDistance(0.f);
		};
		auto Arm = [&](FVector Hand, FVector Dir, FRotator HandRot)
		{
			const FVector D = Dir.GetSafeNormal();
			if (FistM.IsValid()) FP(FistM.Get(), Hand, FVector(1.05f), Glove, HandRot);                // жұдырық (қолғап)
			else
			{
				B(Hand, FVector(8, 6, 9), Glove, HandRot);                                               // алақан
				B(Hand + FVector(2.5f, 0, 3.5f), FVector(3, 6.5f, 3), Glove, HandRot);                     // саусақтар
			}
			Cy(Hand + D * 9.f, FVector(7.5f, 7.5f, 12.f), Glove, FRotationMatrix::MakeFromZ(D).Rotator());   // білек (қолғап жиегі)
			if (SleeveM.IsValid()) FP(SleeveM.Get(), Hand + D * 26.f, FVector(0.105f, 0.105f, 0.26f), Sleeve, FRotationMatrix::MakeFromZ(D).Rotator()); // жең
			else Cy(Hand + D * 26.f, FVector(10.f, 10.f, 26.f), Sleeve, FRotationMatrix::MakeFromZ(D).Rotator());
		};
		if (bLong)
		{
			Arm(FVector(-2, 0, -3.5f), FVector(-1.f, 0.35f, -0.55f), FRotator(-15, 0, 0));
			const float Fx = Id == "shotgun" ? 20.f : 28.f;
			Arm(FVector(Fx, -0.5f, -1.5f), FVector(-0.75f, -0.6f, -0.45f), FRotator(0, 0, -20));
		}
		else if (bPistol)
		{
			Arm(FVector(1, 0.5f, -4.5f), FVector(-1.f, 0.3f, -0.5f), FRotator(-14, 0, 0));
			Arm(FVector(0, -3.5f, -6.f), FVector(-0.85f, -0.55f, -0.4f), FRotator(-10, 0, 25));
		}
		else if (Id != "")
		{
			Arm(FVector(3, 0, 0), FVector(-1.f, 0.35f, -0.55f), FRotator(0, 0, 0));
		}
		// ұңғы ұшы: жарқыл кресті + жарық
		MuzzleLocal = Id == "ak" ? FVector(61, 0, 2.2f) : Id == "sniper" ? FVector(87, 0, 2.2f) : Id == "shotgun" ? FVector(70, 0, 3) : Id == "pm" ? FVector(17, 0, 3.4f) : FVector(20, 0, 3);
		MuzzleFx.Reset();
		if (bLong || bPistol)
		{
			const FKAPMat Fl = FKAPMat::Glow(FLinearColor(1.f, 0.62f, 0.22f), 0);
			static TWeakObjectPtr<UStaticMesh> MuzzleM;
			if (!MuzzleM.IsValid()) MuzzleM = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/KAP/Meshes/SM_KAP_Muzzle.SM_KAP_Muzzle"), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (MuzzleM.IsValid())
			{
				// жұлдыз тәрізді жалын (алға созылған жапырақшалар), әр атқанда бұрылады
				UStaticMeshComponent* F = KAPVis::AddMesh(this, HeldRoot, MuzzleM.Get(), MuzzleLocal + FVector(0.5f, 0, 0), FVector(bLong ? 1.35f : 0.85f), Fl);
				F->SetCastShadow(false); F->SetOnlyOwnerSee(true); F->SetCachedMaxDrawDistance(0.f); F->SetVisibility(false); MuzzleFx.Add(F);
			}
			else for (int32 k = 0; k < 3; k++) { UStaticMeshComponent* F = B(MuzzleLocal + FVector(5, 0, 0), FVector(12, 1, bLong ? 9.f : 6.f), Fl, FRotator(0, 0, k * 60.f)); F->SetVisibility(false); MuzzleFx.Add(F); }
			MuzzleLight = NewObject<UPointLightComponent>(this);
			MuzzleLight->SetupAttachment(HeldRoot); MuzzleLight->SetRelativeLocation(MuzzleLocal + FVector(8, 0, 0));
			MuzzleLight->SetIntensityUnits(ELightUnits::Candelas); MuzzleLight->SetIntensity(0.f);
			MuzzleLight->SetAttenuationRadius(900.f); MuzzleLight->SetLightColor(FLinearColor(1.f, 0.6f, 0.25f)); MuzzleLight->SetCastShadows(false);
			MuzzleLight->RegisterComponent();
		}
		else MuzzleLight = nullptr;
	}
	if (Id == "wrench")
	{
		// гайка кілті (24 мм): хромды сап, резеңке тұтқа, ашық ұшы және сақиналы ұшы
		B(FVector(12, 0, 0), FVector(26, 1.4f, 2.4f), Chrome);
		B(FVector(4, 0, 0), FVector(10, 1.8f, 3.0f), FKAPMat::Paint(FLinearColor(0.6f, 0.05f, 0.03f), 0.7f));
		B(FVector(26.5f, 0, 0), FVector(4, 1.5f, 5.5f), Chrome);
		B(FVector(30, 0, 2.2f), FVector(5, 1.5f, 1.8f), Chrome, FRotator(-12, 0, 0));
		B(FVector(30, 0, -2.2f), FVector(5, 1.5f, 1.8f), Chrome, FRotator(12, 0, 0));
		Cy(FVector(-2, 0, 0), FVector(5.5f, 5.5f, 1.4f), Chrome, AlongY);
		Cy(FVector(-2, 0, 0), FVector(3, 3, 1.6f), Blued, AlongY);
	}
	else if (Id == "knife")
	{
		// әскери пышақ: оралған тұтқа, көлденең қорғаныс, жүзі және ұшы
		B(FVector(0, 0, 0), FVector(11, 2.4f, 2.8f), Rubber);
		for (int32 i = 0; i < 4; i++) B(FVector(-3.5f + i * 2.4f, 0, 0), FVector(0.6f, 2.6f, 3.0f), Gun);
		Cy(FVector(-6, 0, 0), FVector(3, 3, 1.5f), Gun, AlongX);
		B(FVector(6, 0, 0), FVector(1, 2.6f, 5.5f), Gun);
		B(FVector(15, 0, 0.3f), FVector(16, 0.45f, 3.0f), Chrome);
		B(FVector(24.5f, 0, 0.9f), FVector(5, 0.45f, 1.8f), Chrome, FRotator(-18, 0, 0));
		B(FVector(14, 0, 1.6f), FVector(14, 0.6f, 0.4f), Gun);
	}
	else if (Id == "crowbar")
	{
		// қызыл бояуы түскен лом: ұзын шыбық, иілген тырнақ, жалпақ екінші ұшы
		B(FVector(24, 0, 0), FVector(56, 2.2f, 2.2f), RedPaint);
		for (int32 i = 0; i < 5; i++) { const float A = i * 22.f; B(FVector(52 + FMath::Sin(FMath::DegreesToRadians(A)) * 5.f, 0, 5.f - FMath::Cos(FMath::DegreesToRadians(A)) * 5.f), FVector(3, 2.2f, 2.2f), RedPaint, FRotator(-A, 0, 0)); }
		B(FVector(56, 0, 10.5f), FVector(1.6f, 2.8f, 5), Gun, FRotator(-80, 0, 0));
		B(FVector(-6, 0, 0.6f), FVector(8, 2.6f, 1.0f), Gun, FRotator(-10, 0, 0));
		B(FVector(4, 0, 0), FVector(12, 2.6f, 2.6f), Rubber);
	}
	else if (Id == "axe")
	{
		// балта: ағаш сап (сәл иілген), темір басы, жүзі жылтыр, сыналы
		const FKAPMat Handle = FKAPMat::Paint(FLinearColor(0.42f, 0.28f, 0.15f), 0.75f);
		for (int32 i = 0; i < 4; i++) B(FVector(6 + i * 13.f, 0, -0.6f + FMath::Sin(i * 0.9f) * 0.8f), FVector(13.5f, 2.6f, 3.2f - i * 0.15f), Handle, FRotator(-2 + i * 1.5f, 0, 0));
		B(FVector(55, 0, 2.0f), FVector(7, 3.4f, 9.5f), Gun);
		B(FVector(55.5f, 0, 9.5f), FVector(9.5f, 1.4f, 6.5f), Gun, FRotator(0, 0, 0));
		B(FVector(55.5f, 0, 13.4f), FVector(11, 0.5f, 1.6f), Chrome);
		B(FVector(55, 0, -3.8f), FVector(5.5f, 3.6f, 3.0f), Gun);
	}
	else if (Id == "flare")
	{
		// сигнал шамы: қызыл түтік, қақпақ, жарқыраған ұшы
		Cy(FVector(9, 0, 0), FVector(4.5f, 4.5f, 22), FKAPMat::Paint(FLinearColor(0.7f, 0.06f, 0.04f), 0.5f), AlongX);
		Cy(FVector(-2.5f, 0, 0), FVector(5, 5, 3), FKAPMat::Paint(FLinearColor(0.9f, 0.85f, 0.75f), 0.5f), AlongX);
		Cy(FVector(20.5f, 0, 0), FVector(3.5f, 3.5f, 2), FKAPMat::Glow(FLinearColor(1, 0.15f, 0.05f), 20), AlongX);
		B(FVector(9, 2.3f, 0), FVector(12, 0.2f, 3), FKAPMat::Paint(FLinearColor(0.95f, 0.9f, 0.3f), 0.6f));
	}
	else if (Id == "molotov")
	{
		// бөтелке: жасылдау шыны, мойын, жанып тұрған шүберек
		FKAPMat Glass = FKAPMat::Metal(FLinearColor(0.12f, 0.22f, 0.1f), 0.05f); Glass.Grime = 0.3f;
		Cy(FVector(0, 0, 4), FVector(7, 7, 17), Glass, FRotator::ZeroRotator);
		Cy(FVector(0, 0, 14.5f), FVector(3, 3, 6), Glass, FRotator::ZeroRotator);
		B(FVector(0, 0, 18.5f), FVector(2.5f, 2.5f, 4), FKAPMat::Paint(FLinearColor(0.6f, 0.55f, 0.45f), 0.95f), FRotator(10, 0, 15));
		B(FVector(0.5f, 0, 21), FVector(2, 2, 2.5f), FKAPMat::Glow(FLinearColor(1, 0.45f, 0.05f), 25));
		Cy(FVector(0, 0, 2), FVector(6.6f, 6.6f, 9), FKAPMat::Paint(FLinearColor(0.45f, 0.32f, 0.08f), 0.1f), FRotator::ZeroRotator);
	}
	else if (Id == "pm")
	{
		// Макаров тапаншасы: ысырма, рама, бакелит тұтқа, шүріппе қорғаны, нысана
		B(FVector(8, 0, 3.6f), FVector(16, 2.5f, 3.0f), Blued);
		for (int32 i = 0; i < 6; i++) B(FVector(1.5f + i * 0.9f, 0, 3.6f), FVector(0.35f, 2.6f, 2.6f), Gun);
		B(FVector(8, 0, 1.3f), FVector(15, 2.3f, 1.8f), Gun);
		Cy(FVector(16.3f, 0, 3.4f), FVector(1.0f, 1.0f, 0.6f), Gun, AlongX);
		B(FVector(1.2f, 0, -4.2f), FVector(4.6f, 2.8f, 9.5f), Bakelite, FRotator(-14, 0, 0));
		B(FVector(6.5f, 0, -1.4f), FVector(5.5f, 0.8f, 0.5f), Gun);
		B(FVector(9.2f, 0, -0.6f), FVector(0.5f, 0.8f, 1.6f), Gun);
		B(FVector(6.2f, 0, -0.3f), FVector(0.5f, 0.6f, 1.4f), Gun, FRotator(-20, 0, 0));
		B(FVector(15, 0, 5.3f), FVector(0.6f, 0.6f, 0.6f), Gun);
		B(FVector(1.5f, 0, 5.3f), FVector(0.8f, 1.6f, 0.7f), Gun);
		B(FVector(0.2f, 0, -9.3f), FVector(4.0f, 2.6f, 0.8f), Gun, FRotator(-14, 0, 0));
	}
	else if (Id == "flaregun")
	{
		// сигнал тапаншасы: қызғылт сары пластик, жуан ұңғы, сындырмалы құлып
		Cy(FVector(11, 0, 3), FVector(4.5f, 4.5f, 17), Orange, AlongX);
		Cy(FVector(19.6f, 0, 3), FVector(3.4f, 3.4f, 0.5f), Blued, AlongX);
		B(FVector(2.5f, 0, 2.5f), FVector(5, 3.2f, 5), Orange);
		B(FVector(0.5f, 0, -4.5f), FVector(4.5f, 3, 10), Orange, FRotator(-15, 0, 0));
		B(FVector(5, 0, -1), FVector(5, 0.8f, 0.6f), Orange);
		B(FVector(4.5f, 0, -0.2f), FVector(0.5f, 0.6f, 1.6f), Gun, FRotator(-20, 0, 0));
		B(FVector(-0.5f, 0, 5.5f), FVector(1.5f, 1.2f, 1.5f), Gun, FRotator(-30, 0, 0));
	}
	else if (Id == "shotgun")
	{
		// қос ұңғылы мылтық (ТОЗ тәрізді): екі ұңғы, ағаш цевье, болат қорап, дүмбі
		for (float Y : { -1.35f, 1.35f }) Cy(FVector(38, Y, 3), FVector(2.7f, 2.7f, 62), Blued, AlongX);
		B(FVector(38, 0, 4.4f), FVector(62, 0.8f, 0.6f), Blued);
		B(FVector(20, 0, 0.8f), FVector(20, 4.6f, 3.6f), Wood);
		B(FVector(4, 0, 2), FVector(12, 4.4f, 5.4f), Chrome);
		B(FVector(1, 0, 5.2f), FVector(4, 1.2f, 1.4f), Gun, FRotator(-20, 0, 0));
		B(FVector(-4, 0, -1.6f), FVector(6, 3.4f, 6), Wood, FRotator(-20, 0, 0));
		B(FVector(-20, 0, -3.5f), FVector(30, 3.8f, 6.5f), Wood, FRotator(-7, 0, 0));
		B(FVector(-35.5f, 0, -5.4f), FVector(1.5f, 4, 9), Rubber, FRotator(-7, 0, 0));
		B(FVector(5, 0, -1.2f), FVector(5.5f, 0.8f, 0.5f), Gun);
		B(FVector(4.2f, 0, -0.4f), FVector(0.5f, 0.6f, 1.6f), Gun, FRotator(-20, 0, 0));
	}
	else if (Id == "ak")
	{
		// Калашников: қорап, қақпақ, ұңғы + газ түтігі, ағаш цевье, иілген магазин, тапанша тұтқа, дүмбі, нысаналар
		B(FVector(8, 0, 1.2f), FVector(26, 3.6f, 5), Gun);
		B(FVector(8, 0, 4.1f), FVector(24, 3.2f, 1.2f), Blued);
		B(FVector(14, 2.0f, 2.2f), FVector(3, 0.5f, 1), Gun);
		Cy(FVector(40, 0, 2.2f), FVector(1.9f, 1.9f, 34), Blued, AlongX);
		Cy(FVector(30, 0, 4.4f), FVector(2.5f, 2.5f, 16), Gun, AlongX);
		B(FVector(28, 0, 1.4f), FVector(14, 4.2f, 4.2f), Wood);
		Cy(FVector(58.5f, 0, 2.2f), FVector(2.8f, 2.8f, 4.5f), Gun, AlongX);
		B(FVector(54, 0, 4.6f), FVector(1, 0.8f, 3.2f), Gun);
		B(FVector(23, 0, 5.4f), FVector(3, 2.2f, 1.4f), Gun);
		for (int32 i = 0; i < 4; i++) B(FVector(13.5f + i * 1.7f, 0, -3.5f - i * 3.6f), FVector(5.4f, 2.8f, 4.2f), FKAPMat::Paint(FLinearColor(0.32f, 0.12f, 0.04f), 0.55f), FRotator(-10.f - i * 7.f, 0, 0));
		B(FVector(-1.5f, 0, -4.5f), FVector(3.8f, 3.0f, 8.5f), Bakelite, FRotator(-18, 0, 0));
		B(FVector(4.5f, 0, -1.6f), FVector(6, 0.8f, 0.5f), Gun);
		B(FVector(4, 0, -0.6f), FVector(0.5f, 0.6f, 1.6f), Gun, FRotator(-20, 0, 0));
		B(FVector(-18, 0, -0.6f), FVector(26, 3.6f, 5.6f), Wood, FRotator(-5, 0, 0));
		B(FVector(-31.5f, 0, -1.8f), FVector(1.4f, 3.8f, 8), Gun, FRotator(-5, 0, 0));
	}
	else if (Id == "sniper")
	{
		// СВД тәрізді: ұзын ұңғы, жалын сөндіргіш, ағаш цевье, ойық дүмбі, 4× оптика
		B(FVector(6, 0, 1.2f), FVector(24, 3.4f, 4.8f), Gun);
		Cy(FVector(52, 0, 2.2f), FVector(1.8f, 1.8f, 60), Blued, AlongX);
		Cy(FVector(83.5f, 0, 2.2f), FVector(2.6f, 2.6f, 6), Gun, AlongX);
		B(FVector(28, 0, 1.5f), FVector(22, 4.0f, 4.4f), Wood);
		for (int32 i = 0; i < 4; i++) B(FVector(21 + i * 4.5f, 2.05f, 2.2f), FVector(2.2f, 0.2f, 1.2f), Gun);
		for (int32 i = 0; i < 2; i++) B(FVector(10.5f + i * 1.8f, 0, -3.8f), FVector(6.4f, 2.6f, 4.6f), Gun, FRotator(-6.f - i * 4.f, 0, 0));
		B(FVector(-2, 0, -4), FVector(3.8f, 3, 8), Wood, FRotator(-18, 0, 0));
		B(FVector(-14, 0, 2), FVector(20, 3.4f, 2), Wood, FRotator(-3, 0, 0));
		B(FVector(-16, 0, -4.4f), FVector(18, 3.4f, 2), Wood, FRotator(-9, 0, 0));
		B(FVector(-25.5f, 0, -1.5f), FVector(2.5f, 3.6f, 9.5f), Wood, FRotator(-5, 0, 0));
		B(FVector(4, 0, -1.5f), FVector(5.5f, 0.8f, 0.5f), Gun);
		// оптика
		Cy(FVector(4, 0, 8.2f), FVector(3.6f, 3.6f, 22), Blued, AlongX);
		Cy(FVector(16, 0, 8.2f), FVector(4.6f, 4.6f, 5), Blued, AlongX);
		Cy(FVector(-8, 0, 8.2f), FVector(4.2f, 4.2f, 4), Rubber, AlongX);
		Cy(FVector(18.6f, 0, 8.2f), FVector(3.8f, 3.8f, 0.3f), FKAPMat::Metal(FLinearColor(0.1f, 0.2f, 0.35f), 0.02f), AlongX);
		for (float X : { -1.f, 10.f }) B(FVector(X, 0, 5.5f), FVector(2, 2.6f, 3), Gun);
		Cy(FVector(4, 0, 10.6f), FVector(1.6f, 1.6f, 2), Gun, FRotator::ZeroRotator);
		Cy(FVector(4, 2.2f, 8.2f), FVector(1.6f, 1.6f, 2), Gun, AlongY);
	}
}

void AKAPCharacter::SelectSlot(int32 I)
{
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (GI && GI->S.Weapons.IsValidIndex(I)) Equip(GI->S.Weapons[I]);
}
void AKAPCharacter::CycleSlot(int32 Dir)
{
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	if (!GI || !GI->S.Weapons.Num()) return;
	int32 I = GI->S.Weapons.IndexOfByKey(GI->S.Equipped);
	I = (I + Dir + GI->S.Weapons.Num()) % GI->S.Weapons.Num();
	Equip(GI->S.Weapons[I]);
}

void AKAPCharacter::Reload()
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const FKAPWeaponRow* W = UKAPDataSubsystem::Get(this)->Weapon(GI->S.Equipped);
	if (!W || W->Kind != "gun" || ReloadT > 0) return;
	const int32 Need = W->Mag - GI->S.Mag.FindRef(GI->S.Equipped);
	if (Need <= 0 || GI->S.Ammo.FindRef(W->Ammo) <= 0) return;
	ReloadT = GI->S.Equipped == "sniper" ? 2.4f : GI->S.Equipped == "shotgun" ? 1.9f : 1.5f;
	ReloadDur = ReloadT; ReloadStage = 0;
	ReloadId = GI->S.Equipped;
}

void AKAPCharacter::FireStart() { if (!bFrozen && !bDead) { bFiring = true; DoFire(); } }

void AKAPCharacter::DoFire()
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	const FKAPWeaponRow* W = D->Weapon(GI->S.Equipped);
	if (!W || Cooldown > 0 || ReloadT > 0) return;
	Cooldown = 1.f / FMath::Max(0.1f, W->Rate);
	const FVector O = Camera->GetComponentLocation();
	const FVector Dir = GetViewRotation().Vector(); // (камера компоненті кадр ортасында көлденең болуы мүмкін — көздеу бағыты басқарудан алынады)
	FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPFire), false, this);
	if (W->Kind == "melee")
	{
		SwingT = 0.25f;
		UKAPAudio::Get(this)->Whoosh(GI->S.Equipped == "crowbar" || GI->S.Equipped == "axe");
		SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), GetStamina() - 4);
		FHitResult H;
		if (GetWorld()->SweepSingleByChannel(H, O, O + Dir * W->Range * 100.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(25.f), Q))
			if (AKAPNPC* N = Cast<AKAPNPC>(H.GetActor())) { N->SetHitImpulse(Dir * 700.f, FName("spine_03"), false); UGameplayStatics::ApplyDamage(N, W->Damage, GetController(), this, nullptr); UKAPAudio::Get(this)->Impact(H.ImpactPoint, 0, 0.9f); }
				else if (AKAPAnimal* An = Cast<AKAPAnimal>(H.GetActor())) { UGameplayStatics::ApplyDamage(An, W->Damage, GetController(), this, nullptr); UKAPAudio::Get(this)->Impact(H.ImpactPoint, 0, 0.9f); }
		// адамға тимесе — сексеуіл: балтамен шабылады (басқа құралмен — баяу)
		if (!Cast<AKAPNPC>(H.GetActor()) && !Cast<AKAPAnimal>(H.GetActor())) if (AKAPNature* Nat = AKAPNature::Get(GetWorld())) if (Nat->MeleeHit(O, Dir, W->Range * 100.f, W->Damage, GI->S.Equipped == "axe")) { bFiring = false; return; }
		// қабырға/металға тиді ме — беттің түріне қарай соққы дыбысы
		FHitResult Hw;
		if (!H.GetActor() && GetWorld()->LineTraceSingleByChannel(Hw, O, O + Dir * W->Range * 100.f, ECC_Visibility, Q))
			UKAPAudio::Get(this)->Impact(Hw.ImpactPoint, Hw.GetComponent() && Hw.GetComponent()->ComponentHasTag("SurfMetal") ? 1 : Hw.GetComponent() && Hw.GetComponent()->ComponentHasTag("SurfWood") ? 2 : 3, 0.8f);
		bFiring = false;
		return;
	}
	if (W->Kind == "throw")
	{
		int32& N = GI->S.Ammo.FindOrAdd(W->Ammo);
		if (N <= 0) { UKAPAudio::Get(this)->Beep(false); bFiring = false; return; }
		N--;
		AKAPProjectile* P = GetWorld()->SpawnActor<AKAPProjectile>(O + Dir * 60.f, Dir.Rotation());
		if (P) P->Launch(GI->S.Equipped == "molotov" ? EKAPProjectile::Molotov : EKAPProjectile::Flare, Dir * 1600.f + FVector(0, 0, 400), this);
		bFiring = false;
		return;
	}
	int32& Mag = GI->S.Mag.FindOrAdd(GI->S.Equipped);
	if (Mag <= 0) { UKAPAudio::Get(this)->Foley(2); Reload(); bFiring = false; return; }
	Mag--;
	UKAPAudio::Get(this)->Shot(GI->S.Equipped);
	FlashT = 0.055f;
	for (UStaticMeshComponent* F : MuzzleFx) if (F) F->SetRelativeRotation(FRotator(0, 0, FMath::FRandRange(0.f, 60.f)));
	Noise = 1.f;
	Recoil += GI->S.Equipped == "sniper" ? 3.f : GI->S.Equipped == "shotgun" ? 2.5f : 0.8f;
	// (жарқыл — ұңғы ұшындағы MuzzleFx; бұрынғы үлкен FX шары алынды)
	if (GI->S.Equipped == "flaregun")
	{
		if (AKAPProjectile* P = GetWorld()->SpawnActor<AKAPProjectile>(O + Dir * 60.f, Dir.Rotation())) P->Launch(EKAPProjectile::Flare, Dir * 3500.f, this);
		bFiring = false;
		return;
	}
	for (int32 i = 0; i < W->Pellets; i++)
	{
		const float Sp = W->Spread * (bScoped ? 0.2f : bMoving ? 1.8f : 1.f);
		const FVector Dd = (Dir + FVector(FMath::FRandRange(-Sp, Sp), FMath::FRandRange(-Sp, Sp), FMath::FRandRange(-Sp, Sp))).GetSafeNormal();
		FHitResult H;
		if (GetWorld()->LineTraceSingleByChannel(H, O, O + Dd * W->Range * 100.f, ECC_Visibility, Q))
		{
			if (AKAPNPC* N = Cast<AKAPNPC>(H.GetActor()))
			{
				const bool bHead = H.ImpactPoint.Z > N->GetActorLocation().Z + N->GetSimpleCollisionHalfHeight() * 0.7f && N->IsHuman();
				// оқ серпіні: қарудың күшіне қарай (өлсе — дене оқ бағытымен құлайды)
				N->SetHitImpulse(Dd * FMath::Clamp(W->Damage * 70.f, 800.f, 6000.f) * (W->Pellets > 1 ? 2.5f : 1.f), bHead ? FName("head") : FName("spine_03"), false);
				UGameplayStatics::ApplyDamage(N, W->Damage * (bHead ? 2.2f : 1.f), GetController(), this, nullptr);
				UKAPAudio::Get(this)->Impact(H.ImpactPoint, 0, 0.8f);
			}
			else if (AKAPAnimal* An = Cast<AKAPAnimal>(H.GetActor()))
			{
				UGameplayStatics::ApplyDamage(An, W->Damage, GetController(), this, nullptr);
				UKAPAudio::Get(this)->Impact(H.ImpactPoint, 0, 0.8f);
			}
			else
			{
				const bool bMetal = H.GetComponent() && H.GetComponent()->ComponentHasTag("SurfMetal");
				if (i == 0) UKAPAudio::Get(this)->Impact(H.ImpactPoint, bMetal ? 1 : H.GetComponent() && H.GetComponent()->ComponentHasTag("SurfWood") ? 2 : 3, 0.7f);
				if (AKAPFXManager* FX = AKAPFXManager::Get(GetWorld())) FX->Spawn(bMetal ? EKAPFX::Flash : EKAPFX::Dust, H.ImpactPoint, bMetal ? 0.15f : 0.5f);
			}
		}
	}
	// Рұқсатсыз ату — күзет пен жасақ дұшпандыққа көшеді
	if (!GI->S.bPermitWeapon)
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (!It->bDead && (It->Faction == EKAPFaction::Militia || It->Faction == EKAPFaction::Guard) && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < 8000.f)
			{ GM->Provoke(TEXT("Рұқсатсыз атыс! Күзет пен жасақ сізге қарсы.")); break; }
	if (GI->S.Equipped != "ak") bFiring = false;
}

void AKAPCharacter::UseItem(FName Id)
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	int32* N = GI->S.Inv.Find(Id);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	if (!N || *N <= 0) { GM->Toast(D->ItemName(Id) + TEXT(" жоқ"), "warn"); return; }
	if (Id == "medkit") SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetHp() + 50);
	else if (Id == "bandage") SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetHp() + 15);
	else if (Id == "food") { SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetHp() + 12); SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), GetStamina() + 20); }
	else if (Id == "water") SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), GetWater() + 45);
	else if (Id == "iodine") GI->S.IodineUntil = GI->S.Time + 6;
	else if (Id == "antidote") SetAttr(UKAPSurvivalAttributeSet::GetDoseAttribute(), GetDose() - 120);
	else if (Id == "firewood")
	{
		// сексеуіл отыны: алдыңа от жағу (2 бума) — 4 минут жанады, жылытады
		if (*N < 2) { GM->Toast(TEXT("От жағуға кемінде 2 бума отын керек"), "warn"); return; }
		if (bIndoor) { GM->Toast(TEXT("Үй ішінде от жағуға болмайды"), "warn"); return; }
		const FVector At = GetActorLocation() + FRotator(0, GetControlRotation().Yaw, 0).Vector() * 160.f;
		if (GM->Builder) if (AKAPStructure* F = GM->Builder->SpawnStruct((int32)EStructType::Campfire, FVector2D(At.X, At.Y) / 100.0, 0.f, FVector(1), FLinearColor::White)) F->SetLifeSpan(240.f);
		(*N)--;
	}
	else return;
	(*N)--;
	UKAPAudio::Get(this)->Play(EKAPSound::Pickup, 0.6f);
	GM->Toast(D->ItemName(Id) + TEXT(" қолданылды"), "ok");
}

float AKAPCharacter::TakeDamage(float Damage, FDamageEvent const& Event, AController* Inst, AActor* Causer)
{
	// бронежилет киюлі болса — оқ пен соққы 35%-ға әлсірейді
	if (const UKAPGameInstance* GI = UKAPGameInstance::Get(this)) if (GI->S.Wear.Contains("armor")) Damage *= 0.65f;
	DamagePlayer(Damage);
	return Damage;
}

void AKAPCharacter::DamagePlayer(float Amount, float DoseAdd, bool bSilent)
{
	if (bDead) return;
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (GM && GM->bCinematic) return;
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetHp() - Amount);
	if (DoseAdd > 0) SetAttr(UKAPSurvivalAttributeSet::GetDoseAttribute(), GetDose() + DoseAdd * (GI && GI->S.Wear.Contains("suit") ? 0.3f : 1.f));
	if (!bSilent) { HurtFlash = 0.7f; UKAPAudio::Get(this)->Play(EKAPSound::Hurt, 0.5f); }
	if (GetHp() <= 0.01f) { bDead = true; if (GM) GM->OnPlayerDied(); }
}

void AKAPCharacter::Respawn(FVector At)
{
	bDead = false;
	SetActorLocation(At + FVector(0, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
	SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), FMath::Max(60.f, GetMaxHp() * 0.7f));
	SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), FMath::Max(GetWater(), 50.f));
	SetAttr(UKAPSurvivalAttributeSet::GetBodyTempAttribute(), 36.6f);
	SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), 100.f);
}

void AKAPCharacter::Interact()
{
	if (bFrozen || bDead) return;
	if (UObject* T = InteractTarget.Get()) if (IKAPInteractable* I = Cast<IKAPInteractable>(T)) I->Interact(this);
}

void AKAPCharacter::UpdateInteraction()
{
	InteractText.Reset(); InteractTarget = nullptr;
	if (bFrozen) return;
	const FVector P = GetActorLocation(), F = GetViewRotation().Vector();
	float Best = 1e9f;
	for (KAPReg::TIter<AActor> It(GetWorld(), KAPReg::Interactables()); It; ++It)
	{
		IKAPInteractable* I = Cast<IKAPInteractable>(*It);
		if (!I) continue;
		const FVector L = I->GetInteractLocation();
		const float D = FVector::Dist2D(L, P);
		if (D > I->GetInteractRadius() || FMath::Abs(L.Z - P.Z) > 300.f) continue;
		const FVector To = (L - P).GetSafeNormal2D();
		const float Dot = D < 120.f ? 1.f : FVector::DotProduct(To, F.GetSafeNormal2D());
		if (Dot < 0.45f) continue;
		const FString T = I->GetInteractText(this);
		if (T.IsEmpty()) continue;
		const float Sc = D * (2.f - Dot);
		if (Sc < Best) { Best = Sc; InteractText = T; InteractTarget = *It; }
	}
}

void AKAPCharacter::UpdateHeld(float Dt)
{
	SwingT = FMath::Max(0.f, SwingT - Dt);
	Recoil = FMath::Max(0.f, Recoil - Dt * 12.f);
	const float Bob = bMoving ? FMath::Sin(BobT) * 1.5f : 0.f;
	const float Sw = SwingT > 0 ? FMath::Sin(SwingT / 0.25f * UE_PI) : 0.f;
	// оқтау: қаруды бұрып, төмен түсіреді; кезеңдерінде дыбыс (шығару → салу → затвор)
	const float Rp = ReloadT > 0 && ReloadDur > 0 ? 1.f - ReloadT / ReloadDur : 0.f;
	const float Rk = ReloadT > 0 ? FMath::Sin(Rp * UE_PI) : 0.f;
	if (ReloadT > 0)
	{
		const float Marks[3] = { 0.08f, 0.5f, 0.84f };
		while (ReloadStage < 3 && Rp >= Marks[ReloadStage]) { if (UKAPAudio* A = UKAPAudio::Get(this)) A->ReloadSfx(ReloadId, ReloadStage); ReloadStage++; }
	}
	// жүгіргенде қаруды төмен, бүйірге ұстайды
	const bool bRunPose = GetVelocity().Size2D() > 520.f && !bFiring && ReloadT <= 0 && SwingT <= 0;
	RunPose = FMath::FInterpTo(RunPose, bRunPose ? 1.f : 0.f, Dt, 8.f);
	// ұңғы жарқылы
	FlashT = FMath::Max(0.f, FlashT - Dt);
	for (UStaticMeshComponent* F : MuzzleFx) if (F) { F->SetVisibility(FlashT > 0 && !bScoped); if (UMaterialInstanceDynamic* Mi = Cast<UMaterialInstanceDynamic>(F->GetMaterial(0))) Mi->SetVectorParameterValue("Emissive", FLinearColor(1.f, 0.62f, 0.22f) * 80.f); }
	if (MuzzleLight) MuzzleLight->SetIntensity(FlashT > 0 ? 2500.f : 0.f);
	HeldRoot->SetRelativeLocation(HeldBase + FVector(Sw * 15 - RunPose * 6.f, -Sw * 6.f, Bob - Rk * 10.f - RunPose * 7.f));
	HeldRoot->SetRelativeRotation(FRotator(-Sw * 45 + Recoil * 2 + Rk * 14.f - RunPose * 20.f, Sw * 30.f + RunPose * 25.f, -Rk * 32.f));
	HeldRoot->SetVisibility(!bScoped, true);
	for (UStaticMeshComponent* F : MuzzleFx) if (F) F->SetVisibility(FlashT > 0 && !bScoped); // HeldRoot балаларды қайта қосады — жарқылды соңынан қоямыз
	const float Spd = GetVelocity().Size2D();
	const float Fov = bScoped ? 18.f : 85.f + FMath::Clamp((Spd - 450.f) / 200.f, 0.f, 1.f) * 6.f;
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Fov, Dt, 12.f));
	// бас тербелісі (head bob): қадам ырғағымен тік + бүйірге, дауылда желден шайқалу
	const float BobK = bMoving && !bScoped ? FMath::Clamp(Spd / 600.f, 0.f, 1.f) : 0.f;
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Storm = WX && !bIndoor ? WX->Env.Storm : 0.f;
	const float Tm = GetWorld()->GetTimeSeconds();
	CamBob = FMath::FInterpTo(CamBob, BobK, Dt, 6.f);
	Camera->SetRelativeLocation(FVector(0, FMath::Sin(BobT * 0.5f) * 1.6f * CamBob + FMath::PerlinNoise1D(Tm * 1.3f) * 2.f * Storm, 72 + FMath::Abs(FMath::Sin(BobT * 0.5f)) * 3.f * CamBob));
	Camera->SetWorldRotation(GetViewRotation() + FRotator(0, 0, FMath::Sin(BobT * 0.5f) * 0.5f * CamBob + FMath::PerlinNoise1D(Tm * 0.7f + 3.f) * 1.2f * Storm));
	if (Recoil > 0) AddControllerPitchInput(-Recoil * Dt * 2.f);
}

void AKAPCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GM || bDead) return;
	Cooldown = FMath::Max(0.f, Cooldown - Dt);
	HurtFlash = FMath::Max(0.f, HurtFlash - Dt * 1.5f);
	Noise = FMath::Max(0.f, Noise - Dt * 0.6f);
	if (ReloadT > 0)
	{
		ReloadT -= Dt;
		if (ReloadT <= 0)
		{
			UKAPGameInstance* GI = UKAPGameInstance::Get(this);
			const FKAPWeaponRow* W = UKAPDataSubsystem::Get(this)->Weapon(ReloadId);
			if (W) { int32& Mag = GI->S.Mag.FindOrAdd(ReloadId); int32& Am = GI->S.Ammo.FindOrAdd(W->Ammo); const int32 N = FMath::Min(W->Mag - Mag, Am); Mag += N; Am -= N; }
		}
	}
	if (bFiring) DoFire();
	InteractAcc += Dt;
	if (InteractAcc > 0.1f) { InteractAcc = 0; UpdateInteraction(); }
	UpdateHeld(Dt);
	if (bFrozen) { MoveVec = FVector2D::ZeroVector; return; }
	UpdateSurvival(Dt, GM->LastDh);
}

void AKAPCharacter::UpdateSurvival(float Dt, double Dh)
{
	UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GI || !D || !WX) return;
	FKAPSession& S = GI->S;
	const FKAPEnv& E = WX->Env;
	const FVector P = GetActorLocation();
	bIndoor = AKAPStructure::IsIndoor(GetWorld(), P);
	const bool bShade = bIndoor || AKAPStructure::IsShaded(GetWorld(), P);
	const bool bNearFire = AKAPStructure::IsNearFire(GetWorld(), P) || AKAPProjectile::IsNearFire(GetWorld(), P);
	// ---- қозғалыс ----
	const bool bRun = bSprint && MoveVec.Y > 0.1f && GetStamina() > 3;
	float Sp = bRun ? 750.f : 420.f;
	if (GetWater() < 20) Sp *= 0.75f;
	if (GetTemp() < 35) Sp *= 0.8f;
	if (E.Storm > 0.5f && !bIndoor) Sp *= 0.85f;
	// батпақ (жаңбырдан кейін, өзен жағасында) және су: аяқ батады
	if (!bIndoor)
	{
		float Wz = 0.f, Wd = 0.f;
		if (AKAPNature::WaterAt(GetWorld(), P - FVector(0, 0, 88), Wz, Wd)) Sp *= FMath::Clamp(1.f - Wd * 0.6f, 0.3f, 1.f);
		else if (GM->DistToRoad(KAP::CM2M(P)) > 6) { const float Mud = AKAPNature::MudAt(GetWorld(), KAP::CM2M(P)); if (Mud > 0.3f) Sp *= 1.f - (Mud - 0.3f) * 0.3f; }
	}
	// жел: қарсы жүрсе баяулайды, желмен бірге — жылдамдайды (10 м/с жел ≈ ±0.9 м/с)
	if (!bIndoor && !MoveVec.IsNearlyZero())
	{
		const FRotator Yw(0, GetControlRotation().Yaw, 0);
		const FVector Md = (FRotationMatrix(Yw).GetUnitAxis(EAxis::X) * MoveVec.Y + FRotationMatrix(Yw).GetUnitAxis(EAxis::Y) * MoveVec.X).GetSafeNormal2D();
		const float Along = FVector::DotProduct(Md, E.Wind / 100.f);
		Sp = FMath::Max(Sp * 0.35f, Sp + Along * 9.f);
	}
	if (GetHp() < 20) Sp *= 0.8f;
	GetCharacterMovement()->MaxWalkSpeed = Sp;
	if (!MoveVec.IsNearlyZero())
	{
		const FRotator Yaw(0, GetControlRotation().Yaw, 0);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveVec.Y);
		AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveVec.X);
	}
	bMoving = GetVelocity().Size2D() > 50.f;
	BobT += bMoving ? Dt * (bRun ? 11.f : 7.f) : 0.f;
	if (bRun && bMoving) SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), GetStamina() - D->Cfg("staminaRun", 14) * Dt);
	else SetAttr(UKAPSurvivalAttributeSet::GetStaminaAttribute(), GetStamina() + (GetWater() < 20 ? 4.f : 11.f) * Dt);
	Noise = FMath::Max(Noise, bMoving ? (bRun ? 0.45f : 0.15f) : 0.f);
	// ---- су ----
	const float HeatMul = (E.Temp > 33 && !bShade) ? D->Cfg("waterHeatMul", 2.2f) : 1.f;
	SetAttr(UKAPSurvivalAttributeSet::GetWaterAttribute(), GetWater() - D->Cfg("waterDecay", 5) * (float)Dh * HeatMul * (bRun ? 1.5f : 1.f));
	if (GetWater() <= 0) DamagePlayer(0.5f * Dt, 0, true);
	// ---- дене температурасы ----
	float Target = 36.6f;
	if (E.Temp > 32 && !bShade && !E.bNight && E.Storm < 0.5f) Target = 37.f + (E.Temp - 32.f) * 0.22f - (S.Wear.Contains("hat") ? 1.3f : 0.f);
	if (E.Temp < 14 && !bIndoor) Target = 36.6f - (14.f - E.Temp) * 0.18f + (S.Wear.Contains("coat") ? 1.8f : 0.f);
	if (bNearFire) Target = FMath::Max(Target, 36.8f);
	SetAttr(UKAPSurvivalAttributeSet::GetBodyTempAttribute(), GetTemp() + (Target - GetTemp()) * FMath::Min(1.f, Dt * 0.012f));
	if (GetTemp() > 39) DamagePlayer(0.4f * Dt, 0, true);
	if (GetTemp() < 35) DamagePlayer(0.3f * Dt, 0, true);
	// ---- сәулелену ----
	float Rate = 0;
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	const FVector2D Pm = KAP::CM2M(P);
	for (int32 i = 0; i < WM->Wells.Num(); i++)
	{
		const FKAPWellSave& W = S.Wells[i];
		if (W.State != EWellState::Leak && W.State != EWellState::Fire && !(W.State == EWellState::Fault && W.Fault == EFaultType::Acid)) continue;
		const float Dm = (float)FVector2D::Distance(Pm, WM->Wells[i].PosM());
		const float R = W.State == EWellState::Fire ? 60.f : 40.f;
		if (Dm < R) Rate += D->Cfg("playerLeakDose", 0.9f) * (1 - Dm / R) * (W.State == EWellState::Fire ? 2.f : W.State == EWellState::Fault ? 0.3f : 1.f);
	}
	Rate += GM->RadiationZoneAt(Pm);
	if (S.Wear.Contains("suit")) Rate *= 0.3f;
	if (S.Time < S.IodineUntil) Rate *= 0.3f;
	DoseRate = Rate;
	SetAttr(UKAPSurvivalAttributeSet::GetDoseAttribute(), GetDose() + Rate * Dt);
	SetAttr(UKAPSurvivalAttributeSet::GetMaxHealthAttribute(), FMath::Clamp(100.f - FMath::Max(0.f, GetDose() - 200.f) / 8.f, 30.f, 100.f));
	if (GetHp() > GetMaxHp()) SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetMaxHp());
	if (GetWater() > 50 && GetTemp() > 35.5f && GetTemp() < 38.5f) SetAttr(UKAPSurvivalAttributeSet::GetHealthAttribute(), GetHp() + 0.05f * Dt);
	// ---- рұқсатсыз қару ұстау ----
	const FKAPWeaponRow* Wd = D->Weapon(S.Equipped);
	if (Wd && Wd->Permit && !S.bPermitWeapon)
	{
		AKAPNPC* Witness = nullptr;
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (!It->bDead && (It->Faction == EKAPFaction::Militia || It->Faction == EKAPFaction::Guard) && FVector::Dist(It->GetActorLocation(), P) < 3000.f && It->HasLineOfSightTo(this)) { Witness = *It; break; }
		if (Witness)
		{
			WarnT += Dt;
			if (WarnT > 0.1f && WarnT < 0.2f)
			{
				GM->Subtitle(TEXT("Қаруды жасыр! Рұқсатың жоқ!"), TEXT("Убери оружие! У тебя нет разрешения!"), Witness->DisplayName, 3);
				// күзетші/жасақшы өзі айқайлайды (3D)
				if (UKAPAudio* A = UKAPAudio::Get(this)) A->Speak(TEXT("Қаруды жасыр! Рұқсатың жоқ!"), Witness->GetActorLocation() + FVector(0, 0, 60), Witness->IsFemale(), 0, 1.4f);
			}
			if (WarnT > 3.5f) { GM->Provoke(TEXT("Рұқсатсыз қару! Күзет пен жасақ дұшпандыққа көшті.")); WarnT = 0; }
		}
		else WarnT = 0;
	}
	else WarnT = 0;
}
