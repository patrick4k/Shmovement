// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShmovementComponent.generated.h"

UENUM(BlueprintType)
enum EShmovementModes : uint8
{
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

public: // FUNCTIONS
	void BeginPlay() override;

	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool CanGainWallTraction(const FHitResult& Hit) const;

	void InitWallTraction(const FVector& WallNormal);
	void RotateToWallNormal(const FVector& WallNormal);
};
