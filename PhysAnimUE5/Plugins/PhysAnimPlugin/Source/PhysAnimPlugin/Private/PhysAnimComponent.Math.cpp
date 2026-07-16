#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimProtoMannyAdapter.h"

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
	(void)BoneName;
	return bPolicyInfluenceActive;
}


bool UPhysAnimComponent::ShouldSuppressPolicyDispatchForTransitionState(EPhysAnimRuntimeState RuntimeState)
{
	return RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
}


bool UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(
	bool bConfiguredUseSkeletalAnimationTargets,
	bool bPolicyInfluenceActive)
{
	(void)bConfiguredUseSkeletalAnimationTargets;
	(void)bPolicyInfluenceActive;
	// Standing targets are an explicit parent-relative contract. Skeletal-animation
	// offsets would change their reference frame and make policy activation discontinuous.
	return false;
}


bool UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame)
{
	(void)bUseSkeletalAnimationTargetRepresentation;
	(void)bFirstPolicyEnabledFrame;
	return false;
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
	(void)BoneName;
	(void)bUseSkeletalAnimationTargetRepresentation;
	(void)bFirstPolicyEnabledFrame;
	(void)bDistalLocomotionCompositionModeActive;
	(void)DeltaTime;

	// ProtoMotions v2.3 publishes only a DOF position target through
	// Isaac Gym's built-in PD drive. A nonzero delta time here makes Physics
	// Control synthesize a target angular velocity that the source controller
	// never had, turning ordinary 30 Hz action changes into feed-forward kicks.
	return 0.0f;
}


float UPhysAnimComponent::CalculateNeutralCalibratedPelvisTiltDegrees(
	const FQuat& CurrentPelvisWorldRotation,
	const FQuat& NeutralPelvisActorRelativeRotation,
	const FQuat& ActorWorldRotation)
{
	const FQuat ExpectedNeutralWorldRotation =
		(ActorWorldRotation * NeutralPelvisActorRelativeRotation).GetNormalized();
	const FQuat NeutralToCurrentWorldDelta =
		(CurrentPelvisWorldRotation * ExpectedNeutralWorldRotation.Inverse()).GetNormalized();
	const FVector CurrentUpFromNeutral = NeutralToCurrentWorldDelta.RotateVector(FVector::UpVector);
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
		FVector::DotProduct(CurrentUpFromNeutral, FVector::UpVector),
		-1.0f,
		1.0f)));
}


bool UPhysAnimComponent::TryMeasureNeutralCalibratedPelvisTiltDegrees(float& OutTiltDegrees) const
{
	OutTiltDegrees = 180.0f;
	const AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!OwnerActor || !Mesh || !bHasNeutralPelvisActorRelativeRotation)
	{
		return false;
	}

	const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName());
	if (!PelvisBody || !PelvisBody->IsValidBodyInstance())
	{
		return false;
	}

	OutTiltDegrees = CalculateNeutralCalibratedPelvisTiltDegrees(
		PelvisBody->GetUnrealWorldTransform().GetRotation(),
		NeutralPelvisActorRelativeRotation,
		OwnerActor->GetActorQuat());
	return FMath::IsFinite(OutTiltDegrees);
}


float UPhysAnimComponent::ResolvePhase1Uprightness(
	USkeletalMeshComponent* SkeletalMesh,
	AActor* Owner,
	const FName& PelvisBoneName,
	bool bHasNeutralPelvisOrientation,
	const FQuat& NeutralPelvisActorRelativeRotation,
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
		if (bHasNeutralPelvisOrientation && Owner)
		{
			OutSourceName = TEXT("pelvis_body_neutral_calibrated");
			return CalculateNeutralCalibratedPelvisTiltDegrees(
				PelvisTransform.GetRotation(),
				NeutralPelvisActorRelativeRotation,
				Owner->GetActorQuat());
		}
		
		// Fallback for callers that have not completed the startup neutral capture.
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


float UPhysAnimComponent::ResolveObservationGroundWorldZ(
	bool bHasStaticGroundTrace,
	float StaticGroundTraceZ,
	bool bHasWalkableFloor,
	bool bHasBlockingFloorHit,
	float FloorImpactPointZ,
	float CapsuleCenterZ,
	float CapsuleHalfHeight,
	float FloorDistance,
	float FallbackGroundWorldZ)
{
	if (bHasStaticGroundTrace)
	{
		return StaticGroundTraceZ;
	}

	return ResolveObservationGroundWorldZFromFloor(
		bHasWalkableFloor,
		bHasBlockingFloorHit,
		FloorImpactPointZ,
		CapsuleCenterZ,
		CapsuleHalfHeight,
		FloorDistance,
		FallbackGroundWorldZ);
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


void UPhysAnimComponent::MakeMimicTargetDataFrameBodySamples(
	const TArray<FPhysAnimBodySample>& SourceBodySamples,
	const FTransform& CurrentSelectedWorldRoot,
	const FTransform& CurrentSelectedDataRoot,
	TArray<FPhysAnimBodySample>& OutBodySamples)
{
	const FQuat WorldToDataRotation =
		(CurrentSelectedDataRoot.GetRotation() * CurrentSelectedWorldRoot.GetRotation().Inverse()).GetNormalized();
	const FVector WorldRootPosition = CurrentSelectedWorldRoot.GetLocation();
	const FVector DataRootPosition = CurrentSelectedDataRoot.GetLocation();

	OutBodySamples = SourceBodySamples;
	for (FPhysAnimBodySample& BodySample : OutBodySamples)
	{
		BodySample.Position = DataRootPosition + WorldToDataRotation.RotateVector(BodySample.Position - WorldRootPosition);
		BodySample.Rotation = (WorldToDataRotation * BodySample.Rotation).GetNormalized();
		BodySample.LinearVelocity = WorldToDataRotation.RotateVector(BodySample.LinearVelocity);
		BodySample.AngularVelocity = WorldToDataRotation.RotateVector(BodySample.AngularVelocity);
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


FVector UPhysAnimComponent::ResolveEffectiveShellCouplingPlanarVelocityCmPerSecond(
	const FVector& OwnerVelocityCmPerSecond,
	const FVector& AppliedShellCorrectionVelocityCmPerSecond,
	bool bTransitionOwnedShellLocked)
{
	FVector EffectiveVelocity = OwnerVelocityCmPerSecond;
	if (bTransitionOwnedShellLocked)
	{
		EffectiveVelocity += AppliedShellCorrectionVelocityCmPerSecond;
	}

	EffectiveVelocity.Z = 0.0f;
	return EffectiveVelocity;
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


FQuat UPhysAnimComponent::ComposeProtoPolicyTargetInMannyBindFrame(
	const FPhysAnimControlTargetSeed& MannyBindSeed,
	const FQuat& ProtoPolicyRotationUe)
{
	return ComposeProtoPolicyTargetAroundMannyNeutral(
		MannyBindSeed,
		MannyBindSeed.ParentRelativeTargetRotation,
		ProtoPolicyRotationUe);
}


FQuat UPhysAnimComponent::ComposeProtoPolicyTargetAroundMannyNeutral(
	const FPhysAnimControlTargetSeed& MannyBindSeed,
	const FQuat& MannyNeutralParentRelativeRotation,
	const FQuat& ProtoPolicyRotationUe)
{
	const FQuat ParentBindWorldRotation = MannyBindSeed.ParentActionAxisReferenceRotation.GetNormalized();
	const FQuat PolicyRotationInParentBindFrame =
		(ParentBindWorldRotation.Inverse() *
		 ProtoPolicyRotationUe.GetNormalized() *
		 ParentBindWorldRotation).GetNormalized();
	return (PolicyRotationInParentBindFrame *
		MannyNeutralParentRelativeRotation.GetNormalized()).GetNormalized();
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
	bool bStandingPolicyMode,
	TArray<float>& InOutActions)
{
	ApplyCausalStandingPolicyActionCompatibility(
		bStandingPolicyMode,
		true,
		false,
		false,
		true,
		InOutActions);
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
	bool bStandingPolicyMode,
	bool bRestoreNeckHead,
	TArray<float>& InOutActions)
{
	ApplyCausalStandingPolicyActionCompatibility(
		bStandingPolicyMode,
		bRestoreNeckHead,
		bRestoreNeckHead,
		InOutActions);
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
	bool bStandingPolicyMode,
	bool bRestoreNeck,
	bool bRestoreHead,
	TArray<float>& InOutActions)
{
	ApplyCausalStandingPolicyActionCompatibility(
		bStandingPolicyMode,
		false,
		bRestoreNeck,
		bRestoreHead,
		InOutActions);
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
	bool bStandingPolicyMode,
	bool bRestoreSpineChest,
	bool bRestoreNeck,
	bool bRestoreHead,
	TArray<float>& InOutActions)
{
	ApplyCausalStandingPolicyActionCompatibility(
		bStandingPolicyMode,
		bRestoreSpineChest,
		bRestoreNeck,
		bRestoreHead,
		false,
		InOutActions);
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionCompatibility(
	bool bStandingPolicyMode,
	bool bRestoreSpineChest,
	bool bRestoreNeck,
	bool bRestoreHead,
	bool bRestoreDistalHands,
	TArray<float>& InOutActions)
{
	ApplyCausalStandingPolicyActionScales(
		bStandingPolicyMode,
		0.0f,
		bRestoreSpineChest ? 1.0f : 0.0f,
		bRestoreNeck ? 1.0f : 0.0f,
		bRestoreHead ? 1.0f : 0.0f,
		0.0f,
		bRestoreDistalHands ? 1.0f : 0.0f,
		0.0f,
		bRestoreDistalHands ? 1.0f : 0.0f,
		InOutActions);
}

void UPhysAnimComponent::ApplyCausalStandingPolicyActionScales(
	bool bStandingPolicyMode,
	float TorsoScale,
	float SpineChestScale,
	float NeckScale,
	float HeadScale,
	float LeftProximalScale,
	float LeftDistalScale,
	float RightProximalScale,
	float RightDistalScale,
	TArray<float>& InOutActions)
{
	if (!bStandingPolicyMode)
	{
		return;
	}

	TorsoScale = FMath::Clamp(TorsoScale, 0.0f, 1.0f);
	SpineChestScale = FMath::Clamp(SpineChestScale, 0.0f, 1.0f);
	NeckScale = FMath::Clamp(NeckScale, 0.0f, 1.0f);
	HeadScale = FMath::Clamp(HeadScale, 0.0f, 1.0f);
	LeftProximalScale = FMath::Clamp(LeftProximalScale, 0.0f, 1.0f);
	LeftDistalScale = FMath::Clamp(LeftDistalScale, 0.0f, 1.0f);
	RightProximalScale = FMath::Clamp(RightProximalScale, 0.0f, 1.0f);
	RightDistalScale = FMath::Clamp(RightDistalScale, 0.0f, 1.0f);

	for (int32 ScalarIndex = 0; ScalarIndex < InOutActions.Num(); ++ScalarIndex)
	{
		const int32 JointIndex = ScalarIndex / 3;
		float Scale = 1.0f;
		if (JointIndex == 8)
		{
			Scale = TorsoScale;
		}
		else if (JointIndex >= 9 && JointIndex < 11)
		{
			Scale = SpineChestScale;
		}
		else if (JointIndex == 11)
		{
			Scale = NeckScale;
		}
		else if (JointIndex == 12)
		{
			Scale = HeadScale;
		}
		else if (JointIndex >= 13 && JointIndex < 16)
		{
			Scale = LeftProximalScale;
		}
		else if (JointIndex >= 16 && JointIndex < 18)
		{
			Scale = LeftDistalScale;
		}
		else if (JointIndex >= 18 && JointIndex < 21)
		{
			Scale = RightProximalScale;
		}
		else if (JointIndex >= 21)
		{
			Scale = RightDistalScale;
		}
		if (Scale <= 0.0f)
		{
			InOutActions[ScalarIndex] = 0.0f;
		}
		else if (Scale < 1.0f)
		{
			InOutActions[ScalarIndex] *= Scale;
		}
	}
}

float UPhysAnimComponent::ResolveCausalStandingPolicyStrengthFactor(
	bool bStandingPolicyMode,
	bool bFirstActiveStandingPolicyCaptured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bStandingPolicyMode &&
		bFirstActiveStandingPolicyCaptured &&
		InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing
			? 1.5f
			: 1.0f;
}

bool UPhysAnimComponent::ShouldRestoreCausalStandingHeadAfterFirstPolicy(
	bool bFirstActiveStandingPolicyCapturedBeforeCurrentInference,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bFirstActiveStandingPolicyCapturedBeforeCurrentInference &&
		InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
}

float UPhysAnimComponent::ResolveCausalStandingNeckScaleAfterFirstPolicy(
	bool bFirstActiveStandingPolicyCapturedBeforeCurrentInference,
	EPhysAnimRuntimeState InRuntimeState)
{
	return ShouldRestoreCausalStandingHeadAfterFirstPolicy(
		bFirstActiveStandingPolicyCapturedBeforeCurrentInference,
		InRuntimeState)
		? 0.25f
		: 0.0f;
}

bool UPhysAnimComponent::ShouldUseCausalStandingComponentActionAxis(
	EPhysAnimRuntimeState InRuntimeState)
{
	return IsStandingActivationRuntimeState(InRuntimeState);
}

FQuat UPhysAnimComponent::ExpressCachedWorldActionAxisInMeshComponent(
	const FQuat& ActionBindComponentWorldRotation,
	const FQuat& CachedWorldActionAxisRotation)
{
	return (ActionBindComponentWorldRotation.GetNormalized().Inverse() *
		CachedWorldActionAxisRotation.GetNormalized()).GetNormalized();
}

FQuat UPhysAnimComponent::ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxis(
	const FQuat& EffectiveActionAxisRotation,
	const FQuat& MannyNeutralParentRelativeRotation,
	const FQuat& ProtoPolicyRotationUe)
{
	const FQuat NormalizedActionAxis = EffectiveActionAxisRotation.GetNormalized();
	const FQuat PolicyRotationInEffectiveAxisFrame =
		(NormalizedActionAxis.Inverse() *
		 ProtoPolicyRotationUe.GetNormalized() *
		 NormalizedActionAxis).GetNormalized();
	return (PolicyRotationInEffectiveAxisFrame *
		MannyNeutralParentRelativeRotation.GetNormalized()).GetNormalized();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisForRuntimeStateForTesting(
	bool bConfigured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured && InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
}

bool UPhysAnimComponent::ShouldUseExperimentalComponentActionAxisFromFirstPolicyForRuntimeStateForTesting(
	bool bConfigured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured && IsStandingActivationRuntimeState(InRuntimeState);
}

bool UPhysAnimComponent::ShouldUseExperimentalBindNeutralFromFirstPolicyForRuntimeStateForTesting(
	bool bConfigured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured && IsStandingActivationRuntimeState(InRuntimeState);
}

bool UPhysAnimComponent::ShouldBypassExperimentalConstraintRangeRemapFromFirstPolicyForRuntimeStateForTesting(
	bool bConfigured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured && IsStandingActivationRuntimeState(InRuntimeState);
}

bool UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
	bool bHeadEnabled,
	bool bActiveOnly,
	EPhysAnimRuntimeState InRuntimeState)
{
	if (!bHeadEnabled)
	{
		return false;
	}
	return bActiveOnly
		? InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing
		: IsStandingActivationRuntimeState(InRuntimeState);
}

bool UPhysAnimComponent::ShouldRestoreExperimentalCausalStandingHeadAfterFirstPolicyForTesting(
	bool bHeadEnabled,
	bool bFirstActiveStandingPolicyCapturedBeforeCurrentInference,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bHeadEnabled &&
		ShouldRestoreCausalStandingHeadAfterFirstPolicy(
			bFirstActiveStandingPolicyCapturedBeforeCurrentInference,
			InRuntimeState);
}

bool UPhysAnimComponent::CaptureFirstActiveStandingConditionedActionsForTesting(
	bool bActiveStanding,
	TConstArrayView<float> ConditionedActions,
	bool& bInOutCaptured,
	TArray<float>& OutActions)
{
	if (!bActiveStanding || bInOutCaptured)
	{
		return false;
	}

	OutActions.Reset(ConditionedActions.Num());
	OutActions.Append(ConditionedActions.GetData(), ConditionedActions.Num());
	bInOutCaptured = true;
	return true;
}

bool UPhysAnimComponent::ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
	bool bConfigured,
	bool bFirstActiveStandingPolicyCaptured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured &&
		bFirstActiveStandingPolicyCaptured &&
		InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
}

bool UPhysAnimComponent::ShouldUseExperimentalCheckpointForcePdForRuntimeStateForTesting(
	bool bConfigured,
	bool bFirstActiveStandingPolicyCaptured,
	EPhysAnimRuntimeState InRuntimeState)
{
	return bConfigured &&
		bFirstActiveStandingPolicyCaptured &&
		InRuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
}

bool UPhysAnimComponent::TryBuildCheckpointForcePdControlDataForTesting(
	FName BoneName,
	const FPhysicsControlData& BaselineData,
	FPhysicsControlData& OutControlData)
{
	float KpNmPerRad = 0.0f;
	float KdNmSecPerRad = 0.0f;
	if (BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_l") || BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_l") || BoneName == TEXT("foot_r"))
	{
		KpNmPerRad = 800.0f;
		KdNmSecPerRad = 80.0f;
	}
	else if (BoneName == TEXT("ball_l") || BoneName == TEXT("ball_r"))
	{
		KpNmPerRad = 500.0f;
		KdNmSecPerRad = 50.0f;
	}
	else if (BoneName == TEXT("spine_01") || BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03"))
	{
		KpNmPerRad = 1000.0f;
		KdNmSecPerRad = 100.0f;
	}
	else if (BoneName == TEXT("neck_01") || BoneName == TEXT("head") ||
		BoneName == TEXT("clavicle_l") || BoneName == TEXT("clavicle_r") ||
		BoneName == TEXT("upperarm_l") || BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_l") || BoneName == TEXT("lowerarm_r"))
	{
		KpNmPerRad = 500.0f;
		KdNmSecPerRad = 50.0f;
	}
	else if (BoneName == TEXT("hand_l") || BoneName == TEXT("hand_r"))
	{
		KpNmPerRad = 300.0f;
		KdNmSecPerRad = 30.0f;
	}
	else
	{
		return false;
	}

	constexpr float EngineTorqueUnitsPerNewtonMeter = 10000.0f;
	const float KpEnginePerRad = KpNmPerRad * EngineTorqueUnitsPerNewtonMeter;
	OutControlData = BaselineData;
	OutControlData.AngularStrength = FMath::Sqrt(KpEnginePerRad) / (2.0f * PI);
	OutControlData.AngularDampingRatio = 0.0f;
	OutControlData.AngularExtraDamping = KdNmSecPerRad * EngineTorqueUnitsPerNewtonMeter;
	OutControlData.MaxTorque = 500.0f * EngineTorqueUnitsPerNewtonMeter;
	return true;
}

void UPhysAnimComponent::ApplyExperimentalActionFamilyMaskForTesting(
	EPhysAnimExperimentalActionFamilyMask Mask,
	TArray<float>& InOutActions)
{
	if (Mask == EPhysAnimExperimentalActionFamilyMask::All)
	{
		return;
	}

	for (int32 ScalarIndex = 0; ScalarIndex < InOutActions.Num(); ++ScalarIndex)
	{
		const int32 JointIndex = ScalarIndex / 3;
		const bool bKeep =
			(Mask == EPhysAnimExperimentalActionFamilyMask::LowerOnly && JointIndex < 8) ||
			(Mask == EPhysAnimExperimentalActionFamilyMask::AxialOnly && JointIndex >= 8 && JointIndex < 13) ||
			(Mask == EPhysAnimExperimentalActionFamilyMask::ArmsOnly && JointIndex >= 13);
		if (!bKeep)
		{
			InOutActions[ScalarIndex] = 0.0f;
		}
	}
}

void UPhysAnimComponent::ApplyExperimentalActionJointRangeForTesting(
	int32 StartJointIndex,
	int32 JointCount,
	TArray<float>& InOutActions)
{
	if (StartJointIndex == INDEX_NONE)
	{
		return;
	}

	const int32 ClampedStartJoint = FMath::Clamp(StartJointIndex, 0, PhysAnimBridge::NumActionFloats / 3);
	const int32 ClampedEndJoint = FMath::Clamp(
		ClampedStartJoint + FMath::Max(0, JointCount),
		ClampedStartJoint,
		PhysAnimBridge::NumActionFloats / 3);
	for (int32 ScalarIndex = 0; ScalarIndex < InOutActions.Num(); ++ScalarIndex)
	{
		const int32 JointIndex = ScalarIndex / 3;
		if (JointIndex < ClampedStartJoint || JointIndex >= ClampedEndJoint)
		{
			InOutActions[ScalarIndex] = 0.0f;
		}
	}
}

bool UPhysAnimComponent::ApplyExperimentalPolicyActionBaselineResidualForTesting(
	bool bConfigured,
	bool bForceZeroActions,
	TConstArrayView<float> BaselineActions,
	TArray<float>& InOutActions)
{
	if (!bConfigured || bForceZeroActions || BaselineActions.Num() != InOutActions.Num())
	{
		return false;
	}

	for (int32 ActionIndex = 0; ActionIndex < InOutActions.Num(); ++ActionIndex)
	{
		InOutActions[ActionIndex] -= BaselineActions[ActionIndex];
	}
	return true;
}

bool UPhysAnimComponent::ApplyExperimentalPolicyActionZeroUntilBaselineForTesting(
	bool bConfigured,
	bool bBaselineAvailable,
	bool bForceZeroActions,
	TArray<float>& InOutActions)
{
	if (!bConfigured || bBaselineAvailable || bForceZeroActions)
	{
		return false;
	}

	InOutActions.Init(0.0f, InOutActions.Num());
	return true;
}

FVector UPhysAnimComponent::SelectObservationWorldPositionForTesting(
	bool bUsePhysicsBodyPosition,
	const FVector& BoneWorldPosition,
	const FVector& PhysicsBodyWorldPosition)
{
	return bUsePhysicsBodyPosition ? PhysicsBodyWorldPosition : BoneWorldPosition;
}

float UPhysAnimComponent::ResolveExperimentalActiveStrengthFactorForTesting(
	float ConfiguredFactor,
	bool bFirstActiveStandingPolicyCaptured,
	EPhysAnimRuntimeState InRuntimeState)
{
	if (!bFirstActiveStandingPolicyCaptured ||
		InRuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		return 1.0f;
	}
	return FMath::Max(0.0f, ConfiguredFactor);
}

FQuat UPhysAnimComponent::ExpressCachedWorldActionAxisInMeshComponentForTesting(
	const FQuat& ActionBindComponentWorldRotation,
	const FQuat& CachedWorldActionAxisRotation)
{
	return ExpressCachedWorldActionAxisInMeshComponent(
		ActionBindComponentWorldRotation,
		CachedWorldActionAxisRotation);
}

FQuat UPhysAnimComponent::ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxisForTesting(
	const FQuat& EffectiveActionAxisRotation,
	const FQuat& MannyNeutralParentRelativeRotation,
	const FQuat& ProtoPolicyRotationUe)
{
	return ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxis(
		EffectiveActionAxisRotation,
		MannyNeutralParentRelativeRotation,
		ProtoPolicyRotationUe);
}

bool UPhysAnimComponent::BuildMannyLocalFrameRoundtripControlForTesting(
	int32 ControlIndex,
	FName MannyBoneName,
	FName ControlName,
	FName InitialControlChildBoneName,
	FName InitialControlParentBoneName,
	const FPhysAnimControlTargetSeed& MannyBindSeed,
	const FQuat& MannyNeutralParentRelativeRotation,
	const FQuat& ActualDecodedRotationUe,
	const FQuat& ActualMannyPreRangeTargetParentRelative,
	const FString& EffectiveActionAxisMode,
	const FQuat& EffectiveActionAxisRotation,
	PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripControl& OutTrace,
	FString& OutError) const
{
	OutTrace = {};
	OutTrace.ControlIndex = ControlIndex;
	OutTrace.MannyBoneName = MannyBoneName;
	OutTrace.ControlName = ControlName;
	OutTrace.InitialControlChildBoneName = InitialControlChildBoneName;
	OutTrace.InitialControlParentBoneName = InitialControlParentBoneName;
	OutTrace.EffectiveActionAxisMode = EffectiveActionAxisMode;
	if (EffectiveActionAxisMode != PhysAnimBridge::MannyLocalFrameRoundtripWorldAxisMode &&
		EffectiveActionAxisMode != PhysAnimBridge::MannyLocalFrameRoundtripComponentAxisMode)
	{
		OutError = FString::Printf(
			TEXT("Unsupported effective action-axis mode '%s'."),
			*EffectiveActionAxisMode);
		return false;
	}

	const TArray<FName>& ObservationBodyNames = PhysAnimBridge::GetSmplObservationBoneNames();
	if (CachedSmplObservationRestBodyComponentRotations.Num() != PhysAnimBridge::NumSmplBodies ||
		ObservationBodyNames.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = TEXT("The synchronized SMPL observation bind frames are incomplete.");
		return false;
	}

	for (const PhysAnimBridge::FPhysAnimProtoActionJointDescriptor& Descriptor :
		PhysAnimBridge::GetProtoActionJointDescriptors())
	{
		if (Descriptor.MannyBoneName != MannyBoneName)
		{
			continue;
		}
		const int32 ObservationBodyIndex = Descriptor.ProtoJointIndex + 1;
		if (!ObservationBodyNames.IsValidIndex(ObservationBodyIndex))
		{
			OutError = FString::Printf(
				TEXT("Proto joint %d for '%s' has no corresponding observation body."),
				Descriptor.ProtoJointIndex,
				*MannyBoneName.ToString());
			return false;
		}
		OutTrace.SourceProtoJointIndices.Add(Descriptor.ProtoJointIndex);
		OutTrace.SourceProtoJointNames.Add(Descriptor.ProtoJointName);
		OutTrace.ObservationBodyIndices.Add(ObservationBodyIndex);
		OutTrace.ObservationBodyNames.Add(ObservationBodyNames[ObservationBodyIndex]);
	}

	if (OutTrace.SourceProtoJointIndices.IsEmpty())
	{
		OutError = FString::Printf(
			TEXT("Control '%s' has no Proto action owner."),
			*ControlName.ToString());
		return false;
	}

	OutTrace.RoundtripObservationBodyIndex = OutTrace.ObservationBodyIndices[0];
	OutTrace.RoundtripObservationBodyName =
		ObservationBodyNames[OutTrace.RoundtripObservationBodyIndex];
	OutTrace.ObservationParentBodyIndex =
		ObservationBodyNames.IndexOfByKey(InitialControlParentBoneName);
	if (!ObservationBodyNames.IsValidIndex(OutTrace.ObservationParentBodyIndex))
	{
		OutError = FString::Printf(
			TEXT("Control '%s' parent '%s' is not an SMPL observation body."),
			*ControlName.ToString(),
			*InitialControlParentBoneName.ToString());
		return false;
	}
	OutTrace.ObservationParentBodyName =
		ObservationBodyNames[OutTrace.ObservationParentBodyIndex];
	OutTrace.bDecisiveOneToOne =
		OutTrace.SourceProtoJointIndices.Num() == 1 &&
		OutTrace.ObservationBodyIndices.Num() == 1;
	OutTrace.bOwnershipComplete =
		InitialControlChildBoneName == MannyBoneName &&
		OutTrace.RoundtripObservationBodyName == MannyBoneName &&
		OutTrace.ObservationParentBodyName == InitialControlParentBoneName;

	OutTrace.CachedActionAxisReferenceRotation =
		MannyBindSeed.ParentActionAxisReferenceRotation.GetNormalized();
	OutTrace.ActionBindParentRelativeRotation =
		MannyBindSeed.ParentRelativeTargetRotation.GetNormalized();
	OutTrace.PolicyNeutralParentRelativeRotation =
		MannyNeutralParentRelativeRotation.GetNormalized();
	OutTrace.ObservationParentBindComponentRotation =
		CachedSmplObservationRestBodyComponentRotations[
			OutTrace.ObservationParentBodyIndex].GetNormalized();
	OutTrace.ActionBindComponentWorldRotation =
		(MannyBindSeed.ParentWorldRotation *
		 OutTrace.ObservationParentBindComponentRotation.Inverse()).GetNormalized();
	OutTrace.ComponentCorrectedActionAxisRotation =
		ExpressCachedWorldActionAxisInMeshComponentForTesting(
			OutTrace.ActionBindComponentWorldRotation,
			OutTrace.CachedActionAxisReferenceRotation);
	OutTrace.EffectiveActionAxisRotation = EffectiveActionAxisRotation.GetNormalized();
	OutTrace.ObservationBodyBindComponentRotation =
		CachedSmplObservationRestBodyComponentRotations[
			OutTrace.RoundtripObservationBodyIndex].GetNormalized();
	OutTrace.ObservationBindParentRelativeRotation =
		(OutTrace.ObservationParentBindComponentRotation.Inverse() *
		 OutTrace.ObservationBodyBindComponentRotation).GetNormalized();
	OutTrace.ActualDecodedRotationUe = ActualDecodedRotationUe.GetNormalized();
	OutTrace.ActualMannyPreRangeTargetParentRelative =
		ActualMannyPreRangeTargetParentRelative.GetNormalized();
	OutTrace.ActionAxisVsObservationParentBindAngularDeltaDegrees =
		FMath::RadiansToDegrees(
			(OutTrace.ActionBindComponentWorldRotation.Inverse() *
			 OutTrace.CachedActionAxisReferenceRotation).GetNormalized().AngularDistance(
				OutTrace.ObservationParentBindComponentRotation));
	const FQuat EffectiveActionAxisInObservationComponent =
		EffectiveActionAxisMode == PhysAnimBridge::MannyLocalFrameRoundtripWorldAxisMode
			? (OutTrace.ActionBindComponentWorldRotation.Inverse() *
			   OutTrace.EffectiveActionAxisRotation).GetNormalized()
			: OutTrace.EffectiveActionAxisRotation;
	OutTrace.EffectiveActionAxisVsObservationParentBindComponentAngularDeltaDegrees =
		FMath::RadiansToDegrees(
			EffectiveActionAxisInObservationComponent.AngularDistance(
				OutTrace.ObservationParentBindComponentRotation));
	OutTrace.ActionBindVsObservationBindParentRelativeAngularDeltaDegrees =
		FMath::RadiansToDegrees(
			OutTrace.ActionBindParentRelativeRotation.AngularDistance(
				OutTrace.ObservationBindParentRelativeRotation));
	OutTrace.PolicyNeutralVsActionBindParentRelativeAngularDeltaDegrees =
		FMath::RadiansToDegrees(
			OutTrace.PolicyNeutralParentRelativeRotation.AngularDistance(
				OutTrace.ActionBindParentRelativeRotation));
	OutTrace.PolicyNeutralVsObservationBindParentRelativeAngularDeltaDegrees =
		FMath::RadiansToDegrees(
			OutTrace.PolicyNeutralParentRelativeRotation.AngularDistance(
				OutTrace.ObservationBindParentRelativeRotation));

	auto AddRoundtripCase = [&OutTrace](
		const FName Label,
		const FQuat& CanonicalInput)
	{
		PhysAnimBridge::FPhysAnimMannyLocalFrameRoundtripCase& Case =
			OutTrace.RoundtripCases.AddDefaulted_GetRef();
		Case.Label = Label;
		Case.InputCanonicalRotationUe = CanonicalInput.GetNormalized();
		Case.MannyPreRangeTargetParentRelative =
			ComposeProtoPolicyTargetAroundMannyNeutralWithActionAxisForTesting(
				OutTrace.EffectiveActionAxisRotation,
				OutTrace.PolicyNeutralParentRelativeRotation,
				Case.InputCanonicalRotationUe);
		Case.RecoveredCanonicalRotationUe =
			PhysAnimProtoMannyAdapter::RecoverCanonicalJointRotation(
				OutTrace.ObservationParentBindComponentRotation,
				OutTrace.ObservationBindParentRelativeRotation,
				Case.MannyPreRangeTargetParentRelative);
		Case.AngularErrorDegrees = FMath::RadiansToDegrees(
			Case.InputCanonicalRotationUe.AngularDistance(
				Case.RecoveredCanonicalRotationUe));
	};

	const double ProbeRadians = FMath::DegreesToRadians(
		PhysAnimBridge::MannyLocalFrameRoundtripAxisProbeDegrees);
	AddRoundtripCase(TEXT("identity"), FQuat::Identity);
	AddRoundtripCase(TEXT("actual_decoded"), OutTrace.ActualDecodedRotationUe);
	AddRoundtripCase(TEXT("positive_x_10_deg"), FQuat(FVector::ForwardVector, ProbeRadians));
	AddRoundtripCase(TEXT("negative_x_10_deg"), FQuat(FVector::ForwardVector, -ProbeRadians));
	AddRoundtripCase(TEXT("positive_y_10_deg"), FQuat(FVector::RightVector, ProbeRadians));
	AddRoundtripCase(TEXT("negative_y_10_deg"), FQuat(FVector::RightVector, -ProbeRadians));
	AddRoundtripCase(TEXT("positive_z_10_deg"), FQuat(FVector::UpVector, ProbeRadians));
	AddRoundtripCase(TEXT("negative_z_10_deg"), FQuat(FVector::UpVector, -ProbeRadians));

	OutError.Reset();
	return true;
}
#endif


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


void UPhysAnimComponent::ApplyCharacterMovementBridgeOwnership(
	UCharacterMovementComponent* CharacterMovement,
	bool bPreserveGameplayShell)
{
	if (!CharacterMovement)
	{
		return;
	}

	if (bPreserveGameplayShell)
	{
		CharacterMovement->Activate(true);
		CharacterMovement->SetComponentTickEnabled(true);
		if (CharacterMovement->MovementMode == MOVE_None)
		{
			CharacterMovement->SetMovementMode(MOVE_Walking);
		}
		return;
	}

	CharacterMovement->StopMovementImmediately();
	CharacterMovement->DisableMovement();
	CharacterMovement->SetComponentTickEnabled(false);
	CharacterMovement->Deactivate();
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
	(void)bForceZeroActions;
	return EPhysAnimRuntimeState::BridgeActive;
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
			IsBalanceActiveState(State)) && bForceZeroActions;
}


bool UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState State)
{
	return State == EPhysAnimRuntimeState::BridgeActive || 
			State == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			State == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			State == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			State == EPhysAnimRuntimeState::BalanceEntry_Settle ||
			IsBalanceActiveState(State);
}

bool UPhysAnimComponent::ShouldRunRootOnReadinessUltraFineMarginSweep(float RootOnReadinessTotalDeficitDeg)
{
	return RootOnReadinessTotalDeficitDeg <= 2.0f + KINDA_SMALL_NUMBER;
}

#if !UE_BUILD_SHIPPING
void UPhysAnimComponent::FinalizePhase1AutoCalibScore(FPhase1AutoCalibScore& InOutScore)
{
	const bool bStandingBenchmarkSatisfied =
		InOutScore.bReachedBalanceActiveStanding &&
		InOutScore.BalanceActiveStandingHoldSeconds + KINDA_SMALL_NUMBER >= PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds;
	const bool bHardRejected =
		!InOutScore.bContractPassed ||
		InOutScore.bTimedOut ||
		InOutScore.bSafeDenied ||
		!InOutScore.bRestoreDeterministic ||
		!InOutScore.bReachedRootOn ||
		!InOutScore.bNoCouplingProofSatisfied ||
		!bStandingBenchmarkSatisfied;

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
		(InOutScore.PeakRootAngularSpeedDegPerSecond * 0.001f) -
		InOutScore.BalanceActiveStandingHoldSeconds;
}

bool UPhysAnimComponent::IsBetterPhase1AutoCalibScore(const FPhase1AutoCalibScore& Candidate, const FPhase1AutoCalibScore& CurrentBest)
{
	const auto IsRejected = [](const FPhase1AutoCalibScore& Score)
	{
		const bool bStandingBenchmarkSatisfied =
			Score.bReachedBalanceActiveStanding &&
			Score.BalanceActiveStandingHoldSeconds + KINDA_SMALL_NUMBER >= PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds;
		return !Score.bContractPassed ||
			Score.bTimedOut ||
			Score.bSafeDenied ||
			!Score.bRestoreDeterministic ||
			!Score.bReachedRootOn ||
			!Score.bNoCouplingProofSatisfied ||
			!bStandingBenchmarkSatisfied;
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
		TransitionPhase == EBalanceReadyTransitionPhase::BRT_Succeeded ? EPhysAnimRuntimeState::BalanceActive_Standing :
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
	case EPhysAnimRuntimeState::Standing_Preparation:
		return TEXT("Standing_Preparation");
	case EPhysAnimRuntimeState::Standing_FullSimulationActivation:
		return TEXT("Standing_FullSimulationActivation");
	case EPhysAnimRuntimeState::Standing_PolicyBlend:
		return TEXT("Standing_PolicyBlend");
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
	case EPhysAnimRuntimeState::BalanceActive_Standing:
		return TEXT("BalanceActive_Standing");
	case EPhysAnimRuntimeState::LocomotionActiveShell:
		return TEXT("LocomotionActiveShell");
	case EPhysAnimRuntimeState::LocomotionActiveShellDenied:
		return TEXT("LocomotionActiveShellDenied");
	case EPhysAnimRuntimeState::FailStopped:
		return TEXT("FailStopped");
	default:
		return TEXT("Unknown");
	}
}

