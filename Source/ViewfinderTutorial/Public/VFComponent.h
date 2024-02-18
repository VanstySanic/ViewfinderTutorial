// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VFPhoto.h"
#include "VFComponent.generated.h"

class UVFPhotoTakerPlacerComponent;
struct FVFPhotoPlaceRecord;
struct FInputActionValue;
struct FVFPhotoInfo;
class AVFPhoto;
class UInputMappingContext;
class UInputAction;

USTRUCT()
struct FVFRewindRecord
{
	GENERATED_BODY()

	UPROPERTY()
	FTransform ActorTransform;

	UPROPERTY()
	FRotator ControlRotation;
	
	//0无，1拍照，2放置，可以写一个枚举但没必要
	UPROPERTY()
	uint8 Action = 0;
	
	TSharedPtr<FVFPhotoInfo> PhotoTakeInfo;
	TSharedPtr<FVFPhotoPlaceRecord> PhotoPlaceRecord;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class VIEWFINDERTUTORIAL_API UVFComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVFComponent();

protected:
	virtual void BeginPlay() override;

public:
	//在使用取景器摄像机和照片之间切换。
	UFUNCTION(BlueprintCallable)
	void ToggleCameraOrPhoto();

	//拍摄照片
	UFUNCTION(BlueprintCallable)
	void TakeOrPlacePhoto();
	
	//瞄准
	UFUNCTION(BlueprintCallable)
	void Aim();

	//取消/结束瞄准
	UFUNCTION(BlueprintCallable)
	void AimEnd();

	//在已拥有的照片之间切换。
	UFUNCTION(BlueprintCallable)
	void SwitchPhoto();

	UFUNCTION(BlueprintCallable)
	void RotatePhoto(const FInputActionValue& InputValue);
	
	//向组件添加新的照片，并进行一些处理
	UFUNCTION(BlueprintCallable)
	void AddPhoto(AVFPhoto* InPhoto);

	//当前照片沿着组件(或者摄像机)的X轴转动此角度
	void ApplyRotatedAngleDeltaToPhoto(float DeltaAngle);

	UFUNCTION()
	void DoRewindRecord();

	UFUNCTION()
	void DoRewind();
	
protected:
	void TakePhotoUsingComponent(UVFPhotoTakerPlacerComponent* InComponent);
	//void PlacePhotoUsingComponent(UVFPhotoTakerPlacerComponent* InComponent);
	void SetCurrentPhotoByIndex(int Index);
	void TakeOutPhoto();
	void WithdrawPhoto();
	void StartRewind();
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputMappingContext> ViewfinderContext;
	
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> ToggleCameraOrPhotoAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> TakeOrPlacePhotoAction;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> AimAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> AimEndAction;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> SwitchPhotoAction;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> RotatePhotoAction;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Input")
	TObjectPtr<UInputAction> BacktrackAction;
	
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Photo")
	TObjectPtr<UStaticMesh> PhotoFrameMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Photo")
	float PhotoPlaceDistance = 32.f;

	//在旋转照片时，单次的旋转角度
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Photo")
	float PhotoRotateAngle = 15.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Rewind")
	float MaxRewindTime = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Rewind")
	float RewindRecordTimeStep = 0.0333333f;

	UPROPERTY(EditDefaultsOnly, Category = "Viewfinder|Rewind")
	float RewindTimeRate = 5.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	bool bIsUsingCamera = true;
	
	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	bool bIsAiming;

	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	bool bIsCatching;

	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	TArray<AVFPhoto*> Photos;

	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	int CurrentPhotoIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Viewfinder")
	float CurrentRotatedAngle = 0.f;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PhotoFrame;

	TArray<FVFRewindRecord> RewindRecords;
	FTimerHandle RewindTimerHandle;
};
