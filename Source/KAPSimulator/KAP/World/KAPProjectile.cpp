#include "KAPProjectile.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPVisuals.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPFXManager.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/NPC/KAPNPC.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Player/KAPCharacter.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAP/Audio/KAPAudio.h"
#include "KAP/Core/KAPRegistry.h"
#include "Components/PointLightComponent.h"
#include "KAP/Core/KAPRegistry.h"
#include "EngineUtils.h"
#include "KAP/Core/KAPRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "KAP/Core/KAPRegistry.h"

AKAPProjectile::AKAPProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
	Light = CreateDefaultSubobject<UPointLightComponent>("Light");
	Light->SetupAttachment(RootComponent);
	Light->SetIntensity(0);
	Light->SetCastShadows(false);
}

void AKAPProjectile::Launch(EKAPProjectile InType, FVector Velocity, AActor* By)
{
	Type = InType; Vel = Velocity; Thrower = By;
	Body = KAPVis::Add(this, RootComponent, EKAPShape::Cylinder, FVector::ZeroVector, FVector(7, 7, 22),
		Type == EKAPProjectile::Flare ? FKAPMat::Glow(FLinearColor(1, 0.1f, 0.05f), 50) : FKAPMat::Paint(FLinearColor(0.4f, 0.3f, 0.1f), 0.1f), FRotator::ZeroRotator, false);
	if (Type == EKAPProjectile::Flare) { Light->SetIntensity(20000); Light->SetAttenuationRadius(4500); Light->SetLightColor(FLinearColor(1, 0.15f, 0.08f)); }
}

void AKAPProjectile::Tick(float Dt)
{
	Super::Tick(Dt);
	AKAPFXManager* FX = AKAPFXManager::Get(GetWorld());
	if (!bLanded)
	{
		Vel.Z -= 980.f * Dt;
		const FVector From = GetActorLocation(), To = From + Vel * Dt;
		FHitResult H;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(KAPProj), false, this);
		if (Thrower.IsValid()) Q.AddIgnoredActor(Thrower.Get());
		if (GetWorld()->LineTraceSingleByChannel(H, From, To, ECC_Visibility, Q))
		{
			SetActorLocation(H.ImpactPoint + FVector(0, 0, 5));
			bLanded = true; Life = Type == EKAPProjectile::Molotov ? 9.f : 60.f;
			if (Type == EKAPProjectile::Molotov)
			{
				Body->SetVisibility(false);
				Light->SetIntensity(30000); Light->SetAttenuationRadius(2500); Light->SetLightColor(FLinearColor(1, 0.45f, 0.1f));
				UKAPAudio::Get(this)->Play(EKAPSound::Hit, 0.8f);
			}
		}
		else
		{
			SetActorLocation(To);
			Body->AddLocalRotation(FRotator(720 * Dt, 0, 0));
			if (Type == EKAPProjectile::Flare && FX) FX->Spawn(EKAPFX::Flare, To, 1.f);
		}
		if (GetActorLocation().Z < -100000) Destroy();
		return;
	}
	Life -= Dt;
	if (Type == EKAPProjectile::Molotov)
	{
		Light->SetIntensity(25000 + FMath::FRand() * 15000);
		if (FX && FMath::FRand() < Dt * 30) FX->Spawn(EKAPFX::Fire, GetActorLocation() + FVector(FMath::FRandRange(-350.f, 350.f), FMath::FRandRange(-350.f, 350.f), 20), 1.5f);
		for (KAPReg::TIter<AKAPNPC> It(GetWorld(), KAPReg::NPCs()); It; ++It)
			if (!It->bDead && FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < 450.f) UGameplayStatics::ApplyDamage(*It, 28.f * Dt, nullptr, Thrower.Get(), nullptr);
		if (AKAPCharacter* PC = AKAPCharacter::Get(this)) if (FVector::Dist2D(PC->GetActorLocation(), GetActorLocation()) < 450.f) PC->DamagePlayer(12.f * Dt, 0, true);
	}
	else
	{
		Light->SetIntensity(18000 + FMath::FRand() * 6000);
		if (FX && FMath::FRand() < Dt * 12) FX->Spawn(EKAPFX::Flare, GetActorLocation() + FVector(0, 0, 20), 1.f);
	}
	if (Life <= 0) Destroy();
}

bool AKAPProjectile::NearestFlare(UWorld* W, FVector2D FromM, double MaxM, FVector2D& Out)
{
	double Best = MaxM; bool bFound = false;
	for (TActorIterator<AKAPProjectile> It(W); It; ++It)
	{
		if (!It->bLanded || It->Type != EKAPProjectile::Flare) continue;
		const FVector2D P = FVector2D(It->GetActorLocation()) / 100.0;
		const double D = FVector2D::Distance(P, FromM);
		if (D < Best) { Best = D; Out = P; bFound = true; }
	}
	return bFound;
}

bool AKAPProjectile::IsNearFire(UWorld* W, const FVector& P)
{
	for (TActorIterator<AKAPProjectile> It(W); It; ++It)
		if (It->bLanded && It->Type == EKAPProjectile::Molotov && FVector::Dist2D(It->GetActorLocation(), P) < 800.f) return true;
	return false;
}
