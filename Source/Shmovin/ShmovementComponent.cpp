// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <cassert>
#include <GameFramework/Character.h>
#include <Components/CapsuleComponent.h>

#include "Kismet/KismetSystemLibrary.h"

#include "ShmovinCommon.h" 

#define SWITCH_MODE(mode) do { \
	SHMOVIN_DEBUG_LOG("Switching to mode: " TEXT(#mode));\
		SetMovementMode(mode); \
	} while (0)

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
		if (!PhysWallTraction(deltaTime, Iterations))
		{
			SWITCH_MODE(MOVE_Falling);
		}
		break;
	default:
		break;
	}
}

bool UShmovementComponent::CanAttemptJump() const
{
	if (MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_Walltraction)
	{
		return true;
	}
	return Super::CanAttemptJump();
}

bool UShmovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (MovementMode != MOVE_Custom)
	{
		return Super::DoJump(bReplayingMoves, DeltaTime);
	}
	
	switch (CustomMovementMode)
	{
	case CMOVE_Walltraction:
		return DoWallJump(bReplayingMoves, DeltaTime);
	default:
		return false;
	}
}

bool UShmovementComponent::DoWallJump(bool bReplayingMoves, float DeltaTime)
{
	// Compute vector by adding WallJumpAngle to the angle between WallHitData->Hit.ImpactNormal and GravityDirection
	
	const FVector RotationAxis = FVector::CrossProduct(WallHitData->Hit.ImpactNormal, GravityDirection()).GetSafeNormal();
    
	// Create a rotation around this axis by WallJumpAngle degrees
	const FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(-WallJumpAngle));
    
	// Apply the rotation to the wall normal
	const FVector JumpDirection = Rotation.RotateVector(WallHitData->Hit.ImpactNormal);

	Launch(JumpDirection * WallJumpVelocity);

	return true;
}

bool UShmovementComponent::PhysWallTraction(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME
		|| !WallHitData.has_value()
		|| !WallHitData->Hit.bBlockingHit
		|| !IsNextToWallWithTraction())
    {
        return false;
    }

	// Get the wall normal and gravity direction
	const FVector WallNormal = WallHitData->Hit.ImpactNormal;
	
	const FVector GravityAcceleration = GravityDirection() * WallSlidingGravityAcceleration;
	FVector GravityAccelerationAlongWall = GravityAcceleration - (FVector::DotProduct(GravityAcceleration, WallNormal) * WallNormal);
	FVector GravityVelocityAlongWall = GravityAccelerationAlongWall * deltaTime;

	FVector VelocityAlongWall = Velocity - (FVector::DotProduct(Velocity, WallNormal) * WallNormal);
	const float VelocityMagnitude = VelocityAlongWall.Size();

	if (!FMath::IsNearlyZero(VelocityMagnitude))
	{
		// Friction opposes motion
		FVector VelocityDirection = VelocityAlongWall.GetSafeNormal();
		const FVector FrictionDirection = -VelocityDirection;

		const FVector FrictionAcceleration = FrictionDirection * WallSlidingFrictionDeceleration;
		const FVector FrictionVelocityChange = FrictionAcceleration * deltaTime;
		if (FrictionVelocityChange.Size() < VelocityMagnitude)
		{
			Velocity += FrictionVelocityChange;
		}
		else
		{
			Velocity -= VelocityAlongWall;
		}
	}
	
	Velocity += GravityVelocityAlongWall;
    
	// Perform movement using the updated velocity
	FVector Delta = Velocity * deltaTime;

	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	// Handle collision by sliding along the surface
	if (Hit.bBlockingHit)
	{
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	return true;
}

bool UShmovementComponent::IsNextToWallWithTraction() const
{
	if (!WallHitData.has_value())
		return false;

	// Using twice the capsule radius should be enough
	const float TraceDistance = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0;
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
    
	// Trace from our current location towards the wall
	const FVector TraceStart = UpdatedComponent->GetComponentLocation();
	const FVector TraceEnd = TraceStart - WallHitData->Hit.ImpactNormal * TraceDistance;

	// Check for a hit
	bool bHasHit = GetWorld()->SweepSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		UpdatedComponent->GetComponentQuat(),
		ECC_Pawn, // Use appropriate channel
		CharacterOwner->GetCapsuleComponent()->GetCollisionShape(),
		QueryParams
	);
	
	if (!bHasHit)
	{
		return false;
	}

	// Validate the wall angle
	const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, -GravityDirection())));
	const float SignedWallAngle = 90.0f - WallAngle;

	return SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle;
}

void UShmovementComponent::UpdateWallHitData(const FHitResult& Hit)
{
	WallHitData = std::nullopt;
	
	if (IsFalling())
	{
		const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, -GravityDirection())));
		const float SignedWallAngle = 90.0f - WallAngle;
		SHMOVIN_DEBUG_FMT("Signed Wall Angle: %f", SignedWallAngle);
	
		if (SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle)
		{
			WallHitData = {.Hit = Hit, .WallAngle = SignedWallAngle};
		}
	}
}

FVector UShmovementComponent::GravityDirection() const
{
	return CharacterOwner->GetGravityDirection();
}

void UShmovementComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	UpdateWallHitData(Hit);
	if (WallHitData.has_value())
	{
		InitWallTraction();
	}
}

void UShmovementComponent::InitWallTraction()
{
	assert(WallHitData.has_value());
	
	bWallTractionInitiated = false;
	
	SetMovementMode(MOVE_Custom, CMOVE_Walltraction);

	if (!ShouldRotateToWall)
	{
		OnWallRunInitComplete();
		return;
	}

	// Calculate rotation
	const double RightProjWallNormal = FVector::DotProduct(CharacterOwner->GetActorRightVector(), WallHitData->Hit.ImpactNormal);
	const auto WallSide = RightProjWallNormal > 0.0 ? EWallSide::Right : EWallSide::Left;
	const FVector Y = (WallSide == EWallSide::Right? 1 : -1) * WallHitData->Hit.ImpactNormal;
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
	SHMOVIN_DEBUG_LOG("Wall Traction Initiated");
	bWallTractionInitiated = true;
}
