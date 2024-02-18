// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewfinderTutorialGameMode.h"
#include "ViewfinderTutorialCharacter.h"
#include "UObject/ConstructorHelpers.h"

AViewfinderTutorialGameMode::AViewfinderTutorialGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
