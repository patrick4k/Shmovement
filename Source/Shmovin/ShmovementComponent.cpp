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

#define SWITCH_MODE_CUSTOM(mode) do { \
	SHMOVIN_DEBUG_LOG("Switching to mode: " TEXT(#mode));\
		SetMovementMode(MOVE_Custom, mode); \
	} while (0)

void UShmovementComponent::BeginPlay()
{
	Super::BeginPlay();
	// CharacterOwner->GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &UShmovementComponent::OnCapsuleHit);
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
	case CMOVE_Slide:
		if (!PhysSlide(deltaTime, Iterations))
		{
			SWITCH_MODE(MOVE_Walking);
		}
	default:
		break;
	}
}

bool UShmovementComponent::CanAttemptJump() const
{
	if (MovementMode == MOVE_Custom)
	{
		return CustomMovementMode == CMOVE_Walltraction || CustomMovementMode == CMOVE_Slide;
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
		// return DoWallJump(bReplayingMoves, DeltaTime);
		return false;
	case CMOVE_Slide:
		return Super::DoJump(bReplayingMoves, DeltaTime);
	default:
		return false;
	}
}

bool UShmovementComponent::DoWallJump(bool bReplayingMoves, float DeltaTime)
{
	if (!LastWallHitData.has_value())
	{
		return false;
	}
	
	const FVector RotationAxis = FVector::CrossProduct(LastWallHitData->Hit.ImpactNormal, GravityDirection()).GetSafeNormal();
    
	// Create a rotation around this axis by WallJumpAngle degrees
	const FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(-WallJumpAngle));
    
	// Apply the rotation to the wall normal
	const FVector JumpDirection = Rotation.RotateVector(LastWallHitData->Hit.ImpactNormal);

	Launch(Velocity + JumpDirection * WallJumpVelocity);

	return true;
}

void UShmovementComponent::AddInputVector(FVector WorldVector, bool bForce)
{
	if (LastInputVector.has_value() && !WorldVector.IsNearlyZero())
	{
		*LastInputVector += WorldVector;
	}
	else if (!WorldVector.IsNearlyZero())
	{
		LastInputVector = WorldVector;
	}
	
	if (IsFalling() && ((LastWallHitData = TryComputeWallHitData(WorldVector))))
	{
		SHMOVIN_DEBUG_LOG("Hit wall with movement input!");
		InitWallTraction();
	}
	else
	{
		Super::AddInputVector(WorldVector, bForce);
	}
}

void UShmovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LastInputVector = std::nullopt;
}

bool UShmovementComponent::PhysWallTraction(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME
		|| LastInputVector == std::nullopt
		|| LastWallHitData == std::nullopt)
    {
        return false;
    }

	static uint64_t Count = 0;
	SHMOVIN_DEBUG_FMT("Count %llu", Count++);
	auto ToWall = -LastWallHitData->Hit.ImpactNormal;
	float InputDotWall = ToWall.Dot(LastInputVector.value().GetSafeNormal());
	SHMOVIN_DEBUG_FMT("InputDotWall: %f", InputDotWall);
	if (InputDotWall <= 0)
	{
		return false;
	}
	
	LastWallHitData = TryComputeWallHitData(LastInputVector.value());

	if (LastWallHitData == std::nullopt)
	{
		return false;
	}

	// Get the wall normal and gravity direction
	const FVector WallNormal = LastWallHitData->Hit.ImpactNormal;
	
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

std::optional<WallHitData> UShmovementComponent::TryComputeWallHitData(const FVector& Direction) const
{
	// Using twice the capsule radius should be enough
	const float TraceDistance = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
    
	// Trace from our current location towards the wall
	const FVector TraceStart = UpdatedComponent->GetComponentLocation();
	const FVector TraceEnd = TraceStart + Direction.GetSafeNormal() * TraceDistance;

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
	
	if (!bHasHit || Hit.ImpactNormal.Dot(Direction) > 0.f)
	{
		return std::nullopt;
	}

	// Validate the wall angle
	const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, -GravityDirection())));
	const float SignedWallAngle = 90.0f - WallAngle;

	if (SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle)
	{
		return WallHitData{.Hit = Hit, .WallAngle = SignedWallAngle};
	}
	return std::nullopt;
}

bool UShmovementComponent::UpdateWallHitData(const FHitResult& Hit)
{
	LastWallHitData = std::nullopt;
	
	if (IsFalling())
	{
		const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, -GravityDirection())));
		const float SignedWallAngle = 90.0f - WallAngle;
		SHMOVIN_DEBUG_FMT("Signed Wall Angle: %f", SignedWallAngle);
	
		if (SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle)
		{
			LastWallHitData = {.Hit = Hit, .WallAngle = SignedWallAngle};
		}
	}

	return LastWallHitData.has_value();
}

FVector UShmovementComponent::GravityDirection() const
{
	return CharacterOwner->GetGravityDirection();
}

void UShmovementComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (MovementMode == MOVE_Custom)
	{
		return;
	}
	
	if (UpdateWallHitData(Hit))
	{
		// TODO: Either use input vector to verify character wants slide on wall or use another method related to input vector to init wall traction
		InitWallTraction();
	}
	else if (bWantsToCrouch && UpdateSlopeHitData())
	{
		InitSlide();
	}
}

void UShmovementComponent::InitWallTraction()
{
	assert(LastWallHitData.has_value());
	
	bWallTractionInitiated = false;
	
	SWITCH_MODE_CUSTOM(CMOVE_Walltraction);

	if (!ShouldRotateToWall)
	{
		OnWallTractionInitComplete();
		return;
	}

	// Calculate rotation
	const double RightProjWallNormal = FVector::DotProduct(CharacterOwner->GetActorRightVector(), LastWallHitData->Hit.ImpactNormal);
	const auto WallSide = RightProjWallNormal > 0.0 ? EWallSide::Right : EWallSide::Left;
	const FVector Y = (WallSide == EWallSide::Right? 1 : -1) * LastWallHitData->Hit.ImpactNormal;
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

void UShmovementComponent::OnWallTractionInitComplete()
{
	SHMOVIN_DEBUG_LOG("Wall Traction Initiated");
	bWallTractionInitiated = true;
}

void UShmovementComponent::RegisterCrouchInput(UEnhancedInputComponent* EnhancedInputComponent, UInputAction* CrouchAction)
{
	EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &UShmovementComponent::BeginCrouch);
	EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &UShmovementComponent::EndCrouch);	
}

void UShmovementComponent::BeginCrouch()
{
	SHMOVIN_DEBUG_LOG("Crouching");
	bWantsToCrouch = true;
	
	if (UpdateSlopeHitData())
	{
		if (Velocity.Size() >= RequiredSlideVelocity
			|| SlopeHitData->SlopeAngle >= RequiredSlideAngle)
		{
			InitSlide();
		}
	}
}

void UShmovementComponent::EndCrouch()
{
	SHMOVIN_DEBUG_LOG("End Crouching");
	bWantsToCrouch = false;
}

void UShmovementComponent::InitSlide()
{
	SWITCH_MODE_CUSTOM(CMOVE_Slide);
}

bool UShmovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
	if (!bWantsToCrouch || !UpdateSlopeHitData())
	{
		return false;
	}

	auto PrevVelocity = Velocity;
	
	auto VelocityAlongSlope = Velocity - (FVector::DotProduct(Velocity, SlopeHitData->Hit.ImpactNormal) * SlopeHitData->Hit.ImpactNormal);

	auto FrictionDeceleration = -SlideFrictionDeceleration * FMath::Cos(FMath::DegreesToRadians(SlopeHitData->SlopeAngle)) * VelocityAlongSlope.GetSafeNormal();
	
	if (FrictionDeceleration.Size() * deltaTime < VelocityAlongSlope.Size())
	{
		Velocity += FrictionDeceleration * deltaTime;
	}

	auto GravityAccel = SlideGravityAcceleration * GravityDirection();
	auto GravityAccelAlongSlope = GravityAccel - (FVector::DotProduct(GravityAccel, SlopeHitData->Hit.ImpactNormal) * SlopeHitData->Hit.ImpactNormal);
	Velocity += GravityAccelAlongSlope * deltaTime;

	if (Velocity.Size() < StopSlidingVelocity)
	{
		if (SlideTimer)
		{
			*SlideTimer += deltaTime;

			if (*SlideTimer >= ExitSlideFromRestTime)
			{
				Velocity = PrevVelocity;
				return false;
			}
		}
		else
		{
			SlideTimer = 0;
		}
	}
	else
	{
		SlideTimer = std::nullopt;
	}
	
	SlideAlongSurface(Velocity * deltaTime, 1.f - Iterations * deltaTime, SlopeHitData->Hit.ImpactNormal, SlopeHitData->Hit, true);
	
	return true;
}

bool UShmovementComponent::UpdateSlopeHitData()
{
	SlopeHitData = std::nullopt;

	const FVector TraceStart = CharacterOwner->GetCapsuleComponent()->GetComponentLocation();
	const FVector TraceEnd = TraceStart +
		FVector{0, 0, -CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere()};
	
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());
	SlopeHitData.emplace();
	bool bHasHit = GetWorld()->SweepSingleByChannel(
		SlopeHitData->Hit,
		TraceStart,
		TraceEnd,
		CharacterOwner->GetCapsuleComponent()->GetComponentQuat(),
		ECC_Pawn, // Use appropriate channel
		CharacterOwner->GetCapsuleComponent()->GetCollisionShape(),
		QueryParams
	);

	if (!bHasHit)
	{
		SlopeHitData = std::nullopt;
	}
	else
	{
		const float SlopeAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(SlopeHitData->Hit.ImpactNormal, -GravityDirection())));
		SlopeHitData->SlopeAngle = SlopeAngle;
	}
	
	return SlopeHitData.has_value();
}
