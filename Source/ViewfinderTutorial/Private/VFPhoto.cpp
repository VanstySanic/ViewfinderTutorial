// Fill out your copyright notice in the Description page of Project Settings.


#include "VFPhoto.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetMathLibrary.h"

AVFPhoto::AVFPhoto()
{
	PrimaryActorTick.bCanEverTick = true;

	PhotoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhotoMesh"));
	SetRootComponent(PhotoMesh);

	PhotoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PhotoMesh->SetHiddenInSceneCapture(true);
	PhotoMesh->SetCastShadow(false);
}

void AVFPhoto::BeginPlay()
{
	Super::BeginPlay();
	
}

void AVFPhoto::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AVFPhoto::SetPhotoInfo(const FVFPhotoInfo& InPhotoInfo)
{
	PhotoInfo = InPhotoInfo;
	SetRenderTarget(PhotoInfo.RenderTarget);

}

void AVFPhoto::AddCapturedActor(AActor* Actor, const FTransform& CameraTransform)
{
	FVFActorRecord ActorRecord;
	
	ActorRecord.Class = Actor->GetClass();
	ActorRecord.RelativeTransform = UKismetMathLibrary::MakeRelativeTransform(Actor->GetActorTransform(), CameraTransform);

	//场景中大多数的Actor都是StaticMeshActor
	if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
	{
		ActorRecord.NameToMeshMap.Emplace(FString("StaticMeshComponent"), StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh());
	}
	else
	{
		TArray<UStaticMeshComponent*> Components;
		Actor->GetComponents<UStaticMeshComponent>(Components);
		for (UStaticMeshComponent* StaticMeshComponent : Components)
		{
			ActorRecord.NameToMeshMap.Emplace(StaticMeshComponent->GetName(), StaticMeshComponent->GetStaticMesh());
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
				ActorRecord.NameToMeshMap.Emplace(PropertyName, StaticMeshComponent->GetStaticMesh());
			}
		}*/
	}

	PhotoInfo.ActorRecords.Emplace(ActorRecord);
}

void AVFPhoto::SetRenderTarget(UTexture* Texture)
{
	PhotoInfo.RenderTarget = Texture;
	UMaterialInstanceDynamic* PlaneMaterial = UMaterialInstanceDynamic::Create(PhotoMesh->GetMaterial(0), this);
	PlaneMaterial->SetTextureParameterValue(FName("RenderTarget"), PhotoInfo.RenderTarget);
	PhotoMesh->SetMaterial(0, PlaneMaterial);
}
