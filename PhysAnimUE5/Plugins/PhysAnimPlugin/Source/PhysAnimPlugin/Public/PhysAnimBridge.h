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
	int32 NumPolicyTargetsWritten = 0;
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
	PHYSANIMPLUGIN_API inline constexpr float MannyRootHeightMeters = 0.912f;

	PHYSANIMPLUGIN_API const TArray<FName>& GetControlledBoneNames();
	PHYSANIMPLUGIN_API const TArray<FName>& GetRequiredBodyModifierBoneNames();
	PHYSANIMPLUGIN_API const TArray<FName>& GetSmplObservationBoneNames();
	PHYSANIMPLUGIN_API FName GetRootBoneName();

	PHYSANIMPLUGIN_API FName MakeControlName(FName BoneName);
	PHYSANIMPLUGIN_API FName MakeBodyModifierName(FName BoneName);

	PHYSANIMPLUGIN_API bool BuildInputTensorIndexMap(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError);

	PHYSANIMPLUGIN_API TArray<float> BuildFutureSampleTimeSchedule();
	PHYSANIMPLUGIN_API float ResolveFutureTargetTimeSeconds(float CurrentTimeSeconds, float RequestedFutureOffsetSeconds, float AnimationLengthSeconds);

	PHYSANIMPLUGIN_API FVector SmplVectorToUe(const FVector& SmplVector);
	PHYSANIMPLUGIN_API FVector UeVectorToSmpl(const FVector& UeVector);
	PHYSANIMPLUGIN_API FQuat SmplQuaternionToUe(const FQuat& SmplQuaternion);
	PHYSANIMPLUGIN_API FQuat UeQuaternionToSmpl(const FQuat& UeQuaternion);
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
