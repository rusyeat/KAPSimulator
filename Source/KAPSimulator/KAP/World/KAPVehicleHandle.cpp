#include "KAPVehicleHandle.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPSimulatorPawn.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "WeatherManager.h"
#include "KAPVisuals.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"

AKAPVehicleHandle::AKAPVehicleHandle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // дөңгелек сүйектері осы кадрда жаңарған соң
	RootComponent = CreateDefaultSubobject<USceneComponent>("Root");
}

void AKAPVehicleHandle::BeginPlay() { Super::BeginPlay(); KAPReg::Add(KAPReg::Interactables(), (AActor*)this); }

float AKAPVehicleHandle::SpeedKmh() const { return Pawn ? Pawn->GetVelocity().Size() * 0.036f : 0.f; }

FVector AKAPVehicleHandle::GetInteractLocation() const { return Pawn ? Pawn->GetActorLocation() : GetActorLocation(); }

void AKAPVehicleHandle::EnsureLights()
{
	if (!Pawn || Heads.Num()) return;
	USkeletalMeshComponent* M = Pawn->GetMesh();
	if (!M || !M->GetSkeletalMeshAsset()) return;
	if (!bBodyBuilt) BuildBody();
	const FBoxSphereBounds B = M->GetSkeletalMeshAsset()->GetBounds();
	const FVector O = B.Origin, E = B.BoxExtent;
	for (int32 s = 0; s < 2; s++)
	{
		const float Sg = s == 0 ? -1.f : 1.f;
		const FVector Lp = bBodyBuilt ? HeadLocal[s] + FVector(6, 0, 0) : FVector(O.X + E.X * 0.96f, O.Y + Sg * E.Y * 0.62f, O.Z + E.Z * 0.05f);
		USpotLightComponent* L = NewObject<USpotLightComponent>(Pawn);
		L->SetupAttachment(M); L->SetRelativeLocation(Lp); L->SetRelativeRotation(FRotator(-6, 0, 0));
		L->SetIntensityUnits(ELightUnits::Candelas); L->SetIntensity(0.f);
		L->SetOuterConeAngle(40.f); L->SetInnerConeAngle(18.f);
		L->SetAttenuationRadius(6500.f);
		L->SetLightColor(FLinearColor(1.f, 0.9f, 0.72f)); // галоген, жылы
		L->SetVolumetricScatteringIntensity(1.0f);
		L->SetCastShadows(s == 0); // көлеңке тек бір фарадан (perf)
		L->bCastVolumetricShadow = s == 0;
		L->SetVisibility(false);
		L->RegisterComponent();
		Heads.Add(L);
		UStaticMeshComponent* Lens = HeadLens.IsValidIndex(s) ? HeadLens[s].Get() : nullptr;
		if (!Lens) { Lens = KAPVis::Add(Pawn, M, EKAPShape::Sphere, Lp - FVector(4, 0, 0), FVector(8, 22, 18), FKAPMat::Glow(FLinearColor(1.f, 0.92f, 0.75f), 0), FRotator::ZeroRotator, false); Lens->SetCastShadow(false); }
		Lenses.Add(Lens);
		UStaticMeshComponent* Tail = TailLens.IsValidIndex(s) ? TailLens[s].Get() : nullptr;
		if (!Tail) { Tail = KAPVis::Add(Pawn, M, EKAPShape::Cube, FVector(O.X - E.X * 0.98f, O.Y + Sg * E.Y * 0.7f, O.Z + E.Z * 0.15f), FVector(3, 14, 8), FKAPMat::Glow(FLinearColor(1.f, 0.05f, 0.02f), 0), FRotator::ZeroRotator, false); Tail->SetCastShadow(false); }
		Lenses.Add(Tail);
	}
	TailL = NewObject<UPointLightComponent>(Pawn);
	TailL->SetupAttachment(M); TailL->SetRelativeLocation(bBodyBuilt ? (TailLocal[0] + TailLocal[1]) * 0.5f - FVector(60, 0, 25) : FVector(O.X - E.X - 30, 0, O.Z + E.Z * 0.15f));
	TailL->SetIntensityUnits(ELightUnits::Candelas); TailL->SetIntensity(0.f);
	TailL->SetAttenuationRadius(280.f); TailL->SetLightColor(FLinearColor(1.f, 0.06f, 0.03f)); TailL->SetCastShadows(false);
	TailL->RegisterComponent();
}

void AKAPVehicleHandle::UpdateLights(float Dt)
{
	EnsureLights();
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const bool bDark = WX && (WX->Env.bNight || WX->Env.Storm > 0.3f || WX->Env.Fog > 0.4f);
	const bool bOn = (bDriven || bAutoPilot || bLightsForced) && bDark && State != "radiator" && Hp > 0;
	const float Want = bOn ? 1.f : 0.f;
	if (FMath::IsNearlyEqual(LightK, Want, 0.001f) && (LightK == 0.f || LightK == 1.f) && !(bOn && Rpm01 > 0)) return;
	LightK = FMath::FInterpTo(LightK, Want, Dt, 6.f);
	// генератор айналымы аз болса фара сәл әлсірейді (тірі көрінеді)
	const float Gen = 0.85f + 0.15f * FMath::Clamp(Rpm01 * 2.f, 0.f, 1.f);
	const float K = LightK * Gen;
	const bool bBrake = Pawn && Pawn->GetChaosVehicleMovement() && Pawn->GetChaosVehicleMovement()->GetBrakeInput() > 0.1f;
	for (USpotLightComponent* L : Heads) { L->SetVisibility(K > 0.01f); L->SetIntensity(5000.f * K); }
	for (int32 i = 0; i < Lenses.Num(); i++)
		if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Lenses[i]->GetMaterial(0)))
			MI->SetVectorParameterValue("Emissive", (i % 2 == 0) ? FLinearColor(1.f, 0.92f, 0.75f) * 60.f * K : FLinearColor(1.f, 0.05f, 0.02f) * (bBrake ? 40.f : 12.f) * FMath::Max(K, bBrake ? 1.f : 0.f));
	if (TailL) TailL->SetIntensity((bBrake ? 10.f : 2.5f) * FMath::Max(K, bBrake ? 1.f : 0.f));
}

void AKAPVehicleHandle::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Pawn) return;
	SetActorLocation(Pawn->GetActorLocation());
	UChaosWheeledVehicleMovementComponent* Mov = Cast<UChaosWheeledVehicleMovementComponent>(Pawn->GetChaosVehicleMovement());
	const bool bActive = bDriven || bAutoPilot;
	if (Mov && bActive) Rpm01 = FMath::Clamp(Mov->GetEngineRotationSpeed() / FMath::Max(1.f, Mov->GetEngineMaxRotationSpeed()), 0.f, 1.f);
	else Rpm01 = FMath::FInterpTo(Rpm01, 0.f, Dt, 2.f);
	UpdateLights(Dt);
	if (WheelPivots.Num()) UpdateVisualWheels();
	// декоративті дөңгелектер: v / R бұрыштық жылдамдықпен айналады
	if (FakeWheels.Num())
	{
		const float Fwd = FVector::DotProduct(Pawn->GetVelocity(), Pawn->GetActorForwardVector());
		FakeRoll = FMath::Fmod(FakeRoll - FMath::RadiansToDegrees(Fwd / FMath::Max(10.f, FakeR)) * Dt, 360.f);
		for (UStaticMeshComponent* W : FakeWheels) if (W) W->SetRelativeRotation(FRotator(FakeRoll, 0, 90));
	}
	if (!bActive) { Heat = FMath::FInterpTo(Heat, 40.f, Dt, 0.01f); return; }
	const AWeatherManager* WX = AWeatherManager::Get(GetWorld());
	const float Storm = WX ? WX->Env.Storm : 0.f;
	const float V = Pawn->GetVelocity().Size() / 100.f; // м/с
	// жел физикасы: бүйірлік итеру + қарсы кедергі (дауылда жылдамдық 20–35% төмендейді)
	if (Storm > 0.05f && Pawn->GetMesh()->IsSimulatingPhysics())
	{
		const FVector Wd = WX->Env.Wind.GetSafeNormal2D();
		const FVector Vel = Pawn->GetVelocity();
		const float Gust = 0.75f + 0.5f * FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * 0.6f + (GetTypeHash(Id) % 97) * 0.37f);
		Pawn->GetMesh()->AddForce(Wd * Storm * 160.f * Gust - Vel * Storm * 0.12f, NAME_None, true);
	}
	// қозғалтқыш қызуы: газ + ыстық + дауыл (шаң сүзгіні бітейді) − ауа ағынымен салқындау
	const float Thr = Mov ? FMath::Abs(Mov->GetThrottleInput()) : 0.f;
	const float Temp = WX ? WX->Env.Temp : 30.f;
	Heat += Dt * (6.f * Thr + 4.f * Storm + 0.1f * (Temp - 25.f) - (Heat - 60.f) * 0.25f - FMath::Min(V, 15.f) * 0.05f);
	Heat = FMath::Clamp(Heat, 20.f, 115.f);
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	HeatWarnCd -= Dt;
	if (bDriven && Heat > 98.f && HeatWarnCd <= 0 && GM) { HeatWarnCd = 25.f; GM->Toast(TEXT("Двигатель қызып кетті! Баяулаңыз — қуат азаяды."), "warn"); }
	if (bAutoPilot && Mov)
	{
		const FVector To = AutoTarget - Pawn->GetActorLocation();
		const float Dist = To.Size2D();
		const FVector Loc = Pawn->GetActorTransform().InverseTransformVectorNoScale(To);
		const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Loc.Y, Loc.X));
		Mov->SetRequiresControllerForInputs(false);
		Mov->SetSteeringInput(FMath::Clamp(Ang / 25.f, -1.f, 1.f));
		const float Kmh = SpeedKmh(), Want = Dist < 2500.f ? AutoMaxKmh * Dist / 2500.f : AutoMaxKmh;
		if (Dist < 400.f || Kmh > Want + 8) { Mov->SetThrottleInput(0.f); Mov->SetBrakeInput(Dist < 400.f ? 1.f : 0.4f); }
		else { Mov->SetBrakeInput(0.f); Mov->SetThrottleInput(Kmh < Want ? 0.75f : 0.2f); }
	}
	if (!bDriven) return;
	Fuel = FMath::Max(0.f, Fuel - V * Dt * 0.0035f * (Def.FuelCap / 55.f));
	if (Hp < 30 && Hp > 0 && V > 3 && FMath::FRand() < Dt * 0.01f) { State = "broken"; if (GM) GM->Toast(TEXT("Двигатель тоқтады! Жөндеу керек."), "danger"); }
	if (Heat > 112.f && FMath::FRand() < Dt * 0.05f) { State = "broken"; Hp = FMath::Min(Hp, 25.f); if (GM) GM->Toast(TEXT("Двигатель қайнап кетті! Жөндеу керек."), "danger"); }
	if (GM && GM->DistToRoad(FVector2D(Pawn->GetActorLocation()) / 100.0) > 6 && V > 14 && FMath::FRand() < Dt * 0.004f) { Flats = FMath::Min(4, Flats + 1); GM->Toast(TEXT("Дөңгелек жарылды!"), "warn"); }
	if (!CanDrive()) Pawn->DoThrottle(0.f);
}

FString AKAPVehicleHandle::GetInteractText(AKAPCharacter*) const
{
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const AKAPGameMode* GM = AKAPGameMode::Get(this);
	if (!GI || !GM) return FString();
	if (State == "locked") return Def.Title + TEXT(" — құлыпталған");
	if (State == "radiator") return GM->QuestState("m_fixuaz") == EQuestState::Active ? TEXT("Радиаторды ауыстыру (УАЗ капоты)") : Def.Title + TEXT(" — радиатор бұзылған");
	if (State == "parts") return FString::Printf(TEXT("%s — бөлшектер жетіспейді (%d/4)"), *Def.Title, GI->S.QuestData.FindRef("s_parts"));
	if (State == "broken") return TEXT("Двигательді жөндеу (") + Def.Title + TEXT(")");
	if (Flats && GI->S.Inv.FindRef("tire") > 0) return TEXT("Дөңгелекті ауыстыру");
	if (Fuel < Def.FuelCap * 0.5f && GI->S.Inv.FindRef("fuel") > 0) return TEXT("Жанармай құю (канистр)");
	return TEXT("Мінү: ") + Def.Title + (Fuel < 3 ? TEXT(" (жанармай жоқ!)") : TEXT(""));
}

void AKAPVehicleHandle::Interact(AKAPCharacter*)
{
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->VehicleInteract(this);
}

void AKAPVehicleHandle::UpdateVisualWheels()
{
	USkeletalMeshComponent* M = Pawn ? Pawn->GetMesh() : nullptr;
	if (!M) return;
	for (int32 i = 0; i < WheelPivots.Num(); i++)
	{
		if (!WheelPivots[i]) continue;
		const FTransform T = M->GetSocketTransform(WheelBones[i], RTS_Component);
		FVector L = T.GetLocation(); L.Y *= VisK; L.Z -= VisDrop;
		// бастапқы бұрышқа қатысты өзгеріс: айналу (домалау) + руль + аспаның қисаюы
		WheelPivots[i]->SetRelativeLocationAndRotation(L, T.GetRotation() * WheelRest[i].Inverse());
	}
}