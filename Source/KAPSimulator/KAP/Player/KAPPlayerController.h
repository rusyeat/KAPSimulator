// AKAPPlayerController — Enhanced Input (әрекеттер кодта құрылады), жаяу/көлік басқару, UI тышқаны
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "KAPPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class KAPSIMULATOR_API AKAPPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AKAPPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float Dt) override;
	/** Автотест: жүргізу/жүру векторын беру */
	FVector2D AutoMove = FVector2D::ZeroVector;
private:
	UInputAction* MakeAction(FName Name, bool bAxis2D = false, bool bAxis1D = false);
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC;
	UPROPERTY() TMap<FName, TObjectPtr<UInputAction>> Actions;
	void OnMove(const FInputActionValue& V);
	void OnMoveEnd(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnFire(const FInputActionValue& V);
	void OnFireEnd(const FInputActionValue& V);
	void OnKey(FName Key);
	void OnKeyRelease(FName Key);
	bool UIModal() const;
	FVector2D Move = FVector2D::ZeroVector;
};
