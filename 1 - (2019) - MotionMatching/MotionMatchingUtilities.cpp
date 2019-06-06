// Fill out your copyright notice in the Description page of Project Settings.

#include "MotionMatchingUtilities.h"
#include "AnimationDatabase.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Skeleton.h"
#include "AnimationFrameData.h"


namespace MotionMatchingGlobals
{
	// @todo: Replace this with a developer setting values so it is not hard coded
	const TArray<float> TimeDelays = { 0.2f, 0.4f, 0.7f, 1.0f };

	const float PreviousTimeDelta = 0.1f;
	const float NextTimeDelta = 0.1f;
	const int RootBoneIndex = 0;
}


UMotionMatchingUtilities::UMotionMatchingUtilities(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UMotionMatchingUtilities::GetLowestCostAnimation(const UAnimationDatabase* AnimationDatabase, const FGoal& Goal, const FMotionMatchingParams& MotionMatchingParams, int& OutBestCandidateIndex, float& OutBestCandidateCost)
{
	if (AnimationDatabase)
	{
		const TArray<FAnimationFrameData> FrameData = AnimationDatabase->GetMotionFrameData();
		const int NumberOfCandidates = FrameData.Num();

		float BestCost = BIG_NUMBER;
		int BestCandidateIndex = INDEX_NONE;
		int SourceAnimIndex = INDEX_NONE;

		if (NumberOfCandidates > 0)
		{
			for (int CandidateIndex = 0; CandidateIndex < NumberOfCandidates; CandidateIndex++)
			{
				float Cost = ComputeCost(FrameData[CandidateIndex], Goal, MotionMatchingParams);

				if (Cost < BestCost)
				{
					BestCost = Cost;
					BestCandidateIndex = CandidateIndex;
				}

				if (Cost <= 0.0f)
				{
					break;
				}
			}
		}

		OutBestCandidateCost = BestCost;
		OutBestCandidateIndex = BestCandidateIndex;
	}
}

float UMotionMatchingUtilities::ComputeCost(const FAnimationFrameData& CandidatePose, const FGoal& Goal, const FMotionMatchingParams& MotionMatchingParams)
{
	check(CandidatePose.IsValid());

	float Cost = 0.0f;

	// How much the candidate jumping position matches the current situation
	Cost += ComputeCurrentCost(CandidatePose, Goal, MotionMatchingParams);

	// How much the candidate piece of motion matches the desired trajectory
	Cost += MotionMatchingParams.Responsiveness * ComputeFutureCost(CandidatePose, Goal, MotionMatchingParams);

	return Cost;
}

float UMotionMatchingUtilities::ComputeCurrentCost(const FAnimationFrameData& CandidatePose, const FGoal& Goal, const FMotionMatchingParams& MotionMatchingParams)
{
	float Cost = 0.0f;

	// Increase the cost according to the difference in velocity
	Cost += FVector::Dist(MotionMatchingParams.CurrentVelocity, CandidatePose.MotionVelocity);

	// Only Pose match if it is enabled and we have current animation bone data
	if (MotionMatchingParams.bPoseMatching && MotionMatchingParams.CurrentBonesData.Num() > 0)
	{
		check(CandidatePose.MotionBonesData.Num() == MotionMatchingParams.CurrentBonesData.Num());

		// Increase cost according to the distance and velocity between the bones
		for (int i = 0; i < CandidatePose.MotionBonesData.Num(); ++i)
		{
			Cost += CandidatePose.MotionBonesData[i].ComputeCostBetween(MotionMatchingParams.CurrentBonesData[i], MotionMatchingParams.BonePositionAxis);
		}
	}

	// ... add other Cost Calculations that are required for Motion Matching here ...

	return Cost;
}

float UMotionMatchingUtilities::ComputeFutureCost(const FAnimationFrameData& CandidatePose, const FGoal& Goal, const FMotionMatchingParams& MotionMatchingParams)
{
	float Cost = 0.0f;

	if (Goal.IsValid())
	{
		Cost += Goal.CalculateCostBetweenTrajectory(CandidatePose.MotionTrajectory, MotionMatchingParams.TrajectoryPositionAxis);
	}

	// ... add other Future Cost Calculations that are required for Motion Matching here ...

	return Cost;
}

FGoal UMotionMatchingUtilities::MakeGoal(const float DesiredSpeed, const FVector InputDirectionNormal, const FTransform CharacterMeshTM, const TArray<float> TrajectoryIntervals)
{
	FGoal OutGoal = FGoal();

	for (const float TrajectoryInterval : TrajectoryIntervals)
	{
		const FVector TrajectoryLocation = CharacterMeshTM.GetLocation() + ((InputDirectionNormal * DesiredSpeed) * TrajectoryInterval);
		const FTrajectoryPoint TrajectoryPoint = FTrajectoryPoint(TrajectoryLocation, TrajectoryInterval);

		FTransform TrajectoryPointTM;
		TrajectoryPointTM.SetTranslation(TrajectoryLocation);

		FTransform RelativeTM = TrajectoryPointTM.GetRelativeTransform(CharacterMeshTM);
		OutGoal.DesiredTrajectory.Add(FTrajectoryPoint(RelativeTM.GetTranslation(), RelativeTM.GetRotation(), TrajectoryInterval));
		//OutGoal.DesiredTrajectory.Add(TrajectoryPoint);
	}

	return OutGoal;
}

void UMotionMatchingUtilities::DrawDebugGoal(const UWorld* World, const FGoal& InGoal, const FTransform CharacterMeshTM)
{
	if (World)
	{
		if (InGoal.DesiredTrajectory.Num() > 0)
		{
			const FVector InitialLocation = CharacterMeshTM.TransformPosition(InGoal.DesiredTrajectory[0].Location);

			DrawDebugLine(World, CharacterMeshTM.GetLocation(), InitialLocation, FColor::Blue, false, -1.0f, 0, 2.0f);
			DrawDebugPoint(World, InitialLocation, 15.0f, FColor::White);

			for (int i = 1; i < InGoal.DesiredTrajectory.Num(); ++i)
			{
				const FVector CurrentLocation = CharacterMeshTM.TransformPosition(InGoal.DesiredTrajectory[i].Location);
				const FVector PreviousLocation = CharacterMeshTM.TransformPosition(InGoal.DesiredTrajectory[i - 1].Location);

				DrawDebugLine(World, PreviousLocation, CurrentLocation, FColor::Blue, false, -1.0f, 0, 2.0f);
				DrawDebugSphere(World, CurrentLocation, 15.0f, 10, FColor::Blue, false, -1.0f, 0, 2.0f);
			}
		}		
	}
}

void UMotionMatchingUtilities::DrawDebugDirection(const UWorld* World, const FGoal& InGoal, const FTransform CharacterMeshTM)
{
	if (World)
	{
		if (InGoal.DesiredTrajectory.Num() > 0)
		{
			const FVector FutureLocation = CharacterMeshTM.TransformPosition(InGoal.DesiredTrajectory.Last().Location * 0.25f);
			DrawDebugDirectionalArrow(World, CharacterMeshTM.GetLocation(), FutureLocation, 30.0f, FColor::White, false, -1.0f, 0, 2.0f);
		}
	}
}

TArray<struct FMotionBoneData> UMotionMatchingUtilities::GetBoneDataFromAnimation(const UAnimSequence* InAnimSequence, const float InTime, const TArray<FName>& InBones)
{
	TArray<FMotionBoneData> MotionBonesData;

	if (InAnimSequence)
	{
		// Loop through all bones
		for (const FName& BoneName : InBones)
		{
			FMotionBoneData BoneData;

			const FReferenceSkeleton ReferenceSkeleton = InAnimSequence->GetSkeleton()->GetReferenceSkeleton();
			const int BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);

			FTransform CurrentTimeBoneTM = UMotionMatchingUtilities::GetTransformFromBoneSpace(InAnimSequence, InTime, ReferenceSkeleton, BoneIndex);
			FTransform PreviousTimeBoneTM = UMotionMatchingUtilities::GetTransformFromBoneSpace(InAnimSequence, InTime - MotionMatchingGlobals::PreviousTimeDelta, ReferenceSkeleton, BoneIndex);

			// Calculate velocity
			const FVector Velocity = (CurrentTimeBoneTM.GetLocation() - PreviousTimeBoneTM.GetLocation());
			const float VelocityDelta = Velocity.Size() / MotionMatchingGlobals::PreviousTimeDelta;

			// Required for conversion of world to component space
			FTransform RootTM;
			InAnimSequence->GetBoneTransform(RootTM, MotionMatchingGlobals::RootBoneIndex, InTime, false);

			FVector LocalBonePosition = RootTM.InverseTransformPositionNoScale(CurrentTimeBoneTM.GetLocation());
			FVector LocalBoneVelocity = RootTM.InverseTransformVectorNoScale(Velocity.GetSafeNormal() * VelocityDelta);

			BoneData.BonePosition = LocalBonePosition;
			BoneData.BoneVelocity = LocalBoneVelocity;

			// Add to the list of bones
			MotionBonesData.Add(BoneData);
		}
	}

	return MotionBonesData;
}

FTransform UMotionMatchingUtilities::GetTransformFromBoneSpace(const UAnimSequence* InAnimSequence, const float InTime, const struct FReferenceSkeleton& InReferenceSkeleton, const int InBoneIndex)
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
