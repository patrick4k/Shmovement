// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShmovementComponent.generated.h" 

UENUM(BlueprintType)
enum EShmovementModes : int
{
	CMOVE_None UMETA(DisplayName = "None"),
	CMOVE_Walltraction UMETA(DisplayName = "Wall Jump"),
	CMOVE_Slide UMETA(DisplayName = "Slide"),
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

struct SlopeHitData
{
	FHitResult Hit;
	float SlopeAngle;
};

UCLASS()
class SHMOVIN_API UShmovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

protected: // PROPERTIES
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallRotationDuration = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	bool ShouldRotateToWall = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float MaxWallTractionAngle = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float MinWallTractionAngle = -15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingFrictionDeceleration = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingGravityAcceleration = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallJumpAngle = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallJumpVelocity = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float RequiredSlideVelocity = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float RequiredSlideAngle = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float SlideFrictionDeceleration = 100.0f;

private:
	bool bWallTractionInitiated = false;
	EShmovementModes CurrentShmovementMode = EShmovementModes::CMOVE_None;
	std::optional<WallHitData> WallHitData;
	std::optional<SlopeHitData> SlopeHitData;

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

	void RegisterCrouchInput(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* CrouchAction);
	void BeginCrouch();
	void EndCrouch();

	void InitSlide();
	bool PhysSlide(float deltaTime, int32 Iterations);
	bool GetSlopeHitBelow();

public: // OVERRIDES
	void BeginPlay() override;

	void PhysCustom(float deltaTime, int32 Iterations) override;

	bool CanAttemptJump() const override;

	bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	bool DoWallJump(bool bReplayingMoves, float DeltaTime);

protected: // UTILITY FUNCTIONS
	virtual bool PhysWallTraction(float deltaTime, int32 Iterations);
	bool IsNextToWallWithTraction() const;
};
