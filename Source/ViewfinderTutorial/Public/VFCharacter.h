// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ViewfinderTutorial/ViewfinderTutorialCharacter.h"
#include "VFCharacter.generated.h"

UCLASS()
class VIEWFINDERTUTORIAL_API AVFCharacter : public AViewfinderTutorialCharacter
{
	GENERATED_BODY()

public:
	AVFCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
