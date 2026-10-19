// Copyright Epic Games, Inc. All Rights Reserved.

#include "KAPSimulatorGameMode.h"
#include "KAPSimulatorPlayerController.h"

AKAPSimulatorGameMode::AKAPSimulatorGameMode()
{
	PlayerControllerClass = AKAPSimulatorPlayerController::StaticClass();
}
