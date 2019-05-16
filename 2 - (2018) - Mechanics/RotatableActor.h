// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Common/Interfaces/Input/IGamepadInputInterface.h"
#include "RotatableActor.generated.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is the Rotatable Actor Mechanic written for RITE of ILK. It's a co-op mechanic where a single (or both) characters
* can interact with the object to rotate it in different directions. 
* They both need to line up their controller thumbstick with their character to rotate the object.
* Optionaly, a rotatable can be affected by other actors and can rotate back into its original position when the players stop interacting.
*/


class ARotatableActorInput;

UENUM(BlueprintType)
namespace ERotationHandlesEnum
{
	//Max of 256 entries per type, each entry gets associated with 0-255
	enum Type 
	{
		RHE_BOTH	UMETA(DisplayName = "Both"),
		RHE_LEFT	UMETA(DisplayName = "Left"),
		RHE_RIGHT	UMETA(DisplayName = "Right"),
		RHE_NONE	UMETA(DisplayName = "None")
	};
}

UCLASS()
class BOUND_API ARotatableActor : public ACoreActor
{
	GENERATED_BODY()

public:
	ARotatableActor(const FObjectInitializer& ObjectInitializer);
	
public:
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	bool bCanRotate = true;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	bool bPlayerCanBlock = true;

	/** The maximum number of rotations before the actor gets locked, zero means it can rotate indefinetely */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	int MaxNumberOfRotations = 0;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	float PushSpeed = 0.4f;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	float PullSpeed = -0.4f;

	/** Does the rotatable return to its original rotation when the player stops interacting */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	bool bRotateBack = false;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", EditCondition = bRotateBack), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	float ReturnSpeed = -2.0f;

	/** Should the rotatable be clamped between two certain angles */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	bool bYawClamped = false;

	/** Minimum value and Maximum value of the clamp */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", EditCondition = bYawClamped), Category = RotatableActor, EditAnywhere, BlueprintReadOnly)
	FVector2D YawClamp = FVector2D(0,0);

	/** The handles the player can use to rotate the actor */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	TEnumAsByte<ERotationHandlesEnum::Type> Handles;

	/** Dot product of when the player is considered pushing or pulling */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	float PushPullDot = 0.f;

	/** Flip the rotation direction, this is usefull if the animations of players are inversed */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	bool bFlipRotationDirection = false;

public:
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, VisibleDefaultsOnly, BlueprintReadOnly)
	class USceneComponent* RootSceneComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, VisibleDefaultsOnly, BlueprintReadOnly)
	class USceneComponent* RotatableSceneComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = RotatableActor, VisibleDefaultsOnly, BlueprintReadOnly)
	class UArrowComponent* ForwardArrowComponent;

	UPROPERTY(Category = RotatableActor, VisibleDefaultsOnly, BlueprintReadOnly)
	class UChildActorComponent* InteractableLeft;

	UPROPERTY(Category = RotatableActor, VisibleDefaultsOnly, BlueprintReadOnly)
	class UChildActorComponent* InteractableRight;

private:
	TArray<ACharacter*> InteractingCharacters;
	FRotator StartWorldRotation = FRotator(0, 0, 0);
	float DesiredSpeed = 0.f;
	float ActualSpeed = 0.f;
	float CurrentRotation = 0.f;
	float MaxRotation = 0;
	float TotalRotationAmount = 0;
	int SpeedMultiplier = 1;
	float DeltaTimeInSeconds = 0.f;
	bool bBlocked = false;

public:
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor)
	void RegisterCharacter(ACharacter* Character);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor)
	void UnRegisterCharacter(ACharacter* Character);

	/** Wrapper function to enable/disable tick and add extra functionality */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintNativeEvent)
	void OnTickEventChanged(bool bEnable);

	/** Is the Chime currently being operated by a player */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor)
	bool GetIsBeingOperated();
	
	/** Tick for every character registered on this rotatable */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintImplementableEvent)
	void OnRegisteredCharacterTick(ACharacter* Character);

	/** Tick for every character registered on this rotatable */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintNativeEvent)
	void OnRotate(float Speed, float Rotation);

	/** Call this function if you want the rotatable to return to its original rotation when no player is interacting with the chime */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintCallable)
	void SetCanRotateBack(const bool bNewValue);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintCallable)
	FVector PositionFromDirectionDistance(FVector Location, FVector Direction, float Distance);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = RotatableActor, BlueprintNativeEvent)
	void OnDestroyHandle(ERotationHandlesEnum::Type DestroyedHandle);

public:
	/** Sets the desired target speed at which we want to be rotating */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetDesiredSpeed(const float NewSpeed) { DesiredSpeed = NewSpeed; }

	/** Sets the return speed at which we want to be rotating */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetReturnSpeed(const float NewSpeed) { ReturnSpeed = NewSpeed; }

	// @todo: Cairan, Replace the blueprint function that uses this with a c++ function
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetTotalRotationAmount(const float NewAmount) { TotalRotationAmount = NewAmount; }

	/** Set the speed multiplier to boost the desired speed, default = 1 */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetSpeedMultiplier(const int NewMultiplier) { SpeedMultiplier = NewMultiplier; }

	/** Use this if you dont want the rotatable to move, useful for a player or object blocking the rotatable */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetIsBlocked(const bool bNewBlocked) { bBlocked = bNewBlocked; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Setters", BlueprintCallable)
	void SetCurrentRotation(const float NewRotation) { CurrentRotation = NewRotation; }

	/** Get the current rotation speed */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE float GetActualSpeed() const { return ActualSpeed; }

	/** Get the desired rotation speed */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE float GetDesiredSpeed() const { return DesiredSpeed; }

	/** Get whether or not this rotatable will return to its original position */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE bool GetCanRotateBack() const { return bRotateBack; }

	/** Get the maximum rotation amount before the chime will lock */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE float GetMaxRotation() const { return MaxRotation; }

	/** Get the current total rotation amount of the rotatable */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE float GetTotalRotationAmount() const { return TotalRotationAmount; }

	/** Get the speed multiplier that is affecting this rotatable's desired speed */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE int GetSpeedMultiplier() const { return SpeedMultiplier; }

	/** Is the rotatable being blocked by a player or object */
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE bool GetIsBlocked() const { return bBlocked; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE TArray<ACharacter*> GetInteractingCharacters() const { return InteractingCharacters; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "RotatableActor|Getters", BlueprintCallable)
	FORCEINLINE float GetCurrentRotation() const { return CurrentRotation; }

private:
	void Rotate();
	void CalculateRotation(float Speed);
	bool GetCanRotateForward();
	bool GetCanRotateBackward();
	void BindDelegates();
	void BindRotatableInputDelegates(UChildActorComponent* ChildActor);
	void DrawDebugFeatures();
	void ShouldDisplayHandles();
	void ToggleHandleVisibility(UChildActorComponent* ChildActor, bool bActive);
	TArray<ARotatableActorInput*> GetRotatableActorInputs();
	ARotatableActorInput* GetRotatableInputFromComponent(UChildActorComponent* ChildActor);

protected:
#if UE_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
	virtual void EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown) override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnRotate_Implementation(float Speed, float CurrentRotation);
	virtual void OnTickEventChanged_Implementation(bool bEnable);
	virtual void BlockOnCharacterCollision();
	virtual FHitResult BoxTraceForCharacter(FVector StartPosition, FVector EndPosition, FVector DirectionOffset = FVector(0,0,0), float OffsetMultiplier = 0.f);
	
	virtual void OnCoreActorReset_Implementation(AActor* Caller) override;

	virtual void OnRightThumbstickForward_Implementation(ACharacter* Character, float Value);
	virtual void OnRightThumbstickRight_Implementation(ACharacter* Character, float Value);
	virtual void OnLeftThumbstickForward_Implementation(ACharacter* Character, float Value);
	virtual void OnLeftThumbstickRight_Implementation(ACharacter* Character, float Value);
	virtual void OnFaceButtonRightPressed_Implementation(ACharacter* Character);
	virtual void OnFaceButtonRightReleased_Implementation(ACharacter* Character);
	virtual void OnFaceButtonBottomPressed_Implementation(ACharacter* Character);
	virtual void OnFaceButtonBottomReleased_Implementation(ACharacter* Character);
	virtual void OnFaceButtonLeftPressed_Implementation(ACharacter* Character);
	virtual void OnFaceButtonLeftReleased_Implementation(ACharacter* Character);
	virtual void OnRightTriggerPressed_Implementation(ACharacter* Character, float Value);
	virtual void OnRightTriggerReleased_Implementation(ACharacter* Character, float Value);
	virtual void OnLeftTriggerPressed_Implementation(ACharacter* Character, float Value);
	virtual void OnLeftTriggerReleased_Implementation(ACharacter* Character, float Value);
	virtual void OnRightBumperPressed_Implementation(ACharacter* Character);
	virtual void OnRightBumperReleased_Implementation(ACharacter* Character);
	virtual void OnLeftBumperPressed_Implementation(ACharacter* Character);
	virtual void OnLeftBumperReleased_Implementation(ACharacter* Character);
};
