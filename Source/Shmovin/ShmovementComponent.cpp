// Fill out your copyright notice in the Description page of Project Settings.


#include "ShmovementComponent.h"

#include <GameFramework/Character.h>
#include <Components/CapsuleComponent.h>

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
		InitWallJump();
	}
}

bool UShmovementComponent::CanWallJump() const
{
	return IsFalling();
}

void UShmovementComponent::InitWallJump()
{
	SetMovementMode(MOVE_Custom, CMOVE_WallJump);
}
