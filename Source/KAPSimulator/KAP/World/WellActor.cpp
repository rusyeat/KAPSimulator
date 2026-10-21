#include "WellActor.h"
#include "KAP/Core/KAPRegistry.h"
#include "KAPVisuals.h"
#include "KAP/Audio/KAPAudio.h"
#include "Components/AudioComponent.h"
#include "KAPFXManager.h"
#include "WellManagerSubsystem.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

AWellActor::AWellActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Root = CreateDefaultSubobject<USceneComponent>("Root");
	RootComponent = Root;
}

void AWellActor::Build(int32 InIndex, const FKAPWellRow& Row)
{
	Index = InIndex;
	KAPReg::Add(KAPReg::Interactables(), (AActor*)this);
	using namespace KAPVis;
	const FKAPMat Concrete = FKAPMat::Concrete(FLinearColor(0.44f, 0.42f, 0.39f));
	const FKAPMat Steel = FKAPMat::RustySteel(FLinearColor(0.35f, 0.38f, 0.4f), 0.4f);
	const FKAPMat Red = FKAPMat::Paint(FLinearColor(0.55f, 0.06f, 0.04f), 0.5f);
	const FKAPMat Blue = FKAPMat::Sheet(FLinearColor(0.12f, 0.24f, 0.42f), 0.3f, 0.f);
	const FKAPMat Teal = FKAPMat::Sheet(FLinearColor(0.12f, 0.32f, 0.3f), 0.35f, 0.f);
	// бетон алаң
	Add(this, Root, EKAPShape::Cube, FVector(0, 0, 15), FVector(1000, 1000, 30), Concrete);
	// фонтан арматурасы (ұңғыма сағасы)
	Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 40), FVector(100, 100, 20), Steel);
	Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 130), FVector(64, 64, 180), Steel);
	Add(this, Root, EKAPShape::Cube, FVector(0, 0, 150), FVector(160, 22, 22), Steel);
	Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 235), FVector(80, 80, 8), Red, FRotator::ZeroRotator);
	Valves.Add(Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 235), FVector(60, 60, 10), Red));
	Valves.Add(Add(this, Root, EKAPShape::Cylinder, FVector(90, 0, 150), FVector(44, 44, 8), Red, FRotator(90, 0, 0)));
	// құбыр коллекторға
	Add(this, Root, EKAPShape::Cylinder, FVector(320, 0, 55), FVector(26, 26, 560), Steel, FRotator(90, 0, 0));
	Add(this, Root, EKAPShape::Cylinder, FVector(600, 0, 60), FVector(26, 26, 90), Steel);
	// басқару шкафы
	Add(this, Root, EKAPShape::Cube, FVector(-260, 180, 115), FVector(120, 70, 170), Blue);
	Add(this, Root, EKAPShape::Cube, FVector(-260, 216, 140), FVector(90, 4, 60), FKAPMat::Paint(FLinearColor(0.02f, 0.02f, 0.02f), 0.2f), FRotator::ZeroRotator, false);
	Lamp = Add(this, Root, EKAPShape::Sphere, FVector(-260, 205, 212), FVector(18, 18, 18), FKAPMat::Glow(FLinearColor(0, 1, 0.3f), 30), FRotator::ZeroRotator, false);
	LampLight = NewObject<UPointLightComponent>(this);
	LampLight->SetupAttachment(Root); LampLight->SetRelativeLocation(FVector(-260, 230, 212));
	LampLight->SetIntensity(800); LampLight->SetAttenuationRadius(450); LampLight->SetCastShadows(false);
	LampLight->RegisterComponent();
	PanelLocal = FVector(-260, 260, 120);
	// сорғы
	Add(this, Root, EKAPShape::Cube, FVector(180, -240, 75), FVector(120, 160, 90), Teal);
	Add(this, Root, EKAPShape::Cylinder, FVector(180, -240, 140), FVector(60, 60, 120), Teal);
	// қышқыл резервуары
	if (Row.AcidTank)
	{
		Add(this, Root, EKAPShape::Cylinder, FVector(-250, -280, 150), FVector(240, 240, 340), FKAPMat::Sheet(FLinearColor(0.75f, 0.73f, 0.66f), 0.45f, 0.f), FRotator(0, 0, 90));
		Add(this, Root, EKAPShape::Cube, FVector(-360, -280, 60), FVector(30, 90, 90), Steel);
		Add(this, Root, EKAPShape::Cube, FVector(-140, -280, 60), FVector(30, 90, 90), Steel);
		Add(this, Root, EKAPShape::Cube, FVector(-250, -158, 150), FVector(50, 2, 50), FKAPMat::Paint(FLinearColor(0.9f, 0.7f, 0.f), 0.5f), FRotator::ZeroRotator, false);
	}
	// қоршау
	for (int32 X = -400; X <= 400; X += 200)
	{
		Add(this, Root, EKAPShape::Cylinder, FVector(X, 480, 85), FVector(10, 10, 110), FKAPMat::Hazard());
		Add(this, Root, EKAPShape::Cylinder, FVector(X, -480, 85), FVector(10, 10, 110), FKAPMat::Hazard());
	}
	// қоршау белдеулері, алаң шетіндегі сары жолақ, фланецтер, манометр, кабель науасы, радиация белгісі
	for (float Y : { 480.f, -480.f }) for (float Z : { 70.f, 125.f }) Add(this, Root, EKAPShape::Cylinder, FVector(0, Y, Z), FVector(4, 4, 800), Steel, FRotator(90, 0, 0), false);
	for (float S : { -1.f, 1.f }) { Add(this, Root, EKAPShape::Cube, FVector(0, S * 492, 31), FVector(1000, 16, 3), FKAPMat::Hazard(), FRotator::ZeroRotator, false); Add(this, Root, EKAPShape::Cube, FVector(S * 492, 0, 31), FVector(16, 1000, 3), FKAPMat::Hazard(), FRotator::ZeroRotator, false); }
	for (float X : { 120.f, 330.f, 540.f }) Add(this, Root, EKAPShape::Cylinder, FVector(X, 0, 55), FVector(40, 40, 8), Steel, FRotator(90, 0, 0), false);
	for (float X : { 150.f, 420.f }) Add(this, Root, EKAPShape::Cube, FVector(X, 0, 30), FVector(20, 60, 30), Concrete);
	Add(this, Root, EKAPShape::Cylinder, FVector(0, 40, 175), FVector(4, 4, 40), Steel, FRotator(90, 90, 0), false);
	Add(this, Root, EKAPShape::Cylinder, FVector(0, 62, 175), FVector(26, 26, 6), FKAPMat::Paint(FLinearColor(0.9f, 0.9f, 0.86f), 0.3f), FRotator(90, 90, 0), false);
	Add(this, Root, EKAPShape::Cube, FVector(-130, 90, 34), FVector(260, 24, 8), Steel, FRotator(0, -35, 0), false);
	{
		FKAPMat SignM = FKAPMat::Paint(FLinearColor(0.85f, 0.65f, 0.02f), 0.55f); SignM.Grime = 0.7f; SignM.Rust = 0.25f;
		Add(this, Root, EKAPShape::Cube, FVector(420, 486, 150), FVector(90, 3, 64), SignM, FRotator::ZeroRotator, false);
		UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
		T->SetupAttachment(Root); T->SetRelativeLocation(FVector(420, 489, 150)); T->SetRelativeRotation(FRotator(0, 90, 0));
		KAPVis::SignText(T); T->SetText(FText::FromString(TEXT("РАДИАЦИЯ!\nҚАУІПТІ АЙМАҚ"))); T->SetHorizontalAlignment(EHTA_Center); T->SetVerticalAlignment(EVRTA_TextCenter);
		T->SetWorldSize(11); T->SetTextRenderColor(FColor(15, 12, 8)); T->RegisterComponent();
	}
	// «әлемдік әңгіме»: тасталған қорғаныш костюмі мен газтұтқыш, өлген құстар (мутацияның алғашқы белгісі)
	{
		FRandomStream Rg(InIndex * 131 + 7);
		if (Rg.FRand() < 0.6f)
		{
			FKAPMat Suit = FKAPMat::Paint(FLinearColor(0.72f, 0.36f, 0.05f), 0.95f); Suit.Ribs = 0.4f; Suit.RibPeriod = 3.5f; Suit.Grime = 0.95f; Suit.DustTop = 1.f;
			const FVector C(Rg.FRandRange(-650.f, -560.f), Rg.FRandRange(-300.f, 300.f), 36.f);
			const float Yw = Rg.FRandRange(0.f, 360.f);
			const FRotator Ry(0, Yw, 0);
			auto At = [&](FVector L) { return C + Ry.RotateVector(L); };
			Add(this, Root, EKAPShape::Cube, At(FVector(0, 0, 0)), FVector(60, 42, 9), Suit, FRotator(2, Yw, 3), false);                        // кеудесі
			for (float S : { -1.f, 1.f })
			{
				Add(this, Root, EKAPShape::Cube, At(FVector(-62, S * 12, -1)), FVector(70, 16, 8), Suit, FRotator(0, Yw + S * 8.f, 0), false); // балақтар
				Add(this, Root, EKAPShape::Cube, At(FVector(10, S * 40, -1)), FVector(16, 50, 7), Suit, FRotator(0, Yw - S * 30.f, 0), false); // жеңдер
			}
			Add(this, Root, EKAPShape::Sphere, At(FVector(42, 4, 2)), FVector(26, 24, 10), Suit, FRotator(0, Yw, 0), false);                 // капюшон
			Add(this, Root, EKAPShape::Cube, At(FVector(70, -30, 2)), FVector(12, 10, 8), FKAPMat::Paint(FLinearColor(0.04f, 0.04f, 0.04f), 0.7f), FRotator(0, Yw + 40.f, 70.f), false); // газтұтқыш
			Add(this, Root, EKAPShape::Cylinder, At(FVector(72, -22, 2)), FVector(8, 8, 6), FKAPMat::Paint(FLinearColor(0.7f, 0.58f, 0.1f), 0.5f), FRotator(0, Yw, 90), false);
		}
		const int32 Birds = Rg.RandRange(2, 6);
		const FKAPMat Feather = FKAPMat::Paint(FLinearColor(0.06f, 0.055f, 0.05f), 0.9f);
		for (int32 b = 0; b < Birds; b++)
		{
			const FVector P(Rg.FRandRange(-900.f, 900.f), Rg.FRandRange(-900.f, 900.f), 34.f);
			if (FMath::Abs(P.X) < 520 && FMath::Abs(P.Y) < 520) continue;
			const float Yb = Rg.FRandRange(0.f, 360.f);
			Add(this, Root, EKAPShape::Sphere, P, FVector(18, 9, 7), Feather, FRotator(0, Yb, 80), false);
			Add(this, Root, EKAPShape::Cube, P + FRotator(0, Yb + 90, 0).Vector() * 10.f, FVector(10, 18, 1.2f), Feather, FRotator(5, Yb + 20, 0), false);
			Add(this, Root, EKAPShape::Cube, P - FRotator(0, Yb + 90, 0).Vector() * 9.f, FVector(10, 16, 1.2f), Feather, FRotator(-8, Yb - 15, 0), false);
		}
	}
	// ағу шалшығы, күйген дақ
	FKAPMat Tox = FKAPMat::Glow(FLinearColor(0.45f, 0.8f, 0.1f), 2.f); Tox.Roughness = 0.05f;
	Puddle = Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 32), FVector(800, 800, 2), Tox, FRotator::ZeroRotator, false);
	Scorch = Add(this, Root, EKAPShape::Cylinder, FVector(0, 0, 31), FVector(1400, 1400, 2), FKAPMat::Paint(FLinearColor(0.02f, 0.015f, 0.01f), 1.f), FRotator::ZeroRotator, false);
	// өрт
	FireGroup = NewObject<USceneComponent>(this); FireGroup->SetupAttachment(Root); FireGroup->RegisterComponent();
	for (int32 i = 0; i < 7; i++)
	{
		const float H = FMath::FRandRange(250.f, 520.f);
		Flames.Add(Add(this, FireGroup, EKAPShape::Cone, FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 40 + H / 2), FVector(FMath::FRandRange(80.f, 160.f), FMath::FRandRange(80.f, 160.f), H),
			FKAPMat::Glow(i % 2 ? FLinearColor(1, 0.35f, 0.02f) : FLinearColor(1, 0.6f, 0.08f), 25), FRotator::ZeroRotator, false));
	}
	FireLight = NewObject<UPointLightComponent>(this);
	FireLight->SetupAttachment(Root); FireLight->SetRelativeLocation(FVector(0, 0, 300));
	FireLight->SetIntensity(80000); FireLight->SetAttenuationRadius(4000); FireLight->SetLightColor(FLinearColor(1, 0.5f, 0.15f));
	FireLight->RegisterComponent();
	// нөмір
	Sign = NewObject<UTextRenderComponent>(this);
	Sign->SetupAttachment(Root); Sign->SetRelativeLocation(FVector(3, 0, 520));
	Sign->SetText(FText::FromString(FString::Printf(TEXT("No.%d"), Row.Id)));
	Sign->SetHorizontalAlignment(EHTA_Center); Sign->SetWorldSize(90); Sign->SetTextRenderColor(FColor(242, 193, 78));
	Sign->RegisterComponent();
	if (UKAPAudio* Au = UKAPAudio::Get(this)) Hum = Au->AttachHum(Root, 55.f + Row.Id % 4 * 4.f);
	SetVisualState(EWellState::Ok, EFaultType::None);
}

void AWellActor::SetVisualState(EWellState State, EFaultType Fault)
{
	VisState = State; VisFault = Fault;
	if (Hum) Hum->SetVolumeMultiplier(State == EWellState::Ok ? 1.f : State == EWellState::Fault && (Fault == EFaultType::Pump || Fault == EFaultType::Electric) ? 0.f : 0.5f);
	const FLinearColor C = State == EWellState::Ok ? FLinearColor(0, 1, 0.3f) : State == EWellState::Fault ? FLinearColor(1, 0.8f, 0) :
		State == EWellState::Leak ? FLinearColor(1, 0.35f, 0) : State == EWellState::Fire ? FLinearColor(1, 0, 0) : FLinearColor(0.05f, 0.05f, 0.05f);
	if (Lamp) KAPVis::Apply(Cast<UMaterialInstanceDynamic>(Lamp->GetMaterial(0)), FKAPMat::Glow(C, State == EWellState::Lost ? 0 : 30));
	if (LampLight) { LampLight->SetLightColor(C); LampLight->SetVisibility(State != EWellState::Lost); }
	const bool bFire = State == EWellState::Fire || (State == EWellState::Fault && Fault == EFaultType::Fire);
	if (FireGroup) { FireGroup->SetVisibility(bFire, true); FireGroup->SetRelativeScale3D(FVector(State == EWellState::Fire ? 1.6f : 0.6f)); }
	if (FireLight) FireLight->SetVisibility(bFire);
	if (Puddle) Puddle->SetVisibility(State == EWellState::Leak || (State == EWellState::Fault && Fault == EFaultType::Acid));
	if (Scorch) Scorch->SetVisibility(State == EWellState::Fire || State == EWellState::Lost);
	if (Sign) Sign->SetTextRenderColor(State == EWellState::Ok ? FColor::White : State == EWellState::Lost ? FColor(90, 90, 90) : FColor(255, 90, 90));
	for (UStaticMeshComponent* V : Valves) KAPVis::Apply(KAPVis::UniqueMID(V), FKAPMat::Paint(State == EWellState::Lost ? FLinearColor(0.03f, 0.03f, 0.03f) : FLinearColor(0.55f, 0.06f, 0.04f)));
}

void AWellActor::Tick(float Dt)
{
	Super::Tick(Dt);
	AKAPFXManager* FX = AKAPFXManager::Get(GetWorld());
	const APlayerCameraManager* Cam = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->PlayerCameraManager : nullptr;
	if (Hum && Cam) { const bool bNear = FVector::Dist2D(Cam->GetCameraLocation(), GetActorLocation()) < 5000.f; if (bNear && !Hum->IsPlaying()) Hum->Play(); else if (!bNear && Hum->IsPlaying()) Hum->Stop(); }
	if (!FX || !Cam || FVector::Dist2D(Cam->GetCameraLocation(), GetActorLocation()) > 35000.f) return;
	const FVector P = GetActorLocation();
	if (Sign) Sign->SetWorldRotation(FRotator(0, (Cam->GetCameraLocation() - Sign->GetComponentLocation()).Rotation().Yaw, 0));
	const bool bFire = FireGroup && FireGroup->IsVisible();
	if (bFire)
	{
		const float T = GetWorld()->GetTimeSeconds();
		for (int32 i = 0; i < Flames.Num(); i++)
		{
			FVector S = Flames[i]->GetRelativeScale3D();
			S.Z = (2.5f + (i % 4) * 0.7f) * (0.85f + 0.2f * FMath::Sin(T * 9 + i * 1.7f));
			Flames[i]->SetRelativeScale3D(S);
		}
		FireLight->SetIntensity(60000 + FMath::FRand() * 40000);
		if (FMath::FRand() < Dt * (VisState == EWellState::Fire ? 18 : 6)) FX->Spawn(EKAPFX::Smoke, P + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), 450), VisState == EWellState::Fire ? 2.5f : 1.2f);
		if (FMath::FRand() < Dt * 20) FX->Spawn(EKAPFX::Fire, P + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 150), VisState == EWellState::Fire ? 1.6f : 0.8f);
	}
	if (VisState == EWellState::Leak || (VisState == EWellState::Fault && VisFault == EFaultType::Acid))
		if (FMath::FRand() < Dt * 6) FX->Spawn(EKAPFX::Toxic, P + FVector(FMath::FRandRange(-350.f, 350.f), FMath::FRandRange(-350.f, 350.f), 60));
	if (VisState == EWellState::Fault && VisFault == EFaultType::Pipe && FMath::FRand() < Dt * 10) FX->Spawn(EKAPFX::Steam, P + FVector(300, 0, 80), 1.f, FVector(0, 300, 0));
	if (VisState == EWellState::Fault && VisFault == EFaultType::Electric && FMath::FRand() < Dt * 2) FX->Spawn(EKAPFX::Flash, P + FVector(-260, 220, 180), 2.f);
	if (VisState == EWellState::Lost && FMath::FRand() < Dt * 2) FX->Spawn(EKAPFX::Smoke, P + FVector(0, 0, 100), 0.8f);
}

FString AWellActor::GetInteractText(AKAPCharacter*) const
{
	const UKAPGameInstance* GI = UKAPGameInstance::Get(this);
	const UWellManagerSubsystem* WM = GetWorld()->GetSubsystem<UWellManagerSubsystem>();
	if (!GI || !WM || !GI->S.Wells.IsValidIndex(Index)) return FString();
	const FKAPWellSave& W = GI->S.Wells[Index];
	switch (W.State)
	{
	case EWellState::Ok: return FString::Printf(TEXT("№%d ұңғыма — жұмыс істейді (%d%% қауіп)"), Index + 1, FMath::RoundToInt32(WM->Wells[Index].FailProb * 100));
	case EWellState::Fire: return FString::Printf(TEXT("№%d ұңғыма ЖАНЫП ЖАТЫР — жөндеу мүмкін емес"), Index + 1);
	case EWellState::Lost: return FString::Printf(TEXT("№%d ұңғыма жоғалған"), Index + 1);
	default: return FString::Printf(TEXT("Жөндеу: %s (№%d)"), *KAP::FaultName(W.Fault).ToString(), Index + 1);
	}
}

void AWellActor::Interact(AKAPCharacter* By)
{
	if (AKAPGameMode* GM = AKAPGameMode::Get(this)) GM->StartWellRepair(Index);
}

FVector AWellActor::GetInteractLocation() const { return GetActorTransform().TransformPosition(PanelLocal); }
