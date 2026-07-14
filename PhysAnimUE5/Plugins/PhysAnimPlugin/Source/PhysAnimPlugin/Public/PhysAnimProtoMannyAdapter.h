#pragma once

#include "CoreMinimal.h"
#include "PhysicsEngine/ConstraintTypes.h"

class UPhysicsAsset;
struct FPhysAnimBodySample;
struct FPhysAnimFuturePoseSample;

struct PHYSANIMPLUGIN_API FPhysAnimMannyConstraintProfile
{
	// Physics Control targets are parent-relative, while Chaos limits are measured
	// between the authored parent (Frame2) and child (Frame1) constraint frames.
	FQuat ParentConstraintFrameRotation = FQuat::Identity;
	FQuat ChildConstraintFrameRotation = FQuat::Identity;
	EAngularConstraintMotion TwistMotion = ACM_Free;
	EAngularConstraintMotion Swing1Motion = ACM_Free;
	EAngularConstraintMotion Swing2Motion = ACM_Free;
	float TwistLimitDegrees = 180.0f;
	float Swing1LimitDegrees = 180.0f;
	float Swing2LimitDegrees = 180.0f;
};

namespace PhysAnimProtoMannyAdapter
{
	// Inverts the action-side bind-frame composition. All three rotations must
	// be expressed in the same UE reference frame.
	PHYSANIMPLUGIN_API FQuat RecoverCanonicalJointRotation(
		const FQuat& ParentBindRotation,
		const FQuat& BindParentRelativeRotation,
		const FQuat& CurrentParentRelativeRotation);

	// Keeps the policy root in the same motion/bone frame used by PoseSearch.
	// Physics body frames are intentionally reserved for descendant joint deltas.
	PHYSANIMPLUGIN_API FQuat BuildCanonicalSmplRootRotationFromBonePose(
		const FQuat& MeshWorldRotation,
		const FQuat& BindRootBoneComponentRotation,
		const FQuat& CurrentRootBoneComponentRotation);

	// Rebuilds policy-facing global SMPL rotations from live Manny body frames.
	PHYSANIMPLUGIN_API bool BuildCanonicalSmplRotationsFromBodyPose(
		const FQuat& RootCanonicalRotation,
		const TArray<FQuat>& BindBodyRotations,
		const TArray<FQuat>& CurrentBodyRotations,
		TArray<FQuat>& OutCanonicalGlobalRotations,
		FString& OutError);

	// Applies each sampled Manny bone's bind-to-sample delta to its captured
	// physics body frame before rebuilding policy-facing global SMPL rotations.
	PHYSANIMPLUGIN_API bool BuildCanonicalSmplRotationsFromBonePose(
		const FQuat& RootCanonicalRotation,
		const TArray<FQuat>& BindBoneRotations,
		const TArray<FQuat>& SampleBoneRotations,
		const TArray<FQuat>& BindBodyRotations,
		TArray<FQuat>& OutCanonicalGlobalRotations,
		FString& OutError);

	// Reconstructs a virtual ProtoMotions v2.3 SMPL body tree from Manny's live
	// body rotations. This keeps the policy-facing embodiment independent from
	// Manny's bone lengths and from collapsed wrist/hand samples.
	PHYSANIMPLUGIN_API bool AdaptBodySamplesToCanonicalSmpl(
		const TArray<FPhysAnimBodySample>& MannyBodySamples,
		TArray<FPhysAnimBodySample>& OutProtoBodySamples,
		FString& OutError);

	// Applies the same virtual SMPL forward kinematics to PoseSearch targets so
	// current and future observations remain in one policy-facing embodiment.
	PHYSANIMPLUGIN_API bool AdaptFuturePoseSamplesToCanonicalSmpl(
		const TArray<FPhysAnimFuturePoseSample>& MannyFuturePoseSamples,
		TArray<FPhysAnimFuturePoseSample>& OutProtoFuturePoseSamples,
		FString& OutError);

	// Freezes a selected PoseSearch motion into the live policy frame. Capturing
	// this once preserves the recovery error instead of letting the target follow
	// the simulated root after a perturbation.
	PHYSANIMPLUGIN_API FTransform BuildFrozenTargetRootAlignment(
		const FTransform& SelectedDataRoot,
		const FTransform& LivePolicyRoot);

	PHYSANIMPLUGIN_API bool ApplyFrozenTargetRootAlignment(
		const FTransform& DataToPolicyAlignment,
		const TArray<FPhysAnimFuturePoseSample>& DataFrameFuturePoseSamples,
		TArray<FPhysAnimFuturePoseSample>& OutPolicyFrameFuturePoseSamples,
		FString& OutError);

	// Returns false when the requested body pair has no direct authored constraint.
	PHYSANIMPLUGIN_API bool BuildConstraintProfile(
		const UPhysicsAsset* PhysicsAsset,
		FName ChildBoneName,
		FName ParentBoneName,
		FPhysAnimMannyConstraintProfile& OutProfile);

	// Maps Proto's full-range PI * action exponential-map delta into the
	// corresponding authored Manny twist/swing range, relative to bind.
	PHYSANIMPLUGIN_API FQuat MapProtoPolicyTargetToMannyConstraintRange(
		const FQuat& ParentRelativeTargetRotation,
		const FQuat& MannyBindParentRelativeRotation,
		const FPhysAnimMannyConstraintProfile& ConstraintProfile);

	// Preserves the desired absolute joint angle, expresses it in Manny's
	// constraint frame, and projects only values outside the hard envelope.
	PHYSANIMPLUGIN_API FQuat AdaptParentRelativeTarget(
		const FQuat& ParentRelativeTargetRotation,
		const FQuat& MannyBindParentRelativeRotation,
		const FPhysAnimMannyConstraintProfile& ConstraintProfile,
		bool bEnableAdapter = true);
}
