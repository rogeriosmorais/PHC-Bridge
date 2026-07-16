#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimLocomotionFrameAdapter.h"

#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimAnimationDataPose BuildAnimationPose(
		const FTransform& MeshToWorld,
		const float SampleTimeSeconds,
		const float RootProgressCm)
	{
		FPhysAnimAnimationDataPose Pose;
		Pose.SampleTimeSeconds = SampleTimeSeconds;
		const FVector RootAnimationLocalCm = FVector::RightVector * RootProgressCm;
		Pose.AnimationRootWorld = FTransform(
			MeshToWorld.GetRotation(),
			MeshToWorld.TransformPosition(RootAnimationLocalCm));
		Pose.AnimationBodySamplesUeWorldCm.Reserve(PhysAnimBridge::NumSmplBodies);

		for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
		{
			const FVector BodyAnimationLocalCm = RootAnimationLocalCm + FVector(
				0.0f,
				static_cast<float>(BodyIndex) * 0.25f,
				static_cast<float>(BodyIndex) * 2.0f);
			FPhysAnimBodySample Body;
			Body.Position = MeshToWorld.TransformPosition(BodyAnimationLocalCm);
			Body.Rotation = MeshToWorld.GetRotation();
			Body.LinearVelocity = MeshToWorld.TransformVectorNoScale(
				FVector::RightVector * 160.0f);
			Body.AngularVelocity = FVector::ZeroVector;
			Pose.AnimationBodySamplesUeWorldCm.Add(Body);
		}

		return Pose;
	}

	FPhysAnimLocomotionReplayRecord BuildValidReplay()
	{
		FPhysAnimLocomotionReplayRecord Replay;
		Replay.ActorFrame.ActorToWorld = FTransform(
			FQuat::Identity,
			FVector(100.0f, 200.0f, 90.0f));
		Replay.MeshFrame.MeshToWorld = FTransform(
			FQuat(FRotator(0.0f, -90.0f, 0.0f)),
			Replay.ActorFrame.ActorToWorld.GetLocation());
		Replay.AuthoredForwardAnimationLocal = FVector::RightVector;

		FPhysAnimWorldTrajectorySample StationarySample;
		StationarySample.TimeSeconds = 0.0f;
		StationarySample.WorldPositionCm = Replay.ActorFrame.ActorToWorld.GetLocation();
		StationarySample.WorldFacing = Replay.ActorFrame.ActorToWorld.GetRotation();
		StationarySample.WorldVelocityCmPerSecond = FVector::ZeroVector;
		Replay.WorldTrajectory.Samples.Add(StationarySample);

		FPhysAnimWorldTrajectorySample MovingSample;
		MovingSample.TimeSeconds = 0.2f;
		MovingSample.WorldPositionCm =
			StationarySample.WorldPositionCm + FVector::ForwardVector * 32.0f;
		MovingSample.WorldFacing = FQuat(FRotator(0.0f, 15.0f, 0.0f));
		MovingSample.WorldVelocityCmPerSecond = FVector::ForwardVector * 160.0f;
		Replay.WorldTrajectory.Samples.Add(MovingSample);

		Replay.SelectedAnimation.AssetPath = TEXT("/Game/Characters/Mannequins/Animations/MF_Unarmed_Walk_Fwd");
		Replay.SelectedAnimation.TimeSeconds = 0.4f;
		Replay.CurrentAnimationPose = BuildAnimationPose(
			Replay.MeshFrame.MeshToWorld,
			0.0f,
			0.0f);
		Replay.PhysicalBodySamplesUeWorldCm =
			Replay.CurrentAnimationPose.AnimationBodySamplesUeWorldCm;

		Replay.FutureAnimationPoses.Reserve(PhysAnimBridge::NumFutureSteps);
		for (int32 FutureIndex = 0; FutureIndex < PhysAnimBridge::NumFutureSteps; ++FutureIndex)
		{
			const float FutureTimeSeconds =
				static_cast<float>(FutureIndex + 1) * PhysAnimBridge::FutureStepSeconds;
			Replay.FutureAnimationPoses.Add(BuildAnimationPose(
				Replay.MeshFrame.MeshToWorld,
				FutureTimeSeconds,
				160.0f * FutureTimeSeconds));
		}

		FString Error;
		PhysAnimLocomotionFrameAdapter::AnimationPoseToCanonicalProtoPose(
			Replay.CurrentAnimationPose,
			Replay.MeshFrame.MeshToWorld,
			Replay.CanonicalBodyState,
			Error);
		PhysAnimLocomotionFrameAdapter::CanonicalBodyToPolicyObservation(
			Replay.CanonicalBodyState,
			0.0f,
			Replay.SelfObservation,
			Error);
		PhysAnimLocomotionFrameAdapter::AnimationFutureToPolicyMimicTarget(
			Replay.CanonicalBodyState,
			Replay.FutureAnimationPoses,
			Replay.MeshFrame.MeshToWorld,
			Replay.MimicTarget,
			Error);

		TArray<float> Actions;
		Actions.SetNumUninitialized(PhysAnimBridge::NumActionFloats);
		for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
		{
			Actions[ActionIndex] = static_cast<float>(ActionIndex) * 0.01f;
		}
		Replay.ActionSignature =
			PhysAnimLocomotionFrameAdapter::BuildPolicyActionSignature(Actions);
		return Replay;
	}

	bool ContainsFailure(
		const FPhysAnimFrameCandidateResult& Result,
		const TCHAR* ExpectedFragment)
	{
		for (const FString& Failure : Result.FailedInvariants)
		{
			if (Failure.Contains(ExpectedFragment))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimLocomotionFrameReplayContractTest,
	"PhysAnim.Locomotion.FrameAdapter.ReplayContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimLocomotionFrameReplayContractTest::RunTest(const FString& Parameters)
{
	const FPhysAnimLocomotionReplayRecord Replay = BuildValidReplay();
	FString Error;
	TestTrue(
		TEXT("A complete deterministic locomotion step satisfies the replay contract"),
		PhysAnimLocomotionFrameAdapter::ValidateReplayRecord(Replay, Error));
	TestTrue(TEXT("A valid replay reports no error"), Error.IsEmpty());
	TestEqual(
		TEXT("Self observation retains the training-contract shape"),
		Replay.SelfObservation.Values.Num(),
		PhysAnimBridge::SelfObsSize);
	TestEqual(
		TEXT("Mimic target retains the training-contract shape"),
		Replay.MimicTarget.Values.Num(),
		PhysAnimBridge::MimicTargetPosesSize);
	TestEqual(
		TEXT("Action signature records the complete policy action"),
		Replay.ActionSignature.ValueCount,
		PhysAnimBridge::NumActionFloats);

	FPhysAnimLocomotionReplayRecord MissingSelection = Replay;
	MissingSelection.SelectedAnimation.AssetPath.Reset();
	TestFalse(
		TEXT("A replay without selected-animation identity is rejected"),
		PhysAnimLocomotionFrameAdapter::ValidateReplayRecord(MissingSelection, Error));
	TestTrue(
		TEXT("The missing selected-animation failure is explicit"),
		Error.Contains(TEXT("selected animation")));

	FPhysAnimLocomotionReplayRecord MissingFuture = Replay;
	MissingFuture.FutureAnimationPoses.Reset();
	TestFalse(
		TEXT("A replay without the full future animation stream is rejected"),
		PhysAnimLocomotionFrameAdapter::ValidateReplayRecord(MissingFuture, Error));
	TestTrue(
		TEXT("The missing future stream failure is explicit"),
		Error.Contains(TEXT("future animation")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimLocomotionFrameFactorialTest,
	"PhysAnim.Locomotion.FrameAdapter.Factorial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimLocomotionFrameFactorialTest::RunTest(const FString& Parameters)
{
	const FPhysAnimLocomotionReplayRecord Replay = BuildValidReplay();
	TArray<FPhysAnimFrameCandidate> Candidates;
	for (const EPhysAnimPoseSearchFacingBasis PoseSearchBasis :
		{ EPhysAnimPoseSearchFacingBasis::ActorWorld, EPhysAnimPoseSearchFacingBasis::MeshWorld })
	{
		for (const EPhysAnimAnimationReferenceBasis AnimationBasis :
			{ EPhysAnimAnimationReferenceBasis::ActorWorld, EPhysAnimAnimationReferenceBasis::MeshWorld })
		{
			for (const EPhysAnimPolicyHeadingBasis HeadingBasis :
				{ EPhysAnimPolicyHeadingBasis::CurrentCanonicalRoot, EPhysAnimPolicyHeadingBasis::AnimationReferenceRoot })
			{
				FPhysAnimFrameCandidate Candidate;
				Candidate.PoseSearchFacingBasis = PoseSearchBasis;
				Candidate.AnimationReferenceBasis = AnimationBasis;
				Candidate.PolicyHeadingBasis = HeadingBasis;
				Candidates.Add(Candidate);
			}
		}
	}

	const FPhysAnimFrameFactorialReport Report =
		PhysAnimLocomotionFrameAdapter::EvaluateFactorial(Replay, Candidates);
	TestEqual(TEXT("The full 2x2x2 factorial is evaluated"), Report.Results.Num(), 8);
	TestEqual(TEXT("Only one frame contract survives every invariant"), Report.SurvivingCandidates.Num(), 1);

	if (Report.SurvivingCandidates.Num() == 1)
	{
		const FPhysAnimFrameCandidate& Survivor = Report.SurvivingCandidates[0];
		TestTrue(
			TEXT("The surviving Pose Search query is mesh-facing"),
			Survivor.PoseSearchFacingBasis == EPhysAnimPoseSearchFacingBasis::MeshWorld);
		TestTrue(
			TEXT("The surviving animation reference is the mesh frame"),
			Survivor.AnimationReferenceBasis == EPhysAnimAnimationReferenceBasis::MeshWorld);
		TestTrue(
			TEXT("The surviving policy heading is derived from the current canonical root"),
			Survivor.PolicyHeadingBasis == EPhysAnimPolicyHeadingBasis::CurrentCanonicalRoot);
	}

	bool bObservedActorFacingFailure = false;
	bool bObservedAnimationReferenceFailure = false;
	bool bObservedHeadingFailure = false;
	for (const FPhysAnimFrameCandidateResult& Result : Report.Results)
	{
		if (Result.Candidate.PoseSearchFacingBasis == EPhysAnimPoseSearchFacingBasis::ActorWorld)
		{
			bObservedActorFacingFailure |= ContainsFailure(Result, TEXT("actor-forward"));
		}
		if (Result.Candidate.AnimationReferenceBasis == EPhysAnimAnimationReferenceBasis::ActorWorld)
		{
			bObservedAnimationReferenceFailure |= ContainsFailure(Result, TEXT("future-root-progression"));
		}
		if (Result.Candidate.PolicyHeadingBasis == EPhysAnimPolicyHeadingBasis::AnimationReferenceRoot)
		{
			bObservedHeadingFailure |= ContainsFailure(Result, TEXT("training-contract"));
		}
	}

	TestTrue(TEXT("The actor-facing hypothesis is rejected by authored-forward mapping"), bObservedActorFacingFailure);
	TestTrue(TEXT("The actor animation reference is rejected by future-root progression"), bObservedAnimationReferenceFailure);
	TestTrue(TEXT("The animation-reference heading is rejected by the training contract"), bObservedHeadingFailure);

	const FPhysAnimFrameCandidateResult SurvivorResult =
		PhysAnimLocomotionFrameAdapter::EvaluateCandidate(
			Replay,
			Report.SurvivingCandidates[0]);
	TestTrue(TEXT("Zero velocity remains exact zero"), SurvivorResult.bZeroVelocityPreserved);
	TestTrue(TEXT("Frame conversion preserves velocity magnitude"), SurvivorResult.bMagnitudePreserved);
	TestTrue(TEXT("Facing conversion round-trips orientation"), SurvivorResult.bOrientationRoundTripPreserved);
	TestTrue(TEXT("Positive yaw remains positive"), SurvivorResult.bYawDirectionPreserved);
	TestTrue(TEXT("Current and future roots use one convention"), SurvivorResult.bCurrentFutureConventionConsistent);
	return true;
}

#endif
