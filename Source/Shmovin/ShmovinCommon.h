// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <locale>

#include "CoreMinimal.h"

/**
 * 
 */
class SHMOVIN_API ShmovinCommon
{
public:
	template<typename T>
	static void DEBUG_LOG(const T& text)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, text);
		}
	}
};
