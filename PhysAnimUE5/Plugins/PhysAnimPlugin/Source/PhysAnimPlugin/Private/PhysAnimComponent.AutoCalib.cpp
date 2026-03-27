#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

#if !UE_BUILD_SHIPPING

namespace
{
	bool CaptureBodyState(USkeletalMeshComponent* Mesh, const FName BoneName, FPhase1AutoCalibBodyState& OutState)
	{
		FBodyInstance* const BodyInstance = Mesh ? Mesh->GetBodyInstance(BoneName) : nullptr;
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			return false;
		}

		OutState.BoneName = BoneName;
		OutState.WorldTransform = BodyInstance->GetUnrealWorldTransform();
		OutState.LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
		OutState.AngularVelocityRad = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
		OutState.bSimulating = BodyInstance->IsInstanceSimulatingPhysics();
		OutState.bSleeping = BodyInstance->IsInstanceAwake() == false;
		return true;
	}
}

bool UPhysAnimComponent::CapturePhase1AutoCalibBaseline(FPhase1AutoCalibBaselineSnapshot& OutSnapshot, FString& OutError) const
{
	OutError.Reset();

	const AActor* const OwnerActor = GetOwner();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!OwnerActor || !Mesh)
	{
		OutError = TEXT("Phase1 auto-calibration baseline capture requires a valid owner actor and skeletal mesh.");
		return false;
	}

	OutSnapshot = FPhase1AutoCalibBaselineSnapshot();
	OutSnapshot.OwnerActorTransform = OwnerActor->GetActorTransform();
	if (const ACharacter* const CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		if (const UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			OutSnapshot.CharacterVelocity = CharacterMovement->Velocity;
		}
	}

	OutSnapshot.MeshWorldTransform = Mesh->GetComponentTransform();
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		FPhase1AutoCalibBodyState& BodyState = OutSnapshot.Bodies.AddDefaulted_GetRef();
		if (!CaptureBodyState(Mesh, BoneName, BodyState))
		{
			OutSnapshot.Bodies.Pop();
		}
	}

	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		for (const FName ModifierName : PhysicsControl->GetAllBodyModifierNames())
		{
			const FPhysicsBodyModifierRecord* const Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName);
			if (!Record)
			{
				continue;
			}

			FPhase1AutoCalibBodyModifierState& ModifierState = OutSnapshot.BodyModifiers.AddDefaulted_GetRef();
			ModifierState.ModifierName = ModifierName;
			ModifierState.MovementType = Record->BodyModifier.ModifierData.MovementType;
			ModifierState.PhysicsBlendWeight = Record->BodyModifier.ModifierData.PhysicsBlendWeight;
			ModifierState.CollisionType = Record->BodyModifier.ModifierData.CollisionType;
			ModifierState.bUpdateKinematicFromSimulation = Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation;
		}
	}

	OutSnapshot.PreviousControlTargetRotations = PreviousControlTargetRotations;
	OutSnapshot.PolicyBlendStartControlTargetRotations = PolicyBlendStartControlTargetRotations;
	OutSnapshot.ConditionedActionBuffer = ConditionedActionBuffer;
	OutSnapshot.PreviousConditionedActionBuffer = PreviousConditionedActionBuffer;
	OutSnapshot.SelfObservationBuffer = SelfObservationBuffer;
	OutSnapshot.MimicTargetPosesBuffer = MimicTargetPosesBuffer;
	OutSnapshot.TerrainBuffer = TerrainBuffer;
	OutSnapshot.ActionOutputBuffer = ActionOutputBuffer;
	OutSnapshot.PreviousActionOutputBuffer = PreviousConditionedActionBuffer;
	OutSnapshot.LastValidPoseSearchResult = LastValidPoseSearchResult;
	OutSnapshot.ConsecutiveInvalidPoseSearchFrames = ConsecutiveInvalidPoseSearchFrames;
	OutSnapshot.BridgeIntentState = BridgeIntentState;
	OutSnapshot.BridgeTrajectoryState = BridgeTrajectoryState;
	OutSnapshot.BridgeShellState = BridgeShellState;
	OutSnapshot.RuntimeInstabilityState = RuntimeInstabilityState;
	OutSnapshot.LastRuntimeInstabilityDiagnostics = LastRuntimeInstabilityDiagnostics;
	OutSnapshot.LastActionDiagnostics = LastActionDiagnostics;
	OutSnapshot.LastControlTargetDiagnostics = LastControlTargetDiagnostics;
	OutSnapshot.LastAppliedStabilizationSettings = LastAppliedStabilizationSettings;
	OutSnapshot.BalanceTransitionSnapshot = BalanceReadyTransition.ExportSnapshot();
	OutSnapshot.SafePhase1ConvergenceSnapshot = SafePhase1ConvergenceSnapshot;
	OutSnapshot.LastPhase1PelvisCouplingRotationForensics = LastPhase1PelvisCouplingRotationForensics;
	OutSnapshot.PendingBodyModifierCachedResetNames = PendingBodyModifierCachedResetNames;
	OutSnapshot.BringUpGroupActivationTimeSeconds = BringUpGroupActivationTimeSeconds;
	OutSnapshot.BringUpGroupControlRampStartTimeSeconds = BringUpGroupControlRampStartTimeSeconds;
	OutSnapshot.BringUpGroupAlphaActiveLogged = BringUpGroupAlphaActiveLogged;
	OutSnapshot.PreviousDistalBoneIntendedOwnership = PreviousDistalBoneIntendedOwnership;
	OutSnapshot.PreviousDistalBoneModifierOwnership = PreviousDistalBoneModifierOwnership;
	OutSnapshot.LastDistalClassification = LastDistalClassification;
	OutSnapshot.PendingDistalOwnershipChecks = PendingDistalOwnershipChecks;
	OutSnapshot.RuntimeState = RuntimeState;
	OutSnapshot.BridgeLocomotionAuthorityState = BridgeLocomotionAuthorityState;
	OutSnapshot.BalanceTransitionShellAuthorityMode = BalanceTransitionShellAuthorityMode;
	OutSnapshot.SimulationHandoffAlpha = SimulationHandoffAlpha;
	OutSnapshot.bLastAppliedSimulationHandoffSettled = bLastAppliedSimulationHandoffSettled;
	OutSnapshot.LastAppliedControlAuthorityAlpha = LastAppliedControlAuthorityAlpha;
	OutSnapshot.BridgeStartTimeSeconds = BridgeStartTimeSeconds;
	OutSnapshot.SimulationHandoffCompletedTimeSeconds = SimulationHandoffCompletedTimeSeconds;
	OutSnapshot.PolicyInfluenceRampStartTimeSeconds = PolicyInfluenceRampStartTimeSeconds;
	OutSnapshot.HighestUnlockedBringUpGroupIndex = HighestUnlockedBringUpGroupIndex;
	OutSnapshot.BringUpGroupStableAccumulatedSeconds = BringUpGroupStableAccumulatedSeconds;
	OutSnapshot.LastRuntimeDiagnosticsLogTimeSeconds = LastRuntimeDiagnosticsLogTimeSeconds;
	OutSnapshot.PolicyUpdateAccumulatorSeconds = PolicyUpdateAccumulatorSeconds;
	OutSnapshot.LastPolicyElapsedSteps = LastPolicyElapsedSteps;
	OutSnapshot.PolicyControlTicksExecuted = PolicyControlTicksExecuted;
	OutSnapshot.PolicyControlTicksSkipped = PolicyControlTicksSkipped;
	OutSnapshot.LastPolicyControlUpdateTimeSeconds = LastPolicyControlUpdateTimeSeconds;
	OutSnapshot.ShellCouplingReferenceRootLocalOffsetCm = ShellCouplingReferenceRootLocalOffsetCm;
	OutSnapshot.bHasShellCouplingReferenceRootLocalOffset = bHasShellCouplingReferenceRootLocalOffset;
	OutSnapshot.bTransitionOwnedShellReferenceReanchored = bTransitionOwnedShellReferenceReanchored;
	OutSnapshot.bTransitionOwnedShellReferenceReseededAfterLock = bTransitionOwnedShellReferenceReseededAfterLock;
	OutSnapshot.bPolicyTargetsAppliedLastFrame = bPolicyTargetsAppliedLastFrame;
	OutSnapshot.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame;
	OutSnapshot.bStartupBringUpFrozenByBalanceEntry = bStartupBringUpFrozenByBalanceEntry;
	OutSnapshot.bPendingBalanceModeStartRequest = bPendingBalanceModeStartRequest;
	OutSnapshot.bPendingBalanceModeStartAttemptIssued = bPendingBalanceModeStartAttemptIssued;
	OutSnapshot.PendingBalanceModeStartReason = PendingBalanceModeStartReason;
	OutSnapshot.PendingBalanceModeRequestTimeSeconds = PendingBalanceModeRequestTimeSeconds;
	OutSnapshot.bPhase1TiltDiagnosticEmitted = bPhase1TiltDiagnosticEmitted;
	OutSnapshot.bPhase1PelvisCouplingSkipLogged = bPhase1PelvisCouplingSkipLogged;
	OutSnapshot.bPelvisResetAppliedThisTick = bPelvisResetAppliedThisTick;
	OutSnapshot.HipQuarantineTicksRemaining = HipQuarantineTicksRemaining;
	OutSnapshot.BalanceEntryRootOnFrameCount = BalanceEntryRootOnFrameCount;
	OutSnapshot.BalanceEntrySettleFrameCount = BalanceEntrySettleFrameCount;
	OutSnapshot.bLastPelvisRawSim = bLastPelvisRawSim;
	OutSnapshot.LastTotalSimCount = LastTotalSimCount;
	OutSnapshot.bPhase2Tick4AuditArmed = bPhase2Tick4AuditArmed;
	OutSnapshot.LastHipQuarantineLeftPreDeltaDegrees = LastHipQuarantineLeftPreDeltaDegrees;
	OutSnapshot.LastHipQuarantineRightPreDeltaDegrees = LastHipQuarantineRightPreDeltaDegrees;
	return true;
}

bool UPhysAnimComponent::RestorePhase1AutoCalibBaseline(const FPhase1AutoCalibBaselineSnapshot& Snapshot, FString& OutError)
{
	OutError.Reset();

	AActor* const OwnerActor = GetOwner();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!OwnerActor || !Mesh)
	{
		OutError = TEXT("Phase1 auto-calibration baseline restore requires a valid owner actor and skeletal mesh.");
		return false;
	}

	if (BalanceReadyTransition.HasAnyInternalPhase())
	{
		BalanceReadyTransition.Cancel(this);
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
	{
		StopBalancePerturbationMode();
	}

	OwnerActor->SetActorTransform(Snapshot.OwnerActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetWorldTransform(Snapshot.MeshWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);
	if (ACharacter* const CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			CharacterMovement->Velocity = Snapshot.CharacterVelocity;
		}
	}

	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		for (const FPhase1AutoCalibBodyModifierState& ModifierState : Snapshot.BodyModifiers)
		{
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(
				ModifierState.ModifierName,
				ModifierState.bUpdateKinematicFromSimulation,
				false,
				false);
			PhysicsControl->SetBodyModifierPhysicsBlendWeight(
				ModifierState.ModifierName,
				ModifierState.PhysicsBlendWeight,
				false,
				false);
			PhysicsControl->SetBodyModifierCollisionType(
				ModifierState.ModifierName,
				ModifierState.CollisionType,
				false,
				false);
			PhysicsControl->SetBodyModifierMovementType(
				ModifierState.ModifierName,
				ModifierState.MovementType,
				false,
				true);
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierState.ModifierName,
				ModifierState.MovementType,
				ModifierState.PhysicsBlendWeight,
				ModifierState.CollisionType,
				ModifierState.bUpdateKinematicFromSimulation);
		}
	}

	for (const FPhase1AutoCalibBodyState& BodyState : Snapshot.Bodies)
	{
		FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BodyState.BoneName);
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			continue;
		}

		BodyInstance->SetInstanceSimulatePhysics(BodyState.bSimulating, true);
		BodyInstance->SetBodyTransform(BodyState.WorldTransform, ETeleportType::TeleportPhysics, true);
		BodyInstance->SetLinearVelocity(BodyState.LinearVelocity, false);
		BodyInstance->SetAngularVelocityInRadians(BodyState.AngularVelocityRad, false);
		if (BodyState.bSleeping)
		{
			BodyInstance->PutInstanceToSleep();
		}
		else
		{
			BodyInstance->WakeInstance();
		}
	}

	PreviousControlTargetRotations = Snapshot.PreviousControlTargetRotations;
	PolicyBlendStartControlTargetRotations = Snapshot.PolicyBlendStartControlTargetRotations;
	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		for (const TPair<FName, FQuat>& Pair : PreviousControlTargetRotations)
		{
			if (PhysicsControl->GetControlExists(Pair.Key))
			{
				PhysicsControl->SetControlTargetOrientation(Pair.Key, Pair.Value.Rotator(), 0.0f, true, false, true, false);
			}
		}
	}

	ConditionedActionBuffer = Snapshot.ConditionedActionBuffer;
	PreviousConditionedActionBuffer = Snapshot.PreviousConditionedActionBuffer;
	SelfObservationBuffer = Snapshot.SelfObservationBuffer;
	MimicTargetPosesBuffer = Snapshot.MimicTargetPosesBuffer;
	TerrainBuffer = Snapshot.TerrainBuffer;
	ActionOutputBuffer = Snapshot.ActionOutputBuffer;
	LastValidPoseSearchResult = Snapshot.LastValidPoseSearchResult;
	ConsecutiveInvalidPoseSearchFrames = Snapshot.ConsecutiveInvalidPoseSearchFrames;
	BridgeIntentState = Snapshot.BridgeIntentState;
	BridgeTrajectoryState = Snapshot.BridgeTrajectoryState;
	BridgeShellState = Snapshot.BridgeShellState;
	RuntimeInstabilityState = Snapshot.RuntimeInstabilityState;
	LastRuntimeInstabilityDiagnostics = Snapshot.LastRuntimeInstabilityDiagnostics;
	LastActionDiagnostics = Snapshot.LastActionDiagnostics;
	LastControlTargetDiagnostics = Snapshot.LastControlTargetDiagnostics;
	LastAppliedStabilizationSettings = Snapshot.LastAppliedStabilizationSettings;
	BalanceReadyTransition.ImportSnapshot(Snapshot.BalanceTransitionSnapshot);
	SafePhase1ConvergenceSnapshot = Snapshot.SafePhase1ConvergenceSnapshot;
	LastPhase1PelvisCouplingRotationForensics = Snapshot.LastPhase1PelvisCouplingRotationForensics;
	PendingBodyModifierCachedResetNames = Snapshot.PendingBodyModifierCachedResetNames;
	BringUpGroupActivationTimeSeconds = Snapshot.BringUpGroupActivationTimeSeconds;
	BringUpGroupControlRampStartTimeSeconds = Snapshot.BringUpGroupControlRampStartTimeSeconds;
	BringUpGroupAlphaActiveLogged = Snapshot.BringUpGroupAlphaActiveLogged;
	PreviousDistalBoneIntendedOwnership = Snapshot.PreviousDistalBoneIntendedOwnership;
	PreviousDistalBoneModifierOwnership = Snapshot.PreviousDistalBoneModifierOwnership;
	LastDistalClassification = Snapshot.LastDistalClassification;
	PendingDistalOwnershipChecks = Snapshot.PendingDistalOwnershipChecks;
	BridgeLocomotionAuthorityState = Snapshot.BridgeLocomotionAuthorityState;
	BalanceTransitionShellAuthorityMode = Snapshot.BalanceTransitionShellAuthorityMode;
	SimulationHandoffAlpha = Snapshot.SimulationHandoffAlpha;
	bLastAppliedSimulationHandoffSettled = Snapshot.bLastAppliedSimulationHandoffSettled;
	LastAppliedControlAuthorityAlpha = Snapshot.LastAppliedControlAuthorityAlpha;
	BridgeStartTimeSeconds = Snapshot.BridgeStartTimeSeconds;
	SimulationHandoffCompletedTimeSeconds = Snapshot.SimulationHandoffCompletedTimeSeconds;
	PolicyInfluenceRampStartTimeSeconds = Snapshot.PolicyInfluenceRampStartTimeSeconds;
	HighestUnlockedBringUpGroupIndex = Snapshot.HighestUnlockedBringUpGroupIndex;
	BringUpGroupStableAccumulatedSeconds = Snapshot.BringUpGroupStableAccumulatedSeconds;
	LastRuntimeDiagnosticsLogTimeSeconds = Snapshot.LastRuntimeDiagnosticsLogTimeSeconds;
	PolicyUpdateAccumulatorSeconds = Snapshot.PolicyUpdateAccumulatorSeconds;
	LastPolicyElapsedSteps = Snapshot.LastPolicyElapsedSteps;
	PolicyControlTicksExecuted = Snapshot.PolicyControlTicksExecuted;
	PolicyControlTicksSkipped = Snapshot.PolicyControlTicksSkipped;
	LastPolicyControlUpdateTimeSeconds = Snapshot.LastPolicyControlUpdateTimeSeconds;
	ShellCouplingReferenceRootLocalOffsetCm = Snapshot.ShellCouplingReferenceRootLocalOffsetCm;
	bHasShellCouplingReferenceRootLocalOffset = Snapshot.bHasShellCouplingReferenceRootLocalOffset;
	bTransitionOwnedShellReferenceReanchored = Snapshot.bTransitionOwnedShellReferenceReanchored;
	bTransitionOwnedShellReferenceReseededAfterLock = Snapshot.bTransitionOwnedShellReferenceReseededAfterLock;
	bPolicyTargetsAppliedLastFrame = Snapshot.bPolicyTargetsAppliedLastFrame;
	bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = Snapshot.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame;
	bStartupBringUpFrozenByBalanceEntry = Snapshot.bStartupBringUpFrozenByBalanceEntry;
	bPendingBalanceModeStartRequest = Snapshot.bPendingBalanceModeStartRequest;
	bPendingBalanceModeStartAttemptIssued = Snapshot.bPendingBalanceModeStartAttemptIssued;
	PendingBalanceModeStartReason = Snapshot.PendingBalanceModeStartReason;
	PendingBalanceModeRequestTimeSeconds = Snapshot.PendingBalanceModeRequestTimeSeconds;
	bPhase1TiltDiagnosticEmitted = Snapshot.bPhase1TiltDiagnosticEmitted;
	bPhase1PelvisCouplingSkipLogged = Snapshot.bPhase1PelvisCouplingSkipLogged;
	bPelvisResetAppliedThisTick = Snapshot.bPelvisResetAppliedThisTick;
	HipQuarantineTicksRemaining = Snapshot.HipQuarantineTicksRemaining;
	BalanceEntryRootOnFrameCount = Snapshot.BalanceEntryRootOnFrameCount;
	BalanceEntrySettleFrameCount = Snapshot.BalanceEntrySettleFrameCount;
	bLastPelvisRawSim = Snapshot.bLastPelvisRawSim;
	LastTotalSimCount = Snapshot.LastTotalSimCount;
	bPhase2Tick4AuditArmed = Snapshot.bPhase2Tick4AuditArmed;
	LastHipQuarantineLeftPreDeltaDegrees = Snapshot.LastHipQuarantineLeftPreDeltaDegrees;
	LastHipQuarantineRightPreDeltaDegrees = Snapshot.LastHipQuarantineRightPreDeltaDegrees;
	ActivePhase1AutoCalibParams.Reset();
	TransitionRuntimeState(Snapshot.RuntimeState);
	return true;
}

void UPhysAnimComponent::ApplyPhase1AutoCalibParams(const FPhase1AutoCalibParams& Params)
{
	ActivePhase1AutoCalibParams = Params;
}

void UPhysAnimComponent::ClearPhase1AutoCalibParams()
{
	ActivePhase1AutoCalibParams.Reset();
}

bool UPhysAnimComponent::CapturePhase1AutoCalibLiveMetrics(FPhase1AutoCalibLiveMetrics& OutMetrics, FString& OutError) const
{
	OutError.Reset();

	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!Mesh)
	{
		OutError = TEXT("Phase1 auto-calibration metrics require a valid skeletal mesh.");
		return false;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FBodyInstance* const RootBody = Mesh->GetBodyInstance(RootBoneName);
	if (!RootBody)
	{
		OutError = TEXT("Phase1 auto-calibration metrics require a valid pelvis body.");
		return false;
	}

	FString TiltSource;
	OutMetrics.RuntimeState = RuntimeState;
	OutMetrics.TransitionPhase = BalanceReadyTransition.GetPhase();
	OutMetrics.RootLinearSpeedCmPerSecond = RootBody->GetUnrealWorldVelocity().Size();
	OutMetrics.RootAngularSpeedDegPerSecond = FMath::RadiansToDegrees(RootBody->GetUnrealWorldAngularVelocityInRadians().Size());
	OutMetrics.RootTiltDeg = ResolvePhase1Uprightness(MeshComponent.Get(), GetOwner(), RootBoneName, TiltSource);
	OutMetrics.ShellOffsetDeltaCm = GetCurrentShellPlanarOffsetDeltaCm();
	OutMetrics.ShellVelocityDeltaCmPerSecond = GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	OutMetrics.MaxTargetDeltaDeg = LastControlTargetDiagnostics.MaxTargetDeltaDegrees;
	OutMetrics.MeanTargetDeltaDeg = LastControlTargetDiagnostics.MeanTargetDeltaDegrees;
	return true;
}

bool UPhysAnimComponent::CapturePhase1AutoCalibDeterminismFingerprint(FPhase1AutoCalibDeterminismFingerprint& OutFingerprint, FString& OutError) const
{
	OutError.Reset();

	const AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!OwnerActor || !Mesh)
	{
		OutError = TEXT("Phase1 auto-calibration determinism fingerprint requires a valid owner actor and skeletal mesh.");
		return false;
	}

	const FBodyInstance* const RootBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName());
	if (!RootBody)
	{
		OutError = TEXT("Phase1 auto-calibration determinism fingerprint requires a valid pelvis body.");
		return false;
	}

	FPhase1AutoCalibLiveMetrics LiveMetrics;
	if (!CapturePhase1AutoCalibLiveMetrics(LiveMetrics, OutError))
	{
		return false;
	}

	OutFingerprint.RuntimeState = RuntimeState;
	OutFingerprint.TransitionPhase = BalanceReadyTransition.GetPhase();
	OutFingerprint.OwnerTransform = OwnerActor->GetActorTransform();
	OutFingerprint.MeshTransform = Mesh->GetComponentTransform();
	OutFingerprint.RootBodyTransform = RootBody->GetUnrealWorldTransform();
	OutFingerprint.RootLinearVelocity = RootBody->GetUnrealWorldVelocity();
	OutFingerprint.RootAngularVelocity = RootBody->GetUnrealWorldAngularVelocityInRadians();
	OutFingerprint.ShellOffsetDeltaCm = LiveMetrics.ShellOffsetDeltaCm;
	OutFingerprint.ShellVelocityDeltaCmPerSecond = LiveMetrics.ShellVelocityDeltaCmPerSecond;
	OutFingerprint.MaxTargetDeltaDeg = LiveMetrics.MaxTargetDeltaDeg;
	OutFingerprint.MeanTargetDeltaDeg = LiveMetrics.MeanTargetDeltaDeg;
	OutFingerprint.PendingResetCount = PendingBodyModifierCachedResetNames.Num();
	return true;
}

bool UPhysAnimComponent::StartPhase1AutoCalibTrial(FString& OutError)
{
	OutError.Reset();
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	if (!EvaluateBalanceModeQueueGates(EffectiveSettings, OutError))
	{
		return false;
	}

	StartBalancePerturbationMode();
	return true;
}

#endif
