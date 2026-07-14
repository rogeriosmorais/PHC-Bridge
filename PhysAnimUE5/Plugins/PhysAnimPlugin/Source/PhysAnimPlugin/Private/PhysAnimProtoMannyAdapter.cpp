#include "PhysAnimProtoMannyAdapter.h"

#include "PhysAnimBridge.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

namespace
{
	// Exact DFS hierarchy and local body offsets from ProtoMotions v2.3:
	// protomotions/data/assets/mjcf/smpl_humanoid.xml at tag commit
	// 4a905b998101333a2fb91f2de8e2cab4bd0db68e.
	static const int32 CanonicalSmplParentIndices[PhysAnimBridge::NumSmplBodies] =
	{
		INDEX_NONE,
		0, 1, 2, 3,
		0, 5, 6, 7,
		0, 9, 10, 11, 12,
		11, 14, 15, 16, 17,
		11, 19, 20, 21, 22
	};

	static const FVector CanonicalSmplLocalOffsetsMeters[PhysAnimBridge::NumSmplBodies] =
	{
		FVector::ZeroVector,
		FVector(-0.0068,  0.0695, -0.0914), // L_Hip
		FVector(-0.0045,  0.0343, -0.3752), // L_Knee
		FVector(-0.0437, -0.0136, -0.3980), // L_Ankle
		FVector( 0.1193,  0.0264, -0.0558), // L_Toe
		FVector(-0.0043, -0.0677, -0.0905), // R_Hip
		FVector(-0.0089, -0.0383, -0.3826), // R_Knee
		FVector(-0.0423,  0.0158, -0.3984), // R_Ankle
		FVector( 0.1233, -0.0254, -0.0481), // R_Toe
		FVector(-0.0267, -0.0025,  0.1090), // Torso
		FVector( 0.0011,  0.0055,  0.1352), // Spine
		FVector( 0.0254,  0.0015,  0.0529), // Chest
		FVector(-0.0429, -0.0028,  0.2139), // Neck
		FVector( 0.0513,  0.0052,  0.0650), // Head
		FVector(-0.0341,  0.0788,  0.1217), // L_Thorax
		FVector(-0.0089,  0.0910,  0.0305), // L_Shoulder
		FVector(-0.0275,  0.2596, -0.0128), // L_Elbow
		FVector(-0.0012,  0.2492,  0.0090), // L_Wrist
		FVector(-0.0149,  0.0840, -0.0082), // L_Hand
		FVector(-0.0386, -0.0818,  0.1188), // R_Thorax
		FVector(-0.0091, -0.0960,  0.0326), // R_Shoulder
		FVector(-0.0214, -0.2537, -0.0133), // R_Elbow
		FVector(-0.0056, -0.2553,  0.0078), // R_Wrist
		FVector(-0.0103, -0.0846, -0.0061)  // R_Hand
	};

	float ResolveAngularLimitRadians(
		const EAngularConstraintMotion Motion,
		const float LimitDegrees)
	{
		if (Motion == ACM_Free)
		{
			return UE_PI;
		}

		if (Motion == ACM_Locked)
		{
			return 0.0f;
		}

		return FMath::DegreesToRadians(FMath::Clamp(LimitDegrees, 0.0f, 180.0f));
	}

	FVector ResolveRotationVectorRadians(FQuat Rotation)
	{
		Rotation.Normalize();
		Rotation.EnforceShortestArcWith(FQuat::Identity);
		const FVector Imaginary(Rotation.X, Rotation.Y, Rotation.Z);
		const double SinHalfAngle = Imaginary.Size();
		if (SinHalfAngle <= UE_SMALL_NUMBER)
		{
			return Imaginary * 2.0;
		}

		const double AngleRadians = 2.0 * FMath::Atan2(SinHalfAngle, Rotation.W);
		return Imaginary * (AngleRadians / SinHalfAngle);
	}

	FQuat BuildRotationFromVectorRadians(const FVector& RotationVectorRadians)
	{
		const double AngleRadians = RotationVectorRadians.Size();
		if (AngleRadians <= UE_SMALL_NUMBER)
		{
			return FQuat::Identity;
		}

		return FQuat(RotationVectorRadians / AngleRadians, AngleRadians).GetNormalized();
	}

	float ResolveAvailableRangeScale(
		const EAngularConstraintMotion Motion,
		const float LimitDegrees,
		const float BindAngleRadians,
		const double ProtoRotationComponentRadians)
	{
		if (Motion == ACM_Free)
		{
			return 1.0f;
		}

		const float LimitRadians = ResolveAngularLimitRadians(Motion, LimitDegrees);
		if (LimitRadians <= UE_SMALL_NUMBER)
		{
			return 0.0f;
		}

		const float ClampedBindAngle = FMath::Clamp(
			BindAngleRadians,
			-LimitRadians,
			LimitRadians);
		const float AvailableRadians = ProtoRotationComponentRadians >= 0.0
			? LimitRadians - ClampedBindAngle
			: LimitRadians + ClampedBindAngle;
		return FMath::Max(0.0f, AvailableRadians) / UE_PI;
	}

	float ResolveSignedTwistAngleRadians(const FQuat& InTwist)
	{
		FQuat Twist = InTwist.GetNormalized();
		Twist.EnforceShortestArcWith(FQuat::Identity);
		float AngleRadians = Twist.GetAngle();
		if (AngleRadians > UE_PI)
		{
			AngleRadians -= 2.0f * UE_PI;
		}
		if (Twist.X < 0.0f)
		{
			AngleRadians = -AngleRadians;
		}
		return AngleRadians;
	}

	FVector2f ResolveSwingAnglesRadians(const FQuat& InSwing)
	{
		FQuat Swing = InSwing.GetNormalized();
		Swing.EnforceShortestArcWith(FQuat::Identity);
		const float Denominator = FMath::Max(UE_SMALL_NUMBER, 1.0f + static_cast<float>(Swing.W));
		return FVector2f(
			4.0f * FMath::Atan2(static_cast<float>(Swing.Y), Denominator),
			4.0f * FMath::Atan2(static_cast<float>(Swing.Z), Denominator));
	}

	FQuat BuildSwingFromAnglesRadians(const FVector2f& SwingAnglesRadians)
	{
		const float TanY = FMath::Tan(SwingAnglesRadians.X * 0.25f);
		const float TanZ = FMath::Tan(SwingAnglesRadians.Y * 0.25f);
		const float RadiusSquared = TanY * TanY + TanZ * TanZ;
		const float Denominator = 1.0f + RadiusSquared;
		return FQuat(
			0.0f,
			2.0f * TanY / Denominator,
			2.0f * TanZ / Denominator,
			(1.0f - RadiusSquared) / Denominator).GetNormalized();
	}

	FVector2f ProjectSwingAnglesToConstraint(
		FVector2f SwingAnglesRadians,
		const FPhysAnimMannyConstraintProfile& Profile)
	{
		const float Swing1LimitRadians = ResolveAngularLimitRadians(
			Profile.Swing1Motion,
			Profile.Swing1LimitDegrees);
		const float Swing2LimitRadians = ResolveAngularLimitRadians(
			Profile.Swing2Motion,
			Profile.Swing2LimitDegrees);

		if (Profile.Swing2Motion == ACM_Locked)
		{
			SwingAnglesRadians.X = 0.0f;
		}
		if (Profile.Swing1Motion == ACM_Locked)
		{
			SwingAnglesRadians.Y = 0.0f;
		}

		const bool bSwing1HasPositiveLimit =
			Profile.Swing1Motion == ACM_Limited && Swing1LimitRadians > UE_SMALL_NUMBER;
		const bool bSwing2HasPositiveLimit =
			Profile.Swing2Motion == ACM_Limited && Swing2LimitRadians > UE_SMALL_NUMBER;
		if (bSwing1HasPositiveLimit && bSwing2HasPositiveLimit)
		{
			const float EllipseOccupancySquared =
				FMath::Square(SwingAnglesRadians.X / Swing2LimitRadians) +
				FMath::Square(SwingAnglesRadians.Y / Swing1LimitRadians);
			if (EllipseOccupancySquared > 1.0f)
			{
				SwingAnglesRadians /= FMath::Sqrt(EllipseOccupancySquared);
			}
		}
		else
		{
			if (Profile.Swing2Motion == ACM_Limited)
			{
				SwingAnglesRadians.X = FMath::Clamp(
					SwingAnglesRadians.X,
					-Swing2LimitRadians,
					Swing2LimitRadians);
			}
			if (Profile.Swing1Motion == ACM_Limited)
			{
				SwingAnglesRadians.Y = FMath::Clamp(
					SwingAnglesRadians.Y,
					-Swing1LimitRadians,
					Swing1LimitRadians);
			}
		}

		return SwingAnglesRadians;
	}
}

FQuat PhysAnimProtoMannyAdapter::RecoverCanonicalJointRotation(
	const FQuat& ParentBindRotation,
	const FQuat& BindParentRelativeRotation,
	const FQuat& CurrentParentRelativeRotation)
{
	const FQuat NormalizedParentBind = ParentBindRotation.GetNormalized();
	const FQuat RotationInParentBindFrame =
		(CurrentParentRelativeRotation.GetNormalized() *
		 BindParentRelativeRotation.GetNormalized().Inverse()).GetNormalized();
	return (NormalizedParentBind *
		RotationInParentBindFrame *
		NormalizedParentBind.Inverse()).GetNormalized();
}

FQuat PhysAnimProtoMannyAdapter::BuildCanonicalSmplRootRotationFromBonePose(
	const FQuat& MeshWorldRotation,
	const FQuat& BindRootBoneComponentRotation,
	const FQuat& CurrentRootBoneComponentRotation)
{
	return (
		MeshWorldRotation.GetNormalized() *
		CurrentRootBoneComponentRotation.GetNormalized() *
		BindRootBoneComponentRotation.GetNormalized().Inverse()).GetNormalized();
}

bool PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBodyPose(
	const FQuat& RootCanonicalRotation,
	const TArray<FQuat>& BindBodyRotations,
	const TArray<FQuat>& CurrentBodyRotations,
	TArray<FQuat>& OutCanonicalGlobalRotations,
	FString& OutError)
{
	if (BindBodyRotations.Num() != PhysAnimBridge::NumSmplBodies ||
		CurrentBodyRotations.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = FString::Printf(
			TEXT("Expected %d bind/current Manny body rotations but found %d/%d."),
			PhysAnimBridge::NumSmplBodies,
			BindBodyRotations.Num(),
			CurrentBodyRotations.Num());
		OutCanonicalGlobalRotations.Reset();
		return false;
	}

	OutCanonicalGlobalRotations.Init(FQuat::Identity, PhysAnimBridge::NumSmplBodies);
	OutCanonicalGlobalRotations[0] = RootCanonicalRotation.GetNormalized();
	for (int32 BodyIndex = 1; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const int32 ParentIndex = CanonicalSmplParentIndices[BodyIndex];
		const FQuat ParentBindRotation = BindBodyRotations[ParentIndex].GetNormalized();
		const FQuat BindParentRelativeRotation =
			(ParentBindRotation.Inverse() * BindBodyRotations[BodyIndex].GetNormalized()).GetNormalized();
		const FQuat CurrentParentRelativeRotation =
			(CurrentBodyRotations[ParentIndex].GetNormalized().Inverse() *
			 CurrentBodyRotations[BodyIndex].GetNormalized()).GetNormalized();
		const FQuat CanonicalJointRotation = RecoverCanonicalJointRotation(
			ParentBindRotation,
			BindParentRelativeRotation,
			CurrentParentRelativeRotation);
		OutCanonicalGlobalRotations[BodyIndex] =
			(OutCanonicalGlobalRotations[ParentIndex] * CanonicalJointRotation).GetNormalized();
	}

	OutError.Reset();
	return true;
}

bool PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBonePose(
	const FQuat& RootCanonicalRotation,
	const TArray<FQuat>& BindBoneRotations,
	const TArray<FQuat>& SampleBoneRotations,
	const TArray<FQuat>& BindBodyRotations,
	TArray<FQuat>& OutCanonicalGlobalRotations,
	FString& OutError)
{
	if (BindBoneRotations.Num() != PhysAnimBridge::NumSmplBodies ||
		SampleBoneRotations.Num() != PhysAnimBridge::NumSmplBodies ||
		BindBodyRotations.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = FString::Printf(
			TEXT("Expected %d bind/sample bone and bind body rotations but found %d/%d/%d."),
			PhysAnimBridge::NumSmplBodies,
			BindBoneRotations.Num(),
			SampleBoneRotations.Num(),
			BindBodyRotations.Num());
		OutCanonicalGlobalRotations.Reset();
		return false;
	}

	TArray<FQuat> PredictedBodyRotations;
	PredictedBodyRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const FQuat BoneBindToSampleDelta =
			(SampleBoneRotations[BodyIndex].GetNormalized() *
			 BindBoneRotations[BodyIndex].GetNormalized().Inverse()).GetNormalized();
		PredictedBodyRotations.Add(
			(BoneBindToSampleDelta * BindBodyRotations[BodyIndex].GetNormalized()).GetNormalized());
	}

	return BuildCanonicalSmplRotationsFromBodyPose(
		RootCanonicalRotation,
		BindBodyRotations,
		PredictedBodyRotations,
		OutCanonicalGlobalRotations,
		OutError);
}

bool PhysAnimProtoMannyAdapter::AdaptBodySamplesToCanonicalSmpl(
	const TArray<FPhysAnimBodySample>& MannyBodySamples,
	TArray<FPhysAnimBodySample>& OutProtoBodySamples,
	FString& OutError)
{
	if (MannyBodySamples.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = FString::Printf(
			TEXT("Expected %d Manny body samples for SMPL adaptation but found %d."),
			PhysAnimBridge::NumSmplBodies,
			MannyBodySamples.Num());
		OutProtoBodySamples.Reset();
		return false;
	}

	OutProtoBodySamples = MannyBodySamples;
	for (int32 BodyIndex = 1; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const int32 ParentIndex = CanonicalSmplParentIndices[BodyIndex];
		const FPhysAnimBodySample& ProtoParent = OutProtoBodySamples[ParentIndex];
		FPhysAnimBodySample& ProtoBody = OutProtoBodySamples[BodyIndex];
		const FVector ParentToBody = ProtoParent.Rotation.RotateVector(
			CanonicalSmplLocalOffsetsMeters[BodyIndex]);
		ProtoBody.Position = ProtoParent.Position + ParentToBody;
		ProtoBody.LinearVelocity = ProtoParent.LinearVelocity + FVector::CrossProduct(
			ProtoParent.AngularVelocity,
			ParentToBody);
	}

	OutError.Reset();
	return true;
}

bool PhysAnimProtoMannyAdapter::AdaptFuturePoseSamplesToCanonicalSmpl(
	const TArray<FPhysAnimFuturePoseSample>& MannyFuturePoseSamples,
	TArray<FPhysAnimFuturePoseSample>& OutProtoFuturePoseSamples,
	FString& OutError)
{
	OutProtoFuturePoseSamples = MannyFuturePoseSamples;
	for (int32 FutureIndex = 0; FutureIndex < OutProtoFuturePoseSamples.Num(); ++FutureIndex)
	{
		FPhysAnimFuturePoseSample& ProtoFuture = OutProtoFuturePoseSamples[FutureIndex];
		if (ProtoFuture.BodyTransforms.Num() != PhysAnimBridge::NumSmplBodies)
		{
			OutError = FString::Printf(
				TEXT("Expected %d Manny future body transforms at step %d but found %d."),
				PhysAnimBridge::NumSmplBodies,
				FutureIndex,
				ProtoFuture.BodyTransforms.Num());
			OutProtoFuturePoseSamples.Reset();
			return false;
		}

		for (int32 BodyIndex = 1; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
		{
			const int32 ParentIndex = CanonicalSmplParentIndices[BodyIndex];
			const FTransform& ProtoParent = ProtoFuture.BodyTransforms[ParentIndex];
			FTransform& ProtoBody = ProtoFuture.BodyTransforms[BodyIndex];
			ProtoBody.SetLocation(
				ProtoParent.GetLocation() +
				ProtoParent.GetRotation().RotateVector(CanonicalSmplLocalOffsetsMeters[BodyIndex]));
		}
	}

	OutError.Reset();
	return true;
}

bool PhysAnimProtoMannyAdapter::BuildConstraintProfile(
	const UPhysicsAsset* const PhysicsAsset,
	const FName ChildBoneName,
	const FName ParentBoneName,
	FPhysAnimMannyConstraintProfile& OutProfile)
{
	OutProfile = {};
	if (!PhysicsAsset || ChildBoneName.IsNone() || ParentBoneName.IsNone())
	{
		return false;
	}

	const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
	if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
	{
		return false;
	}

	const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
	if (!ConstraintTemplate)
	{
		return false;
	}

	const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
	OutProfile.ParentConstraintFrameRotation =
		Constraint.GetRefFrame(EConstraintFrame::Frame2).GetRotation().GetNormalized();
	OutProfile.ChildConstraintFrameRotation =
		Constraint.GetRefFrame(EConstraintFrame::Frame1).GetRotation().GetNormalized();
	OutProfile.TwistMotion = Constraint.GetAngularTwistMotion();
	OutProfile.Swing1Motion = Constraint.GetAngularSwing1Motion();
	OutProfile.Swing2Motion = Constraint.GetAngularSwing2Motion();
	OutProfile.TwistLimitDegrees = Constraint.GetAngularTwistLimit();
	OutProfile.Swing1LimitDegrees = Constraint.GetAngularSwing1Limit();
	OutProfile.Swing2LimitDegrees = Constraint.GetAngularSwing2Limit();
	return true;
}

FQuat PhysAnimProtoMannyAdapter::MapProtoPolicyTargetToMannyConstraintRange(
	const FQuat& ParentRelativeTargetRotation,
	const FQuat& MannyBindParentRelativeRotation,
	const FPhysAnimMannyConstraintProfile& ConstraintProfile)
{
	const FQuat ParentConstraintFrameRotation =
		ConstraintProfile.ParentConstraintFrameRotation.GetNormalized();
	const FQuat ChildConstraintFrameRotation =
		ConstraintProfile.ChildConstraintFrameRotation.GetNormalized();
	const FQuat BindInConstraintSpace = (
		ParentConstraintFrameRotation.Inverse() *
		MannyBindParentRelativeRotation.GetNormalized() *
		ChildConstraintFrameRotation).GetNormalized();
	const FQuat TargetInConstraintSpace = (
		ParentConstraintFrameRotation.Inverse() *
		ParentRelativeTargetRotation.GetNormalized() *
		ChildConstraintFrameRotation).GetNormalized();
	FQuat BindSwing;
	FQuat BindTwist;
	BindInConstraintSpace.ToSwingTwist(FVector::ForwardVector, BindSwing, BindTwist);
	BindSwing.Normalize();
	BindTwist.Normalize();
	const FVector2f BindSwingAnglesRadians = ResolveSwingAnglesRadians(BindSwing);
	const float BindTwistAngleRadians = ResolveSignedTwistAngleRadians(BindTwist);

	FVector ProtoRotationVectorRadians = ResolveRotationVectorRadians(
		(TargetInConstraintSpace * BindInConstraintSpace.Inverse()).GetNormalized());
	ProtoRotationVectorRadians.X *= ResolveAvailableRangeScale(
		ConstraintProfile.TwistMotion,
		ConstraintProfile.TwistLimitDegrees,
		BindTwistAngleRadians,
		ProtoRotationVectorRadians.X);
	ProtoRotationVectorRadians.Y *= ResolveAvailableRangeScale(
		ConstraintProfile.Swing2Motion,
		ConstraintProfile.Swing2LimitDegrees,
		BindSwingAnglesRadians.X,
		ProtoRotationVectorRadians.Y);
	ProtoRotationVectorRadians.Z *= ResolveAvailableRangeScale(
		ConstraintProfile.Swing1Motion,
		ConstraintProfile.Swing1LimitDegrees,
		BindSwingAnglesRadians.Y,
		ProtoRotationVectorRadians.Z);

	const FQuat MappedTargetInConstraintSpace = (
		BuildRotationFromVectorRadians(ProtoRotationVectorRadians) *
		BindInConstraintSpace).GetNormalized();
	return (
		ParentConstraintFrameRotation *
		MappedTargetInConstraintSpace *
		ChildConstraintFrameRotation.Inverse()).GetNormalized();
}

FQuat PhysAnimProtoMannyAdapter::AdaptParentRelativeTarget(
	const FQuat& ParentRelativeTargetRotation,
	const FQuat& MannyBindParentRelativeRotation,
	const FPhysAnimMannyConstraintProfile& ConstraintProfile,
	const bool bEnableAdapter)
{
	if (!bEnableAdapter)
	{
		return ParentRelativeTargetRotation;
	}

	const FQuat BindParentRelativeRotation = MannyBindParentRelativeRotation.GetNormalized();
	if (ParentRelativeTargetRotation.GetNormalized().AngularDistance(BindParentRelativeRotation) <= UE_SMALL_NUMBER)
	{
		return BindParentRelativeRotation;
	}

	const FQuat ParentConstraintFrameRotation =
		ConstraintProfile.ParentConstraintFrameRotation.GetNormalized();
	const FQuat ChildConstraintFrameRotation =
		ConstraintProfile.ChildConstraintFrameRotation.GetNormalized();
	FQuat TargetInConstraintSpace = (
		ParentConstraintFrameRotation.Inverse() *
		ParentRelativeTargetRotation.GetNormalized() *
		ChildConstraintFrameRotation).GetNormalized();
	// Range mapping has already selected the desired absolute actuator target.
	// This final safety stage only expresses and projects it in Chaos's authored
	// constraint frame.
	TargetInConstraintSpace.EnforceShortestArcWith(FQuat::Identity);

	FQuat FinalSwing;
	FQuat FinalTwist;
	TargetInConstraintSpace.ToSwingTwist(FVector::ForwardVector, FinalSwing, FinalTwist);
	FinalSwing.Normalize();
	FinalTwist.Normalize();
	const FVector2f ProjectedFinalSwingAnglesRadians = ProjectSwingAnglesToConstraint(
		ResolveSwingAnglesRadians(FinalSwing),
		ConstraintProfile);
	float ProjectedFinalTwistAngleRadians = ResolveSignedTwistAngleRadians(FinalTwist);
	if (ConstraintProfile.TwistMotion != ACM_Free)
	{
		const float TwistLimitRadians = ResolveAngularLimitRadians(
			ConstraintProfile.TwistMotion,
			ConstraintProfile.TwistLimitDegrees);
		ProjectedFinalTwistAngleRadians = FMath::Clamp(
			ProjectedFinalTwistAngleRadians,
			-TwistLimitRadians,
			TwistLimitRadians);
	}

	const FQuat ProjectedTargetInConstraintSpace = (
		BuildSwingFromAnglesRadians(ProjectedFinalSwingAnglesRadians) *
		FQuat(FVector::ForwardVector, ProjectedFinalTwistAngleRadians)).GetNormalized();
	return (
		ParentConstraintFrameRotation *
		ProjectedTargetInConstraintSpace *
		ChildConstraintFrameRotation.Inverse()).GetNormalized();
}
