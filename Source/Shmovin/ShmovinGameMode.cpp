// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShmovinGameMode.h"
#include "ShmovinCharacter.h"
#include "UObject/ConstructorHelpers.h"

AShmovinGameMode::AShmovinGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
