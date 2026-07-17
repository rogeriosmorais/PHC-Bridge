#pragma once

#include "CoreMinimal.h"
#include "NNETypes.h"

struct FPhysAnimTensorIndexMap
{
	int32 SelfObs = INDEX_NONE;
	int32 MimicTargetPoses = INDEX_NONE;
	int32 Terrain = INDEX_NONE;

	bool IsValid() const
	{
		return SelfObs != INDEX_NONE && MimicTargetPoses != INDEX_NONE && Terrain != INDEX_NONE;
	}
};

struct FPhysAnimBodySample
{
	FVector Position = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
};

struct FPhysAnimFuturePoseSample
{
	TArray<FTransform> BodyTransforms;
	float FutureTimeSeconds = 0.0f;
};

struct FPhysAnimActionConditioningSettings
{
	bool bForceZeroActions = false;
	float ActionScale = 1.0f;
	float ActionClampAbs = 1.0f;
	float ActionSmoothingAlpha = 1.0f;
};

struct FPhysAnimActionDiagnostics
{
	float RawMin = 0.0f;
	float RawMax = 0.0f;
	float RawMeanAbs = 0.0f;
	float ConditionedMeanAbs = 0.0f;
	int32 NumClampedActionFloats = 0;
};

struct FPhysAnimControlTargetDiagnostics
{
	bool bPolicyInfluenceActive = false;
	bool bFirstPolicyEnabledFrame = false;
	int32 NumNormalPolicyTargetsWritten = 0;
	int32 NumHeldTargetsWritten = 0;
	int32 NumTotalTargetsWritten = 0;
	FName MaxTargetDeltaBoneName = NAME_None;
	float MaxTargetDeltaDegrees = 0.0f;
	float MeanTargetDeltaDegrees = 0.0f;
	FName MaxRawPolicyOffsetBoneName = NAME_None;
	float MaxRawPolicyOffsetDegrees = 0.0f;
	float MeanRawPolicyOffsetDegrees = 0.0f;
	FName MaxLowerLimbLimitOccupancyBoneName = NAME_None;
	float MaxLowerLimbLimitOccupancy = 0.0f;
	float MaxLowerLimbLimitProxyDegrees = 0.0f;
	float MeanLowerLimbLimitOccupancy = 0.0f;
	int32 NumLowerLimbTargetsConsidered = 0;
};

struct FPhysAnimRuntimeInstabilitySettings
{
	bool bEnableAutomaticFailStop = true;
	float MaxRootHeightDeltaCm = 120.0f;
	float MaxRootLinearSpeedCmPerSecond = 1200.0f;
	float MaxRootAngularSpeedDegPerSecond = 720.0f;
	float UnstableGracePeriodSeconds = 0.25f;
};

struct FPhysAnimRuntimeInstabilityState
{
	bool bHasReferenceRootLocation = false;
	FVector ReferenceRootLocation = FVector::ZeroVector;
	float UnstableAccumulatedSeconds = 0.0f;
};

struct FPhysAnimBodyInstabilitySample
{
	FName BoneName = NAME_None;
	FVector Location = FVector::ZeroVector;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocity = FVector::ZeroVector;
	bool bIsSimulatingPhysics = false;
};

struct FPhysAnimRuntimeInstabilityDiagnostics
{
	FVector RawRootLocationCm = FVector::ZeroVector;
	FVector RawRootLinearVelocityCmPerSecondVector = FVector::ZeroVector;
	FVector RootLocationCm = FVector::ZeroVector;
	FVector RootLinearVelocityCmPerSecondVector = FVector::ZeroVector;
	float RootHeightDeltaCm = 0.0f;
	float RootLinearSpeedCmPerSecond = 0.0f;
	float RootAngularSpeedDegPerSecond = 0.0f;
	bool bHeightExceeded = false;
	bool bLinearSpeedExceeded = false;
	bool bAngularSpeedExceeded = false;
	float UnstableAccumulatedSeconds = 0.0f;
	int32 NumBodiesConsidered = 0;
	int32 NumSimulatingBodies = 0;
	FName MaxLinearSpeedBoneName = NAME_None;
	float MaxBodyLinearSpeedCmPerSecond = 0.0f;
	bool bMaxLinearSpeedBoneSimulatingPhysics = false;
	FName MaxAngularSpeedBoneName = NAME_None;
	float MaxBodyAngularSpeedDegPerSecond = 0.0f;
	bool bMaxAngularSpeedBoneSimulatingPhysics = false;
	FName MaxHeightDeltaBoneName = NAME_None;
	float MaxBodyHeightDeltaCm = 0.0f;
	bool bMaxHeightDeltaBoneSimulatingPhysics = false;
};

namespace PhysAnimBridge
{
	PHYSANIMPLUGIN_API inline constexpr int32 NumSmplBodies = 24;
	PHYSANIMPLUGIN_API inline constexpr int32 NumActionJoints = 23;
	PHYSANIMPLUGIN_API inline constexpr int32 NumActionFloats = 69;
	PHYSANIMPLUGIN_API inline constexpr int32 NumControlledBones = 21;
	PHYSANIMPLUGIN_API inline constexpr int32 NumRequiredBodyModifiers = 22;
	PHYSANIMPLUGIN_API inline constexpr int32 SelfObsSize = 358;
	PHYSANIMPLUGIN_API inline constexpr int32 MimicTargetPosesSize = 6495;
	PHYSANIMPLUGIN_API inline constexpr int32 TerrainSize = 256;
	PHYSANIMPLUGIN_API inline constexpr int32 TerrainSamplesPerAxis = 16;
	PHYSANIMPLUGIN_API inline constexpr float TerrainSampleWidth = 1.0f;
	PHYSANIMPLUGIN_API inline constexpr int32 NumFutureSteps = 15;
	PHYSANIMPLUGIN_API inline constexpr float FutureStepSeconds = 1.0f / 30.0f;
	PHYSANIMPLUGIN_API inline constexpr float CmToMeters = 0.01f;
	PHYSANIMPLUGIN_API inline constexpr float MetersToCm = 100.0f;
	PHYSANIMPLUGIN_API inline constexpr float MannyRootHeightMeters = 0.912f;

	struct PHYSANIMPLUGIN_API FPhysAnimProtoActionJointDescriptor
	{
		int32 ProtoJointIndex = INDEX_NONE;
		FName ProtoJointName = NAME_None;
		FName MannyBoneName = NAME_None;
		bool bSharesMappedControl = false;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimActionJointSemanticTrace
	{
		int32 ProtoJointIndex = INDEX_NONE;
		FName ProtoJointName = NAME_None;
		FName MannyBoneName = NAME_None;
		bool bSharesMappedControl = false;
		FVector RawAction = FVector::ZeroVector;
		FVector ConditionedAction = FVector::ZeroVector;
		FQuat RawDecodedRotationUe = FQuat::Identity;
		FQuat ConditionedDecodedRotationUe = FQuat::Identity;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimControlTargetSemanticTrace
	{
		FName MannyBoneName = NAME_None;
		FName ControlName = NAME_None;
		TArray<int32> SourceProtoJointIndices;
		FQuat CombinedDecodedRotationUe = FQuat::Identity;
		FQuat MannyNeutralRotation = FQuat::Identity;
		FQuat BindComposedRotation = FQuat::Identity;
		FQuat RangeScaledRotation = FQuat::Identity;
		FQuat DistalScaledRotation = FQuat::Identity;
		FQuat ConstraintRangeMappedRotation = FQuat::Identity;
		FQuat ConstraintAdaptedRotation = FQuat::Identity;
		FQuat BlendedRotation = FQuat::Identity;
		FQuat PublishedRotation = FQuat::Identity;
		FQuat ReadbackRotation = FQuat::Identity;
		bool bHasConstraintProfile = false;
		bool bTargetWritten = false;
		bool bReadbackSucceeded = false;
		int32 TwistMotion = INDEX_NONE;
		int32 Swing1Motion = INDEX_NONE;
		int32 Swing2Motion = INDEX_NONE;
		float TwistLimitDegrees = 0.0f;
		float Swing1LimitDegrees = 0.0f;
		float Swing2LimitDegrees = 0.0f;
		float LowerLimbRangeScale = 1.0f;
		float DistalRangeScale = 1.0f;
		float RawPolicyOffsetDegrees = 0.0f;
		float RangeScaleDeltaDegrees = 0.0f;
		float DistalScaleDeltaDegrees = 0.0f;
		float ConstraintRangeMappingDeltaDegrees = 0.0f;
		float ConstraintProjectionDeltaDegrees = 0.0f;
		float AdaptedToPublishedDeltaDegrees = 0.0f;
		float ReadbackErrorDegrees = 0.0f;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimActionSemanticTrace
	{
		void Reset()
		{
			bCaptured = false;
			CaptureScope.Reset();
			CaptureError.Reset();
			ActionJoints.Reset();
			ControlTargets.Reset();
		}

		bool bCaptured = false;
		FString CaptureScope;
		FString CaptureError;
		float PolicyStepDeltaTime = 0.0f;
		float PolicyInfluenceAlpha = 0.0f;
		float MaxAngularStepDegrees = 0.0f;
		bool bConstraintAdapterEnabled = false;
		TArray<FPhysAnimActionJointSemanticTrace> ActionJoints;
		TArray<FPhysAnimControlTargetSemanticTrace> ControlTargets;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimPolicyInferenceSnapshot
	{
		bool CaptureFirst(
			TConstArrayView<float> InSelfObservation,
			TConstArrayView<float> InMimicTargetPoses,
			TConstArrayView<float> InTerrain,
			TConstArrayView<float> InActions)
		{
			if (bCaptured)
			{
				return false;
			}

			SelfObservation.Append(InSelfObservation.GetData(), InSelfObservation.Num());
			MimicTargetPoses.Append(InMimicTargetPoses.GetData(), InMimicTargetPoses.Num());
			Terrain.Append(InTerrain.GetData(), InTerrain.Num());
			Actions.Append(InActions.GetData(), InActions.Num());
			bCaptured = true;
			return true;
		}

		bool CaptureFirstIf(
			bool bCondition,
			TConstArrayView<float> InSelfObservation,
			TConstArrayView<float> InMimicTargetPoses,
			TConstArrayView<float> InTerrain,
			TConstArrayView<float> InActions)
		{
			return bCondition && CaptureFirst(
				InSelfObservation,
				InMimicTargetPoses,
				InTerrain,
				InActions);
		}

		void Reset()
		{
			bCaptured = false;
			SelfObservation.Reset();
			MimicTargetPoses.Reset();
			Terrain.Reset();
			Actions.Reset();
		}

		bool bCaptured = false;
		TArray<float> SelfObservation;
		TArray<float> MimicTargetPoses;
		TArray<float> Terrain;
		TArray<float> Actions;
	};


#if WITH_DEV_AUTOMATION_TESTS
	inline constexpr double MannyLocalFrameRoundtripAxisProbeDegrees = 10.0;
	inline constexpr const TCHAR* MannyLocalFrameRoundtripQuaternionMultiplication =
		TEXT("Hamilton product; expressions are evaluated in the explicitly parenthesized order");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripActionCompositionOrder =
		TEXT("inverse(action_axis_reference) * canonical_input * action_axis_reference * policy_neutral");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripObservationRecoveryOrder =
		TEXT("observation_parent_bind * (manny_target * inverse(observation_bind_parent_relative)) * inverse(observation_parent_bind)");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripObservationBodySelection =
		TEXT("lowest_source_proto_joint_index");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripActionAxisFrame =
		TEXT("world_rotation_at_initial_control_bind_capture");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripActionBindComponentWorldFrame =
		TEXT("component_to_world_rotation_derived_from_initial_parent_and_observation_parent_bind");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripObservationBindFrame =
		TEXT("skeletal_mesh_component_space_bind");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripWorldAxisMode =
		TEXT("cached_parent_world");
	inline constexpr const TCHAR* MannyLocalFrameRoundtripComponentAxisMode =
		TEXT("cached_parent_mesh_component");

	struct PHYSANIMPLUGIN_API FPhysAnimMannyLocalFrameRoundtripCase
	{
		FName Label = NAME_None;
		FQuat InputCanonicalRotationUe = FQuat::Identity;
		FQuat MannyPreRangeTargetParentRelative = FQuat::Identity;
		FQuat RecoveredCanonicalRotationUe = FQuat::Identity;
		double AngularErrorDegrees = 0.0;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimMannyLocalFrameRoundtripControl
	{
		int32 ControlIndex = INDEX_NONE;
		FName MannyBoneName = NAME_None;
		FName ControlName = NAME_None;
		FName InitialControlChildBoneName = NAME_None;
		FName InitialControlParentBoneName = NAME_None;
		TArray<int32> SourceProtoJointIndices;
		TArray<FName> SourceProtoJointNames;
		TArray<int32> ObservationBodyIndices;
		TArray<FName> ObservationBodyNames;
		int32 RoundtripObservationBodyIndex = INDEX_NONE;
		FName RoundtripObservationBodyName = NAME_None;
		int32 ObservationParentBodyIndex = INDEX_NONE;
		FName ObservationParentBodyName = NAME_None;
		bool bDecisiveOneToOne = false;
		bool bOwnershipComplete = false;
		FString EffectiveActionAxisMode;
		FQuat CachedActionAxisReferenceRotation = FQuat::Identity;
		FQuat ActionBindComponentWorldRotation = FQuat::Identity;
		FQuat ComponentCorrectedActionAxisRotation = FQuat::Identity;
		FQuat EffectiveActionAxisRotation = FQuat::Identity;
		FQuat ActionBindParentRelativeRotation = FQuat::Identity;
		FQuat PolicyNeutralParentRelativeRotation = FQuat::Identity;
		FQuat ObservationParentBindComponentRotation = FQuat::Identity;
		FQuat ObservationBodyBindComponentRotation = FQuat::Identity;
		FQuat ObservationBindParentRelativeRotation = FQuat::Identity;
		FQuat ActualDecodedRotationUe = FQuat::Identity;
		FQuat ActualMannyPreRangeTargetParentRelative = FQuat::Identity;
		double ActionAxisVsObservationParentBindAngularDeltaDegrees = 0.0;
		double EffectiveActionAxisVsObservationParentBindComponentAngularDeltaDegrees = 0.0;
		double ActionBindVsObservationBindParentRelativeAngularDeltaDegrees = 0.0;
		double PolicyNeutralVsActionBindParentRelativeAngularDeltaDegrees = 0.0;
		double PolicyNeutralVsObservationBindParentRelativeAngularDeltaDegrees = 0.0;
		TArray<FPhysAnimMannyLocalFrameRoundtripCase> RoundtripCases;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimMannyLocalFrameRoundtripTrace
	{
		void Reset()
		{
			bCaptured = false;
			CaptureScope.Reset();
			CaptureError.Reset();
			ConfiguredActionAxisMode.Reset();
			EffectiveActionAxisMode.Reset();
			AxisProbeDegrees = MannyLocalFrameRoundtripAxisProbeDegrees;
			Controls.Reset();
		}

		bool bCaptured = false;
		FString CaptureScope;
		FString CaptureError;
		FString ConfiguredActionAxisMode;
		FString EffectiveActionAxisMode;
		double AxisProbeDegrees = MannyLocalFrameRoundtripAxisProbeDegrees;
		TArray<FPhysAnimMannyLocalFrameRoundtripControl> Controls;
	};

	PHYSANIMPLUGIN_API bool ValidateMannyLocalFrameRoundtripTrace(
		const FPhysAnimMannyLocalFrameRoundtripTrace& Trace,
		FString& OutError);

	struct PHYSANIMPLUGIN_API FPhysAnimPolicyInputProvenanceSnapshot
	{
		bool CaptureFirstIf(
			bool bCondition,
			const FString& InRuntimeState,
			double InWorldTimeSeconds,
			int32 InPolicyControlTick,
			const FString& InPoseSearchAnimation,
			float InPoseSearchSelectedTime,
			bool bInPoseSearchMirrored,
			const FTransform& InOwnerActorWorldTransform,
			const FTransform& InMeshWorldTransform,
			const FTransform& InRootBoneWorldTransform,
			const FTransform& InMimicTargetReferenceWorldRoot,
			const FTransform& InMimicTargetReferenceDataRoot,
			float InSelfObservationGroundHeight,
			TConstArrayView<FPhysAnimBodySample> InMannyBodySamples,
			TConstArrayView<FPhysAnimBodySample> InCanonicalBodySamples,
			TConstArrayView<FPhysAnimBodySample> InMimicReferenceBodySamples,
			TConstArrayView<FPhysAnimFuturePoseSample> InMannyFuturePoseSamples,
			TConstArrayView<FPhysAnimFuturePoseSample> InCanonicalFuturePoseSamples,
			TConstArrayView<float> InTerrainGroundHeights,
			TConstArrayView<float> InPreviousActions)
		{
			if (!bCondition || bCaptured)
			{
				return false;
			}

			CaptureScope = TEXT("first_policy_input_pre_flattening");
			RuntimeState = InRuntimeState;
			WorldTimeSeconds = InWorldTimeSeconds;
			PolicyControlTick = InPolicyControlTick;
			PoseSearchAnimation = InPoseSearchAnimation;
			PoseSearchSelectedTime = InPoseSearchSelectedTime;
			bPoseSearchMirrored = bInPoseSearchMirrored;
			OwnerActorWorldTransform = InOwnerActorWorldTransform;
			MeshWorldTransform = InMeshWorldTransform;
			RootBoneWorldTransform = InRootBoneWorldTransform;
			MimicTargetReferenceWorldRoot = InMimicTargetReferenceWorldRoot;
			MimicTargetReferenceDataRoot = InMimicTargetReferenceDataRoot;
			SelfObservationGroundHeight = InSelfObservationGroundHeight;
			MannyBodySamples.Append(InMannyBodySamples.GetData(), InMannyBodySamples.Num());
			CanonicalBodySamples.Append(InCanonicalBodySamples.GetData(), InCanonicalBodySamples.Num());
			MimicReferenceBodySamples.Append(InMimicReferenceBodySamples.GetData(), InMimicReferenceBodySamples.Num());
			MannyFuturePoseSamples.Append(InMannyFuturePoseSamples.GetData(), InMannyFuturePoseSamples.Num());
			CanonicalFuturePoseSamples.Append(InCanonicalFuturePoseSamples.GetData(), InCanonicalFuturePoseSamples.Num());
			TerrainGroundHeights.Append(InTerrainGroundHeights.GetData(), InTerrainGroundHeights.Num());
			PreviousActions.Append(InPreviousActions.GetData(), InPreviousActions.Num());
			bCaptured = true;
			return true;
		}

		void Reset()
		{
			bCaptured = false;
			CaptureScope.Reset();
			RuntimeState.Reset();
			WorldTimeSeconds = 0.0;
			PolicyControlTick = 0;
			PoseSearchAnimation.Reset();
			PoseSearchSelectedTime = 0.0f;
			bPoseSearchMirrored = false;
			OwnerActorWorldTransform = FTransform::Identity;
			MeshWorldTransform = FTransform::Identity;
			RootBoneWorldTransform = FTransform::Identity;
			MimicTargetReferenceWorldRoot = FTransform::Identity;
			MimicTargetReferenceDataRoot = FTransform::Identity;
			SelfObservationGroundHeight = 0.0f;
			MannyBodySamples.Reset();
			CanonicalBodySamples.Reset();
			MimicReferenceBodySamples.Reset();
			MannyFuturePoseSamples.Reset();
			CanonicalFuturePoseSamples.Reset();
			TerrainGroundHeights.Reset();
			PreviousActions.Reset();
		}

		bool bCaptured = false;
		FString CaptureScope;
		FString RuntimeState;
		double WorldTimeSeconds = 0.0;
		int32 PolicyControlTick = 0;
		FString PoseSearchAnimation;
		float PoseSearchSelectedTime = 0.0f;
		bool bPoseSearchMirrored = false;
		FTransform OwnerActorWorldTransform = FTransform::Identity;
		FTransform MeshWorldTransform = FTransform::Identity;
		FTransform RootBoneWorldTransform = FTransform::Identity;
		FTransform MimicTargetReferenceWorldRoot = FTransform::Identity;
		FTransform MimicTargetReferenceDataRoot = FTransform::Identity;
		float SelfObservationGroundHeight = 0.0f;
		TArray<FPhysAnimBodySample> MannyBodySamples;
		TArray<FPhysAnimBodySample> CanonicalBodySamples;
		TArray<FPhysAnimBodySample> MimicReferenceBodySamples;
		TArray<FPhysAnimFuturePoseSample> MannyFuturePoseSamples;
		TArray<FPhysAnimFuturePoseSample> CanonicalFuturePoseSamples;
		TArray<float> TerrainGroundHeights;
		TArray<float> PreviousActions;
	};

	PHYSANIMPLUGIN_API bool ValidatePolicyInputProvenanceSnapshot(
		const FPhysAnimPolicyInputProvenanceSnapshot& Snapshot,
		FString& OutError);

	inline constexpr const TCHAR* LocomotionFrameReplayActionSignatureAlgorithm =
		TEXT("crc32-ieee754-f32-le-v1");

	struct PHYSANIMPLUGIN_API FPhysAnimLocomotionFrameReplaySnapshot
	{
		bool CaptureFirstIf(
			bool bCondition,
			const FString& InRuntimeState,
			double InWorldTimeSeconds,
			int32 InPolicyControlTick,
			const FString& InPoseSearchAnimation,
			float InPoseSearchSelectedTime,
			bool bInPoseSearchMirrored,
			const FTransform& InOwnerActorWorldTransform,
			const FTransform& InMeshWorldTransform,
			const FTransform& InSelectedAnimationWorldRootProtoMeters,
			const FTransform& InSelectedAnimationDataRootProtoMeters,
			float InIntentMagnitude,
			const FVector& InDesiredVelocityCmPerSecond,
			const FVector& InAcceptedVelocityCmPerSecond,
			const FVector& InResolvedQueryVelocityCmPerSecond,
			bool bInUseStabilizedWalkQuerySpeed,
			float InWalkIntentThreshold,
			float InStabilizedWalkSpeedCmPerSecond,
			float InIdlePredictedSpeedCutoffCmPerSecond,
			TConstArrayView<float> InRawQueryTrajectorySampleTimesSeconds,
			TConstArrayView<FTransform> InRawQueryTrajectoryWorldTransformsCm,
			TConstArrayView<float> InQueryTrajectorySampleTimesSeconds,
			TConstArrayView<FTransform> InQueryTrajectoryWorldTransformsCm,
			TConstArrayView<FPhysAnimBodySample> InLiveBodySamplesProtoWorldMeters,
			TConstArrayView<FPhysAnimBodySample> InPhysicalBodySamplesProtoWorldMeters,
			TConstArrayView<FPhysAnimBodySample> InCanonicalBodySamplesProtoMeters,
			TConstArrayView<FPhysAnimFuturePoseSample> InRawCanonicalFuturePoseSamples,
			TConstArrayView<FPhysAnimFuturePoseSample> InPlacedCanonicalFuturePoseSamples,
			TConstArrayView<float> InSelfObservation,
			TConstArrayView<float> InMimicTarget,
			TConstArrayView<float> InTerrain,
			TConstArrayView<float> InConditionedActions);

		void Reset();

		bool bCaptured = false;
		FString CaptureScope;
		FString RuntimeState;
		double WorldTimeSeconds = 0.0;
		int32 PolicyControlTick = 0;
		FString PoseSearchAnimation;
		float PoseSearchSelectedTime = 0.0f;
		bool bPoseSearchMirrored = false;
		FTransform OwnerActorWorldTransform = FTransform::Identity;
		FTransform MeshWorldTransform = FTransform::Identity;
		FTransform SelectedAnimationWorldRootProtoMeters = FTransform::Identity;
		FTransform SelectedAnimationDataRootProtoMeters = FTransform::Identity;
		float IntentMagnitude = 0.0f;
		FVector DesiredVelocityCmPerSecond = FVector::ZeroVector;
		FVector AcceptedVelocityCmPerSecond = FVector::ZeroVector;
		FVector ResolvedQueryVelocityCmPerSecond = FVector::ZeroVector;
		bool bUseStabilizedWalkQuerySpeed = false;
		float WalkIntentThreshold = 0.0f;
		float StabilizedWalkSpeedCmPerSecond = 0.0f;
		float IdlePredictedSpeedCutoffCmPerSecond = 0.0f;
		TArray<float> RawQueryTrajectorySampleTimesSeconds;
		TArray<FTransform> RawQueryTrajectoryWorldTransformsCm;
		TArray<float> QueryTrajectorySampleTimesSeconds;
		TArray<FTransform> QueryTrajectoryWorldTransformsCm;
		TArray<FPhysAnimBodySample> LiveBodySamplesProtoWorldMeters;
		TArray<FPhysAnimBodySample> PhysicalBodySamplesProtoWorldMeters;
		TArray<FPhysAnimBodySample> CanonicalBodySamplesProtoMeters;
		TArray<FPhysAnimFuturePoseSample> RawCanonicalFuturePoseSamples;
		TArray<FPhysAnimFuturePoseSample> PlacedCanonicalFuturePoseSamples;
		TArray<float> SelfObservation;
		TArray<float> MimicTarget;
		TArray<float> Terrain;
		TArray<float> ConditionedActions;
		FString ConditionedActionSignatureAlgorithm;
		uint32 ConditionedActionCrc32 = 0;
	};

	PHYSANIMPLUGIN_API bool ShouldCaptureLocomotionFrameReplay(
		bool bTraceEnabled,
		bool bPolicyInferenceEnabled,
		bool bVariantUsesPolicyInference,
		bool bLocomotionActive,
		bool bAlreadyCaptured);

	PHYSANIMPLUGIN_API bool ValidateLocomotionFrameReplaySnapshot(
		const FPhysAnimLocomotionFrameReplaySnapshot& Snapshot,
		FString& OutError);

	struct PHYSANIMPLUGIN_API FPhysAnimLocomotionTransitionCandidateScore
	{
		float PoseCost = 0.0f;
		float TransitionDiscontinuity = 0.0f;
	};

	PHYSANIMPLUGIN_API int32 SelectNearOptimalLocomotionTransitionCandidate(
		TConstArrayView<FPhysAnimLocomotionTransitionCandidateScore> Candidates,
		float MaxRelativePoseCostIncrease,
		float MinimumDiscontinuityImprovement);

	PHYSANIMPLUGIN_API bool BlendMimicTargetPosesForTransition(
		TConstArrayView<float> SourceMimicTarget,
		TConstArrayView<float> TargetMimicTarget,
		float Alpha,
		TArray<float>& OutBlendedMimicTarget,
		FString& OutError);

	struct PHYSANIMPLUGIN_API FPhysAnimPoseSearchTransitionCandidateDiagnostics
	{
		FString Animation;
		float SelectedTimeSeconds = 0.0f;
		bool bMirrored = false;
		float PoseCost = 0.0f;
		float LeftFootOrientationDeltaDegrees = 0.0f;
		float RightFootOrientationDeltaDegrees = 0.0f;
		float MaxFootOrientationDeltaDegrees = 0.0f;
		float MimicTargetStepDeltaL2 = 0.0f;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimMimicFrameDiagnostics
	{
		bool bValid = false;
		FString SelectedAnimation;
		float SelectedTimeSeconds = 0.0f;
		bool bMirrored = false;
		float FirstFutureTimeSeconds = 0.0f;
		int32 RotationProbeFutureIndex = INDEX_NONE;
		float RotationProbeFutureTimeSeconds = 0.0f;
		FTransform ReferenceWorldRootProtoMeters = FTransform::Identity;
		FTransform ReferenceDataRootProtoMeters = FTransform::Identity;
		FVector CurrentRootProtoMeters = FVector::ZeroVector;
		FVector CurrentLeftFootProtoMeters = FVector::ZeroVector;
		FVector RawFirstFutureRootProtoMeters = FVector::ZeroVector;
		FVector RawFirstFutureLeftFootProtoMeters = FVector::ZeroVector;
		FVector PlacedFirstFutureRootProtoMeters = FVector::ZeroVector;
		FVector PlacedFirstFutureLeftFootProtoMeters = FVector::ZeroVector;
		FQuat RawMannyProbeFutureRootRotation = FQuat::Identity;
		FQuat RawMannyProbeFutureRightFootRotation = FQuat::Identity;
		FQuat CorrectedMannyProbeFutureRootRotation = FQuat::Identity;
		FQuat CorrectedMannyProbeFutureRightFootRotation = FQuat::Identity;
		FQuat CurrentDataRootRotation = FQuat::Identity;
		FQuat CurrentDataRightFootRotation = FQuat::Identity;
		FQuat RawProbeFutureRootRotation = FQuat::Identity;
		FQuat RawProbeFutureRightFootRotation = FQuat::Identity;
		FQuat PlacedProbeFutureRootRotation = FQuat::Identity;
		FQuat PlacedProbeFutureRightFootRotation = FQuat::Identity;
		bool bIdleToLocomotionCandidateScan = false;
		TArray<FPhysAnimPoseSearchTransitionCandidateDiagnostics> LocomotionTransitionCandidates;

		void Reset()
		{
			*this = FPhysAnimMimicFrameDiagnostics();
		}
	};

	struct PHYSANIMPLUGIN_API FPhysAnimPoseSearchChannelSlice
	{
		FString Label;
		int32 DataOffset = INDEX_NONE;
		int32 Cardinality = 0;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimPoseSearchChannelCost
	{
		FString Label;
		int32 DataOffset = INDEX_NONE;
		int32 Cardinality = 0;
		float Cost = 0.0f;
	};

	PHYSANIMPLUGIN_API bool CalculatePoseSearchChannelCosts(
		TConstArrayView<float> PoseValues,
		TConstArrayView<float> QueryValues,
		TConstArrayView<float> WeightsSqrt,
		TConstArrayView<FPhysAnimPoseSearchChannelSlice> ChannelSlices,
		TArray<FPhysAnimPoseSearchChannelCost>& OutChannelCosts,
		float& OutTotalCost,
		FString& OutError);

	inline constexpr const TCHAR* FirstPolicyBodySourceFingerprintAlgorithm =
		TEXT("fnv1a64-ieee754-f64-le-v1");

	struct PHYSANIMPLUGIN_API FPhysAnimFirstPolicyBodySourceRecord
	{
		void Reset();

		bool bRecorded = false;
		FString Stage;
		double WorldTimeSeconds = -1.0;
		FString RuntimeState;
		int32 PolicyControlTick = INDEX_NONE;
		int32 BodySampleCount = 0;
		FString FingerprintAlgorithm;
		FString Fingerprint;
		TArray<FPhysAnimBodySample> BodySamples;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimFirstPolicyBodySourceTrace
	{
		bool CapturePriorIf(
			bool bCondition,
			const FString& InStage,
			double InWorldTimeSeconds,
			const FString& InRuntimeState,
			int32 InPolicyControlTick,
			TConstArrayView<FPhysAnimBodySample> InBodySamples);

		bool RecordFirstPolicySourceIf(
			bool bCondition,
			const FString& InStage,
			double InWorldTimeSeconds,
			const FString& InRuntimeState,
			int32 InPolicyControlTick,
			TConstArrayView<FPhysAnimBodySample> InLiveBodySamples,
			FString& OutError);

		void Reset();

		bool bFirstInferenceRecorded = false;
		FString ValidationError;
		FPhysAnimFirstPolicyBodySourceRecord Prior;
		FPhysAnimFirstPolicyBodySourceRecord Live;
		FPhysAnimFirstPolicyBodySourceRecord Effective;
	};

	PHYSANIMPLUGIN_API bool BuildFirstPolicyBodySourceFingerprint(
		TConstArrayView<FPhysAnimBodySample> BodySamples,
		FString& OutFingerprint,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ValidateFirstPolicyBodySourceTrace(
		const FPhysAnimFirstPolicyBodySourceTrace& Trace,
		FString& OutError);

	struct PHYSANIMPLUGIN_API FPhysAnimSelfObservationGroundReferenceValues
	{
		double BodyRootProtoZM = 0.0;
		double RootBoneWorldZCm = 0.0;
		bool bStaticTraceAttempted = false;
		bool bStaticTraceSucceeded = false;
		double StaticTraceImpactZCm = 0.0;
		bool bHasWalkableFloor = false;
		bool bHasBlockingFloorHit = false;
		double FloorImpactZCm = 0.0;
		bool bCapsuleAvailable = false;
		double CapsuleCenterZCm = 0.0;
		double CapsuleHalfHeightCm = 0.0;
		double FloorDistanceCm = 0.0;
		double FallbackGroundWorldZCm = 0.0;
		double GroundWorldZCm = 0.0;
		double SyntheticGroundHeightM = 0.0;
		double FinalRootHeightM = 0.0;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimFirstPolicyGroundReferenceRecord
	{
		void Reset();

		bool bRecorded = false;
		FString Stage;
		double WorldTimeSeconds = -1.0;
		FString RuntimeState;
		int32 PolicyControlTick = INDEX_NONE;
		FPhysAnimSelfObservationGroundReferenceValues Values;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimFirstPolicyGroundReferenceTrace
	{
		bool CapturePriorIf(
			bool bCondition,
			const FString& InStage,
			double InWorldTimeSeconds,
			const FString& InRuntimeState,
			int32 InPolicyControlTick,
			const FPhysAnimSelfObservationGroundReferenceValues& InValues);

		bool RecordFirstPolicyIf(
			bool bCondition,
			const FString& InStage,
			double InWorldTimeSeconds,
			const FString& InRuntimeState,
			int32 InPolicyControlTick,
			const FPhysAnimSelfObservationGroundReferenceValues& InValues,
			FString& OutError);

		void Reset();

		bool bFirstPolicyRecorded = false;
		FString ValidationError;
		FPhysAnimFirstPolicyGroundReferenceRecord Prior;
		FPhysAnimFirstPolicyGroundReferenceRecord Live;
	};

	PHYSANIMPLUGIN_API bool ValidateFirstPolicyGroundReferenceTrace(
		const FPhysAnimFirstPolicyGroundReferenceTrace& Trace,
		FString& OutError);

	inline constexpr int32 MaxStartupChronologySamples = 12;

	struct PHYSANIMPLUGIN_API FPhysAnimStartupChronologySample
	{
		int32 Sequence = 0;
		FString Stage;
		double WorldTimeSeconds = 0.0;
		FString RuntimeState;
		float PolicyUpdateAccumulatorSeconds = 0.0f;
		int32 LastPolicyElapsedSteps = 0;
		int32 PolicyControlTicksExecuted = 0;
		bool bFirstPolicyInputCaptured = false;
		FTransform OwnerActorWorldTransform = FTransform::Identity;
		FTransform MeshWorldTransform = FTransform::Identity;
		FTransform RootBoneWorldTransform = FTransform::Identity;
		TArray<FPhysAnimBodySample> BodySamples;
	};

	struct PHYSANIMPLUGIN_API FPhysAnimStartupChronologyTrace
	{
		bool CaptureIf(
			bool bCondition,
			const FString& InStage,
			double InWorldTimeSeconds,
			const FString& InRuntimeState,
			float InPolicyUpdateAccumulatorSeconds,
			int32 InLastPolicyElapsedSteps,
			int32 InPolicyControlTicksExecuted,
			bool bInFirstPolicyInputCaptured,
			const FTransform& InOwnerActorWorldTransform,
			const FTransform& InMeshWorldTransform,
			const FTransform& InRootBoneWorldTransform,
			TConstArrayView<FPhysAnimBodySample> InBodySamples)
		{
			if (!bCondition || bComplete || Samples.Num() >= MaxStartupChronologySamples)
			{
				return false;
			}

			FPhysAnimStartupChronologySample& Sample = Samples.AddDefaulted_GetRef();
			Sample.Sequence = Samples.Num() - 1;
			Sample.Stage = InStage;
			Sample.WorldTimeSeconds = InWorldTimeSeconds;
			Sample.RuntimeState = InRuntimeState;
			Sample.PolicyUpdateAccumulatorSeconds = InPolicyUpdateAccumulatorSeconds;
			Sample.LastPolicyElapsedSteps = InLastPolicyElapsedSteps;
			Sample.PolicyControlTicksExecuted = InPolicyControlTicksExecuted;
			Sample.bFirstPolicyInputCaptured = bInFirstPolicyInputCaptured;
			Sample.OwnerActorWorldTransform = InOwnerActorWorldTransform;
			Sample.MeshWorldTransform = InMeshWorldTransform;
			Sample.RootBoneWorldTransform = InRootBoneWorldTransform;
			Sample.BodySamples.Append(InBodySamples.GetData(), InBodySamples.Num());
			bComplete = InStage == TEXT("post_policy") && InPolicyControlTicksExecuted > 0;
			return true;
		}

		void Reset()
		{
			bComplete = false;
			CaptureError.Reset();
			Samples.Reset();
		}

		bool bComplete = false;
		FString CaptureError;
		TArray<FPhysAnimStartupChronologySample> Samples;
	};

	PHYSANIMPLUGIN_API bool ValidateStartupChronologyTrace(
		const FPhysAnimStartupChronologyTrace& Trace,
		FString& OutError);
#endif

	PHYSANIMPLUGIN_API const TArray<FName>& GetControlledBoneNames();
	PHYSANIMPLUGIN_API const TArray<FPhysAnimProtoActionJointDescriptor>& GetProtoActionJointDescriptors();
	PHYSANIMPLUGIN_API const TArray<FName>& GetRequiredBodyModifierBoneNames();
	PHYSANIMPLUGIN_API const TArray<FName>& GetSmplObservationBoneNames();
	PHYSANIMPLUGIN_API FName GetRootBoneName();

	PHYSANIMPLUGIN_API FName MakeControlName(FName BoneName);
	PHYSANIMPLUGIN_API FName MakeBodyModifierName(FName BoneName);
	PHYSANIMPLUGIN_API FName GetBoneNameFromControlName(FName ControlName);
	PHYSANIMPLUGIN_API FName GetBoneNameFromBodyModifierName(FName ModifierName);

	PHYSANIMPLUGIN_API bool BuildInputTensorIndexMap(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ValidateInputTensorDescs(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ValidateActionOutputTensorDescs(
		TConstArrayView<UE::NNE::FTensorDesc> OutputTensorDescs,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ValidateFiniteFloatBuffer(
		const TCHAR* BufferName,
		TConstArrayView<float> Values,
		FString& OutError);

	PHYSANIMPLUGIN_API TArray<float> BuildFutureSampleTimeSchedule();
	PHYSANIMPLUGIN_API float ResolveFutureTargetTimeSeconds(float CurrentTimeSeconds, float RequestedFutureOffsetSeconds, float AnimationLengthSeconds);

	PHYSANIMPLUGIN_API FVector SmplVectorToUe(const FVector& SmplVector);
	PHYSANIMPLUGIN_API FVector UeVectorToSmpl(const FVector& UeVector);
	PHYSANIMPLUGIN_API FQuat SmplQuaternionToUe(const FQuat& SmplQuaternion);
	PHYSANIMPLUGIN_API FQuat UeQuaternionToSmpl(const FQuat& UeQuaternion);
	// Policy actions are Isaac simulator joint coordinates, not raw SMPL authoring coordinates.
	PHYSANIMPLUGIN_API FQuat ProtoJointQuaternionToUe(const FQuat& ProtoJointQuaternion);
	PHYSANIMPLUGIN_API FVector UeWorldPositionToProtoRuntime(const FVector& UeVector);
	PHYSANIMPLUGIN_API FVector UeWorldVelocityToProtoRuntime(const FVector& UeVector);
	PHYSANIMPLUGIN_API FVector UeWorldRotationVectorToProtoRuntime(const FVector& UeVector);
	PHYSANIMPLUGIN_API FQuat UeWorldQuaternionToProtoRuntime(const FQuat& UeQuaternion);
	PHYSANIMPLUGIN_API FQuat ExpMapToQuaternion(const FVector& ExpMap);
	PHYSANIMPLUGIN_API FQuat CalculateHeadingInverseSmpl(const FQuat& SmplRootRotation);
	PHYSANIMPLUGIN_API void QuaternionToTanNorm(const FQuat& Rotation, float OutTanNorm[6]);
	PHYSANIMPLUGIN_API FQuat CollapseDistalHandRotation(const FQuat& WristRotation, const FQuat& HandRotation);

	PHYSANIMPLUGIN_API bool BuildSelfObservation(
		const TArray<FPhysAnimBodySample>& BodySamples,
		float GroundHeight,
		TArray<float>& OutSelfObservation,
		FString& OutError);

	PHYSANIMPLUGIN_API bool BuildMimicTargetPoses(
		const TArray<FPhysAnimBodySample>& CurrentBodySamples,
		const TArray<FPhysAnimFuturePoseSample>& FuturePoseSamples,
		TArray<float>& OutMimicTargetPoses,
		FString& OutError);

	PHYSANIMPLUGIN_API const TArray<FVector2D>& GetTerrainSampleOffsets();
	PHYSANIMPLUGIN_API FVector BuildTerrainSampleWorldLocation(
		const FVector& RootWorldLocationCm,
		const FQuat& RootWorldRotation,
		const FVector2D& LocalOffsetMeters);
	PHYSANIMPLUGIN_API bool BuildTerrainObservation(
		float RootHeight,
		const TArray<float>& SampleGroundHeights,
		TArray<float>& OutTerrain,
		FString& OutError);
	PHYSANIMPLUGIN_API void BuildZeroTerrain(TArray<float>& OutTerrain);

	PHYSANIMPLUGIN_API bool ConditionModelActions(
		const TArray<float>& RawActions,
		const TArray<float>* PreviousConditionedActions,
		const FPhysAnimActionConditioningSettings& Settings,
		TArray<float>& OutConditionedActions,
		FPhysAnimActionDiagnostics& OutDiagnostics,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ConvertModelActionsToControlRotations(
		const TArray<float>& ModelActions,
		TMap<FName, FQuat>& OutControlRotations,
		FString& OutError);

	PHYSANIMPLUGIN_API bool BuildActionJointSemanticTrace(
		const TArray<float>& RawActions,
		const TArray<float>& ConditionedActions,
		TArray<FPhysAnimActionJointSemanticTrace>& OutTrace,
		FString& OutError);

	PHYSANIMPLUGIN_API bool ValidateActionSemanticTrace(
		const FPhysAnimActionSemanticTrace& Trace,
		FString& OutError);

	PHYSANIMPLUGIN_API FQuat LimitControlRotationStep(
		const FQuat& PreviousRotation,
		const FQuat& TargetRotation,
		float MaxAngularStepDegrees);

	PHYSANIMPLUGIN_API bool UpdateRuntimeInstabilityState(
		const FVector& RootLocationCm,
		const FVector& RootLinearVelocityCmPerSecond,
		const FVector& RootAngularVelocityDegPerSecond,
		float DeltaTimeSeconds,
		const FPhysAnimRuntimeInstabilitySettings& Settings,
		FPhysAnimRuntimeInstabilityState& InOutState,
		FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics,
		FString& OutError);

	PHYSANIMPLUGIN_API void EvaluatePerBodyInstabilitySamples(
		const TArray<FPhysAnimBodyInstabilitySample>& Samples,
		const FVector& ReferenceRootLocationCm,
		FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics);
}
