// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <cassert>
#include <format>
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

bool UShmovementComponent::PhysWallTraction(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
    {
        return false;
    }

	if (!WallHitData.has_value() || !WallHitData->Hit.bBlockingHit)
	{
		return false;
	}

	if (!IsWallTractionValid())
	{
		SHMOVIN_DEBUG_LOG("Wall Traction is not valid");
		return false;
	}

	// Get the wall normal and gravity direction
	const FVector WallNormal = WallHitData->Hit.ImpactNormal;
	
	constexpr float GravityAccelerationMagnitude = 980.0f;
	const FVector GravityAcceleration = GravityDirection() * GravityAccelerationMagnitude;
	FVector GravityAccelerationAlongWall = GravityAcceleration - (FVector::DotProduct(GravityAcceleration, WallNormal) * WallNormal);
	FVector GravityVelocityAlongWall = GravityAccelerationAlongWall * deltaTime;

	FVector VelocityAlongWall = Velocity - (FVector::DotProduct(Velocity, WallNormal) * WallNormal);
	const float VelocityMagnitude = VelocityAlongWall.Size();
	
	FVector FrictionVelocity = FVector::ZeroVector;
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
			Velocity = FVector::ZeroVector;
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

bool UShmovementComponent::IsWallTractionValid() const
{
	if (!WallHitData.has_value())
		return false;

	// Perform a short sweep to check if we're still near the wall
	const float TraceDistance = 2.0f; // Adjust this value based on your needs
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
    
	// Trace from our current location towards the wall
	const FVector TraceStart = UpdatedComponent->GetComponentLocation();
	const FVector TraceEnd = TraceStart - WallHitData->Hit.ImpactNormal * TraceDistance;
    
	bool bHasHit = GetWorld()->SweepSingleByChannel(
		Hit,
		TraceStart,
		TraceEnd,
		UpdatedComponent->GetComponentQuat(),
		ECC_Pawn, // Use appropriate channel
		CharacterOwner->GetCapsuleComponent()->GetCollisionShape(),
		QueryParams
	);

	// If we didn't hit anything, or hit something too far away, exit wall traction
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


bool UShmovementComponent::UpdateWallHitData(const FHitResult& Hit)
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
	
	return WallHitData.has_value();
}

FVector UShmovementComponent::GravityDirection() const
{
	return CharacterOwner->GetGravityDirection();
}

void UShmovementComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (UpdateWallHitData(Hit))
	{
		InitWallTraction();
	}
}

void UShmovementComponent::InitWallTraction()
{
	assert(WallHitData.has_value());
	
	bWallTractionInitiated = false;
	
	SetMovementMode(MOVE_Custom, CMOVE_Walltraction);

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
