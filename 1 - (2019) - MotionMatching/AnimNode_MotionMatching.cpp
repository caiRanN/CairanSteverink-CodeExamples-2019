// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNode_MotionMatching.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimInstanceProxy.h"

#include "Utilities/MotionMatchingUtilities.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"

#include "AnimationRuntime.h"

#include "AnimationDatabase.h"
#include "Goal.h"

float FAnimNode_MotionMatching::GetCurrentAssetTime()
{
	if (AnimationSamples.Num() > 0)
	{
		AnimationSamples.Last().Time;
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
	return GetCurrentAnim() != NULL ? GetCurrentAnim()->SequenceLength : 0.0f;
}

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);

	EvaluateGraphExposedInputs.Execute(Context);

	InternalTimeAccumulator = 0.0f;

	const int NumPoses = AnimationSamples.Num();
	
	if (NumPoses > 0)
	{
		FMotionMatchingSampleData& Data = AnimationSamples.Last();
		Data.BlendWeight = 1.0f;
		SamplesToEvaluate.Add(Data);

		LastActiveChildSample = NULL;

		for (int32 i = 0; i < AnimationSamples.Num(); ++i)
		{
			FAlphaBlend& Blend = AnimationSamples[i].Blend;
			Blend.SetBlendTime(0.0f);
			Blend.SetBlendOption(BlendType);
			Blend.SetCustomCurve(CustomBlendCurve);
		}
		AnimationSamples.Last().Blend.SetAlpha(1.0f);
	}
}

void FAnimNode_MotionMatching::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
}

void FAnimNode_MotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	EvaluateGraphExposedInputs.Execute(Context);

	if (AnimationDatabase)
	{
		UpdateAnimationSampleData(Context);

		if (AnimationSamples.Num() > 0 && GetCurrentAnim() != NULL 
			&& Context.AnimInstanceProxy->IsSkeletonCompatible(GetCurrentAnim()->GetSkeleton()))
		{
			InternalTimeAccumulator = FMath::Clamp(InternalTimeAccumulator, 0.0f, GetCurrentAnim()->SequenceLength);

			const float PlayRate = 1.0f;
			const float bLooping = true;

			CreateTickRecordForNode(Context, GetCurrentAnim(), bLooping, PlayRate);
		}
	}
}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	if (AnimationDatabase)
	{
		FVector Velocity = FVector::ZeroVector;
		TArray<FMotionBoneData> CurrentBonesData;
		bool bHasCurrentAnim = false;

		if (AnimationSamples.Num() > 0)
		{
			// Calculate our current velocity from the animation
			const FVector TempVelocity = GetCurrentAnim()->ExtractRootMotion(AnimationSamples.Last().Time, 0.1f /* DeltaTime */, true).GetTranslation();
			Velocity = TempVelocity.GetSafeNormal() * (TempVelocity.Size() / 0.1f /* DeltaTime */);

			// Get data about our current bones
			CurrentBonesData = UMotionMatchingUtilities::GetBoneDataFromAnimation(GetCurrentAnim(), AnimationSamples.Last().Time, AnimationDatabase->GetMotionMatchingBones());
			bHasCurrentAnim = true;
		}

		FMotionMatchingParams Params;
		Params.Responsiveness = Responsiveness;
		Params.BlendTime = BlendTime;
		Params.bPoseMatching = bEnablePoseMatching;
		Params.CurrentVelocity = Velocity;
		Params.bHasCurrentAnimation = bHasCurrentAnim;
		Params.CurrentBonesData = CurrentBonesData;
		Params.TrajectoryPositionAxis = TrajectoryPositionAxis;
		Params.BonePositionAxis = BonePositionAxis;

		EvaluateBlendPose(Output);

		UpdateMotionMatching(Params, Output);
	}
}

void FAnimNode_MotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	if (LastActiveChildSample.IsValid())
	{
		DebugLine += FString::Printf(TEXT("('%s' Play Time: %.3f)"), *LastActiveChildSample.Animation->GetName(), LastActiveChildSample.Time);
		DebugData.AddDebugItem(DebugLine, true);
	}
}

void FAnimNode_MotionMatching::UpdateAnimationSampleData(const FAnimationUpdateContext& Context)
{
	const int NumPoses = AnimationSamples.Num();

	SamplesToEvaluate.Empty(NumPoses);

	if (NumPoses > 0)
	{
		// Handle a change in the active child index; adjusting the target weights
		FMotionMatchingSampleData& ChildSample = AnimationSamples.Last();

		if (ChildSample != LastActiveChildSample)
		{
			bool bLastChildSampleIsInvalid = !LastActiveChildSample.IsValid();

			const float CurrentWeight = ChildSample.BlendWeight;
			const float DesiredWeight = 1.0f;
			const float WeightDifference = FMath::Clamp<float>(FMath::Abs<float>(DesiredWeight - CurrentWeight), 0.0f, 1.0f);

			// scale by the weight difference since we want always consistency:
			// - if you're moving from 0 to full weight 1, it will use the normal blend time
			// - if you're moving from 0.5 to full weight 1, it will get there in half the time
			const float RemainingBlendTime = bLastChildSampleIsInvalid ? 0.0f : (ChildSample.BlendTime * WeightDifference);

			for (int32 i = 0; i < AnimationSamples.Num(); ++i)
			{
				AnimationSamples[i].RemainingBlendTime = RemainingBlendTime;
			}

			// If we have a valid previous child and we're instantly blending - update that pose with zero weight
			if (RemainingBlendTime == 0.0f && !bLastChildSampleIsInvalid)
			{
				LastActiveChildSample.BlendWeight = 0.0f;
			}

			for (int32 i = 0; i < AnimationSamples.Num(); ++i)
			{
				FAlphaBlend& Blend = AnimationSamples[i].Blend;
				Blend.SetBlendTime(RemainingBlendTime);

				if (AnimationSamples[i] == ChildSample)
				{
					Blend.SetValueRange(AnimationSamples[i].BlendWeight, 1.0f);
				}
				else
				{
					Blend.SetValueRange(AnimationSamples[i].BlendWeight, 0.0f);
				}
			}

			// Assign the sample as the previous
			LastActiveChildSample = ChildSample;
		}

		// Advance the weights and times
		float SumWeight = 0.0f;
		for (int32 i = 0; i < AnimationSamples.Num(); ++i)
		{
			float& SampleBlendWeight = AnimationSamples[i].BlendWeight;
			float& SampleAnimTime = AnimationSamples[i].Time;

			FAlphaBlend& Blend = AnimationSamples[i].Blend;
			Blend.Update(Context.GetDeltaTime());
			SampleBlendWeight = Blend.GetBlendedValue();

			SampleAnimTime += Context.GetDeltaTime();

			SumWeight += SampleBlendWeight;
		}

		// Renormalize the weights
		if ((SumWeight > ZERO_ANIMWEIGHT_THRESH) && (FMath::Abs<float>(SumWeight - 1.0f) > ZERO_ANIMWEIGHT_THRESH))
		{
			float ReciprocalSum = 1.0f / SumWeight;
			for (int32 i = 0; i < AnimationSamples.Num(); ++i)
			{
				float& SampleBlendWeight = AnimationSamples[i].BlendWeight;
				SampleBlendWeight *= ReciprocalSum;
			}
		}

		// Update our active children
		for (int32 i = 0; i < AnimationSamples.Num(); ++i)
		{
			const float SampleBlendWeight = AnimationSamples[i].BlendWeight;
			if (SampleBlendWeight > ZERO_ANIMWEIGHT_THRESH)
			{
				SamplesToEvaluate.Add(AnimationSamples[i]);
			}
		}

		// Remove our inactive samples
		for (int32 i = (AnimationSamples.Num() - 1); i > 0; --i)
		{
			const float SampleBlendWeight = AnimationSamples[i].BlendWeight;
			if (SampleBlendWeight <= ZERO_ANIMWEIGHT_THRESH)
			{
				AnimationSamples.RemoveAt(i);
			}
		}
	}
}

void FAnimNode_MotionMatching::EvaluateBlendPose(FPoseContext& Output)
{
	ANIM_MT_SCOPE_CYCLE_COUNTER(BlendPosesInGraph, !IsInGameThread());

	const int32 NumPoses = SamplesToEvaluate.Num();

	if (NumPoses > 0)
	{
		TArray<FCompactPose, TInlineAllocator<8>> FilteredPoses;
		FilteredPoses.SetNum(NumPoses, false);
		
		TArray<FBlendedCurve, TInlineAllocator<8>> FilteredCurves;
		FilteredCurves.SetNum(NumPoses, false);

		TArray<float, TInlineAllocator<8>> FilteredWeights;
		FilteredWeights.SetNum(NumPoses, false);

		float SumWeight = 0.0f;
		for (int32 i = 0; i < SamplesToEvaluate.Num(); ++i)
		{
			FMotionMatchingSampleData& Sample = SamplesToEvaluate[i];

			FilteredPoses[i].CopyBonesFrom(Output.Pose);
			FilteredCurves[i].InitFrom(Output.Curve);
			FilteredWeights[i] = Sample.BlendWeight;

			Sample.Animation->GetAnimationPose(FilteredPoses[i], FilteredCurves[i], FAnimExtractContext(Sample.Time, true));

			SumWeight += Sample.BlendWeight;
		}

		FAnimationRuntime::BlendPosesTogether(FilteredPoses, FilteredCurves, FilteredWeights, Output.Pose, Output.Curve);
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_MotionMatching::UpdateMotionMatching(const FMotionMatchingParams& MotionMatchingParams, const FPoseContext& Output)
{
	if (AnimationDatabase)
	{
		int WinnerIndex = INDEX_NONE;
		float WinnerCost = BIG_NUMBER;

		UMotionMatchingUtilities::GetLowestCostAnimation(AnimationDatabase, Goal, MotionMatchingParams, WinnerIndex, WinnerCost);

		if (WinnerIndex != INDEX_NONE)
		{
			FAnimationFrameData Winner = AnimationDatabase->GetMotionFrameData()[WinnerIndex];

			if (AnimationSamples.Num() > 0)
			{
				bool bTheWinnerIsAtTheSameLocation = (Winner.SourceAnimationIndex == AnimationSamples.Last().AnimationIndex)
														&& (FMath::Abs(Winner.StartTime - AnimationSamples.Last().Time) < 0.2f);
				
				if (!bTheWinnerIsAtTheSameLocation)
				{
					// Play Anim with Blend
					SetCurrentAnimation(Winner.SourceAnimationIndex, Winner.StartTime);

					// Update our time accumulator to start at the new time
					InternalTimeAccumulator = Winner.StartTime;
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
	NewAnimation.BlendTime = BlendTime;
	NewAnimation.RemainingBlendTime = BlendTime;
	NewAnimation.BlendWeight = 0.0f;
	NewAnimation.Time = InTime;

	FAlphaBlend& Blend = NewAnimation.Blend;
	Blend.SetBlendTime(0.0f);
	Blend.SetBlendOption(BlendType);
	Blend.SetCustomCurve(CustomBlendCurve);

	AnimationSamples.Add(NewAnimation);
}

UAnimSequence* FAnimNode_MotionMatching::GetCurrentAnim()
{
	if (AnimationSamples.Num() > 0)
	{
		return AnimationSamples.Last().Animation;
	}

	return NULL;
}

