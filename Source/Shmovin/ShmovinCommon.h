// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class SHMOVIN_API ShmovinCommon
{
public:
	static void DEBUG_LOG(const FString& str)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, str);
		}
	}
};

#define SHMOVIN_DEBUG_LOG(format) ::ShmovinCommon::DEBUG_LOG(TEXT(format))

#define SHMOVIN_DEBUG_FMT(format, ...) ::ShmovinCommon::DEBUG_LOG(FString::Printf(TEXT(format), __VA_ARGS__))
