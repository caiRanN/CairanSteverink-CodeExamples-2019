// Copyright 2018 Disillusion Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeComponent.generated.h"


////////////////////////////////////////////
// Code Example Description

/*
* This is a Attribute Component written for the seminary of sight.
* The goal of this component was for the design team to easily tweak our character stats.
* Optionaly you can trigger camera and controller shakes to make breath reduction and drowning effects more intense.
* I also added some debug functionality so the designers could easily see how long it would take for the player to regain and diminish stats.
*/



class UCameraShake;
class UForceFeedbackEffect;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FOnHealthChangedSignature, UAttributeComponent*, OwningHealthComp, float, Health, float, HealthDelta, const class UDamageType*, DamageType, class AController*, InstigatedBy, AActor*, DamageCauser);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnBreathChangedSignature, UAttributeComponent*, OwningAttributeComp, float, Breath, float, BreathDelta, class AController*, InstigatedBy, AActor*, Causer);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEUS_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UAttributeComponent();

public:
	UPROPERTY(Category = "AttributeComponent|Delegates", BlueprintAssignable)
	FOnHealthChangedSignature OnHealthChanged;

	UPROPERTY(Category = "AttributeComponent|Delegates", BlueprintAssignable)
	FOnBreathChangedSignature OnBreathChanged;

protected:
	/** The current health value for this actor */
	UPROPERTY(Category = "Attribute Component (Health)", BlueprintReadOnly)
	float Health = 100.0f;
	
	/** The default health value for this actor */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly)
	float DefaultHealth = 100.0f;

	/** Has the actor owning this component died */
	UPROPERTY(Category = "Attribute Component (Health)", VisibleDefaultsOnly, BlueprintReadOnly)
	bool bIsDead = false;

	/** Can this actor take damage */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly)
	uint32 bCanTakeDamage : 1;

	/** Should we recover health when the player has not taken any damage for a while */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly)
	uint32 bHealthRecovery : 1;

	/** The amount of time before we start recovering health */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly, meta=(EditCondition=bHealthRecovery))
	float HealthRecoveryDelay = 15.0f;

	/** Health recovery done per second to the player */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly, meta = (EditCondition = bHealthRecovery))
	float HealthRecoveryPerSec = 20.0f;

	/** Time between recovery applications. */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, BlueprintReadOnly, meta = (EditCondition = bHealthRecovery))
	float HealthRecoveryInterval = 1.0f;

	/** (Editor Functionality) The time until the player has regained full health */
	UPROPERTY(Category = "Attribute Component (Health)", VisibleAnywhere, AdvancedDisplay)
	float SecondsTillFullHealth = 0.0f;

	/** Controller shake that is applied every time the player's breath is reduced */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, AdvancedDisplay)
	UForceFeedbackEffect* GeneralDamageControllerShake;

	/** Camera shake that is played when the player takes damage */
	UPROPERTY(Category = "Attribute Component (Health)", EditAnywhere, AdvancedDisplay)
	TSubclassOf<UCameraShake> GeneralDamageCameraShake;

	/** The current breath value for this actor */
	UPROPERTY(Category = "Attribute Component (Breath)", BlueprintReadOnly)
	float Breath = 100.0f;

	/** The default breath value for this actor */
	UPROPERTY(Category = "Attribute Component (Breath)", EditAnywhere, BlueprintReadOnly)
	float DefaultBreath = 100.0f;

	UPROPERTY(Category = "Attribute Component (Breath)", VisibleDefaultsOnly, BlueprintReadOnly)
	bool bOutOfBreath = false;

	/** Breath recovery done per second to the player */
	UPROPERTY(Category = "Attribute Component (Breath)", EditAnywhere, BlueprintReadOnly)
	float BreathRecoveryPerSec = 20.0f;

	/** Time between recovery applications. */
	UPROPERTY(Category = "Attribute Component (Breath)", EditAnywhere, BlueprintReadWrite)
	float BreathRecoveryInterval = 1.0f;
	
	/** (Editor Functionality) The time until the player has regained full breath */
	UPROPERTY(Category = "Attribute Component (Breath)", VisibleAnywhere, AdvancedDisplay)
	float SecondsTillFullBreath = 0.0f;

	/** Controller shake that is applied every time the player's breath is reduced */
	UPROPERTY(Category = "Attribute Component (Breath)", EditAnywhere, AdvancedDisplay)
	UForceFeedbackEffect* BreathReductionControllerShake;

	/** The delay before we start reducing the breath every second */
	UPROPERTY(Category = "Attribute Component (Holding Breath)", EditAnywhere, BlueprintReadOnly)
	float BreathDecayFirstInDelay = 1.0f;

	/** Breath reduction done per second to the player when bHoldingBreath is true */
	UPROPERTY(Category = "Attribute Component (Holding Breath)", EditAnywhere, BlueprintReadOnly)
	float BreathDecayPerSec = 10.0f;

	/** Time between breath reduction applications. */
	UPROPERTY(Category = "Attribute Component (Holding Breath)", EditAnywhere, BlueprintReadOnly)
	float BreathDecayInterval = 10.0f;

	/** (Editor Functionality) The time until the player has run out of breath */
	UPROPERTY(Category = "Attribute Component (Holding Breath)", VisibleAnywhere, AdvancedDisplay)
	float SecondsTillOutOfBreath = 0.0f;

	/** if bDamageCausing, cause damage when instantly when the player runs out of breath in addition to damage each second */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, BlueprintReadWrite)
	uint32 bEntryDamage : 1;

	/** Should we cause damage when the player runs out of breath */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, BlueprintReadWrite)
	uint32 bDamageCausing : 1;

	/** Damage done per second to the player when out of breath */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, BlueprintReadWrite)
	float DamagePerSec = 10.0f;

	/** Type of damage done */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UDamageType> DrowningDamageType;

	/** If damage causing, time between damage applications. */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, BlueprintReadWrite)
	float DamageInterval = 1.0f;
	
	/** (Editor Functionality) The time until the player will die from drowning */
	UPROPERTY(Category = "Attribute Component (Drowning)", VisibleAnywhere, AdvancedDisplay)
	float SecondsTillDeath = 0.0f;

	/** Controller shake that is applied while the player is drowning */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, AdvancedDisplay)
	UForceFeedbackEffect* DrowningControllerShake;

	/** The camera shake that is played when the player is drowning */
	UPROPERTY(Category = "Attribute Component (Drowning)", EditAnywhere, AdvancedDisplay)
	TSubclassOf<UCameraShake> DrowningCameraShake;

	/** Log the attribute changes */
	UPROPERTY(Category = "Attribute Component (Debug)", EditAnywhere)
	bool bDebugAttributes = false;

protected:
	/** Handles for efficient management of OnTimerTick timer */
	FTimerHandle TimerHandle_DrownTimer;
	FTimerHandle TimerHandle_BreathRecoveryTimer;
	FTimerHandle TimerHandle_HealthRecoveryTimer;
	FTimerHandle TimerHandle_BreathDecayTimer;

	APlayerController* PlayerController;

protected:
	virtual void BeginPlay() override;
	virtual void DrownTimer();
	
	UFUNCTION()
	void CauseDrownDamage();

	UFUNCTION()
	void CauseBreathDecay();

	UFUNCTION()
	void CauseHealthRecovery();

	UFUNCTION()
	void CauseBreathRecovery();

	APlayerController* GetPlayerController() const;

public:
	void StartBreathRecovery();
	void StopBreathRecovery();
	void StartBreathDecay();
	void StopBreathDecay();

public:
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleBreathChanged(AActor* Actor, float BreathDelta, class AController* InstigatedBy, AActor* Causer);

public:
	void Heal(float HealAmount);
	void RecoverBreath(float BreathAmount);

	UFUNCTION(Category = "HealthComponent|Setters", BlueprintCallable)
	void SetCanTakeDamage(const bool bValue) { bCanTakeDamage = bValue; }

public:
	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE float GetHealth() const { return Health; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE float GetDefaultHealth() const { return DefaultHealth; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE bool GetIsDead() const { return bIsDead; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE bool GetCanTakeDamage() const { return bCanTakeDamage; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE float GetBreath() const { return Breath; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE float GetDefaultBreath() const { return DefaultBreath; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	FORCEINLINE bool GetIsOutOfBreath() const { return bOutOfBreath; }

	UFUNCTION(Category = "HealthComponent|Getters", BlueprintCallable, BlueprintPure)
	bool GetIsHoldingBreath() const;

protected:
#if UE_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};