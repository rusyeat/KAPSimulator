#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "KAPInteractable.generated.h"

class AKAPCharacter;

UINTERFACE(MinimalAPI)
class UKAPInteractable : public UInterface { GENERATED_BODY() };

/** E пернесімен әрекет ететін заттар (пульт, жәшік, есік, адам, көлік) */
class KAPSIMULATOR_API IKAPInteractable
{
	GENERATED_BODY()
public:
	/** Бос мәтін — қазір әрекет жоқ */
	virtual FString GetInteractText(AKAPCharacter* By) const = 0;
	virtual void Interact(AKAPCharacter* By) = 0;
	virtual FVector GetInteractLocation() const = 0;
	virtual float GetInteractRadius() const { return 250.f; }
};
