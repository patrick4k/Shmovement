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
	float WallSlidingFrictionDeceleration = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallSlidingGravityAcceleration = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallJumpAngle = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallJumpVelocity = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float RequiredSlideVelocity = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float RequiredSlideAngle = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float SlideFrictionDeceleration = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float StopSlidingVelocity = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float SlideGravityAcceleration = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float ExitSlideFromRestTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallTractionInitInputWallAngleDeg = 45;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shmovin", meta = (AllowPrivateAccess = "true"))
	float WallTractionMaintainInputWallAngleDeg = 90;

private:
	bool bWallTractionInitiated = false;
	EShmovementModes CurrentShmovementMode = EShmovementModes::CMOVE_None;
	std::optional<WallHitData> LastWallHitData;
	std::optional<SlopeHitData> SlopeHitData;

	/**
	 * @brief Updates the WallHitData cache, returns true if cache is valid (i.e., if wall traction is valid)
	 */
	bool UpdateWallHitData(const FHitResult& Hit);

	FVector GravityDirection() const;

	std::optional<float> SlideTimer;

	std::optional<FVector> LastInputVector;

public: // FUNCTIONS
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	void InitWallTraction();

	UFUNCTION()
	void OnWallTractionInitComplete();

	void RegisterCrouchInput(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* CrouchAction);
	void BeginCrouch();
	void EndCrouch();

	void InitSlide();
	bool PhysSlide(float deltaTime, int32 Iterations);
	bool UpdateSlopeHitData();

	UFUNCTION(BlueprintCallable, Category = "Shmovin")
	bool IsSliding() const;

public: // OVERRIDES
	void BeginPlay() override;

	void PhysCustom(float deltaTime, int32 Iterations) override;

	bool CanAttemptJump() const override;

	bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	bool DoWallJump(const FVector& WallNormal);

	void AddInputVector(FVector WorldVector, bool bForce = false) override;
	
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected: // UTILITY FUNCTIONS
	virtual bool PhysWallTraction(float deltaTime, int32 Iterations);
	std::optional<::WallHitData> TryComputeWallHitData(const FVector& Direction) const;
};
