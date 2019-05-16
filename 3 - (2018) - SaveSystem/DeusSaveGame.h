// Copyright 2018 Disillusion Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Structs.h"
#include "DeusSaveGameArchive.h"
#include "MemoryWriter.h"
#include "MemoryReader.h"
#include "DeusSaveGame.generated.h"

////////////////////////////////////////////
// Code Example Description

/*
* This is a Save Game component that could be placed on any actor derived object that you wanted to save on state changes.
* Every object can have it's own implementation of when it wants to write to the save game.
* Optionaly a programmer can decide to trigger a save game instantly.
*/


/**
 * Actor Information that is used when restoring the actor from a save game
 */
USTRUCT(BlueprintType)
struct DEUS_API FActorRecord
{
	GENERATED_BODY()


public:
	FActorRecord() { }

	UPROPERTY()
	UClass* ActorClass;

	UPROPERTY()
	FString ActorLevelName;

	UPROPERTY()
	FTransform ActorTransform;

	UPROPERTY()
	FString ActorName;

	UPROPERTY()
	TArray<uint8> ActorData;

public:
	TArray<uint8> ObjectSaver(UObject* ObjectToSave)
	{
		TArray<uint8> ObjectData;
		if (!ObjectToSave) { return ObjectData; }

		FMemoryWriter MemoryWriter(ObjectData, true);

		// use a wrapper archive that converts FNames and UObject*'s to strings that can be read back in
		FDeusSaveGameArchive SaveArchive(MemoryWriter);

		// Serialize the object
		ObjectToSave->Serialize(SaveArchive);
		return ObjectData;
	}

	void ObjectLoader(UObject* LoadObject, TArray<uint8> ObjectData)
	{
		if (ObjectData.Num() <= 0) { return; }

		FMemoryReader MemoryReader(ObjectData, true);
		FDeusSaveGameArchive SaveArchive(MemoryReader);
		LoadObject->Serialize(SaveArchive);
	}

	/** Equality operators */
	bool operator==(const FActorRecord& Other) const 
	{
		return Other.ActorClass == ActorClass && Other.ActorLevelName == ActorLevelName && Other.ActorName == ActorName;
	}

	bool operator!=(const FActorRecord& Other) const 
	{
		return !(*this == Other);
	}

	/** Implemented so it can be used in Maps/Sets */
	friend inline uint32 GetTypeHash(const FActorRecord& Key) 
	{
		uint32 Hash = 0;

		Hash = HashCombine(Hash, GetTypeHash(Key.ActorClass));
		Hash = HashCombine(Hash, GetTypeHash(Key.ActorLevelName));
		Hash = HashCombine(Hash, GetTypeHash(Key.ActorName));
		return Hash;
	}

	/** Returns true if data is valid */
	bool IsValid() const 
	{
		return ActorClass != nullptr;
	}
};

namespace EDeusSaveGameVersion
{
	enum type
	{
		// Initial version
		Initial,
		// Added Inventory
		ItemSave,
		// Triggered Save
		TriggerSave,

		// -----<new versions must be added before this line>-----
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
}

/** Object that is written to and read from the save game archive, with a data version */
UCLASS(BlueprintType)
class DEUS_API UDeusSaveGame : public USaveGame
{
	GENERATED_BODY()
	
public:
	UDeusSaveGame() { }

public:
	/** Map of items to item data */
	UPROPERTY(Category = "SaveGame", VisibleAnywhere, BlueprintReadWrite)
	TMap<FPrimaryAssetId, FDeusItemData> InventoryData;

	/** User's unique id */
	UPROPERTY(Category = "SaveGame", VisibleAnywhere, BlueprintReadWrite)
	FString UserId;

	/** The levels that were loaded at the time of the save */
	UPROPERTY(Category = "DeusSaveGame", BlueprintReadWrite)
	TSet<FName> LoadedLevels;

	/** The player transform at the time of the save */
	UPROPERTY(Category = "DeusSaveGame", BlueprintReadWrite)
	FTransform PlayerTransform;

	/** What LatestVersion was when the archive was saved */
	UPROPERTY(Category = "DeusSaveGame", BlueprintReadWrite)
	int32 SavedDataVersion;

	UPROPERTY()
	TSet<FActorRecord> SavedActorRecords;

	UPROPERTY()
	TMap<FGuid, FActorRecord> ActorRecords;

protected:
	/** Overridden to allow version fix ups */
	virtual void Serialize(FArchive& Ar) override;

public:
	/** Returns true if the save object is valid */
	UFUNCTION(Category = "SaveGame", BlueprintCallable)
	bool IsValid() const;
};
