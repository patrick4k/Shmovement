// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShmovementComponent.generated.h" 

UENUM(BlueprintType)
enum EShmovementModes : int
{
	CMOVE_None UMETA(DisplayName = "None"),
	CMOVE_Walltraction UMETA(DisplayName = "Wall Jump"),
	CMOVE_MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EWallSide : uint8
{
	Left UMETA(DisplayName = "Left Side"),
	Right UMETA(DisplayName = "Right Side"),
	MAX UMETA(Hidden)
};

struct WallHitData
{
	FHitResult Hit;
	float WallAngle; // 0 for wall parallel to character gravity
};

UCLASS()
class SHMOVIN_API UShmovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected: // PROPERTIES
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallRotationDuration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float MaxWallTractionAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float MinWallTractionAngle = -15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingFriction = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingGravityScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingFrictionCoefficient = 0.5f;


private:
	bool bWallTractionInitiated = false;
	EShmovementModes CurrentShmovementMode = EShmovementModes::CMOVE_None;
	std::optional<WallHitData> WallHitData;

	/**
	 * @brief Updates the WallHitData cache, returns true if cache is valid (i.e., if wall traction is valid)
	 */
	bool UpdateWallHitData(const FHitResult& Hit);

	FVector GravityDirection() const;

public: // FUNCTIONS
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	void InitWallTraction();

	UFUNCTION()
	void OnWallRunInitComplete();

public: // OVERRIDES
	void BeginPlay() override;

	void PhysCustom(float deltaTime, int32 Iterations) override;

protected: // UTILITY FUNCTIONS
	virtual void PhysWallTraction(float deltaTime, int32 Iterations);
	bool IsWallTractionValid() const;
};
