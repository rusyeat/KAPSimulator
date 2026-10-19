#include "KAPPlayerController.h"
#include "KAPCharacter.h"
#include "KAP/Core/KAPGameMode.h"
#include "KAP/Core/KAPGameInstance.h"
#include "KAP/UI/KAPHUD.h"
#include "KAP/World/KAPVehicleHandle.h"
#include "KAPSimulatorPawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

AKAPPlayerController::AKAPPlayerController() { bShowMouseCursor = false; }

UInputAction* AKAPPlayerController::MakeAction(FName Name, bool bAxis2D, bool bAxis1D)
{
	UInputAction* A = NewObject<UInputAction>(this, Name);
	A->ValueType = bAxis2D ? EInputActionValueType::Axis2D : bAxis1D ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
	Actions.Add(Name, A);
	return A;
}

void AKAPPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		Sub->AddMappingContext(IMC, 100);
}

void AKAPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	IMC = NewObject<UInputMappingContext>(this, "IMC_KAP");
	UInputAction* MoveA = MakeAction("Move", true);
	UInputAction* LookA = MakeAction("Look", true);
	UInputAction* WheelA = MakeAction("Wheel", false, true);
	UInputAction* FireA = MakeAction("Fire");
	// WASD → 2D вектор
	{
		FEnhancedActionKeyMapping& W = IMC->MapKey(MoveA, EKeys::W);
		UInputModifierSwizzleAxis* Sw = NewObject<UInputModifierSwizzleAxis>(this); Sw->Order = EInputAxisSwizzle::YXZ; W.Modifiers.Add(Sw);
		FEnhancedActionKeyMapping& S = IMC->MapKey(MoveA, EKeys::S);
		UInputModifierSwizzleAxis* Sw2 = NewObject<UInputModifierSwizzleAxis>(this); Sw2->Order = EInputAxisSwizzle::YXZ; S.Modifiers.Add(Sw2);
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		FEnhancedActionKeyMapping& A = IMC->MapKey(MoveA, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		IMC->MapKey(MoveA, EKeys::D);
	}
	IMC->MapKey(LookA, EKeys::Mouse2D);
	IMC->MapKey(WheelA, EKeys::MouseWheelAxis);
	IMC->MapKey(FireA, EKeys::LeftMouseButton);
	struct FK { FName N; FKey K; };
	const TArray<FK> Keys = {
		{ "Aim", EKeys::RightMouseButton }, { "Jump", EKeys::SpaceBar }, { "Sprint", EKeys::LeftShift }, { "Interact", EKeys::E }, { "Tablet", EKeys::Tab },
		{ "Pause", EKeys::Escape }, { "Reload", EKeys::R }, { "Flash", EKeys::F }, { "Med", EKeys::H }, { "Drink", EKeys::J }, { "Eat", EKeys::K },
		{ "Cam", EKeys::V }, { "Time", EKeys::F9 }, { "Enter", EKeys::Enter },
		{ "S1", EKeys::One }, { "S2", EKeys::Two }, { "S3", EKeys::Three }, { "S4", EKeys::Four }, { "S5", EKeys::Five }, { "S6", EKeys::Six }, { "S7", EKeys::Seven }, { "S8", EKeys::Eight }, { "S9", EKeys::Nine } };
	UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EI) return;
	for (const FK& K : Keys)
	{
		UInputAction* A = MakeAction(K.N);
		IMC->MapKey(A, K.K);
		const FName N = K.N;
		EI->BindActionValueLambda(A, ETriggerEvent::Started, [this, N](const FInputActionValue&) { OnKey(N); });
		EI->BindActionValueLambda(A, ETriggerEvent::Completed, [this, N](const FInputActionValue&) { OnKeyRelease(N); });
	}
	EI->BindAction(MoveA, ETriggerEvent::Triggered, this, &AKAPPlayerController::OnMove);
	EI->BindAction(MoveA, ETriggerEvent::Completed, this, &AKAPPlayerController::OnMoveEnd);
	EI->BindAction(LookA, ETriggerEvent::Triggered, this, &AKAPPlayerController::OnLook);
	EI->BindAction(FireA, ETriggerEvent::Started, this, &AKAPPlayerController::OnFire);
	EI->BindAction(FireA, ETriggerEvent::Completed, this, &AKAPPlayerController::OnFireEnd);
	EI->BindActionValueLambda(WheelA, ETriggerEvent::Triggered, [this](const FInputActionValue& V)
	{
		if (UIModal()) { if (AKAPHUD* H = AKAPHUD::Get(this)) H->Scroll(V.Get<float>()); return; }
		if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->CycleSlot(V.Get<float>() > 0 ? -1 : 1);
	});
}

bool AKAPPlayerController::UIModal() const
{
	const AKAPHUD* H = AKAPHUD::Get(this);
	return H && H->IsModal();
}

void AKAPPlayerController::PlayerTick(float Dt)
{
	Super::PlayerTick(Dt);
	const bool bModal = UIModal();
	if (bModal != bShowMouseCursor)
	{
		bShowMouseCursor = bModal;
		if (bModal) { FInputModeGameAndUI M; M.SetHideCursorDuringCapture(false); M.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways); SetInputMode(M); }
		else SetInputMode(FInputModeGameOnly());
	}
	if (AKAPCharacter* C = AKAPCharacter::Get(this)) C->bFrozen = bModal || (AKAPGameMode::Get(this) && AKAPGameMode::Get(this)->bCinematic);
	// көлік: W/S — газ/тежеу, A/D — руль
	if (!AutoMove.IsZero()) Move = AutoMove;
	if (AKAPSimulatorPawn* V = Cast<AKAPSimulatorPawn>(GetPawn()))
	{
		AKAPGameMode* GM = AKAPGameMode::Get(this);
		AKAPVehicleHandle* H = GM ? GM->CurrentVehicle.Get() : nullptr;
		const bool bCan = H && H->CanDrive() && !bModal;
		const float Flat = H ? (1.f - 0.25f * H->Flats) * (H->Heat > 100.f ? 0.5f : 1.f) : 1.f;
		// DoBrake газды нөлдейді, сондықтан тек біреуін шақырамыз
		if (Move.Y < -0.1f) V->DoBrake(-Move.Y);
		else V->DoThrottle(bCan ? FMath::Max(0.f, Move.Y) * Flat : 0.f);
		V->DoSteering(Move.X);
	}
}

void AKAPPlayerController::OnMove(const FInputActionValue& V)
{
	Move = V.Get<FVector2D>();
	if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->MoveInput(UIModal() ? FVector2D::ZeroVector : Move);
}
void AKAPPlayerController::OnMoveEnd(const FInputActionValue& V)
{
	Move = FVector2D::ZeroVector;
	if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->MoveInput(FVector2D::ZeroVector);
}
void AKAPPlayerController::OnLook(const FInputActionValue& V)
{
	if (UIModal()) return;
	const FVector2D L = V.Get<FVector2D>();
	if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->LookInput(L);
	else if (AKAPSimulatorPawn* P = Cast<AKAPSimulatorPawn>(GetPawn())) P->DoLookAround(L.X);
}
void AKAPPlayerController::OnFire(const FInputActionValue&)
{
	if (UIModal()) { float X, Y; if (GetMousePosition(X, Y)) if (AKAPHUD* H = AKAPHUD::Get(this)) H->Click(FVector2D(X, Y), true); return; }
	if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->FireStart();
}
void AKAPPlayerController::OnFireEnd(const FInputActionValue&)
{
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->Click(FVector2D::ZeroVector, false);
	if (AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn())) C->FireStop();
}

void AKAPPlayerController::OnKey(FName K)
{
	AKAPGameMode* GM = AKAPGameMode::Get(this);
	AKAPHUD* H = AKAPHUD::Get(this);
	AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn());
	AKAPSimulatorPawn* V = Cast<AKAPSimulatorPawn>(GetPawn());
	if (!GM || !H) return;
	if (H->HandleKey(K, true)) return; // мәзір, диалог, мини-ойын, планшет
	if (GM->bCinematic) { if (K == "Enter" || K == "Jump" || K == "Pause") GM->SkipCinematic(); return; }
	if (K == "Pause") { GM->bPaused = !GM->bPaused; return; }
	if (K == "Tablet")
	{
		if (GM->S().bHasTablet) H->ToggleTablet(); else GM->Toast(TEXT("Планшет әлі жоқ"), "warn");
		return;
	}
	if (K == "Time") { GM->TimeScale = GM->TimeScale < 2 ? 4.f : GM->TimeScale < 5 ? 12.f : 1.f; GM->Toast(FString::Printf(TEXT("Уақыт жылдамдығы ×%d"), (int32)GM->TimeScale), "info"); return; }
	if (V)
	{
		if (K == "Interact") GM->ExitVehicle();
		else if (K == "Jump") V->DoHandbrakeStart();
		else if (K == "Cam") V->DoToggleCamera();
		return;
	}
	if (!C) return;
	if (K == "Jump") C->DoJump();
	else if (K == "Sprint") C->SetSprint(true);
	else if (K == "Interact") C->Interact();
	else if (K == "Aim") C->SetAim(true);
	else if (K == "Reload") C->Reload();
	else if (K == "Flash") C->ToggleFlashlight();
	else if (K == "Med") C->UseItem(GM->Count("medkit") > 0 ? FName("medkit") : FName("bandage"));
	else if (K == "Drink") C->UseItem("water");
	else if (K == "Eat") C->UseItem("food");
	else if (K.ToString().StartsWith(TEXT("S")) && K.ToString().Len() == 2) C->SelectSlot(FCString::Atoi(*K.ToString().RightChop(1)) - 1);
}

void AKAPPlayerController::OnKeyRelease(FName K)
{
	if (AKAPHUD* H = AKAPHUD::Get(this)) H->HandleKey(K, false);
	if (!AutoMove.IsZero()) Move = AutoMove;
	if (AKAPSimulatorPawn* V = Cast<AKAPSimulatorPawn>(GetPawn())) { if (K == "Jump") V->DoHandbrakeStop(); return; }
	AKAPCharacter* C = Cast<AKAPCharacter>(GetPawn());
	if (!C) return;
	if (K == "Sprint") C->SetSprint(false);
	else if (K == "Aim") C->SetAim(false);
}
