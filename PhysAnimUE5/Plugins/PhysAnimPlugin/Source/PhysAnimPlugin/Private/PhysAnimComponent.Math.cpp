#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::IsPelvisSimulatingNow() const
{
	const USkeletalMeshComponent* const Mesh = GetMeshComponent();
	if (!Mesh)
	{
		return false;
	}

	const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName());
	return PelvisBody && PelvisBody->IsInstanceSimulatingPhysics();
}


bool UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(double ElapsedSeconds, double TimeoutSeconds)
{
	return TimeoutSeconds > 0.0 && ElapsedSeconds >= TimeoutSeconds;
}


bool UPhysAnimComponent::ShouldApplyPolicyTargetToBone(FName BoneName, bool bPolicyInfluenceActive)
{
	if (!bPolicyInfluenceActive)
	{
		return false;
	}

	return BoneName != TEXT("clavicle_l") &&
		BoneName != TEXT("spine_01") &&
		BoneName != TEXT("spine_02") &&
		BoneName != TEXT("spine_03") &&
		BoneName != TEXT("upperarm_l") &&
		BoneName != TEXT("lowerarm_l") &&
		BoneName != TEXT("hand_l") &&
		BoneName != TEXT("neck_01") &&
		BoneName != TEXT("head") &&
		BoneName != TEXT("clavicle_r") &&
		BoneName != TEXT("upperarm_r") &&
		BoneName != TEXT("lowerarm_r") &&
		BoneName != TEXT("hand_r");
}


bool UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(
	bool bConfiguredUseSkeletalAnimationTargets,
	bool bPolicyInfluenceActive)
{
	return bConfiguredUseSkeletalAnimationTargets || bPolicyInfluenceActive;
}


bool UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame)
{
	return bUseSkeletalAnimationTargetRepresentation && bFirstPolicyEnabledFrame;
}


float UPhysAnimComponent::ResolvePolicyTargetWriteDeltaTime(
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame,
	float DeltaTime)
{
	return (bUseSkeletalAnimationTargetRepresentation && bFirstPolicyEnabledFrame) ? 0.0f : DeltaTime;
}


float UPhysAnimComponent::ResolvePolicyTargetAngularVelocityDeltaTime(
	FName BoneName,
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame,
	bool bDistalLocomotionCompositionModeActive,
	float DeltaTime)
{
	if (bDistalLocomotionCompositionModeActive &&
		ShouldForceExplicitOnlyDistalLocomotionTargetMode(BoneName))
	{
		return 0.0f;
	}

	return ResolvePolicyTargetWriteDeltaTime(
		bUseSkeletalAnimationTargetRepresentation,
		bFirstPolicyEnabledFrame,
		DeltaTime);
}


float UPhysAnimComponent::ResolvePhase1Uprightness(
	USkeletalMeshComponent* SkeletalMesh,
	AActor* Owner,
	const FName& PelvisBoneName,
	FString& OutSourceName)
{
	if (!SkeletalMesh)
	{
		OutSourceName = TEXT("actor_up_no_mesh");
		return Owner ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Owner->GetActorUpVector(), FVector::UpVector), -1.0f, 1.0f))) : 0.0f;
	}

	// 1. Authoritative Phase 1 reference: Pelvis BodyInstance world transform
	if (const FBodyInstance* const PelvisBody = SkeletalMesh->GetBodyInstance(PelvisBoneName))
	{
		const FTransform PelvisTransform = PelvisBody->GetUnrealWorldTransform();
		
		// Robustly find the axis most aligned with World Up
		const FVector AxisX = PelvisTransform.GetUnitAxis(EAxis::X);
		const FVector AxisY = PelvisTransform.GetUnitAxis(EAxis::Y);
		const FVector AxisZ = PelvisTransform.GetUnitAxis(EAxis::Z);
		
		const float DotX = FMath::Abs(FVector::DotProduct(AxisX, FVector::UpVector));
		const float DotY = FMath::Abs(FVector::DotProduct(AxisY, FVector::UpVector));
		const float DotZ = FMath::Abs(FVector::DotProduct(AxisZ, FVector::UpVector));
		
		FVector PelvisUp = AxisZ;
		if (DotX > DotY && DotX > DotZ) PelvisUp = AxisX;
		else if (DotY > DotX && DotY > DotZ) PelvisUp = AxisY;

		OutSourceName = TEXT("pelvis_body");
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(PelvisUp, FVector::UpVector), -1.0f, 1.0f)));
	}

	// 2. Fallback: Mesh Root bone world transform
	const FQuat MeshRootQuat = SkeletalMesh->GetBoneQuaternion(PhysAnimBridge::GetRootBoneName());
	
	const FVector AxisX = MeshRootQuat.GetAxisX();
	const FVector AxisY = MeshRootQuat.GetAxisY();
	const FVector AxisZ = MeshRootQuat.GetAxisZ();
	
	const float DotX = FMath::Abs(FVector::DotProduct(AxisX, FVector::UpVector));
	const float DotY = FMath::Abs(FVector::DotProduct(AxisY, FVector::UpVector));
	const float DotZ = FMath::Abs(FVector::DotProduct(AxisZ, FVector::UpVector));
	
	FVector MeshRootUp = AxisZ;
	if (DotX > DotY && DotX > DotZ) MeshRootUp = AxisX;
	else if (DotY > DotX && DotY > DotZ) MeshRootUp = AxisY;

	OutSourceName = TEXT("mesh_root_world");
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(MeshRootUp, FVector::UpVector), -1.0f, 1.0f)));
}


float UPhysAnimComponent::ResolveObservationGroundWorldZFromFloor(
	bool bHasWalkableFloor,
	bool bHasBlockingFloorHit,
	float FloorImpactPointZ,
	float CapsuleCenterZ,
	float CapsuleHalfHeight,
	float FloorDistance,
	float FallbackGroundWorldZ)
{
	if (!bHasWalkableFloor)
	{
		return FallbackGroundWorldZ;
	}

	if (bHasBlockingFloorHit)
	{
		return FloorImpactPointZ;
	}

	return CapsuleCenterZ - CapsuleHalfHeight - FMath::Max(FloorDistance, 0.0f);
}


float UPhysAnimComponent::ResolveSelfObservationSyntheticGroundHeight(
	float ObservationFrameRootZ,
	float RootWorldZ,
	float GroundWorldZ)
{
	const float DesiredRootHeight = (RootWorldZ - GroundWorldZ) * PhysAnimBridge::CmToMeters;
	return ObservationFrameRootZ - DesiredRootHeight;
}


void UPhysAnimComponent::MakeGroundRelativeCurrentReferenceBodySamples(
	const TArray<FPhysAnimBodySample>& SourceBodySamples,
	float GroundWorldZ,
	TArray<FPhysAnimBodySample>& OutBodySamples)
{
	OutBodySamples = SourceBodySamples;
	for (FPhysAnimBodySample& BodySample : OutBodySamples)
	{
		BodySample.Position.Z -= GroundWorldZ;
	}
}


FVector2D UPhysAnimComponent::ResolveMimicTargetReferenceDataOffsetXY(
	const FVector& CurrentSelectedWorldRootPosition,
	const FVector& CurrentSelectedDataRootPosition)
{
	return FVector2D(
		CurrentSelectedWorldRootPosition.X - CurrentSelectedDataRootPosition.X,
		CurrentSelectedWorldRootPosition.Y - CurrentSelectedDataRootPosition.Y);
}


void UPhysAnimComponent::MakeMimicTargetCurrentReferenceBodySamples(
	const TArray<FPhysAnimBodySample>& SourceBodySamples,
	const FVector2D& DataOffsetXY,
	float GroundWorldZ,
	TArray<FPhysAnimBodySample>& OutBodySamples)
{
	OutBodySamples = SourceBodySamples;
	for (FPhysAnimBodySample& BodySample : OutBodySamples)
	{
		BodySample.Position.X -= DataOffsetXY.X;
		BodySample.Position.Y -= DataOffsetXY.Y;
		BodySample.Position.Z -= GroundWorldZ;
	}
}


float UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(float PolicyControlRateHz)
{
	const float ClampedRateHz = FMath::Max(PolicyControlRateHz, 1.0f);
	return 1.0f / ClampedRateHz;
}


bool UPhysAnimComponent::ShouldPrewarmPhysicsControlActivationPose(
	bool bHasSkeletalMeshComponent,
	bool bHasLeaderPoseComponent)
{
	return bHasSkeletalMeshComponent && !bHasLeaderPoseComponent;
}


float UPhysAnimComponent::ResolveTrainingAlignedMassScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (BoneName == TEXT("pelvis"))
	{
		TargetScale = 0.815f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_r"))
	{
		TargetScale = 1.569f;
	}
	else if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03") ||
		BoneName == TEXT("spine_04") ||
		BoneName == TEXT("spine_05"))
	{
		TargetScale = 0.855f;
	}
	else if (
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("neck_02") ||
		BoneName == TEXT("head"))
	{
		TargetScale = 0.762f;
	}
	else if (
		BoneName == TEXT("clavicle_l") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("clavicle_r") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.725f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedMassScales(bool bApplyTrainingAlignedMassScales, float BlendAlpha)
{
	return bApplyTrainingAlignedMassScales && BlendAlpha > UE_SMALL_NUMBER;
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedToeLimitPolicy(bool bApplyTrainingAlignedToeLimitPolicy, float BlendAlpha)
{
	return bApplyTrainingAlignedToeLimitPolicy && BlendAlpha > UE_SMALL_NUMBER;
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedLowerLimbTargetRangePolicy(
	bool bApplyTrainingAlignedLowerLimbTargetRangePolicy,
	float BlendAlpha)
{
	return bApplyTrainingAlignedLowerLimbTargetRangePolicy && BlendAlpha > UE_SMALL_NUMBER;
}


float UPhysAnimComponent::ResolveTrainingAlignedLowerLimbTargetRangeScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (BoneName == TEXT("calf_l") || BoneName == TEXT("calf_r"))
	{
		TargetScale = 0.50f;
	}
	else if (BoneName == TEXT("foot_l") || BoneName == TEXT("foot_r"))
	{
		TargetScale = 0.50f;
	}
	else if (BoneName == TEXT("ball_l") || BoneName == TEXT("ball_r"))
	{
		TargetScale = 0.35f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedDistalLocomotionTargetPolicy(
	bool bApplyTrainingAlignedDistalLocomotionTargetPolicy,
	float BlendAlpha,
	float OwnerPlanarSpeedCmPerSec,
	float ActivationSpeedCmPerSec)
{
	return bApplyTrainingAlignedDistalLocomotionTargetPolicy &&
		BlendAlpha > UE_SMALL_NUMBER &&
		OwnerPlanarSpeedCmPerSec > FMath::Max(0.0f, ActivationSpeedCmPerSec);
}


float UPhysAnimComponent::ResolveTrainingAlignedDistalLocomotionTargetScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (BoneName == TEXT("foot_l") || BoneName == TEXT("foot_r"))
	{
		TargetScale = 0.75f;
	}
	else if (BoneName == TEXT("ball_l") || BoneName == TEXT("ball_r"))
	{
		TargetScale = 0.50f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


bool UPhysAnimComponent::UpdateBinarySpeedModeWithHysteresis(
	bool bCurrentModeActive,
	float SpeedCmPerSec,
	float EnterThresholdCmPerSec,
	float ExitThresholdCmPerSec,
	float EnterHoldSeconds,
	float ExitHoldSeconds,
	float DeltaTimeSeconds,
	float& InOutTimeAboveEnterSeconds,
	float& InOutTimeBelowExitSeconds)
{
	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTimeSeconds);
	const float EnterThreshold = FMath::Max(0.0f, EnterThresholdCmPerSec);
	const float ExitThreshold = FMath::Min(EnterThreshold, FMath::Max(0.0f, ExitThresholdCmPerSec));
	const float EnterHold = FMath::Max(0.0f, EnterHoldSeconds);
	const float ExitHold = FMath::Max(0.0f, ExitHoldSeconds);

	if (bCurrentModeActive)
	{
		InOutTimeAboveEnterSeconds = 0.0f;
		if (SpeedCmPerSec <= ExitThreshold)
		{
			InOutTimeBelowExitSeconds += ClampedDeltaTime;
			if (InOutTimeBelowExitSeconds >= ExitHold)
			{
				InOutTimeBelowExitSeconds = 0.0f;
				return false;
			}
		}
		else
		{
			InOutTimeBelowExitSeconds = 0.0f;
		}

		return true;
	}

	InOutTimeBelowExitSeconds = 0.0f;
	if (SpeedCmPerSec >= EnterThreshold)
	{
		InOutTimeAboveEnterSeconds += ClampedDeltaTime;
		if (InOutTimeAboveEnterSeconds >= EnterHold)
		{
			InOutTimeAboveEnterSeconds = 0.0f;
			return true;
		}
	}
	else
	{
		InOutTimeAboveEnterSeconds = 0.0f;
	}

	return false;
}


bool UPhysAnimComponent::UpdateBinarySpeedModeWithIntentLatch(
	bool bCurrentModeActive,
	float SpeedCmPerSec,
	bool bHasActiveMovementIntent,
	float EnterThresholdCmPerSec,
	float ExitThresholdCmPerSec,
	float EnterHoldSeconds,
	float ExitHoldSeconds,
	float DeltaTimeSeconds,
	float& InOutTimeAboveEnterSeconds,
	float& InOutTimeBelowExitSeconds)
{
	if (bCurrentModeActive)
	{
		InOutTimeAboveEnterSeconds = 0.0f;
		if (bHasActiveMovementIntent)
		{
			InOutTimeBelowExitSeconds = 0.0f;
			return true;
		}

		if (SpeedCmPerSec <= FMath::Min(FMath::Max(0.0f, EnterThresholdCmPerSec), FMath::Max(0.0f, ExitThresholdCmPerSec)))
		{
			InOutTimeBelowExitSeconds += FMath::Max(0.0f, DeltaTimeSeconds);
			if (InOutTimeBelowExitSeconds >= FMath::Max(0.0f, ExitHoldSeconds))
			{
				InOutTimeBelowExitSeconds = 0.0f;
				return false;
			}
		}
		else
		{
			InOutTimeBelowExitSeconds = 0.0f;
		}

		return true;
	}

	InOutTimeBelowExitSeconds = 0.0f;
	if (bHasActiveMovementIntent)
	{
		InOutTimeAboveEnterSeconds = 0.0f;
		return true;
	}

	return UpdateBinarySpeedModeWithHysteresis(
		bCurrentModeActive,
		SpeedCmPerSec,
		EnterThresholdCmPerSec,
		ExitThresholdCmPerSec,
		EnterHoldSeconds,
		ExitHoldSeconds,
		DeltaTimeSeconds,
		InOutTimeAboveEnterSeconds,
		InOutTimeBelowExitSeconds);
}


bool UPhysAnimComponent::ShouldForceExplicitOnlyDistalLocomotionTargetMode(FName BoneName)
{
	return BoneName == TEXT("foot_l") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_r");
}


float UPhysAnimComponent::ResolveTrainingAlignedControlStrengthScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03"))
	{
		TargetScale = 1.25f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_r"))
	{
		TargetScale = 1.0f;
	}
	else if (
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r"))
	{
		TargetScale = 0.625f;
	}
	else if (
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.375f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


float UPhysAnimComponent::ResolveTrainingAlignedLocomotionLowerLimbDampingRatioScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("thigh_r"))
	{
		TargetScale = 1.10f;
	}
	else if (
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("calf_r"))
	{
		TargetScale = 1.25f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


float UPhysAnimComponent::ResolveTrainingAlignedControlExtraDampingScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03"))
	{
		TargetScale = 1.25f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_r"))
	{
		TargetScale = 1.0f;
	}
	else if (
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r"))
	{
		TargetScale = 0.625f;
	}
	else if (
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.375f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


float UPhysAnimComponent::ResolveTrainingAlignedLocomotionLowerLimbExtraDampingScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("thigh_r"))
	{
		TargetScale = 1.15f;
	}
	else if (
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("calf_r"))
	{
		TargetScale = 1.45f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedControlFamilyProfile(bool bApplyTrainingAlignedControlFamilyProfile, float BlendAlpha)
{
	return bApplyTrainingAlignedControlFamilyProfile && BlendAlpha > UE_SMALL_NUMBER;
}


bool UPhysAnimComponent::ShouldApplyTrainingAlignedLocomotionLowerLimbResponsePolicy(
	bool bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy,
	float BlendAlpha,
	bool bLocomotionModeActive)
{
	return bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy &&
		bLocomotionModeActive &&
		BlendAlpha > UE_SMALL_NUMBER;
}


float UPhysAnimComponent::ResolveShellCouplingPlanarOffsetDeltaCm(
	const FVector& OwnerLocationCm,
	const FVector& RootLocationCm,
	const FVector& ReferenceRootLocalOffsetCm)
{
	const FVector CurrentOffset = RootLocationCm - OwnerLocationCm;
	const FVector CurrentPlanarOffset(CurrentOffset.X, CurrentOffset.Y, 0.0f);
	const FVector ReferencePlanarOffset(ReferenceRootLocalOffsetCm.X, ReferenceRootLocalOffsetCm.Y, 0.0f);
	return FVector::Dist(CurrentPlanarOffset, ReferencePlanarOffset);
}


float UPhysAnimComponent::ResolveShellCouplingPlanarVelocityDeltaCmPerSecond(
	const FVector& OwnerVelocityCmPerSecond,
	const FVector& RootVelocityCmPerSecond)
{
	const FVector OwnerPlanarVelocity(OwnerVelocityCmPerSecond.X, OwnerVelocityCmPerSecond.Y, 0.0f);
	const FVector RootPlanarVelocity(RootVelocityCmPerSecond.X, RootVelocityCmPerSecond.Y, 0.0f);
	return FVector::Dist(OwnerPlanarVelocity, RootPlanarVelocity);
}


float UPhysAnimComponent::ResolveShellCouplingPlanarVelocityAlignment(
	const FVector& OwnerVelocityCmPerSecond,
	const FVector& RootVelocityCmPerSecond)
{
	const FVector OwnerPlanarVelocity(OwnerVelocityCmPerSecond.X, OwnerVelocityCmPerSecond.Y, 0.0f);
	const FVector RootPlanarVelocity(RootVelocityCmPerSecond.X, RootVelocityCmPerSecond.Y, 0.0f);
	const float OwnerSpeed = OwnerPlanarVelocity.Size();
	const float RootSpeed = RootPlanarVelocity.Size();
	if (OwnerSpeed <= UE_SMALL_NUMBER || RootSpeed <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FVector::DotProduct(OwnerPlanarVelocity / OwnerSpeed, RootPlanarVelocity / RootSpeed);
}


float UPhysAnimComponent::CalculateConstraintMinLimitedAngleDegrees(
	EAngularConstraintMotion TwistMotion,
	float TwistLimit,
	EAngularConstraintMotion Swing1Motion,
	float Swing1Limit,
	EAngularConstraintMotion Swing2Motion,
	float Swing2Limit)
{
	return PhysAnimComponentInternal::CalculateConstraintMinLimitedAngleDegrees(
		TwistMotion,
		TwistLimit,
		Swing1Motion,
		Swing1Limit,
		Swing2Motion,
		Swing2Limit);
}


bool UPhysAnimComponent::AdvancePolicyControlAccumulator(
	float DeltaTimeSeconds,
	float PolicyControlIntervalSeconds,
	float& InOutAccumulatorSeconds,
	int32& OutElapsedSteps)
{
	OutElapsedSteps = 0;

	if (PolicyControlIntervalSeconds <= UE_SMALL_NUMBER)
	{
		InOutAccumulatorSeconds = 0.0f;
		OutElapsedSteps = 1;
		return true;
	}

	if (InOutAccumulatorSeconds < 0.0f)
	{
		InOutAccumulatorSeconds = PolicyControlIntervalSeconds;
	}

	InOutAccumulatorSeconds += FMath::Max(DeltaTimeSeconds, 0.0f);
	OutElapsedSteps = FMath::FloorToInt(InOutAccumulatorSeconds / PolicyControlIntervalSeconds);
	if (OutElapsedSteps <= 0)
	{
		return false;
	}

	InOutAccumulatorSeconds = FMath::Fmod(InOutAccumulatorSeconds, PolicyControlIntervalSeconds);
	return true;
}


FQuat UPhysAnimComponent::BlendPolicyTargetRotation(
	const FQuat& BaselineRotation,
	const FQuat& PolicyTargetRotation,
	float PolicyAlpha)
{
	const float ClampedPolicyAlpha = FMath::Clamp(PolicyAlpha, 0.0f, 1.0f);
	if (ClampedPolicyAlpha <= KINDA_SMALL_NUMBER)
	{
		return BaselineRotation;
	}

	if (ClampedPolicyAlpha >= (1.0f - KINDA_SMALL_NUMBER))
	{
		return PolicyTargetRotation;
	}

	return FQuat::Slerp(BaselineRotation, PolicyTargetRotation, ClampedPolicyAlpha).GetNormalized();
}


float UPhysAnimComponent::CalculateControlTargetDeltaDegrees(const FQuat& PreviousRotation, const FQuat& TargetRotation)
{
	return FMath::RadiansToDegrees(static_cast<float>(PreviousRotation.AngularDistance(TargetRotation)));
}


float UPhysAnimComponent::CalculateControlAuthorityAlpha(
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	float ElapsedSinceHandoffSettledSeconds,
	float RampDurationSeconds)
{
	if (bForceZeroActions || !bSimulationHandoffSettled)
	{
		return 0.0f;
	}

	if (RampDurationSeconds <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(ElapsedSinceHandoffSettledSeconds / RampDurationSeconds, 0.0f, 1.0f);
}


float UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(
	int32 ProfileMode,
	float ElapsedSeconds,
	float RampDurationSeconds,
	float TargetMultiplier,
	float HoldSeconds,
	float RecoveryRampSeconds)
{
	const float ClampedTargetMultiplier = FMath::Clamp(TargetMultiplier, 0.0f, 1.0f);
	if (ElapsedSeconds <= 0.0f)
	{
		return 1.0f;
	}

	if (ProfileMode == 1)
	{
		const float ClampedRampDurationSeconds = FMath::Max(RampDurationSeconds, 0.0f);
		const float ClampedHoldSeconds = FMath::Max(HoldSeconds, 0.0f);
		const float ClampedRecoveryRampSeconds = FMath::Max(RecoveryRampSeconds, 0.0f);

		if (ClampedRampDurationSeconds <= UE_SMALL_NUMBER)
		{
			if (ElapsedSeconds < ClampedHoldSeconds)
			{
				return ClampedTargetMultiplier;
			}

			if (ClampedRecoveryRampSeconds <= UE_SMALL_NUMBER)
			{
				return 1.0f;
			}

			const float RecoveryAlpha =
				FMath::Clamp((ElapsedSeconds - ClampedHoldSeconds) / ClampedRecoveryRampSeconds, 0.0f, 1.0f);
			return FMath::Lerp(ClampedTargetMultiplier, 1.0f, RecoveryAlpha);
		}

		if (ElapsedSeconds <= ClampedRampDurationSeconds)
		{
			const float RampAlpha = FMath::Clamp(ElapsedSeconds / ClampedRampDurationSeconds, 0.0f, 1.0f);
			return FMath::Lerp(1.0f, ClampedTargetMultiplier, RampAlpha);
		}

		const float HoldEndSeconds = ClampedRampDurationSeconds + ClampedHoldSeconds;
		if (ElapsedSeconds <= HoldEndSeconds)
		{
			return ClampedTargetMultiplier;
		}

		if (ClampedRecoveryRampSeconds <= UE_SMALL_NUMBER)
		{
			return 1.0f;
		}

		const float RecoveryAlpha =
			FMath::Clamp((ElapsedSeconds - HoldEndSeconds) / ClampedRecoveryRampSeconds, 0.0f, 1.0f);
		return FMath::Lerp(ClampedTargetMultiplier, 1.0f, RecoveryAlpha);
	}

	if (RampDurationSeconds <= UE_SMALL_NUMBER)
	{
		return ClampedTargetMultiplier;
	}

	return FMath::Clamp(1.0f - ((1.0f - ClampedTargetMultiplier) * (ElapsedSeconds / RampDurationSeconds)), ClampedTargetMultiplier, 1.0f);
}


void UPhysAnimComponent::ApplyPresentationPerturbationStabilizationOverride(
	bool bOverrideActive,
	FPhysAnimStabilizationSettings& InOutSettings)
{
	if (!bOverrideActive)
	{
		return;
	}

	InOutSettings.AngularStrengthMultiplier *= PhysAnimComponentInternal::PresentationPerturbationStrengthRelaxationMultiplier;
	InOutSettings.AngularDampingRatioMultiplier *= PhysAnimComponentInternal::PresentationPerturbationDampingRatioRelaxationMultiplier;
	InOutSettings.AngularExtraDampingMultiplier *= PhysAnimComponentInternal::PresentationPerturbationExtraDampingRelaxationMultiplier;
}


void UPhysAnimComponent::ApplyStabilizationStressTestRamp(
	float Multiplier,
	int32 SweepMode,
	FPhysAnimStabilizationSettings& InOutSettings)
{
	const float ClampedMultiplier = FMath::Clamp(Multiplier, 0.0f, 1.0f);
	switch (SweepMode)
	{
	case 1:
		InOutSettings.AngularStrengthMultiplier *= ClampedMultiplier;
		break;
	case 2:
		InOutSettings.AngularDampingRatioMultiplier *= ClampedMultiplier;
		break;
	case 3:
		InOutSettings.AngularExtraDampingMultiplier *= ClampedMultiplier;
		break;
	default:
		InOutSettings.AngularStrengthMultiplier *= ClampedMultiplier;
		InOutSettings.AngularDampingRatioMultiplier *= ClampedMultiplier;
		InOutSettings.AngularExtraDampingMultiplier *= ClampedMultiplier;
		break;
	}
}


bool UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(
	bool bMovementSmokeModeEnabled,
	bool bAllowCharacterMovementInBridgeActive)
{
	return bMovementSmokeModeEnabled || bAllowCharacterMovementInBridgeActive;
}


FString UPhysAnimComponent::BuildBridgeStatusIndicatorText(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics)
{
	const TCHAR* const StateName = GetRuntimeStateName(State);
	return FString::Printf(
		TEXT("PhysAnim Bridge: %s (%s)"),
		StateName,
		bBridgeOwnsPhysics ? TEXT("ACTIVE") : TEXT("INACTIVE"));
}


FColor UPhysAnimComponent::ResolveBridgeStatusIndicatorColor(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics)
{
	if (State == EPhysAnimRuntimeState::FailStopped)
	{
		return FColor::Red;
	}

	if (bBridgeOwnsPhysics)
	{
		return FColor::Green;
	}

	if (State == EPhysAnimRuntimeState::ReadyForActivation)
	{
		return FColor::Yellow;
	}

	return FColor(160, 160, 160);
}

const TCHAR* UPhysAnimComponent::GetPhysicsMovementTypeName(EPhysicsMovementType MovementType)
{
	switch (MovementType)
	{
	case EPhysicsMovementType::Static:
		return TEXT("Static");
	case EPhysicsMovementType::Kinematic:
		return TEXT("Kinematic");
	case EPhysicsMovementType::Simulated:
		return TEXT("Simulated");
	default:
		return TEXT("Unknown");
	}
}



float UPhysAnimComponent::CalculatePolicyInfluenceAlpha(
	bool bForceZeroActions,
	bool bAllBringUpGroupsUnlocked,
	float ElapsedSinceAllBringUpGroupsUnlockedSeconds,
	float RampDurationSeconds)
{
	if (bForceZeroActions || !bAllBringUpGroupsUnlocked)
	{
		return 0.0f;
	}

	if (RampDurationSeconds <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(ElapsedSinceAllBringUpGroupsUnlockedSeconds / RampDurationSeconds, 0.0f, 1.0f);
}


bool UPhysAnimComponent::ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(
	bool bPresentationPerturbationOverrideActive)
{
	(void)bPresentationPerturbationOverrideActive;
	return false;
}

EPhysAnimRuntimeState UPhysAnimComponent::ResolveInitialPoseSearchSuccessState(bool bForceZeroActions)
{
	return bForceZeroActions
		? EPhysAnimRuntimeState::ReadyForActivation
		: EPhysAnimRuntimeState::BridgeActive;
}


bool UPhysAnimComponent::ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions)
{
	return State == EPhysAnimRuntimeState::ReadyForActivation && !bForceZeroActions;
}


bool UPhysAnimComponent::ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions)
{
	return (State == EPhysAnimRuntimeState::BridgeActive || 
			State == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			State == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			State == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			State == EPhysAnimRuntimeState::BalanceEntry_Settle ||
			State == EPhysAnimRuntimeState::BalanceActive_Recovery) && bForceZeroActions;
}


bool UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState State)
{
	return State == EPhysAnimRuntimeState::BridgeActive || 
			State == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			State == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			State == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			State == EPhysAnimRuntimeState::BalanceEntry_Settle ||
			State == EPhysAnimRuntimeState::BalanceActive_Recovery;
}

const TCHAR* UPhysAnimComponent::GetRuntimeStateName(EPhysAnimRuntimeState State)
{
	switch (State)
	{
	case EPhysAnimRuntimeState::Uninitialized:
		return TEXT("Uninitialized");
	case EPhysAnimRuntimeState::RuntimeReady:
		return TEXT("RuntimeReady");
	case EPhysAnimRuntimeState::WaitingForPoseSearch:
		return TEXT("WaitingForPoseSearch");
	case EPhysAnimRuntimeState::ReadyForActivation:
		return TEXT("ReadyForActivation");
	case EPhysAnimRuntimeState::BridgeActive:
		return TEXT("BridgeActive");
	case EPhysAnimRuntimeState::BalanceEntry_Prepare:
		return TEXT("BalanceEntry_Prepare");
	case EPhysAnimRuntimeState::BalanceEntry_LateValidate:
		return TEXT("BalanceEntry_LateValidate");
	case EPhysAnimRuntimeState::BalanceEntry_RootOn:
		return TEXT("BalanceEntry_RootOn");
	case EPhysAnimRuntimeState::BalanceEntry_Settle:
		return TEXT("BalanceEntry_Settle");
	case EPhysAnimRuntimeState::BalanceActive_Recovery:
		return TEXT("BalanceActive_Recovery");
	case EPhysAnimRuntimeState::BalanceSafeDeny:
		return TEXT("BalanceSafeDeny");
	case EPhysAnimRuntimeState::FailStopped:
		return TEXT("FailStopped");
	default:
		return TEXT("Unknown");
	}
}

