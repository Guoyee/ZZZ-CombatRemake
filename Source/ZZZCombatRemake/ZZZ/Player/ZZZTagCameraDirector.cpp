// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZZZTagCameraDirector.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/ActorComponent.h"
#include "Core/CameraDirectorEvaluator.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraRigAsset.h"
#include "GameFramework/Actor.h"

namespace UE::Cameras
{

namespace
{
	/** evaluation context 的 owner（组件的 GameplayCamera 组件 / 或 actor 本身）→ 其所属 Actor。 */
	AActor* ResolveContextOwnerActor(const TSharedPtr<FCameraEvaluationContext>& Context)
	{
		if (!Context)
		{
			return nullptr;
		}
		UObject* Owner = Context->GetOwner();
		if (!Owner)
		{
			return nullptr;
		}
		if (AActor* OwnerAsActor = Cast<AActor>(Owner))
		{
			return OwnerAsActor;
		}
		if (UActorComponent* OwnerAsComponent = Cast<UActorComponent>(Owner))
		{
			return OwnerAsComponent->GetOwner();
		}
		return Owner->GetTypedOuter<AActor>();
	}

	/** Pawn-ASC 项目的取用顺序：先接口（AZZZCharacter），再组件兜底。 */
	UAbilitySystemComponent* ResolveAbilitySystemComponent(AActor* Actor)
	{
		if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor))
		{
			if (UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(ASI->GetAbilitySystemComponent()))
			{
				return ASC;
			}
		}
		return Actor->FindComponentByClass<UAbilitySystemComponent>();
	}
}  // namespace

class FZZZTagCameraDirectorEvaluator : public FCameraDirectorEvaluator
{
	UE_DECLARE_CAMERA_DIRECTOR_EVALUATOR(, FZZZTagCameraDirectorEvaluator)

protected:
	virtual void OnRun(const FCameraDirectorEvaluationParams& Params, FCameraDirectorEvaluationResult& OutResult) override
	{
		const UZZZTagCameraDirector* Director = GetCameraDirectorAs<UZZZTagCameraDirector>();
		if (!Director)
		{
			return;
		}

		TSharedPtr<FCameraEvaluationContext> Context = GetEvaluationContext();
		if (!Context)
		{
			return;
		}

		// 命中最高优先级的映射 rig；无命中 → 兜底主 rig。
		const UCameraRigAsset* BestRig = Director->DefaultRig;
		int32 BestPriority = MIN_int32;

		if (AActor* OwnerActor = ResolveContextOwnerActor(Context))
		{
			if (UAbilitySystemComponent* ASC = ResolveAbilitySystemComponent(OwnerActor))
			{
				FGameplayTagContainer OwnedTags;
				ASC->GetOwnedGameplayTags(OwnedTags);

				for (const FZZZCameraTagMapping& Mapping : Director->TagMappings)
				{
					if (Mapping.CameraRig && Mapping.Tag.IsValid()
							&& Mapping.Priority > BestPriority && OwnedTags.HasTag(Mapping.Tag))
					{
						BestPriority = Mapping.Priority;
						BestRig = Mapping.CameraRig;
					}
				}
			}
		}

		if (BestRig)
		{
			OutResult.Add(Context, BestRig);
		}
	}
};

UE_DEFINE_CAMERA_DIRECTOR_EVALUATOR(FZZZTagCameraDirectorEvaluator)

}  // namespace UE::Cameras

UZZZTagCameraDirector::UZZZTagCameraDirector(const FObjectInitializer& ObjectInit)
	: Super(ObjectInit)
{
}

FCameraDirectorEvaluatorPtr UZZZTagCameraDirector::OnBuildEvaluator(FCameraDirectorEvaluatorBuilder& Builder) const
{
	using namespace UE::Cameras;
	return Builder.BuildEvaluator<FZZZTagCameraDirectorEvaluator>();
}

void UZZZTagCameraDirector::OnGatherRigUsageInfo(FCameraDirectorRigUsageInfo& UsageInfo) const
{
	// 必须声明所有可能用到的 rig —— context 初始化时据此收集 rig 的变量表/分配信息
	// （FCameraEvaluationContext::Initialize → GatherRigUsageInfo）；漏声明的 rig
	// 可能拿不到参数表初始化。
	if (DefaultRig)
	{
		UsageInfo.CameraRigs.Add(DefaultRig);
	}
	for (const FZZZCameraTagMapping& Mapping : TagMappings)
	{
		if (Mapping.CameraRig)
		{
			UsageInfo.CameraRigs.Add(Mapping.CameraRig);
		}
	}
}
