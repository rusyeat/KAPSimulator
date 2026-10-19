// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KAPSimulatorPawn.h"
#include "KAPSimulatorSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class AKAPSimulatorSportsCar : public AKAPSimulatorPawn
{
	GENERATED_BODY()
	
public:

	AKAPSimulatorSportsCar();
};
