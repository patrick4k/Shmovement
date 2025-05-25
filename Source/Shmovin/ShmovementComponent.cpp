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
	if (CanWallJump())
	{
		InitWallJump(Hit.ImpactNormal);
	}
}

bool UShmovementComponent::CanWallJump() const
{
	return IsFalling();
}

void UShmovementComponent::InitWallJump(FVector WallNormal)
{
	SetMovementMode(MOVE_Custom, CMOVE_WallJump);
	auto Rotation = CalcWallRunRotation(WallNormal);
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

FRotator UShmovementComponent::CalcWallRunRotation(const FVector& WallNormal) const
{
	const double RightProjWallNormal = FVector::DotProduct(CharacterOwner->GetActorRightVector(), WallNormal);
	
	auto WallSide = RightProjWallNormal > 0.0 ? EWallSide::Right : EWallSide::Left;
	
	FVector Y{};
	if (WallSide == EWallSide::Right)
	{
		Y = WallNormal;
	}
	else
	{
		Y = -WallNormal;
	}

	const FVector X = FVector::CrossProduct(Y, CharacterOwner->GetActorUpVector()).GetSafeNormal();

	return FRotationMatrix::MakeFromXY(X, Y).Rotator();
}
