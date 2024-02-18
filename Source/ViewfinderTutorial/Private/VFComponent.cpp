// Fill out your copyright notice in the Description page of Project Settings.


#include "VFComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "VFPhoto.h"
#include "VFPhotoTakerPlacerComponent.h"
#include "Kismet/KismetMathLibrary.h"

UVFComponent::UVFComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

}

void UVFComponent::BeginPlay()
{
	Super::BeginPlay();

	//绑定输入
	APlayerController* PlayerController = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (PlayerController)
	{
		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		if (LocalPlayer)
		{
			UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
			Subsystem->AddMappingContext(ViewfinderContext, 10);
		}

		UEnhancedInputComponent* InputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);
		if (InputComponent)
		{
			InputComponent->BindAction(ToggleCameraOrPhotoAction, ETriggerEvent::Triggered, this, &UVFComponent::ToggleCameraOrPhoto);
			InputComponent->BindAction(TakeOrPlacePhotoAction, ETriggerEvent::Triggered, this, &UVFComponent::TakeOrPlacePhoto);
			InputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &UVFComponent::Aim);
			InputComponent->BindAction(AimEndAction, ETriggerEvent::Triggered, this, &UVFComponent::AimEnd);
			InputComponent->BindAction(SwitchPhotoAction, ETriggerEvent::Triggered, this, &UVFComponent::SwitchPhoto);
			InputComponent->BindAction(RotatePhotoAction, ETriggerEvent::Triggered, this, &UVFComponent::RotatePhoto);
			InputComponent->BindAction(BacktrackAction, ETriggerEvent::Triggered, this, &UVFComponent::StartRewind);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(RewindTimerHandle, this, &UVFComponent::DoRewindRecord, RewindRecordTimeStep, true);
}

void UVFComponent::ToggleCameraOrPhoto()
{
	if (bIsAiming || bIsCatching) return;
	
	bIsUsingCamera = !bIsUsingCamera;
}

void UVFComponent::Aim()
{
	if (bIsCatching) return;

	//如果正在使用摄像机，则显示相框，并将其设置在正确的变换上
	if (bIsUsingCamera)
	{
		UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>();
		if (Component)
		{
			PhotoFrame = Cast<UStaticMeshComponent>(GetOwner()->AddComponentByClass(UStaticMeshComponent::StaticClass(), true, FTransform(), false));
			if (PhotoFrame)
			{
				PhotoFrame->SetStaticMesh(PhotoFrameMesh);
				PhotoFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PhotoFrame->SetHiddenInSceneCapture(true);

				PhotoFrame->SetWorldLocation(Component->GetComponentLocation() + Component->GetForwardVector() * PhotoPlaceDistance);
				PhotoFrame->SetWorldRotation(Component->GetComponentRotation());

				float BaseScaleXY = PhotoPlaceDistance / 100.f *  UKismetMathLibrary::DegTan(Component->GetCaptureFOVAngle() / 2.f);
				const FVector2D& AspectRatioScale = FVector2D(Component->GetCaptureAspectRatio() > 1.f ? 1.f : Component->GetCaptureAspectRatio(), Component->GetCaptureAspectRatio() < 1.f ? 1.f : 1.f / Component->GetCaptureAspectRatio());
				PhotoFrame->SetWorldScale3D(FVector(1.0, BaseScaleXY * AspectRatioScale.X, BaseScaleXY * AspectRatioScale.Y));

				PhotoFrame->AttachToComponent(Component, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}
	else
	{
		TakeOutPhoto();
	}

	bIsAiming = true;
}

void UVFComponent::AimEnd()
{
	if (bIsUsingCamera)
	{
		if (PhotoFrame)
		{
			PhotoFrame->DestroyComponent();
		}
	}
	else
	{
		CurrentRotatedAngle = 0.f;
		WithdrawPhoto();
	}

	bIsAiming = false;
}

void UVFComponent::SwitchPhoto()
{
	if (bIsUsingCamera || bIsAiming || bIsCatching) return;
	
	int TargetIndex;
	if (Photos.Num())
	{
		TargetIndex = (CurrentPhotoIndex + 1) % Photos.Num();
	}
	else
	{
		TargetIndex = -1;
	}
	SetCurrentPhotoByIndex(TargetIndex);
}

void UVFComponent::RotatePhoto(const FInputActionValue& InputValue)
{
	if (bIsUsingCamera || !bIsAiming || bIsCatching) return;

	const float Value = InputValue.Get<float>();
	const float Prev = CurrentRotatedAngle;
	CurrentRotatedAngle = CurrentRotatedAngle + (Value > 0.f ? PhotoRotateAngle : -PhotoRotateAngle);
	const float Delta = CurrentRotatedAngle - Prev;
	ApplyRotatedAngleDeltaToPhoto(Delta);

	
}

void UVFComponent::TakeOrPlacePhoto()
{
	if (!bIsAiming || bIsCatching) return;
	
	UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>();
	if (!Component) return;
		
	if (bIsUsingCamera)
	{
		TakePhotoUsingComponent(Component);
		AimEnd();
	}
	else if (Photos.IsValidIndex(CurrentPhotoIndex))
	{
		//放置照片
		AVFPhoto* Photo = Photos[CurrentPhotoIndex];
		FVFPhotoPlaceRecord PhotoPlaceRecord = Component->PlacePhoto(Photo, CurrentRotatedAngle);
		RewindRecords.Last().Action = 2;
		RewindRecords.Last().PhotoPlaceRecord = MakeShared<FVFPhotoPlaceRecord>(PhotoPlaceRecord);
		
		//移除照片
		Photo->Destroy();
		Photos.RemoveAt(CurrentPhotoIndex);
		SetCurrentPhotoByIndex(Photos.IsValidIndex(CurrentPhotoIndex) ? CurrentPhotoIndex : (Photos.IsValidIndex(CurrentPhotoIndex - 1) ? CurrentPhotoIndex - 1 : CurrentPhotoIndex + 1));
	}
}

void UVFComponent::StartRewind()
{
	for (const FVFRewindRecord& BacktrackRecord : RewindRecords)
	{
		if (BacktrackRecord.Action != 0)
		{
			APlayerController* PlayerController = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
			if (PlayerController)
			{
				PlayerController->DisableInput(PlayerController);
			}
			
			GetWorld()->GetTimerManager().ClearTimer(RewindTimerHandle);
			GetWorld()->GetTimerManager().SetTimer(RewindTimerHandle, this, &UVFComponent::DoRewind, RewindRecordTimeStep / RewindTimeRate, true);
			return;
		}
	}
}

void UVFComponent::AddPhoto(AVFPhoto* InPhoto)
{
	Photos.Emplace(InPhoto);
	SetCurrentPhotoByIndex(Photos.Num() - 1);
	if (UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>())
	{
		InPhoto->AttachToComponent(Component, FAttachmentTransformRules::KeepWorldTransform);
		WithdrawPhoto();
	}
}

void UVFComponent::ApplyRotatedAngleDeltaToPhoto(float DeltaAngle)
{
	UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>();
	if (!Component) return;

	if (!Photos.IsValidIndex(CurrentPhotoIndex)) return;
	AVFPhoto* Photo = Photos[CurrentPhotoIndex];
	
	FVector RotationAxis = Component->GetForwardVector();
	float RotationAngleDegrees = DeltaAngle;
	float RotationAngleRadians = FMath::DegreesToRadians(RotationAngleDegrees);
	FQuat QuatRotation = FQuat(RotationAxis, RotationAngleRadians);
	FQuat CurrentRotation = FQuat(Photo->GetActorRotation());
	FQuat NewRotation = QuatRotation * CurrentRotation;
	Photo->SetActorRotation(NewRotation);
}

void UVFComponent::DoRewindRecord()
{
	FVFRewindRecord BacktrackRecord;
	BacktrackRecord.ActorTransform = GetOwner()->GetActorTransform();
	BacktrackRecord.ControlRotation = Cast<APawn>(GetOwner())->GetControlRotation();

	RewindRecords.Emplace(BacktrackRecord);
	if (RewindRecords.Num() > MaxRewindTime / RewindRecordTimeStep)
	{
		RewindRecords.RemoveAt(0);
	}
}

void UVFComponent::DoRewind()
{
	if (RewindRecords.Num() == 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(RewindTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(RewindTimerHandle, this, &UVFComponent::DoRewindRecord, RewindRecordTimeStep, true);
		return;
	}

	const FVFRewindRecord& RewindRecord = RewindRecords.Last();
	GetOwner()->SetActorTransform(RewindRecord.ActorTransform);
	Cast<APawn>(GetOwner())->GetController()->SetControlRotation(RewindRecord.ControlRotation);
	
	if (RewindRecord.Action == 0)
	{
		RewindRecords.RemoveAt(RewindRecords.Num() - 1);
		return;
	}
	if (RewindRecord.Action == 1)
	{
		if (RewindRecord.PhotoTakeInfo)
		{
			for (AVFPhoto* Photo : Photos)
			{
				if (*RewindRecord.PhotoTakeInfo == Photo->GetPhotoInfo())
				{
					if (Photo)
					{
						Photos.Remove(Photo);
						SetCurrentPhotoByIndex(Photos.IsValidIndex(CurrentPhotoIndex) ? CurrentPhotoIndex : (Photos.IsValidIndex(CurrentPhotoIndex - 1) ? CurrentPhotoIndex - 1 : CurrentPhotoIndex + 1));
						Photo->Destroy();
					}
					break;
				}
			}
		}
	}
	else if (RewindRecord.Action == 2)
	{
		TSharedPtr<FVFPhotoPlaceRecord> PhotoPlaceRecord = RewindRecord.PhotoPlaceRecord;
		if (PhotoPlaceRecord)
		{
			for (UPrimitiveComponent* GeneratedComponent : PhotoPlaceRecord->GeneratedComponents)
			{
				GeneratedComponent->DestroyComponent();
			}

			for (AActor* SpawnedActor : PhotoPlaceRecord->SpawnedActors)
			{
				SpawnedActor->Destroy();
			}

			for (UPrimitiveComponent* HiddenComponent : PhotoPlaceRecord->HiddenComponents)
			{
				HiddenComponent->SetVisibility(true);
				HiddenComponent->SetGenerateOverlapEvents(true);
				HiddenComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}

			if (PhotoPlaceRecord && PhotoPlaceRecord->BackgroundPhoto)
			{
				PhotoPlaceRecord->BackgroundPhoto->Destroy();
			}

			if (PhotoPlaceRecord && PhotoPlaceRecord->PhotoInfo.PhotoTakeParams.PhotoClass)
			{
				AVFPhoto* Photo = Cast<AVFPhoto>(GetWorld()->SpawnActor(PhotoPlaceRecord->PhotoInfo.PhotoTakeParams.PhotoClass));
				Photo->SetPhotoInfo(PhotoPlaceRecord->PhotoInfo);
				AddPhoto(Photo);
			}
		}
	}

	if (RewindRecords.Num())
	{
		RewindRecords.RemoveAt(RewindRecords.Num() - 1);
	}
	APlayerController* PlayerController = Cast<APlayerController>(Cast<APawn>(GetOwner())->GetController());
	if (PlayerController)
	{
		PlayerController->EnableInput(PlayerController);
	}
	GetWorld()->GetTimerManager().ClearTimer(RewindTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RewindTimerHandle, this, &UVFComponent::DoRewindRecord, RewindRecordTimeStep, true);
}

void UVFComponent::TakePhotoUsingComponent(UVFPhotoTakerPlacerComponent* InComponent)
{
	AVFPhoto* Photo = InComponent->TakePhoto();
	AddPhoto(Photo);
		
	RewindRecords.Last().Action = 1;
	RewindRecords.Last().PhotoTakeInfo = MakeShared<FVFPhotoInfo>(Photo->GetPhotoInfo());
}

void UVFComponent::SetCurrentPhotoByIndex(int Index)
{
	if (Photos.IsValidIndex(CurrentPhotoIndex))
	{
		AVFPhoto* Photo = Photos[CurrentPhotoIndex];
		Photo->GetPhotoMesh()->SetVisibility(false);
	}
	
	if (!Photos.IsValidIndex(Index)) return;
	CurrentPhotoIndex = Index;

	AVFPhoto* Photo = Photos[CurrentPhotoIndex];
	Photo->GetPhotoMesh()->SetVisibility(true);
}

void UVFComponent::TakeOutPhoto()
{
	UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>();
	if (!Component) return;

	if (!Photos.IsValidIndex(CurrentPhotoIndex)) return;
	AVFPhoto* Photo = Photos[CurrentPhotoIndex];

	Photo->SetActorLocation(Component->GetComponentLocation() + Component->GetForwardVector() * PhotoPlaceDistance);
	Photo->SetActorRotation(Component->GetComponentRotation());
	ApplyRotatedAngleDeltaToPhoto(CurrentRotatedAngle);

	const float AspectRatio = Photo->GetPhotoInfo().PhotoTakeParams.GetAspectRatio();
	float BaseScaleXY = PhotoPlaceDistance / 100.f *  UKismetMathLibrary::DegTan(Component->GetCaptureFOVAngle() / 2.f);
	const FVector2D& AspectRatioScale = FVector2D(Photo->GetPhotoInfo().PhotoTakeParams.GetAspectRatio() > 1.f ? 1.f : AspectRatio, AspectRatio < 1.f ? 1.f : 1.f / AspectRatio);
	Photo->SetActorScale3D(FVector(1.0, BaseScaleXY * AspectRatioScale.X, BaseScaleXY * AspectRatioScale.Y));
}

void UVFComponent::WithdrawPhoto()
{
	UVFPhotoTakerPlacerComponent* Component = GetOwner()->FindComponentByClass<UVFPhotoTakerPlacerComponent>();
	if (!Component) return;

	if (!Photos.IsValidIndex(CurrentPhotoIndex)) return;
	AVFPhoto* Photo = Photos[CurrentPhotoIndex];

	Photo->SetActorLocation(Component->GetComponentLocation() + Component->GetForwardVector() * PhotoPlaceDistance + Component->GetRightVector() * -16.f + Component->GetUpVector() * -8.f);
	Photo->SetActorRotation(Component->GetComponentRotation());

	const float AspectRatio = Photo->GetPhotoInfo().PhotoTakeParams.GetAspectRatio();
	const FVector2D& AspectRatioScale = FVector2D(AspectRatio > 1.f ? 1.f : AspectRatio, AspectRatio < 1.f ? 1.f : 1.f / AspectRatio);
	Photo->SetActorScale3D(FVector(1.0, 0.036 * AspectRatioScale.X, 0.036 * AspectRatioScale.Y));
}
