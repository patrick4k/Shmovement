// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShmovementComponent.generated.h"

UENUM(BlueprintType)
enum EShmovementModes : int
{
	CMOVE_WallJump UMETA(DisplayName = "Wall Jump"),
};

UCLASS()
class SHMOVIN_API UShmovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	void BeginPlay() override;

	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	bool CanWallJump() const;

	void InitWallJump();
	
};
