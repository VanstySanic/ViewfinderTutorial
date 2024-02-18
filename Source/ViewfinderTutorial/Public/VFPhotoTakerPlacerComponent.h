// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "VFPhoto.h"
#include "VFPhotoTakerPlacerComponent.generated.h"

class UDynamicMesh;
class UStaticMesh;
class UStaticMeshComponent;
class AVFPhoto;
enum class EGeometryScriptBooleanOperation : uint8;

//放置照片操作的信息，仅在放置后生成。
USTRUCT(BlueprintType)
struct FVFPhotoPlaceRecord
{
	GENERATED_BODY()

	/**
	 * 被放置的照片的信息，在存档和时间回溯中均有用。
	 * 在读取存档中，会使用该信息生成照片。
	 */
	UPROPERTY()
	FVFPhotoInfo PhotoInfo;

	//照片被放置时的旋转角度。
	UPROPERTY()
	float PlaceRotatedAngle;

	/**
	 * 照片在被放置时的组件变换，忽略Scale，也就是位置和旋转。这只会被用于存档的照片还原。
	 */
	UPROPERTY()
	FTransform PlaceTransformNoScale;
	
	//
	//下方的变量仅在游戏内用于时间回溯，不会被存档序列化。
	//
	
	UPROPERTY(SkipSerialization)
	TArray<AActor*> SpawnedActors;

	//地图原有Actor被隐藏生成的组件
	UPROPERTY(SkipSerialization)
	TArray<UPrimitiveComponent*> HiddenComponents;

	//在地图原有Actor上生成的组件
	UPROPERTY(SkipSerialization)
	TArray<UPrimitiveComponent*> GeneratedComponents;

	UPROPERTY(SkipSerialization)
	TObjectPtr<AVFPhoto> BackgroundPhoto;

	//此处也可以记录一些用于组件还原状态的变量，如模拟物理和碰撞启用等，如：TMap<UPrimitiveComponent*, bool> ComponentPhysicsMap;
};

/**
 * 
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class VIEWFINDERTUTORIAL_API UVFPhotoTakerPlacerComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UVFPhotoTakerPlacerComponent();

#if WITH_EDITOR
	//在编辑器中改变参数数值将实时体现。
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
protected:
	virtual void BeginPlay() override;

public:
	//执行拍摄照片的流畅并返回照片Actor。
	UFUNCTION(BlueprintCallable, Category = "Viewfinder")
	AVFPhoto* TakePhoto();

	UFUNCTION(BlueprintCallable, Category = "Viewfinder")
	AVFPhoto* TakePhotoWithParamAssigned(const FVFAPhotoTakeParams& Params);

	//按照给定照片的参数放置。
	UFUNCTION(BlueprintCallable, Category = "Viewfinder")
	FVFPhotoPlaceRecord PlacePhoto(AVFPhoto* PhotoToPlace, float RotatedAngle);

	//void PlacePhotoWithParamAssigned(AVFPhoto* PhotoToPlace, float RotatedAngle);
	
	float GetCaptureFOVAngle() const { return DefaultPhotoTakeParams.CaptureFOVAngle; }
	float GetCaptureAspectRatio() const { return DefaultPhotoTakeParams.GetAspectRatio(); }
	
protected:
	void SetPyramidScale(float InFOVAngle, float InMaxDistance, float AspectRatio);
	FTransform GetComponentTransformNoScale() const;
	
	//获取前方与Pyramid重叠的Actors。
	void GetPyramidOverlappingActorsFiltered(TArray<AActor*>& InArray);
	
	//获取前方与Pyramid重叠的组件。
	void GetPyramidOverlappingComponentsFiltered(TArray<UPrimitiveComponent*>& InArray);

	//获取其中所有Primitive组件的DynamicMesh的并集
	UDynamicMesh* CalcMeshRecordForComponents(TArray<UPrimitiveComponent*>& Components);

	/* 处理组件网格体的boolean。
	 * 使用DynamicMeshRecord的有效性判断网格体是地图上现存的还是放置照片时生成的，并进行不同的处理。
	 */
	TArray<UPrimitiveComponent*>  ProcessMeshBooleanToComponents(const TArray<UPrimitiveComponent*>& Components, UDynamicMesh* DynamicMeshRecord = nullptr);

	//组件沿着自身X轴转动此角度
	void ApplyRotatedAngleDelta(float DeltaAngle);

protected:
	//拍摄照片的默认参数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Viewfinder")
	FVFAPhotoTakeParams DefaultPhotoTakeParams;
};
