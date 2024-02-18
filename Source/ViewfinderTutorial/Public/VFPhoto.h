// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VFPhoto.generated.h"

class AVFPhoto;
class UDynamicMesh;

//照片在将要拍摄或是已拍摄的参数。
USTRUCT(BlueprintType)
struct FVFAPhotoTakeParams
{
	GENERATED_BODY()

	FVFAPhotoTakeParams()
	{
		TakeTransformNoScale.SetScale3D(FVector(-1.0));
	}
	
	//拍摄照片时摄像机的FOV。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CaptureFOVAngle = 32.f;
	
	//最远记录距离，只有距离在此数值内的Actor才会被记录信息。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float MaxCaptureDistance = 1600.f;

	/**
	 * 放置照片后，会在远处生成张照片作为背景。此数值为背景照片的距离。
	 * 并且，这个距离内的所有Actor都会被切割。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BackgroundDistance = 32768.f;
	
	//照片捕获的纹理大小，按像素计。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector2D CaptureSize = FVector2D(1024.0, 1024.0);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<AVFPhoto> PhotoClass;

	/**
	 * 照片在拍摄时的组件变换，忽略Scale，也就是位置和旋转。这只会被用于存档的照片还原。
	 * 结构体生成时，TakenTransformNoScale的默认值为-1。
	 * 当TakenTransformNoScale为-1时，代表如果使用此结构体作为参数进行拍摄，则不应该重载组件的默认拍摄变换。
	 * 在照片被拍摄后，这个Scale将被更新为不是-1的数值。
	 */
	UPROPERTY()
	FTransform TakeTransformNoScale;

	bool ShouldOverrideTakeTransform() const { return !TakeTransformNoScale.GetScale3D().Equals(FVector(-1.0));}
	float GetAspectRatio() const { return CaptureSize.X / CaptureSize.Y; }
};

USTRUCT()
struct FVFActorRecord
{
	GENERATED_BODY()

	UPROPERTY(SkipSerialization)
	TSubclassOf<AActor> Class;

	//Actor在被记录时，相对于记录者组件的变换。
	UPROPERTY(SkipSerialization)
	FTransform RelativeTransform;

	//对于地图上的静态网格体组件，大多数情况下，其StaticMesh并非类的默认值，因此需要存储
	UPROPERTY(SkipSerialization)
	TMap<FString, UStaticMesh*> NameToMeshMap;

	//如果有需要，此结构体内可以添加更多需要存储的信息。这些信息一般为在地图中Actor指定的与Default不同的数值。
	//或者说，存储可能与类默认值不同的数据，包括物理质量，材质等等。
	//在TakePhoto中记录，PlacePhoto中处理。
};

USTRUCT()
struct FVFPhotoInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVFAPhotoTakeParams PhotoTakeParams;
	
	//
	//下方的变量仅在游戏内用于时间回溯，不会被存档序列化。
	//
	
	//照片的渲染目标，也会作为照片的材质。同时，它也会被用于区分Photo的不同。
	UPROPERTY(SkipSerialization)
	TObjectPtr<UTexture> RenderTarget;

	//如果照片需要存储背景图片，此为背景图片。注意，它并不会被作为材质。
	UPROPERTY(SkipSerialization)
	TObjectPtr<UTexture> BackgroundRenderTarget;

	//照片拍摄所记录到的Actors。
	UPROPERTY(SkipSerialization)
	TArray<FVFActorRecord> ActorRecords;

	//照片中所有记录到的网格体的集合，生成一个动态网格体。
	UPROPERTY(SkipSerialization)
	TObjectPtr<UDynamicMesh> DynamicMeshRecord;

	bool operator==(const FVFPhotoInfo& B) const
	{
		return RenderTarget == B.RenderTarget;
	}
};

UCLASS()
class VIEWFINDERTUTORIAL_API AVFPhoto : public AActor
{
	GENERATED_BODY()

public:
	AVFPhoto();
	
protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Viewfinder")
	UStaticMeshComponent* GetPhotoMesh() const { return PhotoMesh; }

	void SetPhotoInfo(const FVFPhotoInfo& InPhotoInfo);
	const FVFPhotoInfo& GetPhotoInfo() const { return PhotoInfo; }
	void AddCapturedActor(AActor* Actor, const FTransform& CameraTransform);

	void SetRenderTarget(UTexture* Texture);
	void SetBackgroundRenderTarget(UTexture* Texture) { PhotoInfo.BackgroundRenderTarget = Texture; };

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PhotoMesh;

protected:
	UPROPERTY()
	FVFPhotoInfo PhotoInfo;
};
									