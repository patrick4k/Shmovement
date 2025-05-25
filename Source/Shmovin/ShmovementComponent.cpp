// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <GameFramework/Character.h>
#include <Components/CapsuleComponent.h>

#include "Kismet/KismetSystemLibrary.h"

void UShmovementComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterOwner->GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &UShmovementComponent::OnCapsuleHit);
	
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

	// Calculate the angle between the wall normal and the up vector
	const float WallAngle = FMath::RadiansToDegrees(
		FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector)));

	// Convert to a signed angle where positive is above horizontal and negative is below
	const float SignedWallAngle = 90.0f - WallAngle;

	// Check if the wall angle is within our valid range
	return SignedWallAngle >= MinWallTractionAngle && SignedWallAngle <= MaxWallTractionAngle;
}

void UShmovementComponent::InitWallTraction(const FVector& WallNormal)
{
	SetMovementMode(MOVE_Custom, CMOVE_Walltraction);
	RotateToWallNormal(WallNormal);
}

void UShmovementComponent::RotateToWallNormal(const FVector& WallNormal)
{
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

