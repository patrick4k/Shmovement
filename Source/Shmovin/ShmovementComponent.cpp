// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <GameFramework/Character.h>
#include <Components/CapsuleComponent.h>

#include "Kismet/KismetSystemLibrary.h"

#include "ShmovinCommon.h" 

void UShmovementComponent::BeginPlay()
{
	Super::BeginPlay();
	CharacterOwner->GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &UShmovementComponent::OnCapsuleHit);
}

void UShmovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);
	
	switch (CustomMovementMode)
	{
	case CMOVE_Walltraction:
		PhysWallTraction(deltaTime, Iterations);
		break;
	default:
		break;
	}
}

void UShmovementComponent::PhysWallTraction(float deltaTime, int32 Iterations)
{
	if (!bWallTractionInitiated || deltaTime < MIN_TICK_TIME)
	{
		return;
	}
	
	ShmovinCommon::DEBUG_LOG(TEXT("Wall Traction Physics Running!"));
}

void UShmovementComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (CanGainWallTraction(Hit))
	{
		InitWallTraction(Hit.ImpactNormal);
	}
}

bool UShmovementComponent::CanGainWallTraction(const FHitResult& Hit) const
{
	if (!IsFalling())
	{
		return false;
	}

	const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector)));
	const float SignedWallAngle = 90.0f - WallAngle;
	
	return SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle;
}

void UShmovementComponent::InitWallTraction(const FVector& WallNormal)
{
	bWallTractionInitiated = false;
	
	SetMovementMode(MOVE_Custom, CMOVE_Walltraction);

	// Calculate rotation
	const double RightProjWallNormal = FVector::DotProduct(CharacterOwner->GetActorRightVector(), WallNormal);
	const auto WallSide = RightProjWallNormal > 0.0 ? EWallSide::Right : EWallSide::Left;
	const FVector Y = WallSide == EWallSide::Right? WallNormal : -WallNormal;
	const FVector X = FVector::CrossProduct(Y, CharacterOwner->GetActorUpVector()).GetSafeNormal();
	const auto Rotation = FRotationMatrix::MakeFromXY(X, Y).Rotator();

	// Apply rotation
	const FLatentActionInfo LatentActionInfo{ 0, INDEX_NONE, TEXT("OnWallRunInitComplete"), this };
	UKismetSystemLibrary::MoveComponentTo(
		CharacterOwner->GetRootComponent(),
		CharacterOwner->GetActorLocation(),
		Rotation,
		true,
		true,
		WallRotationDuration,
		true,
		EMoveComponentAction::Move,
		LatentActionInfo
	);
}

void UShmovementComponent::OnWallRunInitComplete()
{
	ShmovinCommon::DEBUG_LOG(TEXT("Wall Traction Initiated"));
	bWallTractionInitiated = true;
}
