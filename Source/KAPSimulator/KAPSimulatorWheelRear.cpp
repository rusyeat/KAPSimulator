// Copyright Epic Games, Inc. All Rights Reserved.

#include "KAPSimulatorWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UKAPSimulatorWheelRear::UKAPSimulatorWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}