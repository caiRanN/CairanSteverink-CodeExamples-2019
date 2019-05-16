// Copyright 2018 Disillusion Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/CoreActor.h"
#include "Interfaces/IActivatableActor.h"
#include "BaseTrigger.generated.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is a base trigger utility that was written for The Seminary of Sight.
* The design team requested a more visual trigger that they could easily re-use and blueprint.
* The base trigger adds plenty of options that I thought that would be useful like enter directions and max trigger enters
* The trigger uses an editor only cube mesh with a low opacity material for a better visual representation in the editor.
* The trigger material its color could also be changed to easily differentiate between triggers.
*/


class UBoxComponent;
class UStaticMeshComponent;
class UBillboardComponent;
class UMaterialInstance;
class UMaterialInstanceDynamic;
class ACharacter;
class UArrowComponent;
class ABasePlayerCharacter;
class UDeusSaveGameComponent;


UENUM(BlueprintType)
enum class ETriggerDirection : uint8
{
	TD_FRONT	UMETA(DisplayName = "Front"),
	TD_LEFT		UMETA(DisplayName = "Left"),
	TD_RIGHT	UMETA(DisplayName = "Right"),
	TD_BACK		UMETA(DisplayName = "Rear"),
	TD_TOP		UMETA(DisplayName = "Top")
};

/**
 * 
 */
UCLASS()
class DEUS_API ABaseTrigger : public ACoreActor, public IIActivatableActor
{
	GENERATED_BODY()
	
public:
	ABaseTrigger();

protected:
	UPROPERTY(Category = "BaseTrigger", VisibleDefaultsOnly, BlueprintReadOnly)
	UBoxComponent* RootTriggerBoxComponent;

	UPROPERTY(Category = "BaseTrigger", VisibleDefaultsOnly, BlueprintReadOnly)
	UStaticMeshComponent* VisualStaticMeshComponent;

	UPROPERTY(Category = "BaseTrigger", VisibleDefaultsOnly, BlueprintReadOnly)
	UBillboardComponent* IconBillboardComponent;

	UPROPERTY(Category = "BaseTrigger", VisibleDefaultsOnly, BlueprintReadOnly)
	UArrowComponent* ForwardDirectionArrowComponent;

	UPROPERTY(Category = "BaseTrigger", VisibleDefaultsOnly, BlueprintReadOnly)
	UDeusSaveGameComponent* SaveGameComponent;

protected:
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadWrite, Interp)
	bool bEnabled = true;

	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly)
	bool bUseMaxTriggerCount = false;

	/* The Amount of Time the trigger can be activated by the player */
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly, meta = (EditCondition = bUseMaxTriggerCount, ClampMin = 0))
	int MaxTriggerCount = 0;

	/* Should this only be triggered when entered from a certain direction */
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly)
	bool bUseEnterDirection = false;

	/* The direction the player has to enter the trigger */
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly, meta = (EditCondition = bUseEnterDirection))
	ETriggerDirection TriggerDirection;

	/* The color of the trigger in the editor */
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly, AdvancedDisplay, meta = (HideAlphaChannel))
	FLinearColor TriggerColor;

	/* The trigger material instance to use */
	UPROPERTY(Category = "BaseTrigger (General Settings)", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	UMaterialInstance* MaterialInstance;

	/** Activatable Actor Interface Implementation */
	UPROPERTY(Category = "BaseTrigger (Activatable Actor Settings)", EditAnywhere, BlueprintReadOnly, meta = (ShowOnlyInnerProperties))
	FActivatableActorData ActivatableActorData;

private:
	UMaterialInstanceDynamic* DynamicMaterial;

	UPROPERTY(SaveGame)
	int CurrentTriggerCount = 0;
	
	UPROPERTY(SaveGame)
	int TriggeredAmount = 0;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BindDelegates();
	virtual void HandleTriggerEnter(ABasePlayerCharacter* PlayerCharacter);
	virtual void HandleTriggerExit(ABasePlayerCharacter* PlayerCharacter);
	
	bool IsEnterDirectionValid(const FHitResult& SweepResult);
	bool IsCorrectEnterDirection(FVector DirectionVector, const FVector_NetQuantizeNormal ImpactNormal);
	FVector GetDirectionFromTriggerDirection() const;

public:
	UFUNCTION(Category = "BaseTrigger")
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(Category = "BaseTrigger")
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// Begin Activatable Actor Interface
	void OnActivateActor_Implementation(AActor* Caller);
	void OnDeactivateActor_Implementation(AActor* Caller);
	// End Activatable Actor Interface

protected:
	// Blueprint Events
	UFUNCTION(Category = "BaseTrigger", BlueprintNativeEvent)
	void OnTriggerEnter(ABasePlayerCharacter* PlayerCharacter);

	UFUNCTION(Category = "BaseTrigger", BlueprintNativeEvent)
	void OnTriggerExit(ABasePlayerCharacter* PlayerCharacter);
	// ~Blueprint Event

protected:
#if UE_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void UpdateTriggerDirectionArrow();
#endif

};
