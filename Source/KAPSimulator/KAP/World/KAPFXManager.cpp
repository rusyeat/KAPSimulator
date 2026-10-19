#include "KAPFXManager.h"
#include "KAPVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"

AKAPFXManager::AKAPFXManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

AKAPFXManager* AKAPFXManager::Get(UWorld* W)
{
	if (!W) return nullptr;
	for (TActorIterator<AKAPFXManager> It(W); It; ++It) return *It;
	return W->SpawnActor<AKAPFXManager>();
}

void AKAPFXManager::Spawn(EKAPFX Type, FVector Pos, float Size, FVector Vel)
{
	if (Pool.Num() < 400)
	{
		FP P;
		P.C = NewObject<UStaticMeshComponent>(this);
		P.C->SetStaticMesh(KAPVis::Mesh(EKAPShape::Sphere));
		P.C->SetupAttachment(RootComponent);
		P.C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P.C->SetCastShadow(false);
		P.C->SetAbsolute(true, true, true);
		P.M = UMaterialInstanceDynamic::Create(KAPVis::TranslucentMaterial(), P.C);
		P.C->SetMaterial(0, P.M);
		P.C->RegisterComponent();
		Pool.Add(P);
		Next = Pool.Num() - 1;
	}
	else Next = (Next + 1) % Pool.Num();
	FP& P = Pool[Next];
	struct FDef { FLinearColor C; float Life, Size, Grow, Vz, Op, Spread; bool bEmit; };
	static const FDef Defs[] = {
		{ FLinearColor(0.07f, 0.06f, 0.05f), 5.f, 220, 180, 220, 0.6f, 60, false },
		{ FLinearColor(0.55f, 0.8f, 0.15f), 3.5f, 140, 110, 70, 0.4f, 80, true },
		{ FLinearColor(1.f, 0.45f, 0.08f), 0.9f, 150, -90, 380, 0.9f, 60, true },
		{ FLinearColor(0.9f, 0.9f, 0.9f), 2.2f, 70, 150, 160, 0.5f, 30, false },
		{ FLinearColor(0.6f, 0.47f, 0.3f), 1.6f, 90, 160, 40, 0.5f, 60, false },
		{ FLinearColor(0.4f, 0.01f, 0.01f), 0.5f, 25, 20, 100, 0.95f, 140, false },
		{ FLinearColor(0.4f, 1.f, 0.1f), 0.6f, 30, 20, 100, 0.9f, 140, true },
		{ FLinearColor(1.f, 0.85f, 0.4f), 0.07f, 35, 0, 0, 1.f, 0, true },
		{ FLinearColor(1.f, 0.15f, 0.08f), 0.45f, 45, -50, 60, 1.f, 20, true },
	};
	const FDef& D = Defs[(int32)Type];
	P.bAlive = true; P.T = 0; P.Life = D.Life * FMath::FRandRange(0.7f, 1.3f);
	P.Size = D.Size * Size; P.Grow = D.Grow * Size; P.Op = D.Op; P.Col = D.C;
	P.V = Vel + FVector(FMath::FRandRange(-D.Spread, D.Spread), FMath::FRandRange(-D.Spread, D.Spread), D.Vz * FMath::FRandRange(0.7f, 1.3f));
	P.C->SetWorldLocation(Pos);
	P.C->SetWorldScale3D(FVector(P.Size / 100.f));
	P.C->SetVisibility(true);
	P.M->SetVectorParameterValue("Color", D.C);
	P.M->SetVectorParameterValue("Emissive", D.bEmit ? D.C * 8.f : FLinearColor::Black);
	P.M->SetScalarParameterValue("Opacity", D.Op);
}

void AKAPFXManager::Tick(float Dt)
{
	Super::Tick(Dt);
	for (FP& P : Pool)
	{
		if (!P.bAlive) continue;
		P.T += Dt;
		if (P.T >= P.Life) { P.bAlive = false; P.C->SetVisibility(false); continue; }
		P.C->AddWorldOffset((P.V + Wind * 0.4f) * Dt);
		const float S = FMath::Max(5.f, P.Size + P.Grow * P.T);
		P.C->SetWorldScale3D(FVector(S / 100.f));
		P.M->SetScalarParameterValue("Opacity", P.Op * (1.f - P.T / P.Life));
	}
}
