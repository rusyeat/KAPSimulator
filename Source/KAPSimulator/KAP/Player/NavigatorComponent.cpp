#include "NavigatorComponent.h"
#include "KAP/Core/KAPDataSubsystem.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/World/KAPVisuals.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UNavigatorComponent::UNavigatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.5f;
}

KAPAI::FGrid& UNavigatorComponent::Grid(UWorld* World)
{
	static KAPAI::FGrid G;
	static TWeakObjectPtr<UWorld> Built;
	if (Built.Get() != World)
	{
		Built = World;
		G.Init(1200.0, 60, [World](double X, double Y) { return KAPVis::GroundZ(World, X * 100.0, Y * 100.0, 0.f) / 100.0; });
	}
	return G;
}

void UNavigatorComponent::SetTarget(FVector2D TargetM, const FString& Name)
{
	bHasTarget = true; Target = TargetM; TargetName = Name;
	Recompute();
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Toast(FString::Printf(TEXT("Навигатор: %s"), *Name), "info");
}

void UNavigatorComponent::Clear() { bHasTarget = false; bHasNext = false; for (KAPAI::FRoute& R : Routes) R = KAPAI::FRoute(); }

const KAPAI::FRoute* UNavigatorComponent::Active() const
{
	const KAPAI::FRoute& R = Routes[(int32)Mode];
	return (bHasTarget && R.bValid) ? &R : nullptr;
}

void UNavigatorComponent::Recompute()
{
	if (!bHasTarget) return;
	UKAPDataSubsystem* D = UKAPDataSubsystem::Get(this);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!D || !GM) return;
	GM->RefreshDangerZones();
	const KAPAI::FDangerFn Danger = [GM](double X, double Y) { return GM->DangerAt(X, Y); };
	const FVector2D P = FVector2D(GetOwner()->GetActorLocation()) / 100.0;
	Routes[0] = D->Graph.Route(P, Target, false, Danger);
	Routes[1] = D->Graph.Route(P, Target, true, Danger);
	Routes[2] = Grid(GetWorld()).Route(P, Target, Danger, 1.5);
	LastCalc = GetWorld()->GetTimeSeconds();
}

void UNavigatorComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(Dt, TickType, ThisTickFunction);
	if (!bHasTarget) { bHasNext = false; return; }
	const FVector2D P = FVector2D(GetOwner()->GetActorLocation()) / 100.0;
	if (FVector2D::Distance(P, Target) < 14.0)
	{
		if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->Toast(FString::Printf(TEXT("Нысанаға жеттіңіз: %s"), *TargetName), "ok");
		Clear();
		return;
	}
	const KAPAI::FRoute* R = Active();
	if (!R) return;
	int32 Bi = 0; double Bd = TNumericLimits<double>::Max();
	for (int32 i = 0; i < R->Points.Num(); i++) { const double D = FVector2D::Distance(P, R->Points[i]); if (D < Bd) { Bd = D; Bi = i; } }
	int32 Ni = Bi;
	while (Ni < R->Points.Num() - 1 && FVector2D::Distance(P, R->Points[Ni]) < 20.0) Ni++;
	Ni = FMath::Clamp(FMath::Max(Ni, Bi + (Bd < 20 ? 1 : 0)), 0, R->Points.Num() - 1);
	Next = R->Points[Ni]; bHasNext = true;
	if (Bd > 70 && GetWorld()->GetTimeSeconds() - LastCalc > 4.0) Recompute();
}
