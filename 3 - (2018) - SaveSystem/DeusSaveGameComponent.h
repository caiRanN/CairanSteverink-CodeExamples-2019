// Copyright 2018 Disillusion Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeusSaveGame.h"
#include "DeusSaveGameComponent.generated.h"

////////////////////////////////////////////
// Code Example Description

/*
* This is a Save Game component that could be placed on any actor derived object that you wanted to save on state changes.
* Every object can have it's own implementation of when it wants to write to the save game.
* Optionaly a programmer can decide to trigger a save game instantly.
*/

UCLASS(BlueprintType, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEUS_API UDeusSaveGameComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDeusSaveGameComponent();

	UPROPERTY(Category = "SaveGameComponent", VisibleAnywhere)
	FGuid Guid;

	/** Should we save the transform of the owning object */
	UPROPERTY(Category = "SaveGameComponent", EditAnywhere)
	bool bSaveObjectTransform = true;

	/** DEPRECATED: Do not use this anymore */
	UPROPERTY(Category = "SaveGameComponent", EditAnywhere)
	bool bSavePlayerLocation = false;

public:
	/**
	 * Saves the Actor that this component is attached to.
	 * Call this when the save flagged properties have been changed.
	 * @param Should we trigger a save instantly or wait for the next save point
	 */
	UFUNCTION(Category = "SaveGameComponent", BlueprintCallable)
	void SaveActorToSaveGame(bool bSaveImmediate);

	/**
	 * Attempts to load the owning actor from the save game
	 * SaveActorToSaveGame must have been called for the actor to exists within the Save Game
	 */
	UFUNCTION(Category = "SaveGameComponent", BlueprintCallable)
	void LoadActorFromSaveGame();
};
