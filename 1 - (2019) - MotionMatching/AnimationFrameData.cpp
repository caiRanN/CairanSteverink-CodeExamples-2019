// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimationFrameData.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "MotionMatchingUtilities.h"
#include "MotionMatchingMetaData.h"
#include "AnimNotifyState_MotionCategory.h"


namespace FrameDataGlobals
{
	// Probably need to replace this with a developer setting value sometime
	const TArray<float> TimeDelays = { 0.2f, 0.4f, 0.7f, 1.0f };

	const float PreviousTimeDelta = 0.1f;
	const float NextTimeDelta = 0.1f;
	const int RootBoneIndex = 0;	
}

FAnimationFrameData::FAnimationFrameData()
	: SourceAnimationIndex(INDEX_NONE)
	, StartTime(0.0f)
	, MotionVelocity(FVector::ZeroVector)
{
	MotionTrajectory.Empty();
	MotionBonesData.Empty();
}

FAnimationFrameData::FAnimationFrameData(const UAnimSequence* InAnimSequence, const int InSourceIndex, const float InTime, const TArray<FName> InBones)
	: SourceAnimationIndex(INDEX_NONE)
	, StartTime(0.0f)
	, MotionVelocity(FVector::ZeroVector)
{
	MotionTrajectory.Empty();
	MotionBonesData.Empty();

	ExtractAnimationData(InAnimSequence, InSourceIndex, InTime, InBones);
}

void FAnimationFrameData::ExtractAnimationData(const UAnimSequence* InAnimSequence, const int InSourceIndex, const float InTime, const TArray<FName> InBones)
{
	if (InAnimSequence)
	{
		StartTime = InTime;
		SourceAnimationIndex = InSourceIndex;

		InitializeFromMetaData(InAnimSequence);
		InitializeBoneDataFromAnimation(InAnimSequence, InTime, InBones);
		InitializeTrajectoryData(InAnimSequence, InTime);

		// Get the animation velocity between the current time and the next time
		const FVector TempVelocity = InAnimSequence->ExtractRootMotion(StartTime, FrameDataGlobals::NextTimeDelta, true).GetTranslation();
		const FVector Velocity = TempVelocity.GetSafeNormal() * (TempVelocity.Size() / FrameDataGlobals::NextTimeDelta);

		MotionVelocity = Velocity;
	}
}

void FAnimationFrameData::InitializeFromMetaData(const UAnimSequence* InAnimSequence, const float InTime)
{
	if (InAnimSequence)
	{
		TArray<UAnimMetaData*> MetaData = InAnimSequence->GetMetaData();
		
		if (MetaData.Num() > 0)
		{
			for (const UAnimMetaData* Data : MetaData)
			{
				if (const UMotionMatchingMetaData* MotionMatchingData = Cast<UMotionMatchingMetaData>(Data))
				{
					Categories = MotionMatchingData->AnimationCategories;
					Pose = MotionMatchingData->AnimationPose;
				}
			}
		}
	}

	TArray<FAnimNotifyEventReference> OutNotifies;
	InAnimSequence->GetAnimNotifiesFromDeltaPositions(InTime - FrameDataGlobals::PreviousTimeDelta, InTime, OutNotifies);

	if (OutNotifies.Num() > 0)
	{
		for (const FAnimNotifyEventReference& Event : OutNotifies)
		{
			const UAnimNotifyState_MotionCategory* Category = Cast<UAnimNotifyState_MotionCategory>(Event.GetNotify());
			if (Category)
			{
				TArray<FGameplayTag> OutTags;
				Category->Categories.GetGameplayTagArray(OutTags);

				if (OutTags.Num() > 0)
				{
					for (FGameplayTag& Tag : OutTags)
					{
						Categories.AddTag(Tag);
					}
				}
			}
		}
	}
}

void FAnimationFrameData::InitializeBoneDataFromAnimation(const UAnimSequence* InAnimSequence, const float InTime, const TArray<FName>& InBones)
{
	if (InAnimSequence)
	{
		MotionBonesData.Empty();

		// Loop through all bones
		for (const FName& BoneName : InBones)
		{
			FMotionBoneData BoneData;

			const FReferenceSkeleton ReferenceSkeleton = InAnimSequence->GetSkeleton()->GetReferenceSkeleton();
			const int BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);

			FTransform CurrentTimeBoneTM = GetTransformFromBoneSpace(InAnimSequence, InTime, ReferenceSkeleton, BoneIndex);
			FTransform PreviousTimeBoneTM = GetTransformFromBoneSpace(InAnimSequence, InTime - FrameDataGlobals::PreviousTimeDelta, ReferenceSkeleton, BoneIndex);

			// Calculate velocity
			const FVector Velocity = (CurrentTimeBoneTM.GetLocation() - PreviousTimeBoneTM.GetLocation());
			const float VelocityDelta = Velocity.Size() / FrameDataGlobals::PreviousTimeDelta;

			// Required for conversion of world to component space
			FTransform RootTM;
			InAnimSequence->GetBoneTransform(RootTM, FrameDataGlobals::RootBoneIndex, InTime, false);
			
			FVector LocalBonePosition = RootTM.InverseTransformPositionNoScale(CurrentTimeBoneTM.GetLocation());
			FVector LocalBoneVelocity = RootTM.InverseTransformVectorNoScale(Velocity.GetSafeNormal() * VelocityDelta);

			BoneData.BonePosition = LocalBonePosition;
			BoneData.BoneVelocity = LocalBoneVelocity;

			// Add to the list of bones
			MotionBonesData.Add(BoneData);
		}
	}
}

void FAnimationFrameData::InitializeTrajectoryData(const UAnimSequence* InAnimSequence, const float InTime)
{
	if (InAnimSequence)
	{
		MotionTrajectory.Empty();

		AnimationTransform = InAnimSequence->ExtractRootMotion(InTime, 0.0f, true);

		for (const float& TimeDelay : FMotionMatchingUtils::TrajectoryIntervals)
		{
			FTransform RootMotionTM = InAnimSequence->ExtractRootMotion(InTime, TimeDelay, true);
			MotionTrajectory.Add(FTrajectoryPoint(RootMotionTM.GetTranslation(), RootMotionTM.GetRotation(), TimeDelay));
		}
	}
}

FTransform FAnimationFrameData::GetTransformFromBoneSpace(const UAnimSequence* InAnimSequence, const float InTime, const struct FReferenceSkeleton& InReferenceSkeleton, const int InBoneIndex)
{
	if (InAnimSequence)
	{
		FTransform BoneWorldTM;
		int CurrentIndex = InBoneIndex;

		if (CurrentIndex == 0)
		{
			InAnimSequence->GetBoneTransform(BoneWorldTM, CurrentIndex, InTime, false);
			return BoneWorldTM;
		}

		while (InReferenceSkeleton.GetParentIndex(CurrentIndex) != INDEX_NONE)
		{
			const int ParentIndex = InReferenceSkeleton.GetParentIndex(CurrentIndex);

			FTransform ParentBoneTM;
			InAnimSequence->GetBoneTransform(ParentBoneTM, ParentIndex, InTime, false);

			BoneWorldTM = BoneWorldTM * ParentBoneTM;
			CurrentIndex = ParentIndex;
		}

		return BoneWorldTM;
	}

	return FTransform::Identity;
}

bool FAnimationFrameData::IsValid() const
{
	return (SourceAnimationIndex != INDEX_NONE);
}
