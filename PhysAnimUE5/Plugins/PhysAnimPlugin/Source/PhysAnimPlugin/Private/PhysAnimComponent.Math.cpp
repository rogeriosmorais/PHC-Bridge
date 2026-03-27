#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"

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

bool UPhysAnimComponent::ShouldRunRootOnReadinessUltraFineMarginSweep(float RootOnReadinessTotalDeficitDeg)
{
	return RootOnReadinessTotalDeficitDeg <= 2.0f + KINDA_SMALL_NUMBER;
}

#if !UE_BUILD_SHIPPING
void UPhysAnimComponent::FinalizePhase1AutoCalibScore(FPhase1AutoCalibScore& InOutScore)
{
	const bool bHardRejected =
		!InOutScore.bContractPassed ||
		InOutScore.bTimedOut ||
		InOutScore.bSafeDenied ||
		!InOutScore.bRestoreDeterministic ||
		!InOutScore.bReachedRootOn ||
		!InOutScore.bNoCouplingProofSatisfied;

	const float RejectionPenalty = bHardRejected ? 1000000.0f : 0.0f;
	InOutScore.StableSortScalar =
		RejectionPenalty +
		(InOutScore.WorstDirectLinkAngularErrorDeg * 1000.0f) +
		(InOutScore.MeanTargetDeltaDeg * 100.0f) +
		(InOutScore.MaxTargetDeltaDeg * 10.0f) +
		(InOutScore.ThighAsymmetryDeg * 5.0f) +
		(InOutScore.PeakRootTiltDeg * 2.0f) +
		InOutScore.ShellOffsetDeltaCm +
		(InOutScore.ShellVelocityDeltaCmPerSecond * 0.1f) +
		(InOutScore.PeakRootLinearSpeedCmPerSecond * 0.01f) +
		(InOutScore.PeakRootAngularSpeedDegPerSecond * 0.001f);
}

bool UPhysAnimComponent::IsBetterPhase1AutoCalibScore(const FPhase1AutoCalibScore& Candidate, const FPhase1AutoCalibScore& CurrentBest)
{
	const auto IsRejected = [](const FPhase1AutoCalibScore& Score)
	{
		return !Score.bContractPassed ||
			Score.bTimedOut ||
			Score.bSafeDenied ||
			!Score.bRestoreDeterministic ||
			!Score.bReachedRootOn ||
			!Score.bNoCouplingProofSatisfied;
	};

	const bool bCandidateRejected = IsRejected(Candidate);
	const bool bCurrentRejected = IsRejected(CurrentBest);
	if (bCandidateRejected != bCurrentRejected)
	{
		return !bCandidateRejected;
	}

	const auto Less = [](float A, float B)
	{
		return A + KINDA_SMALL_NUMBER < B;
	};
	const auto Equal = [](float A, float B)
	{
		return FMath::IsNearlyEqual(A, B, KINDA_SMALL_NUMBER);
	};

	if (Less(Candidate.WorstDirectLinkAngularErrorDeg, CurrentBest.WorstDirectLinkAngularErrorDeg))
	{
		return true;
	}
	if (!Equal(Candidate.WorstDirectLinkAngularErrorDeg, CurrentBest.WorstDirectLinkAngularErrorDeg))
	{
		return false;
	}
	if (Less(Candidate.MeanTargetDeltaDeg, CurrentBest.MeanTargetDeltaDeg))
	{
		return true;
	}
	if (!Equal(Candidate.MeanTargetDeltaDeg, CurrentBest.MeanTargetDeltaDeg))
	{
		return false;
	}
	if (Less(Candidate.MaxTargetDeltaDeg, CurrentBest.MaxTargetDeltaDeg))
	{
		return true;
	}
	if (!Equal(Candidate.MaxTargetDeltaDeg, CurrentBest.MaxTargetDeltaDeg))
	{
		return false;
	}
	if (Less(Candidate.ThighAsymmetryDeg, CurrentBest.ThighAsymmetryDeg))
	{
		return true;
	}
	if (!Equal(Candidate.ThighAsymmetryDeg, CurrentBest.ThighAsymmetryDeg))
	{
		return false;
	}
	if (Less(Candidate.PeakRootTiltDeg, CurrentBest.PeakRootTiltDeg))
	{
		return true;
	}
	if (!Equal(Candidate.PeakRootTiltDeg, CurrentBest.PeakRootTiltDeg))
	{
		return false;
	}
	if (Less(Candidate.ShellOffsetDeltaCm, CurrentBest.ShellOffsetDeltaCm))
	{
		return true;
	}
	if (!Equal(Candidate.ShellOffsetDeltaCm, CurrentBest.ShellOffsetDeltaCm))
	{
		return false;
	}
	if (Less(Candidate.ShellVelocityDeltaCmPerSecond, CurrentBest.ShellVelocityDeltaCmPerSecond))
	{
		return true;
	}
	if (!Equal(Candidate.ShellVelocityDeltaCmPerSecond, CurrentBest.ShellVelocityDeltaCmPerSecond))
	{
		return false;
	}
	if (Less(Candidate.PeakRootLinearSpeedCmPerSecond, CurrentBest.PeakRootLinearSpeedCmPerSecond))
	{
		return true;
	}
	if (!Equal(Candidate.PeakRootLinearSpeedCmPerSecond, CurrentBest.PeakRootLinearSpeedCmPerSecond))
	{
		return false;
	}
	if (Less(Candidate.PeakRootAngularSpeedDegPerSecond, CurrentBest.PeakRootAngularSpeedDegPerSecond))
	{
		return true;
	}
	if (!Equal(Candidate.PeakRootAngularSpeedDegPerSecond, CurrentBest.PeakRootAngularSpeedDegPerSecond))
	{
		return false;
	}
	return Candidate.StableSortScalar + KINDA_SMALL_NUMBER < CurrentBest.StableSortScalar;
}
#endif

bool UPhysAnimComponent::ShouldAcceptStepLimitedPhase1PelvisRotation(
	bool bBestTiltAdmissible,
	bool bBestRootOnAngularReady,
	bool bBestRootOnReadinessMarginSatisfied,
	bool bStepTiltAdmissible,
	bool bStepRootOnAngularReady,
	bool bStepRootOnReadinessMarginSatisfied)
{
	if (bBestTiltAdmissible && !bStepTiltAdmissible)
	{
		return false;
	}
	if (bBestRootOnAngularReady && !bStepRootOnAngularReady)
	{
		return false;
	}
	if (bBestRootOnReadinessMarginSatisfied && !bStepRootOnReadinessMarginSatisfied)
	{
		return false;
	}
	return true;
}

bool UPhysAnimComponent::ShouldRunSpineOnlyRootOnReadinessRescueSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	const float LeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - LeftThighAngularErrorDeg;
	const float RightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - RightThighAngularErrorDeg;
	const float SpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - SpineAngularErrorDeg;
	return LeftMarginDeg >= -KINDA_SMALL_NUMBER &&
		RightMarginDeg >= -KINDA_SMALL_NUMBER &&
		SpineMarginDeg < -KINDA_SMALL_NUMBER &&
		SpineMarginDeg >= -2.0f - KINDA_SMALL_NUMBER;
}

bool UPhysAnimComponent::ShouldRunSpineBiasedDirectConstraintBlendSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	const float LeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - LeftThighAngularErrorDeg;
	const float RightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - RightThighAngularErrorDeg;
	const float SpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - SpineAngularErrorDeg;
	return LeftMarginDeg >= -KINDA_SMALL_NUMBER &&
		RightMarginDeg >= -KINDA_SMALL_NUMBER &&
		SpineMarginDeg < -KINDA_SMALL_NUMBER &&
		SpineMarginDeg >= -2.0f - KINDA_SMALL_NUMBER;
}

bool UPhysAnimComponent::ShouldRunSpineFocusedPairBlendSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	return ShouldRunSpineBiasedDirectConstraintBlendSweep(
		LeftThighAngularErrorDeg,
		RightThighAngularErrorDeg,
		SpineAngularErrorDeg);
}

bool UPhysAnimComponent::ShouldRunAlternateReferenceDirectConstraintBlendSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	return ShouldRunSpineBiasedDirectConstraintBlendSweep(
		LeftThighAngularErrorDeg,
		RightThighAngularErrorDeg,
		SpineAngularErrorDeg);
}

bool UPhysAnimComponent::ShouldRunSpineConstraintInterpolationSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	return ShouldRunSpineBiasedDirectConstraintBlendSweep(
		LeftThighAngularErrorDeg,
		RightThighAngularErrorDeg,
		SpineAngularErrorDeg);
}

bool UPhysAnimComponent::ShouldRunWorstThighConstraintInterpolationSweep(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	const float LeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - LeftThighAngularErrorDeg;
	const float RightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - RightThighAngularErrorDeg;
	const float SpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - SpineAngularErrorDeg;
	const float WorstThighMarginDeg = FMath::Min(LeftMarginDeg, RightMarginDeg);
	return SpineMarginDeg >= KINDA_SMALL_NUMBER &&
		WorstThighMarginDeg < -KINDA_SMALL_NUMBER &&
		WorstThighMarginDeg >= -3.0f - KINDA_SMALL_NUMBER;
}

bool UPhysAnimComponent::ShouldRunSpineSafeWorstThighFocusedDelta(
	float LeftThighAngularErrorDeg,
	float RightThighAngularErrorDeg,
	float SpineAngularErrorDeg)
{
	return ShouldRunWorstThighConstraintInterpolationSweep(
		LeftThighAngularErrorDeg,
		RightThighAngularErrorDeg,
		SpineAngularErrorDeg);
}

bool UPhysAnimComponent::IsConstraintSampleRelevantToFocusedBone(
	FName SampleChildBoneName,
	const FString& SampleSource,
	FName FocusChildBone)
{
	if (FocusChildBone.IsNone())
	{
		return false;
	}

	if (SampleChildBoneName == FocusChildBone)
	{
		return true;
	}

	const FString FocusBoneToken = FocusChildBone.ToString();
	return !FocusBoneToken.IsEmpty() && SampleSource.Contains(FocusBoneToken, ESearchCase::CaseSensitive);
}

bool UPhysAnimComponent::ShouldAcceptWorstThighConstraintInterpolationCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg)
{
	const float CurrentLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentLeftThighAngularErrorDeg;
	const float CurrentRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentRightThighAngularErrorDeg;
	const float CurrentSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CurrentSpineAngularErrorDeg;
	const float CandidateLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateLeftThighAngularErrorDeg;
	const float CandidateRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateRightThighAngularErrorDeg;
	const float CandidateSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CandidateSpineAngularErrorDeg;

	if (CurrentSpineMarginDeg >= -KINDA_SMALL_NUMBER && CandidateSpineMarginDeg < -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float CurrentWorstThighMarginDeg = FMath::Min(CurrentLeftMarginDeg, CurrentRightMarginDeg);
	const float CandidateWorstThighMarginDeg = FMath::Min(CandidateLeftMarginDeg, CandidateRightMarginDeg);
	return CandidateWorstThighMarginDeg > CurrentWorstThighMarginDeg + KINDA_SMALL_NUMBER;
}

bool UPhysAnimComponent::ShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg)
{
	const float CurrentLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentLeftThighAngularErrorDeg;
	const float CurrentRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentRightThighAngularErrorDeg;
	const float CurrentSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CurrentSpineAngularErrorDeg;
	const float CandidateLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateLeftThighAngularErrorDeg;
	const float CandidateRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateRightThighAngularErrorDeg;
	const float CandidateSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CandidateSpineAngularErrorDeg;

	if (CurrentSpineMarginDeg < -KINDA_SMALL_NUMBER)
	{
		return false;
	}
	if (CandidateSpineMarginDeg < -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float CurrentWorstThighMarginDeg = FMath::Min(CurrentLeftMarginDeg, CurrentRightMarginDeg);
	const float CandidateWorstThighMarginDeg = FMath::Min(CandidateLeftMarginDeg, CandidateRightMarginDeg);
	if (CandidateWorstThighMarginDeg <= CurrentWorstThighMarginDeg + KINDA_SMALL_NUMBER)
	{
		return false;
	}

	return CandidateSpineMarginDeg + 0.25f >= CurrentSpineMarginDeg;
}

bool UPhysAnimComponent::ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg)
{
	const float CurrentLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentLeftThighAngularErrorDeg;
	const float CurrentRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CurrentRightThighAngularErrorDeg;
	const float CurrentSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CurrentSpineAngularErrorDeg;

	const float CandidateLeftMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateLeftThighAngularErrorDeg;
	const float CandidateRightMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg - CandidateRightThighAngularErrorDeg;
	const float CandidateSpineMarginDeg =
		BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg - CandidateSpineAngularErrorDeg;

	const bool bCurrentThighsReady =
		CurrentLeftMarginDeg >= -KINDA_SMALL_NUMBER &&
		CurrentRightMarginDeg >= -KINDA_SMALL_NUMBER;
	const bool bCandidateThighsReady =
		CandidateLeftMarginDeg >= -KINDA_SMALL_NUMBER &&
		CandidateRightMarginDeg >= -KINDA_SMALL_NUMBER;
	if (!bCurrentThighsReady || !bCandidateThighsReady)
	{
		return false;
	}

	if (CandidateSpineMarginDeg > CurrentSpineMarginDeg + KINDA_SMALL_NUMBER)
	{
		return true;
	}

	if (FMath::Abs(CandidateSpineMarginDeg - CurrentSpineMarginDeg) <= KINDA_SMALL_NUMBER)
	{
		const float CurrentMinThighMarginDeg = FMath::Min(CurrentLeftMarginDeg, CurrentRightMarginDeg);
		const float CandidateMinThighMarginDeg = FMath::Min(CandidateLeftMarginDeg, CandidateRightMarginDeg);
		return CandidateMinThighMarginDeg > CurrentMinThighMarginDeg + KINDA_SMALL_NUMBER;
	}

	return false;
}

bool UPhysAnimComponent::ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg)
{
	return ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
		CurrentLeftThighAngularErrorDeg,
		CurrentRightThighAngularErrorDeg,
		CurrentSpineAngularErrorDeg,
		CandidateLeftThighAngularErrorDeg,
		CandidateRightThighAngularErrorDeg,
		CandidateSpineAngularErrorDeg);
}

EPhysAnimRuntimeState UPhysAnimComponent::MapBalanceTransitionPhaseToRuntimeState(EBalanceReadyTransitionPhase TransitionPhase)
{
	return TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ? EPhysAnimRuntimeState::BalanceEntry_Prepare :
		TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ? EPhysAnimRuntimeState::BalanceEntry_LateValidate :
		TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ? EPhysAnimRuntimeState::BalanceEntry_RootOn :
		(TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3 ||
		 TransitionPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle) ? EPhysAnimRuntimeState::BalanceEntry_Settle :
		TransitionPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ? EPhysAnimRuntimeState::BalanceSafeDeny :
		EPhysAnimRuntimeState::BridgeActive;
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

