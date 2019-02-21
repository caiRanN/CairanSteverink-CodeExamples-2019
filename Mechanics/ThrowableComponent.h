// Copyright (C) 2015 Turtleneck Studios - All Rights Reserved. This file is part of RITE of ILK which is made by Turtleneck Studios www.turtleneckstudios.com info@turtleneckstudios.com

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/Core/CoreComponent.h"
#include "Actors/Splines/Throwable/ThrowableSplinePath.h"
#include "ThrowableComponent.generated.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is the Throwable Component written for RITE of ILK. This component could be attached to any Carryable Actor which will allow the
* player to throw the object. The component allows some designer set options, for example, should the throwable always target the other player
* and and the option to add good and bad actors to change the visualization of the throw bezier according to the actor class that is hit.
*/



class ABasePlayerCharacter;
class AGameCamera;
class UTimelineComponent;
class UCurveFloat;
class ACarryableActor;


UENUM(BlueprintType)
namespace EThrowableTraceShape
{
	//Max of 256 entries per type, each entry gets associated with 0-255
	enum Type {
		TTS_SPHERE			UMETA(DisplayName = "Sphere"),
		TTS_BOX				UMETA(DisplayName = "Box"),
	};
}

UENUM()
namespace EThrowableState
{
	//Max of 256 entries per type, each entry gets associated with 0-255
	enum Type {
		TS_Idle				UMETA(DisplayName = "IdleState"),
		TS_FocusMode		UMETA(DisplayName = "FocusModeState"),
		TS_Throw			UMETA(DisplayName = "ThrowState"),
		TS_Catch			UMETA(DisplayName = "CatchState")
	};
}

USTRUCT()
struct FSplineTraceData
{
	GENERATED_USTRUCT_BODY()

public:
	FSplineTraceData() { }

	FSplineTraceData(FVector Start, FVector End) 
		: TraceStart(Start)
		, TraceEnd(End) 
	{ 
	}

	UPROPERTY()
	FVector TraceStart;

	UPROPERTY()
	FVector TraceEnd;
};


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable, AutoExpandCategories = ("ThowableComponent|Controls", "ThowableComponent|SpeedCalculation"))
class BOUND_API UThrowableComponent : public UCoreComponent
{
	GENERATED_BODY()

public:	
	UThrowableComponent(const FObjectInitializer& ObjectInitializer);

protected:
	/* Should we always target the other player */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadWrite)
	bool bAlwaysHitOtherPlayer = false;

	/** The name of the Socket inside the Character Mesh this Carryable should be attached to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = CarryableActor, meta = (EditCondition = "bInteractable"))
	FName SocketName = FName("Character_Wrist_R_JointSocket");

	/* IMPORTANT: This should go from ZERO to ONE in ONE second */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly)
	UCurveFloat* TimelineCurve;

	/* The max range that the player can throw */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 1000.0f))
	float MaxRange = 2000.0f;

	/* The min range that the player can throw */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 400.0f))
	float MinRange = 400.0f;

	/* Prevents character movement when using the aim mechanic */
	UPROPERTY(Category = "ThowableComponent|Controls", EditAnywhere, BlueprintReadWrite)
	bool bLockCharacterMovement = true;

	/* The Horizontal Sensitivity used for the aim mechanic */
	UPROPERTY(Category = "ThowableComponent|Controls", EditAnywhere, BlueprintReadWrite)
	float HorizontalSensitivity = 1.0f;
	
	/* The Vertical Sensitivity used for the aim mechanic */
	UPROPERTY(Category = "ThowableComponent|Controls", EditAnywhere, BlueprintReadWrite)
	float VerticalSensitivity = 0.8f;

	/* The delay after catching the orb before being able to throw it */
	UPROPERTY(Category = "ThowableComponent|Controls", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.1f))
	float CatchThrowDelay = 0.2f;

	/* The distance we use to divide our total distance with and calculate a throw speed */
	UPROPERTY(Category = "ThowableComponent|SpeedCalculation", EditAnywhere, BlueprintReadOnly)
	float DistanceDivision = 1000.0f;

	/* The Default Play Rate for our throw timeline */
	UPROPERTY(Category = "ThowableComponent|SpeedCalculation", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 1.0f))
	float DefaultPlayRate = 1.0f;

	/* The Speed Multiplier used to multiply our Default Play Rate with */
	UPROPERTY(Category = "ThowableComponent|SpeedCalculation", EditAnywhere, BlueprintReadOnly)
	float SpeedMultiplier = 1.5f;

	/* The maximum play rate that can be reached when calculating speed */
	UPROPERTY(Category = "ThowableComponent|SpeedCalculation", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.01f))
	float MaxPlayRate = 5.0f;

	/* The minimum play rate that can be reached when calculating speed */
	UPROPERTY(Category = "ThowableComponent|SpeedCalculation", EditAnywhere, BlueprintReadOnly, meta = (ClampMin = 0.01f))
	float MinPlayRate = 0.01f;

	/* This overrides Bad Classes. The list of actors that are treated as good and visualized different */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly)
	TArray<AActor*> GoodActors;

	/* This overrides Good Classes. The list of actors that are treated as bad and visualized different */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly)
	TArray<AActor*> BadActors;

	/* The list of classes that are treated as good and visualized different */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<AActor>> GoodClasses;

	/* The list of actors that are treated as bad and visualized different */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<AActor>> BadClasses;

	/* The height offset for our trace */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	float TraceHeightOffset = 300.0f;

	/* (Debug) Visualize the trace for debug reasons */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	bool bDebugTrace = false;

	/* The shape that is generated to check for collisions during the throw */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	TEnumAsByte<EThrowableTraceShape::Type> TraceCollisionShape = EThrowableTraceShape::TTS_SPHERE;

	/* The amount of points for our generated bezier curve, higher means better precision but less performance */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	int BezierCurvePoints = 12;

	/* The sphere radius of our sphere trace, only necessary when the collision shape has been set to Sphere */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	float TraceRadius = 60.0f;

	/* The range of the trace that is executed when the spline can't find a valid floor point */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	float SnapToFloorTraceRange = 5000.0f;

	/* The box extents of our box trace, only necessary when the collision shape has been set to Box */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	FVector TraceBoxExtents = FVector(50.0f, 50.0f, 50.0f);

	/* Skip floor collision, adds the surface the player is currently standing on to the ignore list */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	bool bIgnorePlayerSurface = true;

	/* Additional actors that should be ignore by the collision trace when thrown */
	UPROPERTY(Category = "ThowableComponent", EditAnywhere, BlueprintReadOnly, AdvancedDisplay)
	TArray<AActor*> AdditionalIgnoreActors;

private:
	ABasePlayerCharacter* PlayerCharacter;
	ABasePlayerCharacter* OtherCharacter;
	AThrowableSplinePath* ThrowSplinePath;
	ACarryableActor* CarryableOwner;

	UTimelineComponent* VisualizationTimeline;
	UTimelineComponent* ThrowTimeline;

	bool bInitialized = false;
	bool bValidThrowLocation = false;
	float ThrowPlayRate = 1.5f;

	FTimerHandle CatchThrowDelayHandle;
	FTransform FocusModeTargetTransform;
	TEnumAsByte<EThrowableState::Type> ThrowableState = EThrowableState::TS_Idle;

protected:
	virtual void BeginPlay() override;

public:
	virtual void EnterAimMode(ACharacter* Character);
	virtual void ExitAimMode(ACharacter* Character);
	virtual void Throw(ACharacter* Character);
	virtual void AddRightInput(float Value);
	virtual void AddForwardInput(float Value);
	virtual void InitializeComponentValues(ACharacter* Character);
	virtual void ShouldResetComponentValues();

private:
	virtual FVector GetPointInValidRange(FVector CurrentLocation);
	virtual void TraceForCollision();
	virtual FCollisionShape GetCollisionShape(FVector Start, FVector End);
	virtual void ChangeVisualizationColor(AActor* HitActor);
	virtual TArray<TWeakObjectPtr<AActor>> GetDefaultIgnoredActors();
	
	virtual void CalculateThrowPath();
	virtual void GeneratePathFromLocation(FVector EndLocation, bool bOverrideVisual = false, EVisualizeType::Type OverrideVisualize = EVisualizeType::VT_VALIDPLACEMENT);
	virtual bool CheckValidPointAtLocation(FHitResult& Hit, FVector StartLocation, FVector CenterLocation, FVector EndLocation);
	virtual void Catch(AActor* HitActor);
	virtual FVector GetClampedLocation(FVector CenterLocation, FVector CurrentLocation);

	void SetSpeedAccordingToDistance(const float Distance);
	void SetInvalidThrowLocation();
	void ShouldRotateToTarget(const FVector Target);
	void InitializeFocusPointFromCamera();

	UFUNCTION()
	virtual void VisualizeInterpolation(float DeltaTime);

	UFUNCTION()
	virtual void ThrowInterpolation(float DeltaTime);
	
	UFUNCTION()
	virtual void ThrowFinished();

public:

	UFUNCTION(Category = "ThrowableComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE bool GetIsFocusModeEnabled() const { return ThrowableState == EThrowableState::TS_FocusMode; }

	UFUNCTION(Category = "ThrowableComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE EThrowableState::Type GetThrowableState() const { return ThrowableState; }

	UFUNCTION(Category = "ThrowableComponent", BlueprintNativeEvent)
	void OnEnterAimMode(ABasePlayerCharacter* PlayerCharacter);

	UFUNCTION(Category = "ThrowableComponent", BlueprintNativeEvent)
	void OnExitAimMode(ABasePlayerCharacter* PlayerCharacter);

	UFUNCTION(Category = "ThrowableComponent", BlueprintNativeEvent)
	void OnThrow(ABasePlayerCharacter* ThrowingCharacter);

	UFUNCTION(Category = "ThrowableComponent", BlueprintNativeEvent)
	void OnCatch(ABasePlayerCharacter* CatchingCharacter);

#if WITH_EDITOR
private:
	bool bWarningShown = false;

private:
	virtual void OnRegister() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};