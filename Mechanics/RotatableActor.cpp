// Fill out your copyright notice in the Description page of Project Settings.

#include "bound.h"
#include "RotatableActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "Actors/Mechanics/InteractableActor/InteractableActor.h"
#include "Actors/Mechanics/InteractableActor/Rotatable/RotatableActorInput.h"
#include "Actors/Mechanics/InteractableActor/InteractableActor_InputComponent.h"
#include "Utilities/BPFL_Debug.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is the Rotatable Actor Mechanic written for RITE of ILK. It's a co-op mechanic where a single (or both) characters
* can interact with the object to rotate it in different directions. 
* They both need to line up their controller thumbstick with their character to rotate the object.
* Optionaly, a rotatable can be affected by other actors and can rotate back into its original position when the players stop interacting.
*/



ARotatableActor::ARotatableActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RootSceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootSceneComponent"));
	if (RootSceneComponent) 
	{
		RootComponent = RootSceneComponent;
	}

	RotatableSceneComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RotatableSceneComponent"));
	if (RotatableSceneComponent) 
	{
		RotatableSceneComponent->SetupAttachment(RootComponent);
	}

	ForwardArrowComponent = ObjectInitializer.CreateDefaultSubobject<UArrowComponent>(this, TEXT("ForwardArrowComponent"));
	if (ForwardArrowComponent) 
	{
		ForwardArrowComponent->SetupAttachment(RotatableSceneComponent);
	}

	InteractableLeft = ObjectInitializer.CreateDefaultSubobject<UChildActorComponent>(this, TEXT("InteractableLeft"));
	if (InteractableLeft) 
	{
		InteractableLeft->SetupAttachment(RotatableSceneComponent);
	}

	InteractableRight = ObjectInitializer.CreateDefaultSubobject<UChildActorComponent>(this, TEXT("InteractableRight"));
	if (InteractableRight) 
	{
		InteractableRight->SetupAttachment(RotatableSceneComponent);
	}
}

void ARotatableActor::RegisterCharacter(ACharacter* Character)
{
	if (!Character) { return; }
	InteractingCharacters.AddUnique(Character);
	if (InteractingCharacters.Num() == 0) { return; }
	OnTickEventChanged(true);
}

void ARotatableActor::UnRegisterCharacter(ACharacter* Character)
{
	if (!Character) { return; }
	if (!InteractingCharacters.Contains(Character)) { return; }
	
	InteractingCharacters.Remove(Character);
	
	if (InteractingCharacters.Num() != 0) { return; }
	if (bRotateBack) { return; } /** Execute rotate back logic first*/
	
	OnTickEventChanged(false);
}

void ARotatableActor::OnTickEventChanged_Implementation(bool bEnable) 
{
	SetActorTickEnabled(bEnable);
}

bool ARotatableActor::GetIsBeingOperated()
{
	if (InteractingCharacters.Num() > 0) { return true; }
	return false;
}

void ARotatableActor::SetCanRotateBack(const bool bNewValue)
{
	bRotateBack = bNewValue;
	if (!bRotateBack) { return; }
	OnTickEventChanged(true);
}

FVector ARotatableActor::PositionFromDirectionDistance(FVector Location, FVector Direction, float Distance)
{
	return Location + RotatableSceneComponent->GetRelativeTransform().GetRotation() * (Direction * Distance);
}

void ARotatableActor::Rotate()
{
	if (!bCanRotate) { return; }
	if (GetCanRotateForward()) 
	{ 
		CalculateRotation(PushSpeed); 
		return; 
	}

	if (GetCanRotateBackward()) 
	{ 
		CalculateRotation(PullSpeed); 
		return; 
	}

	if (GetIsBeingOperated()) 
	{ 
		CalculateRotation(0.f); 
		return;
	}
	
	if (bRotateBack && MaxNumberOfRotations != 0)
	{
		if (TotalRotationAmount > 0)
		{
			CalculateRotation(ReturnSpeed);
			return;
		}

		OnTickEventChanged(false);
		return;
	}
	
	CalculateRotation(0.f);
}

void ARotatableActor::CalculateRotation(float Speed)
{
	DesiredSpeed = Speed * (DeltaTimeInSeconds * 100);
	DesiredSpeed *= SpeedMultiplier;

	float DirectionValue = 1.f;
	if (bFlipRotationDirection) { DirectionValue = -1.f; }
	
	DesiredSpeed *= DirectionValue;
	ActualSpeed = FMath::FInterpTo(ActualSpeed, DesiredSpeed, DeltaTimeInSeconds, 3.f);

	float RotationValue = bYawClamped ? FMath::Clamp(ActualSpeed + CurrentRotation, YawClamp.X, YawClamp.Y) : ActualSpeed + CurrentRotation;
	CurrentRotation = RotationValue;

	OnRotate(ActualSpeed, CurrentRotation); /** Notify BP that we are rotating */

	if (bBlocked)
	{
		CurrentRotation = CurrentRotation -= ActualSpeed;
		ActualSpeed = 0;
		return;
	}

	TotalRotationAmount += ActualSpeed;
	if (MaxNumberOfRotations != 0 && TotalRotationAmount >= MaxRotation)
	{
		ActualSpeed = 0;
		TotalRotationAmount = MaxRotation;
		CurrentRotation = MaxRotation;
	}

	if (MaxNumberOfRotations != 0 && TotalRotationAmount <= 0)
	{
		ActualSpeed = 0;
		TotalRotationAmount = 0;
		CurrentRotation = 0;
	}

	FRotator NewRotation = UKismetMathLibrary::ComposeRotators(FRotator(0, CurrentRotation, 0), StartWorldRotation);
	if (!RotatableSceneComponent) { return; }
	RotatableSceneComponent->SetRelativeRotation(NewRotation);
}

bool ARotatableActor::GetCanRotateForward()
{
	TArray<ARotatableActorInput*> RotatableInputs = GetRotatableActorInputs();
	int Counter = 0;

	for (ARotatableActorInput* RotatableInput : RotatableInputs)
	{
		if (!RotatableInput) { continue; }
		if (RotatableInput->GetLeftStickInputDirectionForward() > PushPullDot)
		{
			Counter++;
		}
	}

	// If there are enough players interacting, and both are giving input, rotate.
	if (RotatableInputs.Num() > 0 && RotatableInputs.Num() == Counter) 
	{ 
		return true; 
	}
	
	return false;
}

bool ARotatableActor::GetCanRotateBackward()
{
	TArray<ARotatableActorInput*> RotatableInputs = GetRotatableActorInputs();
	int Counter = 0;

	for (ARotatableActorInput* RotatableInput : RotatableInputs)
	{
		if (!RotatableInput) { continue; }
		bool DotRequirement = RotatableInput->GetLeftStickInputDirectionForward() < (PushPullDot * -1.f);
		bool ValueRequirement = RotatableInput->GetLeftStickInputDirectionForward() != -2.f;
		if (DotRequirement && ValueRequirement) { Counter++; }
	}

	// If there are enough players interacting, and both are giving input, rotate.
	if (RotatableInputs.Num() > 0 && RotatableInputs.Num() == Counter) 
	{ 
		return true; 
	}

	return false;
}

TArray<ARotatableActorInput*> ARotatableActor::GetRotatableActorInputs()
{
	TArray<ARotatableActorInput*> RotatableActorInputs;

	ARotatableActorInput* LeftInput = GetRotatableInputFromComponent(InteractableLeft);
	ARotatableActorInput* RightInput = GetRotatableInputFromComponent(InteractableRight);

	if (LeftInput) 
	{ 
		RotatableActorInputs.AddUnique(LeftInput); 
	}

	if (RightInput) 
	{ 
		RotatableActorInputs.AddUnique(RightInput); 
	}

	return RotatableActorInputs;
}

ARotatableActorInput* ARotatableActor::GetRotatableInputFromComponent(UChildActorComponent* ChildActor)
{
	if (!ChildActor) { return nullptr; }
	
	ARotatableActorInput* RotatableInput = Cast<ARotatableActorInput>(ChildActor->GetChildActor());
	return RotatableInput;
}

void ARotatableActor::BindDelegates()
{
	BindRotatableInputDelegates(InteractableLeft);
	BindRotatableInputDelegates(InteractableRight);
}

void ARotatableActor::BindRotatableInputDelegates(UChildActorComponent* ChildActor)
{
	if (!ChildActor) { return; }
	ARotatableActorInput* RotatableInput = GetRotatableInputFromComponent(ChildActor);
	if (!RotatableInput) { return; }
	if (!RotatableInput->IsA(ARotatableActorInput::StaticClass())) { return; }

	RotatableInput->OnRegisterCharacter.Clear();
	RotatableInput->OnUnRegisterCharacter.Clear();
	RotatableInput->OnRegisterCharacter.AddDynamic(this, &ARotatableActor::RegisterCharacter);
	RotatableInput->OnUnRegisterCharacter.AddDynamic(this, &ARotatableActor::UnRegisterCharacter);
}

void ARotatableActor::DrawDebugFeatures()
{
	if (!bDebug) { return; }

	if (bYawClamped)
	{
		UKismetSystemLibrary::FlushPersistentDebugLines(GetWorld());
		UBPFL_Debug::DebugVisualizeAngle(GetWorld(), GetActorTransform(), 100.f, YawClamp.X, YawClamp.Y, ERotateAxis::RAE_Yaw);
	}
}

void ARotatableActor::BeginPlay()
{
	Super::BeginPlay();

	if (!this->Implements<UICoreActorInterface>()) { return; }
	IICoreActorInterface::Execute_OnCoreActorReset(this, this);

	BindDelegates();
	ShouldDisplayHandles();
}

void ARotatableActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DeltaTimeInSeconds = DeltaSeconds;
	
	Rotate();
	if (InteractingCharacters.Num() == 0) { return; }
	for (ACharacter* Character : InteractingCharacters) 
	{
		OnRegisteredCharacterTick(Character);
	}
}

void ARotatableActor::ToggleHandleVisibility(UChildActorComponent* ChildActor, bool bActive)
{
	if (!ChildActor) { return; }
	if (bActive) { return; }
	ChildActor->DestroyComponent();
}

void ARotatableActor::ShouldDisplayHandles()
{
	switch (Handles)
	{
	case ERotationHandlesEnum::RHE_BOTH:
		ToggleHandleVisibility(InteractableLeft, true);
		ToggleHandleVisibility(InteractableRight, true);
		OnDestroyHandle(ERotationHandlesEnum::RHE_NONE);
		break;
	case ERotationHandlesEnum::RHE_LEFT:
		ToggleHandleVisibility(InteractableLeft, true);
		ToggleHandleVisibility(InteractableRight, false);
		OnDestroyHandle(ERotationHandlesEnum::RHE_RIGHT);
		break;
	case ERotationHandlesEnum::RHE_RIGHT:
		ToggleHandleVisibility(InteractableLeft, false);
		ToggleHandleVisibility(InteractableRight, true);
		OnDestroyHandle(ERotationHandlesEnum::RHE_LEFT);
		break;
	case ERotationHandlesEnum::RHE_NONE:
		ToggleHandleVisibility(InteractableLeft, false);
		ToggleHandleVisibility(InteractableRight, false);
		OnDestroyHandle(ERotationHandlesEnum::RHE_BOTH);
		break;
	default:
		ToggleHandleVisibility(InteractableLeft, true);
		ToggleHandleVisibility(InteractableRight, true);
		OnDestroyHandle(ERotationHandlesEnum::RHE_NONE);
		break;
	}
}

void ARotatableActor::OnRotate_Implementation(float Speed, float Rotation) 
{
	if (bPlayerCanBlock) 
	{ 
		BlockOnCharacterCollision(); 
	}
}

void ARotatableActor::BlockOnCharacterCollision()
{
	if (GetActualSpeed() == 0) { return; }

	FVector StartPosition = GetActorLocation();
	FVector EndPosition = GetActorLocation() + (RotatableSceneComponent->GetForwardVector() * 200);

	FVector DirectionOffset = RotatableSceneComponent->GetForwardVector();

	TArray<FHitResult> HitResults;
	HitResults.Add(BoxTraceForCharacter(GetActorLocation(), GetActorLocation() + (RotatableSceneComponent->GetRightVector() * 100), DirectionOffset * -1, 25.f));
	HitResults.Add(BoxTraceForCharacter(GetActorLocation(), GetActorLocation() + (RotatableSceneComponent->GetRightVector() * -100), DirectionOffset, 25.f));

	bool bLocalBlocked = false;
	for (FHitResult Result : HitResults)
	{
		if (!Result.GetActor()) { continue; }
		if (!Cast<ACharacter>(Result.GetActor())) { continue; }
		bLocalBlocked = true;
		break;
	}

	SetIsBlocked(bLocalBlocked);
}

FHitResult ARotatableActor::BoxTraceForCharacter(FVector StartPosition, FVector EndPosition, FVector DirectionOffset /*= FVector(0,0,0)*/, float OffsetMultiplier /*= 0.f*/)
{
	if (GetActualSpeed() > 0)
	{
		StartPosition += (DirectionOffset * OffsetMultiplier);
		EndPosition += (DirectionOffset * OffsetMultiplier);
	}
	else if (GetActualSpeed() < 0)
	{
		StartPosition += ((DirectionOffset * -1) * OffsetMultiplier);
		EndPosition += ((DirectionOffset * -1) * OffsetMultiplier);
	}

	FVector HalfSize = FVector(10.f, 40.f, 10.f);

	EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::None;
	if (bDebug) 
	{ 
		DebugTrace = EDrawDebugTrace::ForOneFrame; 
	}

	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

	TArray<AActor*> ActorsToIgnore;
	for (ACharacter* RegisteredActors : GetInteractingCharacters())
	{
		if (!RegisteredActors) { continue; }
		ActorsToIgnore.Add(RegisteredActors);
	}

	FHitResult Hit;
	UKismetSystemLibrary::BoxTraceSingleForObjects(
		GetWorld(),
		StartPosition,
		EndPosition,
		HalfSize,
		RotatableSceneComponent->RelativeRotation,
		ObjectTypes,
		false,
		ActorsToIgnore,
		DebugTrace,
		Hit,
		true
	);

	return Hit;
}

void ARotatableActor::OnDestroyHandle_Implementation(ERotationHandlesEnum::Type DestroyedHandle)
{

}

///////////////////////////////////////////
// Editor Only

#if UE_EDITOR
void ARotatableActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	DrawDebugFeatures();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void ARotatableActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	DrawDebugFeatures();
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
}

void ARotatableActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	DrawDebugFeatures();
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
}
#endif


///////////////////////////////////////////
// Blueprint Event CPP Implementation

void ARotatableActor::OnCoreActorReset_Implementation(AActor* Caller)
{
	ActualSpeed = 0.f;
	DesiredSpeed = 0.f;
	CurrentRotation = 0.f;
	MaxRotation = MaxNumberOfRotations * 360;
}

void ARotatableActor::OnRightThumbstickForward_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnRightThumbstickRight_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnLeftThumbstickForward_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnLeftThumbstickRight_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnFaceButtonRightPressed_Implementation(ACharacter * Character) { }

void ARotatableActor::OnFaceButtonRightReleased_Implementation(ACharacter * Character) { }

void ARotatableActor::OnFaceButtonBottomPressed_Implementation(ACharacter * Character) { }

void ARotatableActor::OnFaceButtonBottomReleased_Implementation(ACharacter * Character) { }

void ARotatableActor::OnFaceButtonLeftPressed_Implementation(ACharacter * Character) { }

void ARotatableActor::OnFaceButtonLeftReleased_Implementation(ACharacter * Character) { }

void ARotatableActor::OnRightTriggerPressed_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnRightTriggerReleased_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnLeftTriggerPressed_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnLeftTriggerReleased_Implementation(ACharacter * Character, float Value) { }

void ARotatableActor::OnRightBumperPressed_Implementation(ACharacter * Character) { }

void ARotatableActor::OnRightBumperReleased_Implementation(ACharacter * Character) { }

void ARotatableActor::OnLeftBumperPressed_Implementation(ACharacter * Character) { }

void ARotatableActor::OnLeftBumperReleased_Implementation(ACharacter * Character) { }
