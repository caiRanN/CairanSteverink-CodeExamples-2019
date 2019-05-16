// Copyright 2018 Disillusion Games. All Rights Reserved.

#include "BaseTrigger.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/ArrowComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "BasePlayerCharacter.h"
#include "DeusSaveGameComponent.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is a base trigger utility that was written for The Seminary of Sight.
* The design team requested a more visual trigger that they could easily re-use and blueprint.
* The base trigger adds plenty of options that I thought that would be useful like enter directions and max trigger enters
* The trigger uses an editor only cube mesh with a low opacity material for a better visual representation in the editor.
* The trigger material its color could also be changed to easily differentiate between triggers.
*/



ABaseTrigger::ABaseTrigger() 
{
	RootTriggerBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("RootBoxComponent"));
	if (RootTriggerBoxComponent)
	{
		RootComponent = RootTriggerBoxComponent;
		RootTriggerBoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		RootTriggerBoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	VisualStaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualStaticMeshComponent"));
	if (VisualStaticMeshComponent) 
	{
		VisualStaticMeshComponent->SetupAttachment(RootComponent);
		VisualStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisualStaticMeshComponent->bVisible = true;
		VisualStaticMeshComponent->SetHiddenInGame(true);

		if (MaterialInstance) 
		{
			DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialInstance, this);
			DynamicMaterial->SetVectorParameterValue("TriggerColor", TriggerColor);
			VisualStaticMeshComponent->SetMaterial(0, DynamicMaterial);
		}
	}

	IconBillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("IconBillboardComponent"));
	if (IconBillboardComponent) 
	{
		IconBillboardComponent->SetupAttachment(VisualStaticMeshComponent);
		IconBillboardComponent->bVisible = true;
		IconBillboardComponent->SetHiddenInGame(true);
	}

	ForwardDirectionArrowComponent = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardDirectionArrowComponent"));
	if (ForwardDirectionArrowComponent) 
	{
		ForwardDirectionArrowComponent->SetupAttachment(RootComponent);
		ForwardDirectionArrowComponent->SetWorldScale3D(FVector(0.2f, 0.2f, 0.2f));
		
#if UE_EDITOR
		ForwardDirectionArrowComponent->bTreatAsASprite = true;
		ForwardDirectionArrowComponent->ArrowColor = FColor(150, 200, 255);
		ForwardDirectionArrowComponent->ArrowSize = 1.0f;
		ForwardDirectionArrowComponent->bIsScreenSizeScaled = true;
		ForwardDirectionArrowComponent->ArrowColor = FColor(150, 200, 255);
		UpdateTriggerDirectionArrow();
#endif
	}

	SaveGameComponent = CreateDefaultSubobject<UDeusSaveGameComponent>(TEXT("SaveGameComponent"));
	if (SaveGameComponent) 
	{
		AddOwnedComponent(SaveGameComponent);
	}
}

void ABaseTrigger::BeginPlay() 
{
	Super::BeginPlay();

	if (!VisualStaticMeshComponent || !IconBillboardComponent) { return; }
	VisualStaticMeshComponent->SetHiddenInGame(!bDebug);
	IconBillboardComponent->SetHiddenInGame(!bDebug);
}

void ABaseTrigger::OnConstruction(const FTransform& Transform) 
{
	Super::OnConstruction(Transform);

	BindDelegates();
}

void ABaseTrigger::BindDelegates() 
{
	if (!RootTriggerBoxComponent) { return; }

	RootTriggerBoxComponent->OnComponentBeginOverlap.Clear();
	RootTriggerBoxComponent->OnComponentEndOverlap.Clear();
	RootTriggerBoxComponent->OnComponentBeginOverlap.AddDynamic(this, &ABaseTrigger::OnOverlapBegin);
	RootTriggerBoxComponent->OnComponentEndOverlap.AddDynamic(this, &ABaseTrigger::OnOverlapEnd);
}

void ABaseTrigger::HandleTriggerEnter(ABasePlayerCharacter* PlayerCharacter)
{
	ActivatableActorData.ActivateActors(this);
	ActivatableActorData.DeactivateActors(this);

	TriggeredAmount++;

	OnTriggerEnter(PlayerCharacter);
}

void ABaseTrigger::HandleTriggerExit(ABasePlayerCharacter* PlayerCharacter) 
{
	OnTriggerExit(PlayerCharacter);
}

bool ABaseTrigger::IsEnterDirectionValid(const FHitResult& SweepResult) 
{
	if (!bUseEnterDirection) 
	{ 
		return true; 
	}

	const FVector Direction = GetDirectionFromTriggerDirection();
	if (IsCorrectEnterDirection(Direction, SweepResult.ImpactNormal)) 
	{
		return true;
	}

	return false;
}

bool ABaseTrigger::IsCorrectEnterDirection(FVector DirectionVector, const FVector_NetQuantizeNormal ImpactNormal) 
{
	// Compare the impact normal to the direction vector
	// This gives us a good enough result for our use cases
	// to check if a player entered from the correct location
	const float Dot = FVector::DotProduct(DirectionVector, ImpactNormal);
	if (FMath::IsNearlyEqual(Dot, -1.0f, KINDA_SMALL_NUMBER)) 
	{
		return true;
	}

	return false;
}

FVector ABaseTrigger::GetDirectionFromTriggerDirection() const 
{	
	FVector Direction;

	switch (TriggerDirection) 
	{
	case ETriggerDirection::TD_FRONT:
		Direction = GetActorForwardVector();
		break;
	case ETriggerDirection::TD_BACK:
		Direction = GetActorForwardVector() * -1;
		break;
	case ETriggerDirection::TD_LEFT:
		Direction = GetActorRightVector() * -1;
		break;
	case ETriggerDirection::TD_RIGHT:
		Direction = GetActorRightVector();
		break;
	case ETriggerDirection::TD_TOP:
		Direction = GetActorUpVector();
		break;
	}

	return Direction;
}

void ABaseTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) 
{	
	if (!bEnabled) 
	{ 
		return; 
	}

	ABasePlayerCharacter* Character = Cast<ABasePlayerCharacter>(OtherActor);
	const bool bTriggerCountExceeded = bUseMaxTriggerCount && CurrentTriggerCount >= MaxTriggerCount;

	if (!Character ||
		!IsEnterDirectionValid(SweepResult) ||
		bTriggerCountExceeded) 
	{
		return;
	}

	CurrentTriggerCount++;
	HandleTriggerEnter(Character);
}

void ABaseTrigger::OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) 
{	
	if (!bEnabled) 
	{ 
		return; 
	}

	ABasePlayerCharacter* Character = Cast<ABasePlayerCharacter>(OtherActor);
	const bool bTriggerCountExceeded = bUseMaxTriggerCount && CurrentTriggerCount >= MaxTriggerCount;

	if (!Character || bTriggerCountExceeded) 
	{
		return;
	}

	HandleTriggerExit(Character);
}

void ABaseTrigger::OnActivateActor_Implementation(AActor* Caller) 
{
	bEnabled = true;
}

void ABaseTrigger::OnDeactivateActor_Implementation(AActor* Caller) 
{
	bEnabled = false;
}

//////////////////////////////////////////////////////////////////////////
// Blueprint Events

void ABaseTrigger::OnTriggerEnter_Implementation(ABasePlayerCharacter* PlayerCharacter) { }

void ABaseTrigger::OnTriggerExit_Implementation(ABasePlayerCharacter* PlayerCharacter) { }

//////////////////////////////////////////////////////////////////////////
// Editor Only

#if UE_EDITOR
void ABaseTrigger::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateTriggerDirectionArrow();

	if (GetWorld()) 
	{
		ActivatableActorData.DrawDebug(GetWorld(), this, FColor::Green, FColor::Red, 60.0f);
	}

	if (!MaterialInstance) { return; }
	DynamicMaterial = UMaterialInstanceDynamic::Create(MaterialInstance, this);
	DynamicMaterial->SetVectorParameterValue("TriggerColor", TriggerColor);
	VisualStaticMeshComponent->SetMaterial(0, DynamicMaterial);
}

void ABaseTrigger::UpdateTriggerDirectionArrow() 
{
	if (!ForwardDirectionArrowComponent) { return; }

	if (!bUseEnterDirection) 
	{
		ForwardDirectionArrowComponent->SetVisibility(false);
		return;
	}
	
	ForwardDirectionArrowComponent->SetVisibility(true);
	const FVector Direction = GetDirectionFromTriggerDirection();
	const FRotator Rotation = GetActorTransform().InverseTransformRotation(Direction.Rotation().Quaternion()).Rotator();
	ForwardDirectionArrowComponent->SetRelativeRotation(Rotation);
}
#endif

