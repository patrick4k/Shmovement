// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <format>
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
	if (deltaTime < MIN_TICK_TIME)
    {
        return;
    }
	
    const float WallAngle = FMath::RadiansToDegrees(
        FMath::Acos(FVector::DotProduct(CharacterOwner->GetCapsuleComponent()->GetUpVector(), FVector::UpVector)));

	SHMOVIN_DEBUG_FMT("Wall Angle: %f", WallAngle);

	// TODO: Compute sliding physics based on wall angle
	
    // const float SlideFactor = FMath::GetMappedRangeValueClamped(
    //     FVector2D(0.0f, 90.0f),
    //     FVector2D(0.1f, 1.0f),
    //     WallAngle
    // );
    //
    // // Apply gravity-based sliding
    // FVector GravityDir = FVector::DownVector;
    // const FVector WallRight = FVector::CrossProduct(UpdatedComponent->GetUpVector(), -GravityDir).GetSafeNormal();
    // const FVector WallDown = FVector::CrossProduct(WallRight, UpdatedComponent->GetUpVector()).GetSafeNormal();
    //
    // // Calculate sliding velocity
    // FVector SlideVelocity = WallDown * GetGravityZ() * SlideFactor * deltaTime;
    //
    // // Preserve horizontal momentum when initially attaching to wall
    // if (!bWallTractionInitiated)
    // {
    //     // Project current velocity onto the wall plane
    //     const FVector WallNormal = UpdatedComponent->GetUpVector();
    //     const FVector VelocityOnWall = FVector::VectorPlaneProject(Velocity, WallNormal);
    //     
    //     // Blend between current velocity and slide velocity
    //     const float MomentumRetention = 0.8f; // Adjust this value to control how much momentum is preserved
    //     Velocity = VelocityOnWall * MomentumRetention + SlideVelocity;
    //     
    //     bWallTractionInitiated = true;
    // }
    // else
    // {
    //     // Regular wall sliding
    //     Velocity = SlideVelocity;
    // }
    //
    // // Apply movement
    // FVector Adjusted = Velocity * deltaTime;
    // SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, PrevHit);
    //
    // // Handle collision during movement
    // if (PrevHit.Time < 1.f)
    // {
    //     HandleImpact(PrevHit, deltaTime, Adjusted);
    //     SlideAlongSurface(Adjusted, (1.f - PrevHit.Time), PrevHit.Normal, PrevHit, true);
    // }
}

void UShmovementComponent::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                        UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (CanGainWallTraction(Hit))
	{
		InitWallTraction(Hit.ImpactNormal);
	}
	PrevHit = Hit;
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
	SHMOVIN_DEBUG_LOG("Wall Traction Initiated");
	bWallTractionInitiated = true;
}
