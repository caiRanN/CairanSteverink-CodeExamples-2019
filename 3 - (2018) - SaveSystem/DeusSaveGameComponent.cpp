// Copyright 2018 Disillusion Games. All Rights Reserved.

#include "DeusSaveGameComponent.h"
#include "Engine/World.h"
#include "DeusGameInstance.h"
#include "DeusSaveGame.h"
#include "DisillusionFunctionLibrary.h"
#include "BasePlayerCharacter.h"

////////////////////////////////////////////
// Code Example Description

/*
* This is a Save Game component that could be placed on any actor derived object that you wanted to save on state changes.
* Every object can have it's own implementation of when it wants to write to the save game.
* Optionaly a programmer can decide to trigger a save game instantly.
*/


UDeusSaveGameComponent::UDeusSaveGameComponent() 
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void UDeusSaveGameComponent::SaveActorToSaveGame(bool bSaveImmediate) 
{
	UWorld* World = GetWorld();
	if (!World) 
	{ 
		return; 
	}

	UDeusGameInstance* GameInstance = World ? World->GetGameInstance<UDeusGameInstance>() : nullptr;
	if (GameInstance && GameInstance->GetCurrentSaveGame()) 
	{
		
		UDeusSaveGame* SaveGame = GameInstance->GetCurrentSaveGame();
		AActor* Owner = GetOwner();

		FActorRecord ActorRecord;
		ActorRecord.ActorClass = Owner->GetClass();
		ActorRecord.ActorName = Owner->GetName();
		ActorRecord.ActorLevelName = Owner->GetLevel()->GetOuter()->GetName();

		// Serialize the actor to the byte stream
		ActorRecord.ActorData = ActorRecord.ObjectSaver(Owner);

		if (bSaveObjectTransform) 
		{
			ActorRecord.ActorTransform = Owner->GetTransform();
		}

		// Add the actor data to the save game
		SaveGame->SavedActorRecords.Add(ActorRecord);
		
		if (bSavePlayerLocation) 
		{
			AActor* Player = UDisillusionFunctionLibrary::GetFirstActorOfClass(World, ABasePlayerCharacter::StaticClass());
			SaveGame->PlayerTransform = Player->GetActorTransform();
		}

		// Should we write to the save game or wait for the next save point
		if (bSaveImmediate) 
		{
			GameInstance->WriteSaveGame();
		}
	}
}

void UDeusSaveGameComponent::LoadActorFromSaveGame() 
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();

	if (!Owner || !World) 
	{ 
		return; 
	}

	UDeusGameInstance* GameInstance = World ? World->GetGameInstance<UDeusGameInstance>() : nullptr;
	if (GameInstance && GameInstance->GetCurrentSaveGame()) 
	{
		UDeusSaveGame* SaveGame = GameInstance->GetCurrentSaveGame();

		for (FActorRecord Record : SaveGame->SavedActorRecords) 
		{
			const bool bMatchingNames = Record.ActorName == Owner->GetName() && Record.ActorLevelName == Owner->GetLevel()->GetOuter()->GetName();
			const bool bMatchingClass = Record.ActorClass == Owner->GetClass();
			if (bMatchingClass && bMatchingNames) 
			{
				// Load the actor data from the byte stream
				Record.ObjectLoader(Owner, Record.ActorData);

				// Set the object location from the save game
				if (bSaveObjectTransform)
				{
					GetOwner()->SetActorTransform(Record.ActorTransform);
				}

				break;
			}
		}
	}
}
