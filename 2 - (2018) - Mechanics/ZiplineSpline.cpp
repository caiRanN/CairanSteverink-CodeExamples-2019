// Fill out your copyright notice in the Description page of Project Settings.

#include "bound.h"
#include "Actors/Splines/Zipline/ZiplineAnchor.h"
#include "Actors/Mechanics/Rope/RopeSegment.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Actors/Characters/Players/BasePlayerCharacter.h"
#include "Actors/Mechanics/Rope/RopeManager.h"
#include "Core/CoreGameInstance.h"
#include "Actors/Camera/ZiplineCamera.h"
#include "Actors/Camera/GameCamera.h"
#include "public/TimerManager.h"
#include "Actors/Mechanics/PivotPointBall/PivotPointBall.h"
#include "ZiplineSpline.h"

////////////////////////////////////////////
// Code Example Description

/*
* This is the Zipline Mechanic written for RITE of ILK. It's a co-op mechanic where both characters follow a zipline
* On the zipline they can swing left and right to dodge objects in their path.
* Designer tweaking is really important as we want the zipline to behave differently in different levels.
* I also added some camera options to allow for more cinematic effects while on the zipline.
*/


AZiplineSpline::AZiplineSpline(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ActorRootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ActorRootSceneComponent"));
	if (ActorRootSceneComponent) 
	{
		RootComponent = ActorRootSceneComponent;
	}

	ZiplineSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("ZiplineSplineComponent"));
	if (ZiplineSplineComponent && RootComponent) 
	{
		ZiplineSplineComponent->SetupAttachment(RootComponent);
		ZiplineSplineComponent->bGenerateOverlapEvents = true;
		ZiplineSplineComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		ZiplineSplineComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		ZiplineSplineComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
#if UE_EDITOR
		ZiplineSplineComponent->ScaleVisualizationWidth = 30.f;
		ZiplineSplineComponent->bShouldVisualizeScale = true;
#endif
	}

	RepeatingInstancedStaticMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RepeatingInstancedStaticMeshComponent"));
	if (RepeatingInstancedStaticMeshComponent && RootComponent) 
	{
		RepeatingInstancedStaticMeshComponent->SetupAttachment(RootComponent);
	}

	PhysicsAnchorSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PhysicsAnchorSphereComponent"));
	if (PhysicsAnchorSphereComponent && ZiplineSplineComponent) 
	{
		PhysicsAnchorSphereComponent->SetupAttachment(ZiplineSplineComponent);
	}

	ZiplineCameraChildActorComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("ZiplineCameraChildActorComponent"));
	if (ZiplineCameraChildActorComponent && PhysicsAnchorSphereComponent) 
	{
		ZiplineCameraChildActorComponent->SetupAttachment(PhysicsAnchorSphereComponent);
		ZiplineCameraChildActorComponent->SetChildActorClass(TSubclassOf<AZiplineCamera>());
	}

	LeftPhysicsConstraintComponent = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("LeftPhysicsConstraintComponent"));
	if (LeftPhysicsConstraintComponent && PhysicsAnchorSphereComponent) 
	{
		LeftPhysicsConstraintComponent->SetupAttachment(PhysicsAnchorSphereComponent);
		LeftPhysicsConstraintComponent->SetAngularSwing2Limit(ACM_Limited, MaxAngularAngle);
		LeftPhysicsConstraintComponent->SetAngularTwistLimit(ACM_Limited, MaxTwistAngle);
	}

	RightPhysicsConstraintComponent = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("RightPhysicsConstraintComponent"));
	if (RightPhysicsConstraintComponent && PhysicsAnchorSphereComponent) 
	{
		RightPhysicsConstraintComponent->SetupAttachment(PhysicsAnchorSphereComponent);
		RightPhysicsConstraintComponent->SetAngularSwing2Limit(ACM_Limited, MaxAngularAngle);
		RightPhysicsConstraintComponent->SetAngularTwistLimit(ACM_Limited, MaxTwistAngle);
	}

	RightRailSplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("RightRailSplineComponent"));
	if (RightRailSplineComponent && RootComponent) 
	{
		RightRailSplineComponent->SetupAttachment(RootComponent);
	}
}

void AZiplineSpline::BeginPlay()
{
	Super::BeginPlay();

	// Get the required Camera's and Instance when the zipline gets loaded
	ZiplineCamera = Cast<AZiplineCamera>(ZiplineCameraChildActorComponent->GetChildActor());
	CoreGameInstance = UBPFL_Turtleneck::GetCoreGameInstance(this);
	GameCamera = CoreGameInstance->GetGameCamera();
}

void AZiplineSpline::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateZipline(DeltaSeconds);
}

void AZiplineSpline::InitializeCharacters(TArray<ABasePlayerCharacter*> Characters)
{
	if (Characters.Num() != 2) { return; }
	if (RegisteredCharacters.Num() != 0) { return; }

	// This is safe, the characters array always contains both Mokh and Tarh
	InitializeCharacter(Characters[0], RightPhysicsConstraintComponent);
	InitializeCharacter(Characters[1], LeftPhysicsConstraintComponent);
}

void AZiplineSpline::InitializeCharacter(ABasePlayerCharacter* Character, UPhysicsConstraintComponent* TargetContraint)
{
	if (!TargetContraint) { return; }
	if (!Character) { return; }
	if (!ZiplineCamera) { return; }

	// Get the camera manager if none exists, we need it for FOV changes and camera transitions.
	if (!CameraManager) 
	{
		CameraManager = Character->GetPlayerCameraManager();
	}

	if (RegisteredCharacters.Contains(Character)) { return; }
	RegisteredCharacters.Add(Character);

	bZiplineEnabled = true;

	UpdateZiplineCameraState(Character, false);
	ShouldBlendToNewCamera(Character, ZiplineCamera);
	PlayZiplineCameraEffects();

	const float StartRopeLength = (MaxRopeLength + MinRopeLength) / 2;
	AZiplineAnchor* SpawnedAnchor = InitializeAnchor(Character, TargetContraint, StartRopeLength);

	FZiplinePlayerData PlayerData;
	PlayerData.RopeLength = -FMath::Abs(StartRopeLength);
	PlayerData.SpawnedAnchor = SpawnedAnchor;
	PlayersData.Add(PlayerData);

	TargetContraint->SetConstrainedComponents(PhysicsAnchorSphereComponent, TEXT("None"), SpawnedAnchor->PhysicsAnchorCapsuleComponent, TEXT("None"));
	TargetContraint->SetConstraintReferencePosition(EConstraintFrame::Frame1, TargetContraint->RelativeLocation);
	SpawnedAnchor->PhysicsAnchorCapsuleComponent->SetSimulatePhysics(true);

	Character->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Character->GetMovementComponent()->Velocity = FVector(0.f, 0.f, 0.f);
	Character->GetRopeManager()->ResetRope(false);
	Character->SetQueueZipline(true);
	Character->SetActorEnableCollision(false);

	OnZiplineEnter();
	SetActorTickEnabled(true);
}

void AZiplineSpline::UpdateZiplineCameraState(ABasePlayerCharacter* Character, bool bCompleted /*= false*/)
{
	if (!bUseZiplineCamera) { return; }
	if (!CameraManager) { return; }
	if (!ZiplineCamera) { return; }
	if (!CoreGameInstance) { return; }
	if (!CoreGameInstance->GetPivotPointBall()) { return; }

	if (!bCompleted) 
	{
		ZiplineCamera->GetCharacters().AddUnique(Character);
		ZiplineCamera->HookCameraToTargetActorSimple(CoreGameInstance->GetPivotPointBall(), CameraDistanceFromCharacters, PhysicsAnchorSphereComponent->GetComponentRotation(), 0, true);
		return;
	} 
	
	CameraManager->StopAllCameraAnims(true);
	ZiplineCamera->UnhookCamera();
	ZiplineCamera->GetCharacters().Empty();
}

void AZiplineSpline::ShouldBlendToNewCamera(ABasePlayerCharacter* Character, AActor* TargetCamera)
{
	if (!bUseZiplineCamera) { return; }
	if (!PhysicsAnchorSphereComponent) { return; }
	if (!CameraManager) { return; }
	if (!TargetCamera) { return; }

	CameraManager->UnlockFOV();
	CameraManager->SetFOV(CameraManager->DefaultFOV);

	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	if (!PlayerController) { return; }
	PlayerController->SetViewTargetWithBlend(TargetCamera, 2.f, EViewTargetBlendFunction::VTBlend_Cubic, 2.f);
}

void AZiplineSpline::PlayZiplineCameraEffects()
{
	if (RegisteredCharacters.Num() == 0) { return; }
	if (!RegisteredCharacters[0]) { return; }
	if (!CameraManager) { return; }

	// Play camera animation if none of the same type is currently playing
	if (ZiplineCameraAnim.CameraAnim && CameraManager->FindInstanceOfCameraAnim(ZiplineCameraAnim.CameraAnim) == NULL) 
	{
		CameraManager->PlayCameraAnim(ZiplineCameraAnim.CameraAnim);
	}

	if (ZiplineCameraShake.CameraShake) 
	{
		CameraManager->PlayCameraShake(ZiplineCameraShake.CameraShake);
	}
}

AZiplineAnchor* AZiplineSpline::InitializeAnchor(ABasePlayerCharacter* Character, UPhysicsConstraintComponent* TargetContraint, float VerticalOffset)
{
	FActorSpawnParameters SpawnInfo;
	FVector Location = TargetContraint->GetComponentLocation() + (TargetContraint->GetUpVector() * -FMath::Abs(VerticalOffset));
	FRotator Rotation = TargetContraint->GetComponentRotation();

	AZiplineAnchor* SpawnedAnchor;
	if (!AnchorToSpawn) 
	{
		SpawnedAnchor = GetWorld()->SpawnActor<AZiplineAnchor>(Location, Rotation, SpawnInfo);
	}
	else 
	{
		SpawnedAnchor = GetWorld()->SpawnActor<AZiplineAnchor>(AnchorToSpawn, Location, Rotation, SpawnInfo);
	}

	if (!SpawnedAnchor) { return nullptr; }
	SpawnedAnchor->SetAttachedCharacter(Character);
	SpawnedAnchor->GetCapsuleComponent()->WakeAllRigidBodies();

	return SpawnedAnchor;
}

void AZiplineSpline::UpdateZipline(float DeltaTime)
{
	if (!bZiplineEnabled) { return; }
	if (!RightPhysicsConstraintComponent) { return; }
	if (!LeftPhysicsConstraintComponent) { return; }
	if (!GameCamera) { return; }

	CalculatePositionOnSpline(DeltaTime);
	CalculateRopeLength();
	CalculateSidewaySwing();
	ShouldDetachFromZipline();

	RightPhysicsConstraintComponent->SetConstraintReferencePosition(EConstraintFrame::Frame1, RightPhysicsConstraintComponent->RelativeLocation);
	LeftPhysicsConstraintComponent->SetConstraintReferencePosition(EConstraintFrame::Frame1, LeftPhysicsConstraintComponent->RelativeLocation);

	for (int i = 0; i < RegisteredCharacters.Num(); ++i)
	{
		if (!RegisteredCharacters[i]) { continue; }
		if (!PlayersData[i].SpawnedAnchor) { continue; }

		FVector NewLoc = PlayersData[i].SpawnedAnchor->GetActorLocation() + (PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetUpVector() * -FMath::Abs(PlayersData[i].RopeLength));
		PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->SetWorldLocation(NewLoc);
		PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->WakeAllRigidBodies();

		FVector Location = PlayersData[i].SpawnedAnchor->GetActorLocation();
		FRotator Rotation = PlayersData[i].SpawnedAnchor->GetActorRotation();
		RegisteredCharacters[i]->GetCapsuleComponent()->SetWorldLocationAndRotation(Location, Rotation);

		// Clamp Character velocity
		FVector LinearVelocity = PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetPhysicsLinearVelocity();
		PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->SetPhysicsLinearVelocity(UKismetMathLibrary::ClampVectorSize(LinearVelocity, MinVelocity, MaxVelocity));
		
		// Make sure the characters do not kill themselves
		RegisteredCharacters[i]->GetMovementComponent()->Velocity.Z = 0.f;

		if (bDebug) 
		{ 
			FVector EndLocation = RegisteredCharacters[i]->GetActorLocation() + (RegisteredCharacters[i]->GetActorUpVector() * (PlayersData[i].RopeLength * -1));
			UKismetSystemLibrary::DrawDebugLine(GetWorld(), RegisteredCharacters[i]->GetActorLocation(), EndLocation, FColor::Green, 0.f, 10.f);
		}
	}
}

void AZiplineSpline::StartZiplineAtLocation(USplineComponent* Spline, FVector Location)
{
	if (!Spline) { return; }

	SetCurrentDistanceOnSpline(UBPFL_Turtleneck::FindDistanceAlongSplineClosestToWorldLocation(Spline, Location));
	const FVector NewLocation = ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);
	const FRotator NewRotation = ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);

	PhysicsAnchorSphereComponent->SetWorldLocation(NewLocation);
	PhysicsAnchorSphereComponent->SetWorldRotation(NewRotation);
}

void AZiplineSpline::CalculatePositionOnSpline(float DeltaTime)
{
	if (!ZiplineSplineComponent) { return; }
	if (!PhysicsAnchorSphereComponent) { return; }
	
	float DistanceSpeedIncrease = 0.f;
	if (bSpeedIncreaseOnDistanceTraveled)
	{
		const float SpeedIncrements = MaxSpeedIncrease / ZiplineSplineComponent->GetSplineLength();
		DistanceSpeedIncrease = CurrentDistanceOnSpline * SpeedIncrements;
	}

	DesiredSpeed = ((DeltaTime * 100 /* percentages */) * (ZiplineSpeedMultiplier + DistanceSpeedIncrease));
	CurrentDistanceOnSpline += DesiredSpeed;

	CalculateFieldOfView(CurrentDistanceOnSpline, 0.f, ZiplineSplineComponent->GetSplineLength());

	const FVector NewLocation = ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);
	const FRotator NewRotation = ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);

	PhysicsAnchorSphereComponent->SetWorldLocation(NewLocation);
	PhysicsAnchorSphereComponent->SetWorldRotation(NewRotation);

	if (CurrentDistanceOnSpline >= ZiplineSplineComponent->GetSplineLength()) 
	{
		ZiplineCompleted();
	}
}

void AZiplineSpline::CalculateRopeLength()
{
	float InputValue = 0.f;
	for (int i = 0; i < RegisteredCharacters.Num(); ++i)
	{
		if (!RegisteredCharacters[i]) { continue; }

		PlayersData[i].SpawnedAnchor->UpdateIconIndicator(false, nullptr);
		if (RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_UP) 
		{
			PlayersData[i].SpawnedAnchor->UpdateIconIndicator(true, PlayersData[i].SpawnedAnchor->UpIndicatorTexture2D);
			InputValue = RegisteredCharacters[i]->GetLeftStickPressureLength();
		}
		else if (RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_DOWN) 
		{
			PlayersData[i].SpawnedAnchor->UpdateIconIndicator(true, PlayersData[i].SpawnedAnchor->DownIndicatorTexture2D);
			InputValue = RegisteredCharacters[i]->GetLeftStickPressureLength() * -1;
		}

		ABasePlayerCharacter* OtherPlayer = RegisteredCharacters[i]->GetOtherPlayerCharacter();
		if (!OtherPlayer) { continue; }
		
		const bool bCanGoUp = RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_UP && OtherPlayer->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_DOWN;
		const bool bCanGoDown = RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_DOWN && OtherPlayer->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_UP;

		if (bCanGoUp || bCanGoDown)
		{
			// This is safe since length is always equal to RegisteredCharacters.Num, see InitializeCharacter()
			PlayersData[i].RopeLength += (InputValue * VerticalSpeedMultiplier);
			PlayersData[i].RopeLength = FMath::Clamp(PlayersData[i].RopeLength, -fabsf(MaxRopeLength), -fabsf(MinRopeLength));
		}

		const bool bRequestUp = RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_UP && OtherPlayer->GetLeftStickPressureDirection() != EStickPressureDirectionEnum::ESPDE_DOWN;
		const bool bRequestDown = RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_DOWN && OtherPlayer->GetLeftStickPressureDirection() != EStickPressureDirectionEnum::ESPDE_UP;
		
		if (bRequestUp) 
		{ 
			PlayerMoveUpRequest(RegisteredCharacters[i], PlayersData[i].SpawnedAnchor); 
		}

		if (bRequestDown) 
		{ 
			PlayerMoveDownRequest(RegisteredCharacters[i], PlayersData[i].SpawnedAnchor); 
		}
	}
}

void AZiplineSpline::CalculateSidewaySwing()
{
	if (!LeftPhysicsConstraintComponent) { return; }
	if (!RightPhysicsConstraintComponent) { return; }

	for (int i = 0; i < RegisteredCharacters.Num(); ++i)
	{
		if (!RegisteredCharacters[i]) { continue; }
		if (RegisteredCharacters[i]->GetQueueDead()) { return; }
		if (!PlayersData[i].SpawnedAnchor) { continue; }

		FVector DotCompareVector = RightPhysicsConstraintComponent->GetRightVector();
		float Dot = FVector::DotProduct(
			(PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetUpVector() * -1).GetSafeNormal(),
			DotCompareVector.GetSafeNormal()
		);

		FVector ImpulseDirection = FVector::ZeroVector;
		float NewImpulseStrength = SwingImpulseStrength * Dot;

		// Begin toggle states
		if (Dot > DotRequirement) 
		{
			PlayersData[i].bSwingRightEnabled = false;
			PlayersData[i].bSwingLeftEnabled = true;
			ImpulseDirection = PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetRightVector() * -1;
			NewImpulseStrength = NewImpulseStrength * 4;
		} 
		else if (Dot < -DotRequirement) 
		{
			PlayersData[i].bSwingLeftEnabled = false;
			PlayersData[i].bSwingRightEnabled = true;
			ImpulseDirection = PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetRightVector();
			NewImpulseStrength = NewImpulseStrength * 4;
		} 
		else if (Dot < DotDeadZone && Dot > -DotDeadZone) 
		{
			PlayersData[i].bSwingRightEnabled = true;
			PlayersData[i].bSwingLeftEnabled = true;
		}
		// end toggle states
		
		if (RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_LEFT && PlayersData[i].bSwingLeftEnabled) 
		{
			ImpulseDirection = PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetRightVector() * -1;
		}
		else if (RegisteredCharacters[i]->GetLeftStickPressureDirection() == EStickPressureDirectionEnum::ESPDE_RIGHT && PlayersData[i].bSwingRightEnabled) 
		{
			ImpulseDirection = PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->GetRightVector();
		}
		
		PlayersData[i].SpawnedAnchor->GetCapsuleComponent()->AddImpulse(ImpulseDirection * SwingImpulseStrength);
	}
}

void AZiplineSpline::CalculateFieldOfView(const float CurrentSpeed, const float MinSpeed, const float MaxSpeed)
{
	if (!CameraManager) { return; }
	if (!bChangeFovOnDistanceTraveled) { return; }
	if (CurrentSpeed >= MaxSpeed) { return; }
	
	const float NewFov = UKismetMathLibrary::MapRangeClamped(CurrentSpeed, MinSpeed, MaxSpeed, CameraManager->DefaultFOV, MaxFieldOfView);

	CameraManager->UnlockFOV();
	CameraManager->SetFOV(NewFov);
}

void AZiplineSpline::ShouldDetachFromZipline()
{
	if (!bTeleportEnabled) { return; }
	if (!bZiplineEnabled) { return; }

	for (int i = 0; i < RegisteredCharacters.Num(); ++i) 
	{
		if (!RegisteredCharacters[i]) { continue; }
		if (RegisteredCharacters[i]->GetIsTeleporting() && bZiplineEnabled) 
		{
			DetachFromZipline();
		}
	}
}

void AZiplineSpline::ZiplineCompleted()
{
	if (GetWorld()->GetTimerManager().GetTimerRemaining(DetachHandle) > 0.f) { return; }

	OnZiplineEndReached();

	CameraManager->UnlockFOV();
	CameraManager->SetFOV(CameraManager->DefaultFOV);

	GetWorld()->GetTimerManager().SetTimer(DetachHandle, [this]() { DetachFromZipline(); }, DetachDelay, false, DetachDelay);
}

void AZiplineSpline::DetachFromZipline()
{
	SetActorTickEnabled(false);

	if (!PhysicsAnchorSphereComponent) { return; }
	if (!RightPhysicsConstraintComponent) { return; }
	if (!LeftPhysicsConstraintComponent) { return; }

	bZiplineEnabled = false;

	RightPhysicsConstraintComponent->BreakConstraint();
	LeftPhysicsConstraintComponent->BreakConstraint();

	FRotator CameraRotation = FRotator(GameCamera->GetActorRotation().Pitch, PhysicsAnchorSphereComponent->GetComponentRotation().Yaw, GameCamera->GetActorRotation().Roll);
	GameCamera->UpdateCameraRotation(CameraRotation, 1.f, true);

	OnZiplineExit();
	DetachAndClearAnchors();
	DetachAndClearCharacters();

	FTimerHandle ResetTimer;
	GetWorld()->GetTimerManager().SetTimer(ResetTimer, [this]() { ResetZipline(); }, 6.f, false, 6.f);
}

void AZiplineSpline::ResetZipline()
{
	if (!PhysicsAnchorSphereComponent) { return; }
	if (!ZiplineSplineComponent) { return; }

	PlayersData.Empty();
	RegisteredCharacters.Empty();
	CurrentDistanceOnSpline = 0.f;

	const FVector NewLocation = ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);
	const FRotator NewRotation = ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(CurrentDistanceOnSpline, ESplineCoordinateSpace::World);
	PhysicsAnchorSphereComponent->SetWorldLocation(NewLocation);
	PhysicsAnchorSphereComponent->SetWorldRotation(NewRotation);
}

void AZiplineSpline::DetachAndClearAnchors()
{
	if (PlayersData.Num() == 0) { return; }

	for (int i = 0; i < PlayersData.Num(); ++i) 
	{
		if (!PlayersData[i].SpawnedAnchor) { continue; }
		PlayersData[i].SpawnedAnchor->Destroy();
	}
}

void AZiplineSpline::DetachAndClearCharacters()
{
	if (!PhysicsAnchorSphereComponent) { return; }
	if (RegisteredCharacters.Num() == 0) { return; }

	for (int i = 0; i < RegisteredCharacters.Num(); ++i) 
	{
		if (!RegisteredCharacters[i]) { continue; }

		UpdateZiplineCameraState(RegisteredCharacters[i], true);
		
		if (bReturnToGameCameraOnCompleted) 
		{
			ShouldBlendToNewCamera(RegisteredCharacters[i], GameCamera);
		}

		bool bInitializedRope = false;
		RegisteredCharacters[i]->LaunchCharacter(RegisteredCharacters[i]->GetActorForwardVector() * DetachVelocity, true, true);
		RegisteredCharacters[i]->SetQueueZipline(false);
		RegisteredCharacters[i]->GetRopeManager()->ResetRope(false);
		RegisteredCharacters[i]->GetRopeManager()->Initialize(bInitializedRope);
		RegisteredCharacters[i]->SetActorEnableCollision(true);
		RegisteredCharacters[i]->SetActorRotation(FRotator(0.f, PhysicsAnchorSphereComponent->GetComponentRotation().Yaw, 0.f));
	}
}

void AZiplineSpline::OnCoreActorReset_Implementation(AActor* Caller)
{
	ResetZipline();
}

void AZiplineSpline::UpdatePhysicsContraints()
{
	if (!LeftPhysicsConstraintComponent) { return; }
	if (!RightPhysicsConstraintComponent) { return; }
	LeftPhysicsConstraintComponent->SetAngularSwing2Limit(ACM_Limited, MaxAngularAngle);
	LeftPhysicsConstraintComponent->SetAngularTwistLimit(ACM_Limited, MaxTwistAngle);
	RightPhysicsConstraintComponent->SetAngularSwing2Limit(ACM_Limited, MaxAngularAngle);
	RightPhysicsConstraintComponent->SetAngularTwistLimit(ACM_Limited, MaxTwistAngle);
}

void AZiplineSpline::InitializeZipline()
{
	if (!ZiplineSplineComponent) { return; }

	PhysicsAnchorSphereComponent->SetRelativeLocation(ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(0, ESplineCoordinateSpace::Local));
	PhysicsAnchorSphereComponent->SetRelativeRotation(ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(0, ESplineCoordinateSpace::Local));
	
	LeftPhysicsConstraintComponent->SetRelativeLocation(LeftPhysicsConstraintComponent->RelativeLocation.RightVector * PlayerOffsetFromCenter * -1);
	RightPhysicsConstraintComponent->SetRelativeLocation(RightPhysicsConstraintComponent->RelativeLocation.RightVector * PlayerOffsetFromCenter);

	GenerateRepeatingMesh();
	GenerateSplinePoints();
	GenerateCable(CableRailArray, RightRailSplineComponent);
	GenerateInvisibleCollisionShape();

	// Cleanup Spline array so the points are not visible in the editor, but do not update.
	RightRailSplineComponent->ClearSplinePoints(false);
}

void AZiplineSpline::GenerateRepeatingMesh()
{
	if (!RepeatingInstancedStaticMeshComponent) { return; }
	RepeatingInstancedStaticMeshComponent->ClearInstances();

	if (!bGenerateRepeatingMesh) { return; }
	if (!ZiplineSplineComponent) { return; }

	int TotalRepeatingMeshes = UKismetMathLibrary::Round(ZiplineSplineComponent->GetSplineLength() / GenerateMeshFrequency);
	if (RemoveRepeatingMeshFromStart > TotalRepeatingMeshes) { return; }

	int CurrentIndex = RemoveRepeatingMeshFromStart;
	for (CurrentIndex; CurrentIndex < TotalRepeatingMeshes; ++CurrentIndex)
	{
		FVector Location = ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(CurrentIndex * GenerateMeshFrequency, ESplineCoordinateSpace::Local);
		FRotator Rotation = ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(CurrentIndex * GenerateMeshFrequency, ESplineCoordinateSpace::Local);

		FTransform InstanceTransform = FTransform(Rotation, Location + (UKismetMathLibrary::GetRightVector(Rotation) * (PointOffsetFromCenter * -1)), FVector(1, 1, 1));
		RepeatingInstancedStaticMeshComponent->AddInstance(InstanceTransform);
	}

	RepeatingInstancedStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AZiplineSpline::GenerateSplinePoints()
{
	CableRailArray.Empty();

	int TotalPointAmount = UKismetMathLibrary::Round(ZiplineSplineComponent->GetSplineLength() / PointCalculationFrequency);
	for (int CurrentIndex = 0; CurrentIndex < TotalPointAmount; ++CurrentIndex)
	{
		FVector Location = ZiplineSplineComponent->GetLocationAtDistanceAlongSpline(CurrentIndex * PointCalculationFrequency, ESplineCoordinateSpace::Local);
		FRotator Rotation = ZiplineSplineComponent->GetRotationAtDistanceAlongSpline(CurrentIndex * PointCalculationFrequency, ESplineCoordinateSpace::Local);
		
		float LocalOffset = PointOffsetFromCenter * -1; // @example: You can use modulo and offset to generate left and right rails
		CableRailArray.Add(Location + (UKismetMathLibrary::GetRightVector(Rotation) * LocalOffset));
	}
}

void AZiplineSpline::GenerateCable(TArray<FVector> CableLocations, USplineComponent* RailSpline)
{
	/** Do not change this to indices based iteration, it will yield undifined behaviour */
	for (int i = 0; i < CableMeshData.Num(); ++i) 
	{
		if (!CableMeshData[i]) { continue; }
		CableMeshData[i]->DestroyComponent();
	}
	CableMeshData.Empty();

	if (!RailSpline) { return; }

	if (!bGenerateCable) { return; }
	RailSpline->ClearSplinePoints(true);
	RailSpline->SetSplinePoints(CableLocations, ESplineCoordinateSpace::Local, true);

	int CurrentIndex = RemoveCableFromStart;
	for (CurrentIndex; CurrentIndex < CableLocations.Num(); ++CurrentIndex)
	{
		FVector CurrentLocation = FVector::ZeroVector;
		FVector CurrentTangent = FVector::ZeroVector;
		RailSpline->GetLocationAndTangentAtSplinePoint(CurrentIndex, CurrentLocation, CurrentTangent, ESplineCoordinateSpace::World);

		FVector NextLocation = FVector::ZeroVector;
		FVector NextTangent = FVector::ZeroVector;
		RailSpline->GetLocationAndTangentAtSplinePoint(CurrentIndex + 1, NextLocation, NextTangent, ESplineCoordinateSpace::World);

		USplineMeshComponent* CableMeshSegment = NewObject<USplineMeshComponent>(this);
		if (!CableMeshSegment) { continue; }
		CableMeshSegment->Mobility = EComponentMobility::Movable;
		CableMeshSegment->RegisterComponent();
		CableMeshSegment->SetStaticMesh(CableMesh);
		CableMeshSegment->SetStartAndEnd(CurrentLocation, CurrentTangent, NextLocation, NextTangent);
		CableMeshSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		CableMeshData.Add(CableMeshSegment);
	}
}

void AZiplineSpline::GenerateInvisibleCollisionShape()
{
	/** Do not change this to indices based iteration, it will yield undifined behaviour */
	for (int i = 0; i < CollisionShapeData.Num(); ++i) 
	{
		if (!CollisionShapeData[i]) { continue; }
		CollisionShapeData[i]->DestroyComponent();
	}
	CollisionShapeData.Empty();

	if (!SimpleCollisionShapeMesh) { return; }
	if (!RightRailSplineComponent) { return; }
	if (CableRailArray.Num() == 0) { return; }

	for (int i = 0; i < CableRailArray.Num(); ++i) 
	{
		FVector CurrentLocation = FVector::ZeroVector;
		FVector CurrentTangent = FVector::ZeroVector;
		FRotator CurrentRotation = RightRailSplineComponent->GetRotationAtSplinePoint(i, ESplineCoordinateSpace::World);
		RightRailSplineComponent->GetLocationAndTangentAtSplinePoint(i, CurrentLocation, CurrentTangent, ESplineCoordinateSpace::World);
		CurrentLocation = CurrentLocation + (UKismetMathLibrary::GetRightVector(CurrentRotation) * PointOffsetFromCenter);

		FVector NextLocation = FVector::ZeroVector;
		FVector NextTangent = FVector::ZeroVector;
		FRotator NextRotation = RightRailSplineComponent->GetRotationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
		RightRailSplineComponent->GetLocationAndTangentAtSplinePoint(i + 1, NextLocation, NextTangent, ESplineCoordinateSpace::World);
		NextLocation = NextLocation + (UKismetMathLibrary::GetRightVector(NextRotation) * PointOffsetFromCenter);

		USplineMeshComponent* SimpleCollisionMesh = NewObject<USplineMeshComponent>(this);
		if (!SimpleCollisionMesh) { return; }
		
		SimpleCollisionMesh->Mobility = EComponentMobility::Movable;
		SimpleCollisionMesh->RegisterComponent();
		SimpleCollisionMesh->SetStaticMesh(SimpleCollisionShapeMesh);
		SimpleCollisionMesh->SetStartAndEnd(CurrentLocation, CurrentTangent, NextLocation, NextTangent);
		SimpleCollisionMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SimpleCollisionMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		SimpleCollisionMesh->SetHiddenInGame(true);
		SimpleCollisionMesh->bVisible = false;

		CollisionShapeData.Add(SimpleCollisionMesh);
	}
}

///////////////////////////////////////////
// Blueprint Event CPP Implementation

void AZiplineSpline::OnZiplineEnter_Implementation()
{

}

void AZiplineSpline::OnZiplineEndReached_Implementation()
{

}

void AZiplineSpline::OnZiplineExit_Implementation()
{

}

void AZiplineSpline::PlayerMoveDownRequest_Implementation(ABasePlayerCharacter* RequestingPlayer, AZiplineAnchor* Anchor)
{

}

void AZiplineSpline::PlayerMoveUpRequest_Implementation(ABasePlayerCharacter* RequestingPlayer, AZiplineAnchor* Anchor)
{

}

///////////////////////////////////////////
// Editor Only

#if UE_EDITOR
void AZiplineSpline::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InitializeZipline();
	UpdatePhysicsContraints();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif