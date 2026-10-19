// Copyright Epic Games, Inc. All Rights Reserved.

#include "KAPSimulatorWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UKAPSimulatorWheelFront::UKAPSimulatorWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}