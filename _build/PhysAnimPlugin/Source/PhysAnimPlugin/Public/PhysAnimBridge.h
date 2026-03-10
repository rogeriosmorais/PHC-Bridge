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
	PHYSANIMPLUGIN_API inline constexpr int32 NumFutureSteps = 15;
	PHYSANIMPLUGIN_API inline constexpr float FutureStepSeconds = 1.0f / 30.0f;

	PHYSANIMPLUGIN_API const TArray<FName>& GetControlledBoneNames();
	PHYSANIMPLUGIN_API const TArray<FName>& GetRequiredBodyModifierBoneNames();
	PHYSANIMPLUGIN_API const TArray<FName>& GetSmplObservationBoneNames();

	PHYSANIMPLUGIN_API FName MakeControlName(FName BoneName);
	PHYSANIMPLUGIN_API FName MakeBodyModifierName(FName BoneName);

	PHYSANIMPLUGIN_API bool BuildInputTensorIndexMap(
		const TArray<UE::NNE::FTensorDesc>& InputTensorDescs,
		FPhysAnimTensorIndexMap& OutIndexMap,
		FString& OutError);

	PHYSANIMPLUGIN_API TArray<float> BuildFutureSampleTimeSchedule();

	PHYSANIMPLUGIN_API FVector SmplVectorToUe(const FVector& SmplVector);
	PHYSANIMPLUGIN_API FVector UeVectorToSmpl(const FVector& UeVector);
	PHYSANIMPLUGIN_API FQuat SmplQuaternionToUe(const FQuat& SmplQuaternion);
	PHYSANIMPLUGIN_API FQuat UeQuaternionToSmpl(const FQuat& UeQuaternion);
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

	PHYSANIMPLUGIN_API void BuildZeroTerrain(TArray<float>& OutTerrain);

	PHYSANIMPLUGIN_API bool ConvertModelActionsToControlRotations(
		const TArray<float>& ModelActions,
		TMap<FName, FQuat>& OutControlRotations,
		FString& OutError);
}
