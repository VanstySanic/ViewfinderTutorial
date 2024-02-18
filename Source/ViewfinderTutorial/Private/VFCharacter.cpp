// Fill out your copyright notice in the Description page of Project Settings.


#include "ViewfinderTutorial/Public/VFCharacter.h"

AVFCharacter::AVFCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	
}

void AVFCharacter::BeginPlay()
{
	Super::BeginPlay();

}

void AVFCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVFCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

