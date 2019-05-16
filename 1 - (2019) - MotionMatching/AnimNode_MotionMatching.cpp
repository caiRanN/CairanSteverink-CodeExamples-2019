// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNode_MotionMatching.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstanceProxy.h"

#include "Utilities/MotionMatchingUtilities.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"

#include "AnimationDatabase.h"
#include "Goal.h"


FAnimNode_MotionMatching::FAnimNode_MotionMatching()
	: AnimationDatabase(nullptr)
	, Goal(FGoal())
	, Responsiveness(0.5f)
	, BlendTime(0.2f)
	, bEnablePoseMatching(true)
{
}

float FAnimNode_MotionMatching::GetCurrentAssetTime()
{
	if (CurrentAnimation.IsValid())
	{
		return CurrentAnimation.Time;
	}

	// No sample
	return 0.0f;
}

float FAnimNode_MotionMatching::GetCurrentAssetTimePlayRateAdjusted()
{
	return Super::GetCurrentAssetTimePlayRateAdjusted();
}

float FAnimNode_MotionMatching::GetCurrentAssetLength()
{
	return GetCurrentAnimationAsset() ? GetCurrentAnimationAsset()->SequenceLength : 0.0f;
}

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	EvaluateGraphExposedInputs.Execute(Context);

	InternalTimeAccumulator = 0.0f;

}

void FAnimNode_MotionMatching::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_MotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if (AnimationDatabase)
	{
		if (GetCurrentAnimationAsset() != nullptr && Context.AnimInstanceProxy->IsSkeletonCompatible(GetCurrentAnimationAsset()->GetSkeleton()))
		{
			InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.0f, GetCurrentAnimationAsset()->SequenceLength);
			
			const float PlayRate = 1.0f;
			const float bLooping = true;
			
			CreateTickRecordForNode(Context, GetCurrentAnimationAsset(), bLooping, PlayRate);
		}
	}
}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	if (AnimationDatabase)
	{
		FVector Velocity = FVector::ZeroVector;
		TArray<FMotionBoneData> CurrentBonesData;

		if (CurrentAnimation.IsValid())
		{
			// Calculate our current velocity from the animation
			//const FVector TempVelocity = CurrentAnimation.Animation->ExtractRootMotion(CurrentAnimation.Time, 0.1f /* DeltaTime */, true).GetTranslation();
			const FVector TempVelocity = CurrentAnimation.Animation->ExtractRootMotion(InternalTimeAccumulator, 0.1f /* DeltaTime */, true).GetTranslation();
			Velocity = TempVelocity.GetSafeNormal() * (TempVelocity.Size() / 0.1f /* DeltaTime */);

			// Get data about our current bones
			//CurrentBonesData = UMotionMatchingUtilities::GetBoneDataFromAnimation(CurrentAnimation.Animation, CurrentAnimation.Time, AnimationDatabase->GetMotionMatchingBones());
			CurrentBonesData = UMotionMatchingUtilities::GetBoneDataFromAnimation(CurrentAnimation.Animation, InternalTimeAccumulator, AnimationDatabase->GetMotionMatchingBones());
		}

		FMotionMatchingParams Params;
		Params.Responsiveness = Responsiveness;
		Params.BlendTime = BlendTime;
		Params.bPoseMatching = bEnablePoseMatching;
		Params.bHasCurrentAnimation = CurrentAnimation.IsValid();
		Params.CurrentVelocity = Velocity;
		Params.CurrentBonesData = CurrentBonesData;

		if (GetCurrentAnimationAsset() && Output.AnimInstanceProxy->IsSkeletonCompatible(GetCurrentAnimationAsset()->GetSkeleton()))
		{
			CurrentAnimation.Animation->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(InternalTimeAccumulator, true));
			//CurrentAnimation.Animation->GetAnimationPose(Output.Pose, Output.Curve, FAnimExtractContext(CurrentAnimation.Time, true)); // @todo: we need to increment time
		}
		else
		{
			Output.ResetToRefPose();
		}

		UpdateMotionMatching(Params);
	}
}

void FAnimNode_MotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	if (CurrentAnimation.IsValid())
	{
		DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *CurrentAnimation.Animation->GetName(), InternalTimeAccumulator);
		DebugData.AddDebugItem(DebugLine, true);
	}
}

void FAnimNode_MotionMatching::UpdateInternal(const FAnimationUpdateContext& Context)
{
	if ((GetCurrentAnimationAsset() != NULL) && (Context.AnimInstanceProxy->IsSkeletonCompatible(GetCurrentAnimationAsset()->GetSkeleton())))
	{

	}
}

void FAnimNode_MotionMatching::UpdateMotionMatching(const FMotionMatchingParams& MotionMatchingParams)
{
	if (AnimationDatabase)
	{
		int WinnerIndex = INDEX_NONE;
		float WinnerCost = BIG_NUMBER;

		UMotionMatchingUtilities::GetLowestCostAnimation(AnimationDatabase, Goal, MotionMatchingParams, WinnerIndex, WinnerCost);

		// Check if the Winner Index is the Same as our current animation
		if (WinnerIndex != INDEX_NONE)
		{
			FAnimationFrameData Winner = AnimationDatabase->GetMotionFrameData()[WinnerIndex];

			if (CurrentAnimation.IsValid())
			{
				//bool bTheWinnerIsAtTheSameLocation = (Winner.SourceAnimationIndex == CurrentAnimation.AnimationIndex) && (FMath::Abs(Winner.StartTime - CurrentAnimation.Time) < 0.2f);
				bool bTheWinnerIsAtTheSameLocation = (Winner.SourceAnimationIndex == CurrentAnimation.AnimationIndex) && (FMath::Abs(Winner.StartTime - InternalTimeAccumulator) < 0.2f);

				if (!bTheWinnerIsAtTheSameLocation)
				{
					SetCurrentAnimation(Winner.SourceAnimationIndex, Winner.StartTime);

					// Update our time accumulator to start at the new time
					InternalTimeAccumulator = Winner.StartTime;

					//UE_LOG(LogTemp, Warning, TEXT("Winner with index ('%d') playing animation at time ('%f') with a cost of ('%f')"), WinnerIndex, Winner.StartTime, WinnerCost);
				}
				else
				{
					//UE_LOG(LogTemp, Warning, TEXT("Winner with index ('%d') is at the same location at time ('%f') with a cost of ('%f')"), WinnerIndex, Winner.StartTime, WinnerCost);
				}	
			}
			else
			{
				SetCurrentAnimation(Winner.SourceAnimationIndex, Winner.StartTime);
			}
		}
	}
}

void FAnimNode_MotionMatching::SetCurrentAnimation(const int InAnimationIndex, const float InTime)
{
	FMotionMatchingSampleData NewAnimation;
	NewAnimation.AnimationIndex = InAnimationIndex;
	NewAnimation.Animation = AnimationDatabase->GetSourceAnimations()[InAnimationIndex];
	NewAnimation.Time = InTime;

	PreviousAnimation = CurrentAnimation;
	CurrentAnimation = NewAnimation;
}

UAnimSequence* FAnimNode_MotionMatching::GetCurrentAnimationAsset() const
{
	return CurrentAnimation.Animation;
}

