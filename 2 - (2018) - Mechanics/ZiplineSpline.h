// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Core/CoreActor.h"
#include "Actors/Mechanics/InteractableActor/InteractableActor.h"
#include "ZiplineSpline.generated.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is the Zipline Mechanic written for RITE of ILK. It's a co-op mechanic where both characters follow a zipline
* On the zipline they can swing left and right to dodge objects in their path.
* Designer tweaking is really important as we want the zipline to behave differently in different levels.
* I also added some camera options to allow for more cinematic effects while on the zipline.
*/



class AGameCamera;
class AZiplineCamera;
class ABasePlayerCharacter;

USTRUCT()
struct FZiplinePlayerData
{
	GENERATED_USTRUCT_BODY()

	FZiplinePlayerData()
		: RopeLength(0.f)
		, bSwingLeftEnabled(true)
		, bSwingRightEnabled(true)
		, SpawnedAnchor(nullptr) 
	{ 
	}
	
	UPROPERTY()
	float RopeLength;

	UPROPERTY()
	bool bSwingLeftEnabled;

	UPROPERTY()
	bool bSwingRightEnabled;

	UPROPERTY()
	AZiplineAnchor* SpawnedAnchor;
};

/**
 * 
 */
UCLASS()
class BOUND_API AZiplineSpline : public ACoreActor
{
	GENERATED_BODY() 
	
public:
	AZiplineSpline(const FObjectInitializer& ObjectInitializer);
	
public:
	/** The zipline anchor to spawn, if not set it will spawn the default C++ variant with no custom behaviour */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	TSubclassOf<AZiplineAnchor> AnchorToSpawn;

	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float PlayerOffsetFromCenter = 100.f;

	/** The value with which the zipline base speed is multiplied */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float ZiplineSpeedMultiplier = 12.5f;

	/** Does the travelspeed increase according to the distance traveled on the zipline */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bSpeedIncreaseOnDistanceTraveled = true;

	/** The maximum speed that is added to the base speed of the zipline */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 1.f, EditCondition = bSpeedIncreaseOnDistanceTraveled))
	float MaxSpeedIncrease = 5.f;

	/** Allows the players to detach from the zipline by using their teleport ability */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bTeleportEnabled = true;

	/** The speed multiplier applied to vertical rope movement caused by the players */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float VerticalSpeedMultiplier = 4.f;

	/** The maximum length of the rope that the players can reach when moving up */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 0.f))
	float MinRopeLength = 20.f;

	/** The maximum length of the rope that the players can reach when moving down */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 200.f))
	float MaxRopeLength = 400.f;

	/** How long it takes for the characters to be released from the zipline when it has reached the end */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 0.01f))
	float DetachDelay = 0.15f;

	/** The Velocity that is applied to the players forward vector when detaching from the zipline */
	UPROPERTY(Category = "Zipline", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float DetachVelocity = 2000.f;

	/** Change the viewport to the custom zipline camera */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bUseZiplineCamera = true;

	/** Should we return to the game camera after the zipline has been completed, false is usefull when jumping on another zipline */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bReturnToGameCameraOnCompleted = true;

	/** Should the FOV change according to the distance the players have traveled along the zipline */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bChangeFovOnDistanceTraveled = true;

	/** The maximum FOV that we will increase or decrease to according to the initial starting value */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 100.f, ClampMax = 160.f, EditCondition = bChangeFovOnDistanceTraveled))
	float MaxFieldOfView = 115.f;

	/** The distance the camera will be from the characters */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 60.f))
	float CameraDistanceFromCharacters = 300.f;

	/** Camera animation to play during the zipline, if any */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	FInteractableActorCameraAnimStruct ZiplineCameraAnim;

	/** Camera Shake to play during the zipline, if any */
	UPROPERTY(Category = "Zipline|Camera", EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	FInteractableActorCameraShakeStruct ZiplineCameraShake;

	/** The Twist restriction on the physics contraints */
	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 0, ClampMax = 180))
	float MaxTwistAngle = 45.f;

	/** The angular restriction on the physics contraints */
	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 0, ClampMax = 180))
	float MaxAngularAngle = 25.f;

	/** The impulse that the character can apply to swing */
	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float SwingImpulseStrength = 2500.f;

	/** The minimum requirement to allow the characters to start swinging */
	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float DotRequirement = 0.6f;

	/** The value the dot has to reach to be able to swing to both sides again */
	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float DotDeadZone = 0.07f;

	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float MinVelocity = 0.f;

	UPROPERTY(Category = "Zipline|Physics", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float MaxVelocity = 10000.f;

	/** Generate a cable along the length of the spline */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bGenerateCable = true;

	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", EditCondition = bGenerateCable))
	int RemoveCableFromStart = 0;

	/** Generate a mesh that repeats along the length of the spline */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	bool bGenerateRepeatingMesh = true;

	/** The frequency at which the generate a repeating mesh */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", EditCondition = bGenerateRepeatingMesh, ClampMin = 25.f))
	float GenerateMeshFrequency = 100.f;

	/** Remove increments of the repeating mesh from the start of the zipline */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", EditCondition = bGenerateRepeatingMesh))
	int RemoveRepeatingMeshFromStart = 0;

	/** The offset the generated rail points have from the center of the spline */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	float PointOffsetFromCenter = 125.f;

	/** Higher is faster, lower is smoother (this affects cable smoothness) */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true", ClampMin = 25.f))
	float PointCalculationFrequency = 100.f;

	/** This should be a mesh with a simple collision shape, for example a square, this will be hidden in game */
	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	class UStaticMesh* SimpleCollisionShapeMesh;

	UPROPERTY(Category = "Zipline|Generation", EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"))
	class UStaticMesh* CableMesh;

private:
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class USplineComponent* ZiplineSplineComponent;

	/** This is one of the anchor points for the physics components, this will also be used to move down the zipline */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class USphereComponent* PhysicsAnchorSphereComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class UChildActorComponent* ZiplineCameraChildActorComponent;

	/** The mesh that will be repeated every (GenerateMeshFrequency) amount along the spline */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class UInstancedStaticMeshComponent* RepeatingInstancedStaticMeshComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class UPhysicsConstraintComponent* LeftPhysicsConstraintComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class UPhysicsConstraintComponent* RightPhysicsConstraintComponent;

	/** We use this to generate the cable mesh along the length of this spline */
	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadOnly)
	class USplineComponent* RightRailSplineComponent;

	UPROPERTY(meta = (AllowPrivateAccess = "true", BlueprintProtected = "true"), Category = "Zipline", VisibleDefaultsOnly, BlueprintReadWrite)
	class USceneComponent* ActorRootSceneComponent;

private:
	TArray<FVector> CableRailArray;
	TArray<ABasePlayerCharacter*> RegisteredCharacters;
	TArray<USplineMeshComponent*> CableMeshData;
	TArray<USplineMeshComponent*> CollisionShapeData;
	TArray<FZiplinePlayerData> PlayersData;
	FTimerHandle DetachHandle;
	APlayerCameraManager* CameraManager;
	AGameCamera* GameCamera;
	AZiplineCamera* ZiplineCamera;
	UCoreGameInstance* CoreGameInstance;

	float DesiredSpeed = 0.f;
	float CurrentDistanceOnSpline = 0.f;
	bool bZiplineEnabled = false;

public:
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline")
	void InitializeCharacter(ABasePlayerCharacter* Character, UPhysicsConstraintComponent* TargetContraint);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline")
	void InitializeCharacters(TArray<ABasePlayerCharacter*> Characters);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintCallable)
	void InitializeZipline();

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintCallable)
	void UpdateZipline(float DeltaTime);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintCallable)
	void StartZiplineAtLocation(USplineComponent* Spline, FVector Location);

public:
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Setters", BlueprintCallable)
	void SetZiplineEnabled(bool bEnabled) { bZiplineEnabled = bEnabled; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE USphereComponent* GetPhysicsAnchor() const { return PhysicsAnchorSphereComponent; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE UPhysicsConstraintComponent* GetLeftPhysicsContraint() const { return LeftPhysicsConstraintComponent; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE UPhysicsConstraintComponent* GetRightPhysicsContraint() const { return RightPhysicsConstraintComponent; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE TArray<USplineMeshComponent*> GetRightCableMeshData() const { return CableMeshData; }
	
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE USplineComponent* GetZiplineSplineComponent() const { return ZiplineSplineComponent; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE USplineComponent* GetRightRailSplineComponent() const { return RightRailSplineComponent; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE bool GetIsZiplineEnabled() const { return bZiplineEnabled; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE float GetZiplineSpeed() const { return DesiredSpeed; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE float GetCurrentDistanceOnSpline() const { return CurrentDistanceOnSpline; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	FORCEINLINE TArray<ABasePlayerCharacter*> GetRegisteredCharacters() const { return RegisteredCharacters; }

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline|Getters", BlueprintCallable)
	void SetCurrentDistanceOnSpline(float NewDistance) { CurrentDistanceOnSpline = NewDistance; }

public:
	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintNativeEvent)
	void OnZiplineEnter();

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintNativeEvent)
	void OnZiplineEndReached();

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintNativeEvent)
	void OnZiplineExit();

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintNativeEvent)
	void PlayerMoveDownRequest(ABasePlayerCharacter* RequestingPlayer, AZiplineAnchor* Anchor);

	UFUNCTION(meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"), Category = "Zipline", BlueprintNativeEvent)
	void PlayerMoveUpRequest(ABasePlayerCharacter* RequestingPlayer, AZiplineAnchor* Anchor);

public:
	/** Force character detachment from the zipline, safe to call from other objects */
	virtual void DetachFromZipline();

private:
	void UpdatePhysicsContraints();
	void ShouldBlendToNewCamera(ABasePlayerCharacter* Character, AActor* TargetCamera);
	void UpdateZiplineCameraState(ABasePlayerCharacter* Character, bool bCompleted = false);
	AZiplineAnchor* InitializeAnchor(ABasePlayerCharacter* Character, UPhysicsConstraintComponent* TargetContraint, float VerticalOffset);
	void ResetZipline();
	void DetachAndClearAnchors();
	void DetachAndClearCharacters();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DetaSeconds) override;
	virtual void GenerateSplinePoints();
	virtual void GenerateRepeatingMesh();
	virtual void GenerateCable(TArray<FVector> CableLocations, USplineComponent* RailSpline);
	virtual void GenerateInvisibleCollisionShape();
	virtual void CalculateRopeLength();
	virtual void CalculateSidewaySwing();
	virtual void ZiplineCompleted();
	virtual void CalculatePositionOnSpline(float DeltaTime);
	virtual void CalculateFieldOfView(float CurrentSpeed, float MinSpeed, float MaxSpeed);
	virtual void ShouldDetachFromZipline();
	virtual void OnCoreActorReset_Implementation(AActor* Caller);
	virtual void PlayZiplineCameraEffects();

#if UE_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
