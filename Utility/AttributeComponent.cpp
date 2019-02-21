// Copyright 2018 Disillusion Games. All Rights Reserved.

#include "AttributeComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ForceFeedbackEffect.h"
#include "Camera/CameraShake.h"
#include "Kismet/GameplayStatics.h"
#include "DamageType_Drowning.h"

////////////////////////////////////////////
// Code Example Description

/*
* This is a Attribute Component written for the seminary of sight.
* The goal of this component was for the design team to easily tweak our character stats.
* Optionaly you can trigger camera and controller shakes to make breath reduction and drowning effects more intense.
* I also added some debug functionality so the designers could easily see how long it would take for the player to regain and diminish stats.
*/


UAttributeComponent::UAttributeComponent() 
	: bDamageCausing(true)
	, bEntryDamage(false)
	, bCanTakeDamage(true) 
	, bHealthRecovery(true) 
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;

#if UE_EDITOR
	SecondsTillFullHealth = DefaultHealth / (HealthRecoveryPerSec * HealthRecoveryInterval);
	SecondsTillFullBreath = DefaultBreath / (BreathRecoveryPerSec * BreathRecoveryInterval);
	SecondsTillOutOfBreath = DefaultBreath / (BreathDecayPerSec * BreathDecayInterval);
	SecondsTillDeath = DefaultHealth / (DamagePerSec * DamageInterval);
#endif
}

void UAttributeComponent::BeginPlay() 
{
	Super::BeginPlay();

	AActor* OwningActor = GetOwner();
	if (OwningActor) 
	{
		OwningActor->OnTakeAnyDamage.AddDynamic(this, &UAttributeComponent::HandleTakeAnyDamage);
	}

	PlayerController = GetPlayerController();
	Health = DefaultHealth;
}

void UAttributeComponent::DrownTimer() 
{
	if (!bDamageCausing) 
	{
		return;
	}

	if (Breath > 0) 
	{
		// Clear the timer if we regained breath
		GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_DrownTimer);
		return;
	}

	CauseDrownDamage();
}

void UAttributeComponent::CauseDrownDamage() 
{
	if (DamagePerSec <= 0.0f) { return; }
	TSubclassOf<UDamageType> DmgTypeClass = DrowningDamageType ? *DrowningDamageType : UDamageType::StaticClass();
	GetOwner()->TakeDamage(DamagePerSec * DamageInterval, FDamageEvent(DmgTypeClass), GetOwner()->GetInstigatorController(), GetOwner());
}

void UAttributeComponent::CauseBreathDecay() 
{
	if (BreathDecayPerSec <= 0.0f) { return; }
	HandleBreathChanged(GetOwner(), BreathDecayPerSec * BreathDecayInterval, GetOwner()->GetInstigatorController(), GetOwner());
}

void UAttributeComponent::CauseHealthRecovery() 
{
	if (HealthRecoveryPerSec <= 0.0f || Health >= DefaultHealth) 
	{
		if (GetOwner() && GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_HealthRecoveryTimer)) 
		{
			GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_HealthRecoveryTimer);
		}

		return; 
	}
	
	Heal(HealthRecoveryPerSec * HealthRecoveryInterval);
}

void UAttributeComponent::CauseBreathRecovery() 
{
	if (BreathRecoveryPerSec <= 0.0f || Breath >= DefaultBreath) 
	{
		if (GetOwner() && GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_BreathRecoveryTimer)) 
		{
			GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_BreathRecoveryTimer);
		}

		return;
	}
	
	RecoverBreath(BreathRecoveryPerSec * BreathRecoveryInterval);
}

void UAttributeComponent::StartBreathRecovery() 
{
	if (BreathRecoveryPerSec <= 0.0f || 
		!GetOwner() ||
		GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_BreathRecoveryTimer)) 
	{ 
		return; 
	}

	// if the player is taking damage from drowning, cancel the damage
	GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_DrownTimer);
	StopBreathDecay();

	if (GetPlayerController()) 
	{
		GetPlayerController()->ClientStopCameraShake(DrowningCameraShake, true);
		GetPlayerController()->ClientStopForceFeedback(DrowningControllerShake, "DrowningDamage");
	}

	GetOwner()->GetWorldTimerManager().SetTimer(TimerHandle_BreathRecoveryTimer, this, &UAttributeComponent::CauseBreathRecovery, 0.01f, true, bHealthRecovery);
}

void UAttributeComponent::StopBreathRecovery() 
{
	if (GetOwner()) 
	{
		GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_BreathRecoveryTimer);
	}
}

void UAttributeComponent::StartBreathDecay() 
{
	if (GetOwner() && !GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_BreathDecayTimer)) 
	{
		StopBreathRecovery();
		GetOwner()->GetWorldTimerManager().SetTimer(TimerHandle_BreathDecayTimer, this, &UAttributeComponent::CauseBreathDecay, BreathDecayInterval, true, BreathDecayFirstInDelay);
	}
}

void UAttributeComponent::StopBreathDecay() 
{
	if (GetOwner()) 
	{
		GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_BreathDecayTimer);
	}
}

APlayerController* UAttributeComponent::GetPlayerController() const 
{
	return PlayerController ? PlayerController : UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

void UAttributeComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser) 
{
	if (Damage <= 0.0f || bIsDead || !bCanTakeDamage) 
	{
		return;
	}

	Health = FMath::Clamp(Health - Damage, 0.0f, DefaultHealth);
	bIsDead = Health <= 0.0f;

#if UE_EDITOR
	if (bDebugAttributes) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Reducing Stat Health: %f"), Health);
	}
#endif

	OnHealthChanged.Broadcast(this, Health, Damage, DamageType, InstigatedBy, DamageCauser);

	if (GetPlayerController()) 
	{
		// If we are drowning, we want to play a different effect
		if (Cast<UDamageType_Drowning>(DamageType)) 
		{
			GetPlayerController()->ClientPlayForceFeedback(DrowningControllerShake, true, true, "DrowningDamage");
			GetPlayerController()->ForceFeedbackScale = 1.8f - (Health / DefaultHealth);
			GetPlayerController()->ClientPlayCameraShake(DrowningCameraShake, 1.2f - (Health / DefaultHealth));
		} 
		else 
		{
			GetPlayerController()->ClientPlayForceFeedback(GeneralDamageControllerShake, false, true, "GeneralDamage");
			GetPlayerController()->ForceFeedbackScale = 1.8f - (Health / DefaultHealth);
			GetPlayerController()->ClientPlayCameraShake(GeneralDamageCameraShake, 1.2f - (Health / DefaultHealth));
		}

		// Stop all effects when the player dies
		if (bIsDead) 
		{
			GetPlayerController()->ClientStopForceFeedback(DrowningControllerShake, "DrowningDamage");
			GetPlayerController()->ClientStopForceFeedback(GeneralDamageControllerShake, "GeneralDamage");
			GetPlayerController()->ClientStopForceFeedback(BreathReductionControllerShake, "BreathReduction");
			GetPlayerController()->ClientStopCameraShake(GeneralDamageCameraShake, false);
			GetPlayerController()->ClientStopCameraShake(DrowningCameraShake, false);
		}
	}

	// Health Recovery
	if (GetOwner()) 
	{
		GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_HealthRecoveryTimer);
		if (!bIsDead && bHealthRecovery) 
		{
			GetOwner()->GetWorldTimerManager().SetTimer(TimerHandle_HealthRecoveryTimer, this, &UAttributeComponent::CauseHealthRecovery, HealthRecoveryInterval, true, HealthRecoveryDelay);
		}
	}
}

void UAttributeComponent::HandleBreathChanged(AActor* Actor, float BreathDelta, class AController* InstigatedBy, AActor* Causer) 
{	
	if (Breath <= 0.0f || BreathDelta <= 0.0f || bIsDead) 
	{ 
		return; 
	}

	Breath = FMath::Clamp(Breath - BreathDelta, 0.0f, DefaultBreath);
	bOutOfBreath = Breath <= 0.0f;

#if UE_EDITOR
	if (bDebugAttributes) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Reducing Stat Breath: %f"), Breath);
	}
#endif

	OnBreathChanged.Broadcast(this, Breath, BreathDelta, InstigatedBy, Causer);

	if (GetPlayerController()) 
	{
		GetPlayerController()->ClientPlayForceFeedback(BreathReductionControllerShake, false, true, "BreathReduction");
		GetPlayerController()->ForceFeedbackScale = 1.0f - (Breath / DefaultBreath);
	}

	if (!GetOwner()) 
	{
		return;
	}

	if (!bOutOfBreath) 
	{
		GetOwner()->GetWorldTimerManager().ClearTimer(TimerHandle_DrownTimer);
		return;
	} 

	if (bEntryDamage) 
	{
		CauseDrownDamage();
	}

	if (!GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_DrownTimer)) 
	{
		GetOwner()->GetWorldTimerManager().SetTimer(TimerHandle_DrownTimer, this, &UAttributeComponent::DrownTimer, DamageInterval, true);
	}
}

void UAttributeComponent::Heal(float HealAmount) 
{
	if (HealAmount <= 0.0f || bIsDead) 
	{ 
		return; 
	}

	Health = FMath::Clamp(Health + HealAmount, 0.0f, DefaultHealth);
	OnHealthChanged.Broadcast(this, Health, -HealAmount, nullptr, nullptr, nullptr);

#if UE_EDITOR
	if (bDebugAttributes) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Recovering Stat Health: %f"), Health);
	}
#endif
}

void UAttributeComponent::RecoverBreath(float BreathAmount) 
{
	if (BreathAmount <= 0.0f || bIsDead) 
	{
		return;
	}

	if (GetPlayerController()) 
	{
		GetPlayerController()->ClientStopCameraShake(DrowningCameraShake, true);
		GetPlayerController()->ClientStopForceFeedback(DrowningControllerShake, "DrowningDamage");
	}

	Breath = FMath::Clamp(Breath + BreathAmount, 0.0f, DefaultBreath);
	OnBreathChanged.Broadcast(this, Breath, -BreathAmount, nullptr, nullptr);

#if UE_EDITOR
	if (bDebugAttributes) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Recovering Stat Breath: %f"), Breath);
	}
#endif
}

bool UAttributeComponent::GetIsHoldingBreath() const 
{
	return GetOwner() ? GetOwner()->GetWorldTimerManager().IsTimerActive(TimerHandle_BreathDecayTimer) : false;
}

#if UE_EDITOR
void UAttributeComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SecondsTillFullHealth = DefaultHealth / (HealthRecoveryPerSec * HealthRecoveryInterval);
	SecondsTillFullBreath = DefaultBreath / (BreathRecoveryPerSec * BreathRecoveryInterval);
	SecondsTillOutOfBreath = DefaultBreath / (BreathDecayPerSec * BreathDecayInterval);
	SecondsTillDeath = DefaultHealth / (DamagePerSec * DamageInterval);
}
#endif
