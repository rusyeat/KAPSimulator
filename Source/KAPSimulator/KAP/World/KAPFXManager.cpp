#include "KAPFXManager.h"
#include "KAPVisuals.h"
#include "WeatherManager.h"
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
	if (Pool.Num() < 600)
	{
		FP P;
		P.C = NewObject<UStaticMeshComponent>(this);
		P.C->SetStaticMesh(KAPVis::Mesh(EKAPShape::Sphere));
		P.C->SetupAttachment(RootComponent);
		P.C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		P.C->SetCastShadow(false);
		P.C->SetAbsolute(true, true, true);
		P.M = UMaterialInstanceDynamic::Create(KAPVis::TranslucentMaterial(), P.C);
		static TWeakObjectPtr<UMaterialInterface> PuffBase;
		if (!PuffBase.IsValid()) PuffBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/KAP/Materials/M_KAP_Puff.M_KAP_Puff"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		P.MPuff = PuffBase.IsValid() ? UMaterialInstanceDynamic::Create(PuffBase.Get(), P.C) : P.M;
		Mids.Add(P.M); Mids.Add(P.MPuff);
		P.C->SetMaterial(0, P.M); P.Cur = P.M;
		P.C->RegisterComponent();
		Pool.Add(P);
		Next = Pool.Num() - 1;
	}
	else Next = (Next + 1) % Pool.Num();
	FP& P = Pool[Next];
	struct FDef { FLinearColor C; float Life, Size, Grow, Vz, Op, Spread; bool bEmit; float Grav = 0.f; };
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
		// лай (дөңгелектен ұшады, түседі), су шашырауы, ағаш жаңқасы, жаңбыр тамшысының шашырауы
		{ FLinearColor(0.12f, 0.085f, 0.055f), 1.1f, 9, 2, 380, 0.97f, 160, false, 980.f },
		{ FLinearColor(0.7f, 0.74f, 0.75f), 0.8f, 10, 14, 420, 0.45f, 150, false, 980.f },
		{ FLinearColor(0.5f, 0.42f, 0.3f), 1.2f, 4, 0, 300, 1.f, 200, false, 980.f },
		{ FLinearColor(0.75f, 0.78f, 0.8f), 0.25f, 3, 8, 120, 0.35f, 40, false, 980.f },
		// радиатор буы: кішкентай жұмсақ бұлттар, тез ұлғайып жоғары көтеріледі, ақ (күнде сұр болып көрінбесін)
		{ FLinearColor(1.3f, 1.3f, 1.32f), 2.1f, 22, 62, 85, 0.28f, 18, false },
	};
	const FDef& D = Defs[(int32)Type];
	P.bAlive = true; P.T = 0; P.Life = D.Life * FMath::FRandRange(0.7f, 1.3f);
	P.Size = D.Size * Size; P.Grow = D.Grow * Size; P.Op = D.Op; P.Col = D.C; P.Grav = D.Grav;
	P.Floor = D.Grav > 0.f ? KAPVis::GroundZ(GetWorld(), Pos.X, Pos.Y, Pos.Z - 300.f) + 2.f : -1e9f;
	P.V = Vel + FVector(FMath::FRandRange(-D.Spread, D.Spread), FMath::FRandRange(-D.Spread, D.Spread), D.Vz * FMath::FRandRange(0.7f, 1.3f));
	P.C->SetWorldLocation(Pos);
	P.C->SetWorldScale3D(FVector(P.Size / 100.f));
	P.C->SetVisibility(true);
	// газ (түтін, улы бу, бу, шаң) — үлпек материал; қалғаны (от, қан, жарқыл, лай, шашырау) — қатты шар
	const bool bGas = Type == EKAPFX::Smoke || Type == EKAPFX::Toxic || Type == EKAPFX::Steam || Type == EKAPFX::Dust || Type == EKAPFX::Vapor;
	UMaterialInstanceDynamic* Want = bGas ? P.MPuff : P.M;
	if (P.Cur != Want) { P.C->SetMaterial(0, Want); P.Cur = Want; }
	if (bGas && P.Cur != P.M) { const AWeatherManager* WX = AWeatherManager::Get(GetWorld()); P.Cur->SetScalarParameterValue("Ambient", WX && WX->Env.bNight ? 0.04f : 0.6f); } // күндіз көлеңкедегі бу да ақшыл; түнде жарқырамасын
	P.Cur->SetVectorParameterValue("Color", D.C);
	P.Cur->SetVectorParameterValue("Emissive", D.bEmit ? D.C * 8.f : FLinearColor::Black);
	P.Cur->SetScalarParameterValue("Opacity", D.Op * (bGas && P.Cur != P.M ? 1.6f : 1.f)); // үлпек материал мөлдірлеу — тығыздығын сәл көтереміз
}

void AKAPFXManager::Tick(float Dt)
{
	Super::Tick(Dt);
	for (FP& P : Pool)
	{
		if (!P.bAlive) continue;
		P.T += Dt;
		if (P.T >= P.Life) { P.bAlive = false; P.C->SetVisibility(false); continue; }
		P.V.Z -= P.Grav * Dt;
		if (P.C->GetComponentLocation().Z < P.Floor && P.V.Z < 0.f) { P.V = FVector::ZeroVector; P.Grav = 0.f; P.Grow = -P.Size * 0.8f; } // жерге түсті
		P.C->AddWorldOffset((P.V + Wind * (P.Grav > 0.f ? 0.1f : 0.4f)) * Dt);
		const float S = FMath::Max(5.f, P.Size + P.Grow * P.T);
		P.C->SetWorldScale3D(FVector(S / 100.f));
		P.Cur->SetScalarParameterValue("Opacity", P.Op * (P.Cur != P.M ? 1.6f : 1.f) * (1.f - P.T / P.Life));
	}
}
