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
			LastWallHitData = std::nullopt;
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

// TODO: Allow wall jumping based on input vector and wall detection
bool UShmovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (MovementMode == MOVE_Custom)
	{
		switch (CustomMovementMode)
		{
		case CMOVE_Walltraction:
			if (LastWallHitData)
			{
				return DoWallJump(LastWallHitData->Hit.ImpactNormal);
			}
			break;

		case CMOVE_Slide:
			break;

		default:
			break;
		}
	}

	if (LastInputVector)
	{
		if (auto NewWallHit = TryComputeWallHitData(-LastInputVector->GetSafeNormal()))
		{
			return DoWallJump(NewWallHit->Hit.ImpactNormal);
		}
	}

	return Super::DoJump(bReplayingMoves, DeltaTime);
	
}

bool UShmovementComponent::DoWallJump(const FVector& WallNormal)
{
	const FVector RotationAxis = FVector::CrossProduct(WallNormal, GravityDirection()).GetSafeNormal();
	const FQuat Rotation = FQuat(RotationAxis, FMath::DegreesToRadians(-WallJumpAngle));
	const FVector JumpDirection = Rotation.RotateVector(WallNormal);

	Velocity -= Velocity.Dot(JumpDirection) * JumpDirection;

	Launch(Velocity + JumpDirection * WallJumpVelocity);
	return true;
}

void UShmovementComponent::AddInputVector(FVector WorldVector, bool bForce)
{
	Super::AddInputVector(WorldVector, bForce);
	
	if (LastInputVector.has_value() && !WorldVector.IsNearlyZero())
	{
		*LastInputVector += WorldVector;
	}
	else if (!WorldVector.IsNearlyZero())
	{
		LastInputVector = WorldVector;
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
		SHMOVIN_DEBUG_LOG("Exiting because deltaTime < MIN_TICK_TIME || LastInputVector == std::nullopt || LastWallHitData == std::nullopt");
        return false;
    }

	LastWallHitData = TryComputeWallHitData(-LastWallHitData->Hit.ImpactNormal);

	if (!LastWallHitData)
	{
		SHMOVIN_DEBUG_LOG("Exiting because TryComputeWallHitData failed");
		return false;
	}
	
	auto ToWall = -LastWallHitData->Hit.ImpactNormal;
	float InputWallAngle = FMath::Acos(ToWall.Dot(LastInputVector.value().GetSafeNormal()));
	if (InputWallAngle > FMath::DegreesToRadians(WallTractionMaintainInputWallAngleDeg))
	{
		SHMOVIN_DEBUG_FMT("Exiting because input dot wall is > %f", WallTractionMaintainInputWallAngleDeg);
		return false;
	}
	
	const FVector WallNormal = LastWallHitData->Hit.ImpactNormal;
	
	const FVector GravityAcceleration = GravityDirection() * WallSlidingGravityAcceleration * GravityScale;
	FVector GravityAccelerationAlongWall = GravityAcceleration - (FVector::DotProduct(GravityAcceleration, WallNormal) * WallNormal);
	FVector GravityVelocityAlongWall = GravityAccelerationAlongWall * deltaTime;

	FVector VelocityAlongWall = Velocity - (FVector::DotProduct(Velocity, WallNormal) * WallNormal);
	const float VelocityMagnitude = VelocityAlongWall.Size();

	if (!FMath::IsNearlyZero(VelocityMagnitude))
	{
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

	MoveWithVelocity(deltaTime, Iterations);

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
	
	if (!bHasHit)
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

void UShmovementComponent::MoveWithVelocity(float deltaTime, int32 Iterations, bool ForceSlide)
{
	FHitResult Hit;
	SafeMoveUpdatedComponent(Velocity * deltaTime, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.bBlockingHit)
	{
		SlideAlongSurface(Velocity * deltaTime, 1.f - Iterations * deltaTime, SlopeHitData->Hit.ImpactNormal, SlopeHitData->Hit, true);
	}
}

bool UShmovementComponent::UpdateWallHitData(const FHitResult& Hit)
{
	LastWallHitData = std::nullopt;
	
	if (IsFalling())
	{
		const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, -GravityDirection())));
		const float SignedWallAngle = 90.0f - WallAngle;
	
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
	
	if (UpdateWallHitData(Hit) && LastInputVector && IsFalling())
	{
		auto InputWallAngle = FMath::Acos(LastInputVector->Dot(-LastWallHitData->Hit.ImpactNormal));
		if (InputWallAngle <= FMath::DegreesToRadians(WallTractionInitInputWallAngleDeg))
		{
			InitWallTraction();
		}
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
	Velocity *= SlideInitBoostFactor;
}

bool UShmovementComponent::PhysSlide(float deltaTime, int32 Iterations)
{
	if (!bWantsToCrouch || !UpdateSlopeHitData())
	{
		return false;
	}

	auto PrevVelocity = Velocity;
	
	auto VelocityAlongSlope = Velocity - FVector::DotProduct(Velocity, SlopeHitData->Hit.ImpactNormal) * SlopeHitData->Hit.ImpactNormal;

	auto FrictionDeceleration = -SlideFrictionDeceleration * FMath::Cos(FMath::DegreesToRadians(SlopeHitData->SlopeAngle)) * VelocityAlongSlope.GetSafeNormal();
	auto FrictionVelocityLoss = FrictionDeceleration * deltaTime;
	
	if (FrictionVelocityLoss.Size() < VelocityAlongSlope.Size())
	{
		Velocity += FrictionVelocityLoss;
	}
	else
	{
		Velocity -= VelocityAlongSlope;
	}

	auto GravityAccel = GravityScale * SlideGravityAcceleration * GravityDirection();
	auto GravityAccelAlongSlope = GravityAccel - FVector::DotProduct(GravityAccel, SlopeHitData->Hit.ImpactNormal) * SlopeHitData->Hit.ImpactNormal;
	Velocity += GravityAccelAlongSlope * deltaTime;
	
	if (VelocityAlongSlope.Size() < StopSlidingVelocity)
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
