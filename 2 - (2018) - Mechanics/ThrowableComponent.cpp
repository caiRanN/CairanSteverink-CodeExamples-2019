// Copyright (C) 2015 Turtleneck Studios - All Rights Reserved. This file is part of RITE of ILK which is made by Turtleneck Studios www.turtleneckstudios.com info@turtleneckstudios.com

#include "ThrowableComponent.h"
#include "Actors/Mechanics/InteractableActor/Carryable/CarryableActor.h"
#include "Actors/Characters/Players/BasePlayerCharacter.h"
#include "Utilities/BPFL_Debug.h"
#include "Utilities/BPFL_Turtleneck.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Actors/Splines/Throwable/ThrowableSplinePath.h"
#include "Actors/Mechanics/Rope/RopeManager.h"
#include "Actors/Mechanics/Rope/RopeSegment.h"
#include "Components/SplineComponent.h"
#include "TimerManager.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/StaticMeshComponent.h"


// Code Example Description

/*
* This is the Throwable Component written for RITE of ILK. This component could be attached to any Carryable Actor which will allow the
* player to throw the object. The component allows some designer set options, for example, should the throwable always target the other player
* and and the option to add good and bad actors to change the visualization of the throw bezier according to the actor class that is hit.
*/


UThrowableComponent::UThrowableComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) 
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UThrowableComponent::BeginPlay() 
{
	Super::BeginPlay();

	if (!TimelineCurve) { return; }

	FOnTimelineFloat OnTimelineTickCallback;
	FOnTimelineEventStatic OnTimelineFinishedCallback;
	
	ThrowTimeline = UBPFL_Turtleneck::CreateTimeline(this, FName("ThrowTimeline"), false, ThrowPlayRate, 1.0f);
	if (ThrowTimeline) {
		OnTimelineTickCallback.BindUFunction(this, FName{ TEXT("ThrowInterpolation") });
		OnTimelineFinishedCallback.BindUFunction(this, FName{ TEXT("ThrowFinished") });
		ThrowTimeline->AddInterpFloat(TimelineCurve, OnTimelineTickCallback);
		ThrowTimeline->SetTimelineFinishedFunc(OnTimelineFinishedCallback);
		ThrowTimeline->RegisterComponent();
	}

	FOnTimelineFloat OnVisualizeTimelineTickCallback;
	VisualizationTimeline = UBPFL_Turtleneck::CreateTimeline(this, FName("VisualizationTimeline"), true, 1.0f, 1.0f);
	if (VisualizationTimeline) {
		OnVisualizeTimelineTickCallback.BindUFunction(this, FName{ TEXT("VisualizeInterpolation") });
		VisualizationTimeline->AddInterpFloat(TimelineCurve, OnVisualizeTimelineTickCallback);
		VisualizationTimeline->RegisterComponent();
	}
}

void UThrowableComponent::EnterAimMode(ACharacter* Character) 
{
	InitializeComponentValues(Character); 
	InitializeFocusPointFromCamera();

	ThrowableState = EThrowableState::TS_FocusMode;
	OnEnterAimMode(PlayerCharacter);

	if (!VisualizationTimeline) { return; }
	VisualizationTimeline->PlayFromStart();
}

void UThrowableComponent::ExitAimMode(ACharacter* Character) 
{
	if (!ThrowSplinePath) { return; }
	ThrowSplinePath->HideVisualization();
	
	ThrowableState = EThrowableState::TS_Idle;
	OnExitAimMode(PlayerCharacter);

	if (!VisualizationTimeline) { return; }
	VisualizationTimeline->Stop();
}

void UThrowableComponent::CalculateThrowPath() 
{
	if (bAlwaysHitOtherPlayer && OtherCharacter) {
		GeneratePathFromLocation(OtherCharacter->GetActorLocation());
		return;
	}

	GeneratePathFromLocation(FocusModeTargetTransform.GetLocation());
}

void UThrowableComponent::GeneratePathFromLocation(FVector EndLocation, bool bOverrideVisual, EVisualizeType::Type OverrideVisualize) 
{
	const FVector StartLocation = PlayerCharacter->GetMesh()->GetSocketLocation(SocketName);
	FVector CenterLocation = (PlayerCharacter->GetActorLocation() + EndLocation) * 0.5f;

	const float Distance = FVector::Distance(StartLocation, EndLocation);
	const FVector BezierHeight = FVector(0.0f, 0.0f, Distance * 0.25f /* Percentages */);
	
	ShouldRotateToTarget(EndLocation);
	SetSpeedAccordingToDistance(Distance);

	// Update our FocusModeTargetTransform with the new camera rotation
	const float CameraYaw = PlayerCharacter->GetPlayerCameraViewportTransform().GetRotation().Rotator().Yaw;
	const FRotator Rotation = FRotator(0.0f, CameraYaw, 0.0f);
	FocusModeTargetTransform.SetRotation(Rotation.Quaternion());

	FHitResult Hit;
	if (!CheckValidPointAtLocation(Hit, StartLocation, CenterLocation + BezierHeight, EndLocation)) {
		SetInvalidThrowLocation();
		return;
	}

	FocusModeTargetTransform.SetLocation(Hit.Location);

	TArray<FVector> ControlPoints;
	ControlPoints.Add(StartLocation);
	ControlPoints.Add(CenterLocation + BezierHeight);
	ControlPoints.Add(CenterLocation + BezierHeight);
	ControlPoints.Add(Hit.ImpactPoint);

	TArray<FVector> OutPoints;
	UBPFL_Turtleneck::BP_EvaluateBezier(ControlPoints, BezierCurvePoints, OutPoints);

	bValidThrowLocation = true;
	ThrowSplinePath->UpdateSpline(OutPoints, PlayerCharacter->GetActorLocation());
	
	ChangeVisualizationColor(Hit.GetActor());
	if (bOverrideVisual) {
		ThrowSplinePath->SetPlacementType(OverrideVisualize);
	}
}

bool UThrowableComponent::CheckValidPointAtLocation(FHitResult& Hit, FVector StartLocation, FVector CenterLocation, FVector EndLocation) 
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActors(GetDefaultIgnoredActors());
	Params.AddIgnoredActors(AdditionalIgnoreActors);

	const FVector Dir = UKismetMathLibrary::GetDirectionUnitVector(CenterLocation, EndLocation);
	const FVector NewEnd = EndLocation + (Dir * SnapToFloorTraceRange);

	TArray<FSplineTraceData> TraceData;
	TraceData.Add(FSplineTraceData(StartLocation, CenterLocation));
	TraceData.Add(FSplineTraceData(CenterLocation, EndLocation));
	TraceData.Add(FSplineTraceData(CenterLocation, NewEnd));

	for (FSplineTraceData Trace : TraceData) {
		if (GetWorld()->LineTraceSingleByChannel(Hit, Trace.TraceStart, Trace.TraceEnd, ECC_WorldDynamic, Params)) {
			if (bDebugTrace) {
				DrawDebugLine(GetWorld(), Trace.TraceStart, Hit.Location, FColor::Red);
			}

			return true;
		}
	}

	return false;
}

void UThrowableComponent::Throw(ACharacter* Character) 
{
	InitializeComponentValues(Character);
	const bool bDelayFinished = !GetWorld()->GetTimerManager().IsTimerActive(CatchThrowDelayHandle);

	if (!bDelayFinished ||
		!bValidThrowLocation ||
		!PlayerCharacter ||
		!ThrowTimeline ||
		!VisualizationTimeline ||
		!ThrowSplinePath) {
		return;
	}

	ThrowableState = EThrowableState::TS_Throw;
	PlayerCharacter->UnpossessInteractableActor(true);
	ThrowSplinePath->HideVisualization();
	VisualizationTimeline->Stop();

	CalculateThrowPath();
	OnThrow(PlayerCharacter);

	ThrowTimeline->SetPlayRate(ThrowPlayRate);
	ThrowTimeline->PlayFromStart();

	GetOwner()->SetActorEnableCollision(false);
	CarryableOwner = static_cast<ACarryableActor*>(GetOwner());
	if (CarryableOwner) {
		CarryableOwner->GetStaticMeshComponentCarryableMesh()->SetSimulatePhysics(true);
	}
}

void UThrowableComponent::AddRightInput(float Value) 
{
	if (FMath::IsNearlyEqual(FMath::Abs(Value), KINDA_SMALL_NUMBER)) { return; }
	const float Sensitivity = (Value * 10.0f) * HorizontalSensitivity;
	const FVector InputVector = FocusModeTargetTransform.GetRotation().GetRightVector() * Sensitivity;
	const FVector UpdatedLocation = FMath::VInterpTo(FocusModeTargetTransform.GetLocation(), FocusModeTargetTransform.GetLocation() + InputVector, GetWorld()->GetDeltaSeconds(), 90.0f);
	
	if (!PlayerCharacter) { return; }
	FocusModeTargetTransform.SetLocation(GetClampedLocation(PlayerCharacter->GetActorLocation(), UpdatedLocation));
}

void UThrowableComponent::AddForwardInput(float Value) 
{
	if (FMath::IsNearlyEqual(FMath::Abs(Value), KINDA_SMALL_NUMBER)) { return; }
	const float Sensitivity = (Value * 10.0f) * VerticalSensitivity;
	const FVector InputVector = FocusModeTargetTransform.GetRotation().GetForwardVector() * Sensitivity;
	const FVector UpdatedLocation = FMath::VInterpTo(FocusModeTargetTransform.GetLocation(), FocusModeTargetTransform.GetLocation() + InputVector, GetWorld()->GetDeltaSeconds(), 90.0f);

	if (!PlayerCharacter) { return; }
	FocusModeTargetTransform.SetLocation(GetClampedLocation(PlayerCharacter->GetActorLocation(), UpdatedLocation));
}

void UThrowableComponent::Catch(AActor* HitActor) 
{
	if (!HitActor || !OtherCharacter) { return; }

	ThrowableState = EThrowableState::TS_Catch;

	const bool bOtherCharacterValid = HitActor == OtherCharacter && !OtherCharacter->GetInteractableActor();
	if (bOtherCharacterValid && CarryableOwner) {
		OtherCharacter->PossessInteractableActor(CarryableOwner);
		OnCatch(OtherCharacter);
	}

	// Set a throw delay timer to prevent button mashing after the catch
	GetWorld()->GetTimerManager().SetTimer(CatchThrowDelayHandle, CatchThrowDelay, false, CatchThrowDelay);

	ThrowTimeline->Stop();
	ThrowableState = EThrowableState::TS_Idle;
	
	ShouldResetComponentValues();
}

FVector UThrowableComponent::GetClampedLocation(FVector CenterLocation, FVector CurrentLocation) 
{
	const float CachedZ = CurrentLocation.Z;
	CurrentLocation.Z = CenterLocation.Z;

	FVector Dir = UKismetMathLibrary::GetDirectionUnitVector(CenterLocation, CurrentLocation);
	FVector MinLoc = PlayerCharacter->GetActorLocation() + (Dir * MinRange);
	FVector MaxLoc = PlayerCharacter->GetActorLocation() + (Dir * MaxRange);

	FVector ClampedLocation = CurrentLocation;

	float Dist = FVector::Distance(PlayerCharacter->GetActorLocation(), CurrentLocation);
	if (Dist > MaxRange) { ClampedLocation = MaxLoc; }
	if (Dist < MinRange) { ClampedLocation = MinLoc; }
	ClampedLocation.Z = CachedZ;

	return ClampedLocation;
}

void UThrowableComponent::SetSpeedAccordingToDistance(const float Distance) 
{
	const float UnclampedPlayRate = DefaultPlayRate / (Distance / DistanceDivision);
	const float DistanceBasedPlayRate = FMath::Clamp(UnclampedPlayRate * SpeedMultiplier, MinPlayRate, MaxPlayRate);
	ThrowPlayRate = DistanceBasedPlayRate;
}

void UThrowableComponent::SetInvalidThrowLocation() 
{
	if (!ThrowSplinePath) { return; }
	ThrowSplinePath->HideVisualization();
	bValidThrowLocation = false;
}

void UThrowableComponent::ShouldRotateToTarget(const FVector Target) 
{
	if (!bLockCharacterMovement ||
		!PlayerCharacter ||
		!GetIsFocusModeEnabled()) {
		return;
	}
	
	FRotator LookAt = FRotationMatrix::MakeFromX(Target - PlayerCharacter->GetActorLocation()).Rotator();
	LookAt.Roll = PlayerCharacter->GetActorRotation().Roll;
	LookAt.Pitch = PlayerCharacter->GetActorRotation().Pitch;
	PlayerCharacter->SetActorRotation(LookAt);
}

void UThrowableComponent::InitializeFocusPointFromCamera() 
{
	if (!GetWorld()) { return; }

	const float Range = (MinRange + MaxRange) * 0.5f;
	const FVector NewLocation = PlayerCharacter->GetActorLocation() + (PlayerCharacter->GetActorForwardVector() * Range);
	
	const FRotator CameraRotation = PlayerCharacter->GetPlayerCameraViewportTransform().Rotator();
	const FRotator NewRotation = FRotator(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector NewScale = FVector(1.0f, 1.0f, 1.0f);

	FocusModeTargetTransform.SetLocation(NewLocation);
	FocusModeTargetTransform.SetRotation(NewRotation.Quaternion());
	FocusModeTargetTransform.SetScale3D(NewScale);
}

FVector UThrowableComponent::GetPointInValidRange(FVector CurrentLocation) 
{
	FVector ClampedMaxLocation = UBPFL_Turtleneck::ClampLocation(CurrentLocation, PlayerCharacter->GetActorLocation(), MaxRange);

	const float Distance = FVector::Distance(PlayerCharacter->GetActorLocation(), ClampedMaxLocation);
	if (Distance < MinRange) {
		return UBPFL_Turtleneck::ClampLocation(CurrentLocation, PlayerCharacter->GetActorLocation(), MinRange);
	}
	
	return CurrentLocation;
}

void UThrowableComponent::InitializeComponentValues(ACharacter* Character) 
{
	if (!GetOwner() ||
		!Character ||
		bInitialized) {
		return;
	}

	ABasePlayerCharacter* BasePlayerCharacter = Cast<ABasePlayerCharacter>(Character);

	const bool bValidCharacters = BasePlayerCharacter && BasePlayerCharacter->GetOtherPlayerCharacter();
	if (!bValidCharacters || !BasePlayerCharacter->GetThrowVisualization()) { return; }

	PlayerCharacter = BasePlayerCharacter;
	OtherCharacter = PlayerCharacter->GetOtherPlayerCharacter();
	ThrowSplinePath = PlayerCharacter->GetThrowVisualization();

	bInitialized = true;
}

void UThrowableComponent::ShouldResetComponentValues() 
{
	const bool bThrowing = ThrowableState == EThrowableState::TS_Throw;
	const bool bCatching = ThrowableState == EThrowableState::TS_Catch;
	if (bThrowing || bCatching) { return; }

	GetOwner()->SetActorEnableCollision(true);

	if (ThrowTimeline) {
		ThrowTimeline->Stop();
	}

	if (ThrowSplinePath) {
		ThrowSplinePath->ClearSpline();
		ThrowSplinePath->HideVisualization();
		ThrowSplinePath = nullptr;
	}

	ThrowableState = EThrowableState::TS_Idle;
	PlayerCharacter = nullptr;
	OtherCharacter = nullptr;
	bInitialized = false;
}

void UThrowableComponent::TraceForCollision() 
{
	if (!GetWorld() ||
		!GetOwner() ||
		!OtherCharacter ||
		!PlayerCharacter ||
		!PlayerCharacter->GetRopeManager()) {
		return;
	}

	FVector Start = GetOwner()->GetActorLocation() + (GetOwner()->GetActorUpVector() * 2.0f);
	FVector End = GetOwner()->GetActorLocation() + (GetOwner()->GetActorUpVector() * -2.0f);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActors(GetDefaultIgnoredActors());
	QueryParams.AddIgnoredActors(AdditionalIgnoreActors);

	// Also add the current surface the player is standing on
	if (bIgnorePlayerSurface) {
		FVector Start = PlayerCharacter->GetActorLocation();
		FVector End = PlayerCharacter->GetActorLocation() + (PlayerCharacter->GetActorUpVector() * -1) * 100;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldDynamic, QueryParams)) {
			QueryParams.AddIgnoredActor(Hit.GetActor());
		}
	}

	FHitResult Hit;
	FCollisionShape SweepShape = GetCollisionShape(Start, End);
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_WorldDynamic, SweepShape, QueryParams)) {
		Catch(Hit.GetActor());
	}
}

FCollisionShape UThrowableComponent::GetCollisionShape(FVector Start, FVector End) 
{
	FCollisionShape Shape;

	switch (TraceCollisionShape) 
	{
	case EThrowableTraceShape::TTS_SPHERE:
		Shape = FCollisionShape::MakeSphere(TraceRadius);
		if (bDebugTrace) {
			UBPFL_Turtleneck::DrawDebugSweptSphere(GetWorld(), Start, End, TraceRadius, FColor::Red, false, -1.0f, 0);
		}
		break;

	case EThrowableTraceShape::TTS_BOX:
		Shape = FCollisionShape::MakeBox(TraceBoxExtents);
		if (bDebugTrace) {
			UBPFL_Turtleneck::DrawDebugSweptBox(GetWorld(), Start, End, GetOwner()->GetActorRotation(), TraceBoxExtents, FColor::Red, false, -1.0f, 0);
		}
		break;
	}

	return Shape;
}

void UThrowableComponent::ChangeVisualizationColor(AActor* HitActor) 
{
	if (!HitActor || !OtherCharacter) { return; }

	ThrowSplinePath->SetPlacementType(EVisualizeType::VT_VALIDPLACEMENT);

	bool bGoodPlacement = GoodActors.Contains(HitActor) || GoodClasses.Contains(HitActor->GetClass()) && !BadActors.Contains(HitActor);
	if (bGoodPlacement || HitActor == OtherCharacter) {
		ThrowSplinePath->SetPlacementType(EVisualizeType::VT_GOODPLACEMENT);
		return;
	}

	bool bBadPlacement = BadActors.Contains(HitActor) || BadClasses.Contains(HitActor->GetClass()) && !GoodActors.Contains(HitActor);
	if (bBadPlacement) {
		ThrowSplinePath->SetPlacementType(EVisualizeType::VT_INVALIDPLACEMENT);
		return;
	}
}

TArray<TWeakObjectPtr<AActor>> UThrowableComponent::GetDefaultIgnoredActors() 
{	
	TArray<TWeakObjectPtr<AActor>> IgnoredActors = TArray<TWeakObjectPtr<AActor>>();
	if (!PlayerCharacter || !PlayerCharacter->GetRopeManager()) { return IgnoredActors; }
	
	IgnoredActors.Add(PlayerCharacter);
	IgnoredActors.Add(GetOwner());

	TArray<ARopeSegment*> RopeSegments = PlayerCharacter->GetRopeManager()->Particles;
	if (RopeSegments.Num() == 0) { return IgnoredActors; }

	for (ARopeSegment* RopeSegment : RopeSegments) {
		IgnoredActors.Add(RopeSegment);
	}

	return IgnoredActors;
}

void UThrowableComponent::VisualizeInterpolation(float DeltaTime) 
{
	CalculateThrowPath();
	
	if (!bValidThrowLocation) { return; }
	ThrowSplinePath->VisualizeSpline();
}

void UThrowableComponent::ThrowInterpolation(float DeltaTime) 
{
	if (!PlayerCharacter || !ThrowSplinePath) { return; }

	if (bAlwaysHitOtherPlayer) {
		CalculateThrowPath();
	}

	FVector Location = ThrowSplinePath->GetRootSplineComponent()->GetLocationAtTime(DeltaTime, ESplineCoordinateSpace::World);
	GetOwner()->SetActorLocation(Location);

	TraceForCollision();
}

void UThrowableComponent::ThrowFinished() 
{
	if (!ThrowTimeline) { return; }

	GetOwner()->SetActorEnableCollision(true);
	ThrowTimeline->Stop();

	ThrowableState = EThrowableState::TS_Idle;
	ShouldResetComponentValues();
}

//////////////////////////////////////////////////////////////////////////
// Blueprint Events

void UThrowableComponent::OnEnterAimMode_Implementation(ABasePlayerCharacter* PlayerCharacter) { }

void UThrowableComponent::OnExitAimMode_Implementation(ABasePlayerCharacter* PlayerCharacter) { }

void UThrowableComponent::OnThrow_Implementation(ABasePlayerCharacter* ThrowingCharacter) { }

void UThrowableComponent::OnCatch_Implementation(ABasePlayerCharacter* CatchingCharacter) { }

//////////////////////////////////////////////////////////////////////////
// Editor Only

#if WITH_EDITOR
void UThrowableComponent::OnRegister() {
	Super::OnRegister();
	if (bWarningShown || !GetOwner()) { return; }

	ACarryableActor* Carryable = Cast<ACarryableActor>(GetOwner());
	if (Carryable) { return; }

	bWarningShown = true;
	UBPFL_Debug::ShowMessageBox(TEXT("Throwable Components should only be attached to Carryable Actors!"), TEXT("Error: Invalid Owner"));
}

void UThrowableComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!GetWorld() || !GetOwner()) { return; }

	// Visualization of the TraceShape for easy tweaking
	FVector Start = GetOwner()->GetActorLocation() + (GetOwner()->GetActorUpVector() * 2.0f);
	FVector End = GetOwner()->GetActorLocation() + (GetOwner()->GetActorUpVector() * -2.0f);

	FlushPersistentDebugLines(GetWorld());

	// Add additional shapes if necessary
	switch (TraceCollisionShape) {
	case EThrowableTraceShape::TTS_SPHERE: 
		UBPFL_Turtleneck::DrawDebugSweptSphere(GetWorld(), Start, End, TraceRadius, FColor::Green, false, 40.0f, 0);
		break;
	case EThrowableTraceShape::TTS_BOX:
		UBPFL_Turtleneck::DrawDebugSweptBox(GetWorld(), Start, End, GetOwner()->GetActorRotation(), TraceBoxExtents, FColor::Green, false, 40.0f, 0);
		break;
	}
}
#endif
