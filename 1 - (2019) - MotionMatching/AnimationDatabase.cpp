// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimationDatabase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"

namespace AnimationDatabaseGlobals
{
	// @todo: replace this with developer settings
	const float TimeStep = 0.1f;
	const float MaxFutureTime = 1.0f;
}


UAnimationDatabase::UAnimationDatabase(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	Skeleton = nullptr;
	MotionFrameData.Empty();
	MotionMatchingBones.Empty();
}

USkeleton* UAnimationDatabase::GetSkeleton() const
{
	return Skeleton;
}

TArray<FName> UAnimationDatabase::GetMotionMatchingBones() const
{
	return MotionMatchingBones;
}

TArray<UAnimSequence*> UAnimationDatabase::GetSourceAnimations() const
{
	return SourceAnimations;
}

TArray<FAnimationFrameData> UAnimationDatabase::GetMotionFrameData() const
{
	return MotionFrameData;
}

void UAnimationDatabase::Initialize(class USkeleton* InSkeleton, const TArray<FName>& InBones)
{
	Skeleton = InSkeleton;
	MotionMatchingBones = InBones;
}

#if WITH_EDITOR

void UAnimationDatabase::AddSourceAnimations(TArray<UAnimSequence*> InAnimations)
{
	for (UAnimSequence* Anim : InAnimations)
	{
		if (Anim)
		{
			ProcessAnimation(Anim);
		}
	}
}

void UAnimationDatabase::RemoveSourceAnimationAtIndex(const int InAnimationIndex)
{
	Modify();

	// Clear the frame data for this animation before removing it
	ClearFrameDataForAnimation(InAnimationIndex);
	SourceAnimations.RemoveAt(InAnimationIndex);

	MarkPackageDirty();
}

void UAnimationDatabase::ProcessAnimation(UAnimSequence* InAnimation)
{
	Modify();

	int32 Index = SourceAnimations.Add(InAnimation);
	
	// Generate new Frame Data for this animation
	RabakeFrameDataForAnimation(Index);

	MarkPackageDirty();
}

void UAnimationDatabase::ClearAllFrameData()
{
	Modify();

	MotionFrameData.Empty();
	SourceAnimations.Empty();

	MarkPackageDirty();
}

void UAnimationDatabase::RebakeAllFrameData()
{
	if (SourceAnimations.Num() > 0)
	{
		Modify();

		for (int i = 0; i < SourceAnimations.Num(); ++i)
		{
			RabakeFrameDataForAnimation(i);
		}

		MarkPackageDirty();
	}
}

void UAnimationDatabase::ClearFrameDataForAnimation(const int InAnimationIndex)
{
	if (MotionFrameData.Num() > 0)
	{
		Modify();

		for (int i = (MotionFrameData.Num() - 1); i > -1; --i)
		{
			if (MotionFrameData[i].SourceAnimationIndex == InAnimationIndex)
			{
				MotionFrameData.RemoveAt(i);
			}
		}

		MarkPackageDirty();
	}
}

void UAnimationDatabase::RabakeFrameDataForAnimation(const int InAnimationIndex, bool bClearPreviousData /*= false*/)
{
	UAnimSequence* AnimationSequence = SourceAnimations[InAnimationIndex];
	if (AnimationSequence)
	{
		Modify();

		if (bClearPreviousData)
		{
			ClearFrameDataForAnimation(InAnimationIndex);
		}

		const float PlayLength = AnimationSequence->GetPlayLength();

		// Make sure we do not generate new frames at the end of the animation
		const float MaxCurrentTime = PlayLength - AnimationDatabaseGlobals::MaxFutureTime;
		
		float CurrentPlayTime = 0.0f;

		while (CurrentPlayTime <= MaxCurrentTime)
		{
			CurrentPlayTime += AnimationDatabaseGlobals::TimeStep;

			FAnimationFrameData AnimationFrameData = FAnimationFrameData();
			AnimationFrameData.ExtractAnimationData(AnimationSequence, InAnimationIndex, CurrentPlayTime, MotionMatchingBones);

			MotionFrameData.Add(AnimationFrameData);
		}

		MarkPackageDirty();
	}
}

#endif//WITH_EDITOR