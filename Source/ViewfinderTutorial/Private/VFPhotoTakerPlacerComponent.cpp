// Fill out your copyright notice in the Description page of Project Settings.

#include "VFPhotoTakerPlacerComponent.h"
#include "VFPhoto.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/KismetMathLibrary.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshComparisonFunctions.h"

UVFPhotoTakerPlacerComponent::UVFPhotoTakerPlacerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

#if WITH_EDITOR
	SetCollisionProfileName(FName("OverlapAll"));
	UpdateCollisionProfile();
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
#endif

	SetGenerateOverlapEvents(true);

	ComponentTags.Emplace(FName("NonCapture"));
}

#if WITH_EDITOR
void UVFPhotoTakerPlacerComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UVFPhotoTakerPlacerComponent, DefaultPhotoTakeParams))
	{
		SetPyramidScale(DefaultPhotoTakeParams.CaptureFOVAngle, DefaultPhotoTakeParams.MaxCaptureDistance, GetCaptureAspectRatio());
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UVFPhotoTakerPlacerComponent::BeginPlay()
{
	Super::BeginPlay();

	SetCollisionProfileName(FName("OverlapAll"));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	SetVisibility(false);
}

AVFPhoto* UVFPhotoTakerPlacerComponent::TakePhoto()
{
	return TakePhotoWithParamAssigned(DefaultPhotoTakeParams);
}

AVFPhoto* UVFPhotoTakerPlacerComponent::TakePhotoWithParamAssigned(const FVFAPhotoTakeParams& Params)
{
	if (!Params.PhotoClass) return nullptr;

	const bool bShouldOverrideTakeTransform = Params.ShouldOverrideTakeTransform();
	if (bShouldOverrideTakeTransform)
	{
		SetWorldLocation(Params.TakeTransformNoScale.GetLocation());
		SetWorldRotation(Params.TakeTransformNoScale.GetRotation());
	}
	
	SetPyramidScale(Params.CaptureFOVAngle, Params.MaxCaptureDistance, Params.GetAspectRatio());
	
	//生成Photo Actor并拍摄照片
	AVFPhoto* Photo = Cast<AVFPhoto>(GetWorld()->SpawnActor(Params.PhotoClass));
	if (!Photo) return nullptr;
	
	ASceneCapture2D* SceneCapture = Cast<ASceneCapture2D>(GetWorld()->SpawnActor(ASceneCapture2D::StaticClass()));
	if (!SceneCapture)
	{
		Photo->Destroy();
		return nullptr;
	}
	
	SceneCapture->SetActorLocation(GetComponentLocation());
	SceneCapture->SetActorRotation(GetComponentRotation());
	SceneCapture->GetCaptureComponent2D()->FOVAngle = Params.CaptureFOVAngle;
	SceneCapture->GetCaptureComponent2D()->bCaptureEveryFrame = false;
	SceneCapture->GetCaptureComponent2D()->bCaptureOnMovement = false;
	SceneCapture->GetCaptureComponent2D()->bAlwaysPersistRenderingState = true;

	TArray<UPrimitiveComponent*> CurrentOverlappingComponents;
	GetPyramidOverlappingComponentsFiltered(CurrentOverlappingComponents);
	
	//对场景捕获隐藏这些组件，拍摄一张背景
	UTextureRenderTarget2D* BackgroundRenderTarget = NewObject<UTextureRenderTarget2D>(this);
	BackgroundRenderTarget->InitAutoFormat(Params.CaptureSize.X, Params.CaptureSize.Y);
	SceneCapture->GetCaptureComponent2D()->TextureTarget = BackgroundRenderTarget;
	for (UPrimitiveComponent* CurrentOverlappingComponent : CurrentOverlappingComponents)
	{
		CurrentOverlappingComponent->SetHiddenInSceneCapture(true);
	}
	SceneCapture->GetCaptureComponent2D()->CaptureScene();

	//还原这些Actor的对场景捕获的显示，拍摄照片
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->InitAutoFormat(Params.CaptureSize.X, Params.CaptureSize.Y);
	SceneCapture->GetCaptureComponent2D()->TextureTarget = RenderTarget;
	for (UPrimitiveComponent* CurrentOverlappingComponent : CurrentOverlappingComponents)
	{
		CurrentOverlappingComponent->SetHiddenInSceneCapture(false);
	}
	SceneCapture->GetCaptureComponent2D()->CaptureScene();
	SceneCapture->Destroy();

	//初始化照片信息
	FVFPhotoInfo PhotoInfo;
	PhotoInfo.PhotoTakeParams = Params;
	PhotoInfo.PhotoTakeParams.TakeTransformNoScale = GetComponentTransformNoScale();
	PhotoInfo.RenderTarget = RenderTarget;
	PhotoInfo.BackgroundRenderTarget = BackgroundRenderTarget;
	PhotoInfo.DynamicMeshRecord = CalcMeshRecordForComponents(CurrentOverlappingComponents);

	Photo->SetPhotoInfo(PhotoInfo);
	
	TArray<AActor*> OverlappingActors;
	GetPyramidOverlappingActorsFiltered(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		Photo->AddCapturedActor(OverlappingActor, GetComponentTransform());
	}

	//还原组件变换
	if (bShouldOverrideTakeTransform)
	{
		SetRelativeLocation(FVector());
		SetRelativeRotation(FRotator());
	}
	
	return Photo;
}

FVFPhotoPlaceRecord UVFPhotoTakerPlacerComponent::PlacePhoto(AVFPhoto* PhotoToPlace, float RotatedAngle)
{
	if (!PhotoToPlace) return FVFPhotoPlaceRecord();

	FVFPhotoPlaceRecord PhotoPlaceRecord;
	PhotoPlaceRecord.PlaceTransformNoScale = GetComponentTransformNoScale();

	const FVFPhotoInfo& PhotoInfo = PhotoToPlace->GetPhotoInfo();
	PhotoPlaceRecord.PhotoInfo = PhotoInfo;
	
	ApplyRotatedAngleDelta(RotatedAngle);
	PhotoPlaceRecord.PlaceRotatedAngle = RotatedAngle;

	//存储地图中原有的与Pyramid重叠的组件
	SetPyramidScale(PhotoInfo.PhotoTakeParams.CaptureFOVAngle, PhotoInfo.PhotoTakeParams.BackgroundDistance, PhotoInfo.PhotoTakeParams.GetAspectRatio());
	TArray<UPrimitiveComponent*> LevelOverlappingComponents;
	GetPyramidOverlappingComponentsFiltered(LevelOverlappingComponents);
	PhotoPlaceRecord.HiddenComponents.Append(LevelOverlappingComponents);
	//对地图上原来存在的Actor进行切割，剔除与Pyramid重叠的部分
	PhotoPlaceRecord.GeneratedComponents.Append(ProcessMeshBooleanToComponents(LevelOverlappingComponents, nullptr));

	//生成照片中的Actors
	SetPyramidScale(PhotoInfo.PhotoTakeParams.CaptureFOVAngle, PhotoInfo.PhotoTakeParams.MaxCaptureDistance, PhotoInfo.PhotoTakeParams.GetAspectRatio());
	TArray<AActor*> ActorSpawned;
	for (const FVFActorRecord& ActorRecord : PhotoInfo.ActorRecords)
	{
		const FTransform& WorldTransform = UKismetMathLibrary::ComposeTransforms(ActorRecord.RelativeTransform, GetComponentTransform());
		AActor* Actor = GetWorld()->SpawnActor(ActorRecord.Class, &WorldTransform);
		if (Actor)
		{
			ActorSpawned.Emplace(Actor);

			//场景中大多数的Actor都是StaticMeshActor
			if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
			{
				UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->GetStaticMeshComponent();
				if (ActorRecord.NameToMeshMap.Contains(FString("StaticMeshComponent")))
				{
					EComponentMobility::Type PrevMobility = StaticMeshComponent->Mobility;
					StaticMeshComponent->SetMobility(EComponentMobility::Movable);
					StaticMeshComponent->SetStaticMesh(ActorRecord.NameToMeshMap[FString("StaticMeshComponent")]);
					StaticMeshComponent->SetGenerateOverlapEvents(true);
					StaticMeshComponent->SetMobility(PrevMobility);
					StaticMeshComponent->UpdateOverlaps();
				}
			}
			else
			{
				TArray<UStaticMeshComponent*> Components;
				Actor->GetComponents<UStaticMeshComponent>(Components);
				for (UStaticMeshComponent* StaticMeshComponent : Components)
				{
					EComponentMobility::Type PrevMobility = StaticMeshComponent->Mobility;
					StaticMeshComponent->SetMobility(EComponentMobility::Movable);
					StaticMeshComponent->SetStaticMesh(ActorRecord.NameToMeshMap[StaticMeshComponent->GetName()]);
					StaticMeshComponent->SetGenerateOverlapEvents(true);
					StaticMeshComponent->SetMobility(PrevMobility);
					StaticMeshComponent->UpdateOverlaps();
				}
				/*for (FProperty* Property = Actor->GetClass()->PropertyLink; Property; Property = Property->PropertyLinkNext)
				{
					FString ClassName = Property->GetCPPType();
					if (!ClassName.Contains(FString("StaticMeshComponent"))) continue;

					FString PropertyName = Property->GetName();
					FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
					void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Actor);
					UObject* Value = ObjectProperty->GetPropertyValue(ValuePtr);
					if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Value))
					{
						if (ActorRecord.NameToMeshMap.Contains(PropertyName))
						{
							EComponentMobility::Type PrevMobility = StaticMeshComponent->Mobility;
							StaticMeshComponent->SetMobility(EComponentMobility::Movable);
							StaticMeshComponent->SetStaticMesh(ActorRecord.NameToMeshMap[PropertyName]);
							StaticMeshComponent->SetGenerateOverlapEvents(true);
							StaticMeshComponent->SetMobility(PrevMobility);
							StaticMeshComponent->UpdateOverlaps();
						}
					}
				}*/
			}
		}
	}
	PhotoPlaceRecord.SpawnedActors = ActorSpawned;
	
	//存储生成Actor后与Pyramid重叠的组件，需要排除前面原有的组件
	TArray<UPrimitiveComponent*> GeneratedOverlappingComponents;
	GetPyramidOverlappingComponentsFiltered(GeneratedOverlappingComponents);
	for (UPrimitiveComponent* GeneratedOverlappingComponent : GeneratedOverlappingComponents)
	{
		if (!ActorSpawned.Contains(GeneratedOverlappingComponent->GetOwner()))
		{
			GeneratedOverlappingComponents.Remove(GeneratedOverlappingComponent);
		}
	}
	//PhotoPlaceRecord.HiddenComponents.Append(GeneratedOverlappingComponents);
	//对生成的Actor已重叠的组件进行切割，保留与Pyramid重叠的部分。
	//PhotoPlaceRecord.GeneratedComponents.Append(ProcessMeshBooleanToComponents(GeneratedOverlappingComponents, PhotoInfo.DynamicMeshRecord));
	ProcessMeshBooleanToComponents(GeneratedOverlappingComponents, PhotoInfo.DynamicMeshRecord);

	//在远处生成一张背景照片
	FTransform BackgroundTransform;
	const float ScaleZ = PhotoInfo.PhotoTakeParams.BackgroundDistance / 100.f;
	const float BaseScaleXY = ScaleZ * UKismetMathLibrary::DegTan(PhotoInfo.PhotoTakeParams.CaptureFOVAngle / 2.f);
	const float AspectRatio = GetCaptureAspectRatio();
	BackgroundTransform.SetLocation(GetComponentLocation() + PhotoInfo.PhotoTakeParams.BackgroundDistance * GetForwardVector());
	BackgroundTransform.SetRotation(GetComponentQuat());
	BackgroundTransform.SetScale3D(FVector(
		ScaleZ,
		BaseScaleXY * (AspectRatio > 1.f ? 1.f : AspectRatio),
		BaseScaleXY * (AspectRatio < 1.f ? 1.f : 1.f / AspectRatio)));
	
	AVFPhoto* BackgroundPhoto = Cast<AVFPhoto>(GetWorld()->SpawnActor(PhotoInfo.PhotoTakeParams.PhotoClass, &BackgroundTransform));
	BackgroundPhoto->SetRenderTarget(PhotoInfo.BackgroundRenderTarget);
	//背景图片的重叠需要启用
	BackgroundPhoto->GetPhotoMesh()->SetCollisionProfileName(FName("OverlapAll"));
	BackgroundPhoto->GetPhotoMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BackgroundPhoto->GetPhotoMesh()->SetGenerateOverlapEvents(true);
	BackgroundPhoto->GetPhotoMesh()->SetHiddenInSceneCapture(false);

	PhotoPlaceRecord.BackgroundPhoto = BackgroundPhoto;
	
	//还原组件变换
	//SetPyramidScale(DefaultPhotoTakeParams.CaptureFOVAngle, DefaultPhotoTakeParams.MaxCaptureDistance, GetCaptureAspectRatio());
	ApplyRotatedAngleDelta(-RotatedAngle);

	return PhotoPlaceRecord;
}

void UVFPhotoTakerPlacerComponent::SetPyramidScale(float InFOVAngle, float InMaxDistance, float AspectRatio)
{
	const float ScaleZ = InMaxDistance / 100.f;
	const float BaseScaleXY = ScaleZ * UKismetMathLibrary::DegTan(InFOVAngle / 2.f);

	SetWorldScale3D(FVector(
		ScaleZ,
		BaseScaleXY * (AspectRatio > 1.f ? 1.f : AspectRatio),
		BaseScaleXY * (AspectRatio < 1.f ? 1.f : 1.f / AspectRatio)));
}

FTransform UVFPhotoTakerPlacerComponent::GetComponentTransformNoScale() const
{
	FTransform Transform = GetComponentTransform();
	Transform.SetScale3D(FVector(1.0));
	return Transform;
}

void UVFPhotoTakerPlacerComponent::GetPyramidOverlappingActorsFiltered(TArray<AActor*>& InArray)
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UpdateOverlaps();
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//此处对重叠的Actor进行过滤
	InArray.Empty();
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (OverlappingActor->ActorHasTag(FName("NonCapture"))) continue;
		
		InArray.Emplace(OverlappingActor);
	}
}

void UVFPhotoTakerPlacerComponent::GetPyramidOverlappingComponentsFiltered(TArray<UPrimitiveComponent*>& InArray)
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UpdateOverlaps();
	TArray<UPrimitiveComponent*> CurrentOverlappingComponents;
	GetOverlappingComponents(CurrentOverlappingComponents);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	//此处对重叠的组件进行过滤
	InArray.Empty();
	for (UPrimitiveComponent* OverlappingComponent : CurrentOverlappingComponents)
	{
		if (OverlappingComponent->ComponentHasTag(FName("NonCapture"))) continue;
		if (OverlappingComponent->GetOwner()->ActorHasTag(FName("NonCapture"))) continue;
		
		InArray.Emplace(OverlappingComponent);
	}
}

UDynamicMesh* UVFPhotoTakerPlacerComponent::CalcMeshRecordForComponents(TArray<UPrimitiveComponent*>& Components)
{
	UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>(this);
	UDynamicMesh* TempDynamicMesh = NewObject<UDynamicMesh>(this);
	
	for (UPrimitiveComponent* Component : Components)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			TEnumAsByte<EGeometryScriptOutcomePins> Pins;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				StaticMeshComponent->GetStaticMesh(),
				TempDynamicMesh,
				FGeometryScriptCopyMeshFromAssetOptions(),
				FGeometryScriptMeshReadLOD(),
				Pins);

			UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
				DynamicMesh,
				FTransform(),
				TempDynamicMesh,
				//个人感觉应该不需要Inverse，但实际上需要Inverse才正确，大概是因为我不清楚它的算法
				GetComponentTransformNoScale().GetRelativeTransform(StaticMeshComponent->GetComponentTransform()).Inverse(),
				EGeometryScriptBooleanOperation::Union,
				FGeometryScriptMeshBooleanOptions());
		}
		else if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
		{
			UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
				DynamicMesh,
				FTransform(),
				DynamicMeshComponent->GetDynamicMesh(),
				GetComponentTransformNoScale().GetRelativeTransform(DynamicMeshComponent->GetComponentTransform()).Inverse(),
				EGeometryScriptBooleanOperation::Union,
				FGeometryScriptMeshBooleanOptions());
		}
	}

	return DynamicMesh;
}

TArray<UPrimitiveComponent*> UVFPhotoTakerPlacerComponent::ProcessMeshBooleanToComponents(const TArray<UPrimitiveComponent*>& Components, UDynamicMesh* DynamicMeshRecord)
{
	TArray<UPrimitiveComponent*> GeneratedComponents;
	
	//以DynamicMeshRecord是否有效传入为依据，判断是处理地图中的组件还是生成的组件
	bool bAreComponentsGenerated = DynamicMeshRecord != nullptr;
	
	//为Pyramid生成动态网格体组件，用于模型运算。
	UDynamicMeshComponent* PyramidDynamicMesh = NewObject<UDynamicMeshComponent>(this);
	if (PyramidDynamicMesh)
	{
		TEnumAsByte<EGeometryScriptOutcomePins> Pins;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
			GetStaticMesh(),
			PyramidDynamicMesh->GetDynamicMesh(),
			FGeometryScriptCopyMeshFromAssetOptions(),
			FGeometryScriptMeshReadLOD(),
			Pins);
	}

	UDynamicMesh* PrevMesh = NewObject<UDynamicMesh>(this);

	for (UPrimitiveComponent* Component : Components)
	{
		const FCollisionResponseContainer& CollisionResponseContainer = Component->GetCollisionResponseToChannels();
		const ECollisionEnabled::Type CollisionEnabled = Component->GetCollisionEnabled();
		const bool bPhysicsEnabled = Component->IsSimulatingPhysics();
		
		//隐藏地图中原有的模型
		Component->SetVisibility(false);
		Component->SetGenerateOverlapEvents(false);
		//Component->SetSimulatePhysics(false);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UDynamicMesh* TargetMesh = NewObject<UDynamicMesh>(this);

		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			TEnumAsByte<EGeometryScriptOutcomePins> Pins;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
				StaticMeshComponent->GetStaticMesh(),
				PrevMesh,
				FGeometryScriptCopyMeshFromAssetOptions(),
				FGeometryScriptMeshReadLOD(),
				Pins);
		}
		else if (UDynamicMeshComponent* DynamicMeshComponent = Cast<UDynamicMeshComponent>(Component))
		{
			UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(
				DynamicMeshComponent->GetDynamicMesh(),
				PrevMesh,
				PrevMesh);
		}

		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(
				PrevMesh,
				TargetMesh,
				TargetMesh);
		
		//如果是放置照片的生成Actor阶段，则需要与照片中存储的动态网格体进行一次相交
		if (bAreComponentsGenerated)
		{
			UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
			TargetMesh,
			Component->GetComponentTransform(),
			DynamicMeshRecord,
			GetComponentTransformNoScale(),
			EGeometryScriptBooleanOperation::Intersection,
			FGeometryScriptMeshBooleanOptions());
		}
			
		//与视口Pyramid进行相交/相减
		UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
			TargetMesh,
			Component->GetComponentTransform(),
			PyramidDynamicMesh->GetDynamicMesh(),
			GetComponentTransform(),
			bAreComponentsGenerated ? EGeometryScriptBooleanOperation::Intersection : EGeometryScriptBooleanOperation::Subtract,
			FGeometryScriptMeshBooleanOptions());

		bool bIsSameMesh;
		UGeometryScriptLibrary_MeshComparisonFunctions::IsSameMeshAs(PrevMesh, TargetMesh, FGeometryScriptIsSameMeshOptions(), bIsSameMesh);
		//只有在模型进行boolean操作的过程中完全消除，即三角面为0时，此时不需要创建动态网格体
		if (bIsSameMesh) continue;

		//之所以不直接对DynamicMeshComponent进行操作，而是也要生成新的动态网格体，是考虑到时间回溯。
		UDynamicMeshComponent* NewDynamicMeshComponent = Cast<UDynamicMeshComponent>(Component->GetOwner()->AddComponentByClass(UDynamicMeshComponent::StaticClass(), true, FTransform(), false));
		if (NewDynamicMeshComponent)
		{
			GeneratedComponents.Emplace(NewDynamicMeshComponent);
			
			NewDynamicMeshComponent->RegisterComponent();
			NewDynamicMeshComponent->SetWorldTransform(Component->GetComponentTransform());
			NewDynamicMeshComponent->SetDynamicMesh(TargetMesh);
			
			/**
			 * 一般情况下，需要模拟物理的Actor通常只有根组件。
			 * 如果根组件开启了模拟物理，则新的动态网格体需要代替根组件进行模拟物理，所以需要将根组件设置为动态网格体。
			 */
			/*if (bPhysicsEnabled && Component == Component->GetOwner()->GetRootComponent())
			{
				Component->GetOwner()->SetRootComponent(NewDynamicMeshComponent);
				Component->AttachToComponent(NewDynamicMeshComponent, FAttachmentTransformRules::KeepWorldTransform);
			}
			else
			{
				NewDynamicMeshComponent->AttachToComponent(Component->GetAttachParent() ? Component->GetAttachParent() : Component, FAttachmentTransformRules::KeepWorldTransform);
			}*/
			
			NewDynamicMeshComponent->AttachToComponent(Component->GetAttachParent() ? Component->GetAttachParent() : Component, FAttachmentTransformRules::KeepWorldTransform);
			NewDynamicMeshComponent->SetCollisionResponseToChannels(CollisionResponseContainer);
			NewDynamicMeshComponent->SetCollisionEnabled(CollisionEnabled);
			NewDynamicMeshComponent->SetGenerateOverlapEvents(true);
			NewDynamicMeshComponent->SetSimulatePhysics(bPhysicsEnabled);
			
			//物理模拟无法开启复杂碰撞，但是动态网格体的简单碰撞不知道怎么手动生成。
			if (!bPhysicsEnabled)
			{
				NewDynamicMeshComponent->EnableComplexAsSimpleCollision();
			}
			
			for (int i = 0;i < Component->GetNumMaterials();i++)
			{
				NewDynamicMeshComponent->SetMaterial(i, Component->GetMaterial(i));
			}

			//TODO 想办法在进行Boolean操作后，生成动态网格体的简单碰撞
			NewDynamicMeshComponent->UpdateCollision(false);
		}
	}
	
	PyramidDynamicMesh->DestroyComponent();
	return GeneratedComponents;
}

void UVFPhotoTakerPlacerComponent::ApplyRotatedAngleDelta(float DeltaAngle)
{
	FVector RotationAxis = GetForwardVector();
	float RotationAngleDegrees = DeltaAngle;
	float RotationAngleRadians = FMath::DegreesToRadians(RotationAngleDegrees);
	FQuat QuatRotation = FQuat(RotationAxis, RotationAngleRadians);
	FQuat CurrentRotation = FQuat(GetComponentRotation());
	FQuat NewRotation = QuatRotation * CurrentRotation;
	SetWorldRotation(NewRotation);
}
