#include "PhysAnimBalanceReadyTransitionPrivate.h"

namespace
{
	FTransform ResolveForensicBodyOrBoneTransform(
		const USkeletalMeshComponent* Mesh,
		const FName BoneName,
		bool& bOutUsedBodyInstance)
	{
		bOutUsedBodyInstance = false;
		if (!Mesh)
		{
			return FTransform::Identity;
		}

		if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
		{
			if (BodyInstance->IsValidBodyInstance())
			{
				bOutUsedBodyInstance = true;
				return BodyInstance->GetUnrealWorldTransform();
			}
		}

		const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
		return BoneIndex != INDEX_NONE ? Mesh->GetBoneTransform(BoneIndex) : FTransform::Identity;
	}
}

bool BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(
	const USkeletalMeshComponent* Mesh,
	FName ParentBoneName,
	FName ChildBoneName,
	FDirectPelvisLinkForensicRecord& OutRecord)
{
	OutRecord = {};
	OutRecord.ParentBoneName = ParentBoneName;
	OutRecord.ChildBoneName = ChildBoneName;
	OutRecord.LinkName = FString::Printf(TEXT("%s_%s"), *ParentBoneName.ToString(), *ChildBoneName.ToString());

	if (!Mesh)
	{
		return false;
	}

	const UPhysicsAsset* const PhysicsAsset = Mesh->GetPhysicsAsset();
	OutRecord.PhysicsAssetPath = PhysicsAsset ? PhysicsAsset->GetPathName() : TEXT("<none>");

	const FTransform ParentTransform = ResolveForensicBodyOrBoneTransform(Mesh, ParentBoneName, OutRecord.bParentUsedBodyInstance);
	const FTransform ChildTransform = ResolveForensicBodyOrBoneTransform(Mesh, ChildBoneName, OutRecord.bChildUsedBodyInstance);
	OutRecord.ParentWorldRotation = ParentTransform.GetRotation();
	OutRecord.ChildWorldRotation = ChildTransform.GetRotation();
	OutRecord.ParentSpaceLabel = OutRecord.bParentUsedBodyInstance ? TEXT("pelvis_body_local(body_instance)") : TEXT("pelvis_body_local(bone_fallback)");
	OutRecord.ChildSpaceLabel = OutRecord.bChildUsedBodyInstance ? TEXT("child_body_local(body_instance)") : TEXT("child_body_local(bone_fallback)");
	OutRecord.BodyOriginDistanceCm = FVector::Dist(
		ResolveBodyOrBoneLocationCm(Mesh, ParentBoneName),
		ResolveBodyOrBoneLocationCm(Mesh, ChildBoneName));

	if (!PhysicsAsset)
	{
		return false;
	}

	const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
	if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
	{
		return false;
	}

	const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
	const FConstraintInstance* const Constraint = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
	if (!Constraint)
	{
		return false;
	}

	OutRecord.bConstraintFound = true;
	OutRecord.AuthoredChildAnchorLocalCm = Constraint->Pos1;
	OutRecord.AuthoredParentAnchorLocalCm = Constraint->Pos2;
	OutRecord.AuthoredChildRefFrame = Constraint->GetRefFrame(EConstraintFrame::Frame1).GetRotation();
	OutRecord.AuthoredParentRefFrame = Constraint->GetRefFrame(EConstraintFrame::Frame2).GetRotation();
	OutRecord.EvaluatedChildAnchorWorldCm = ChildTransform.TransformPosition(Constraint->Pos1);
	OutRecord.EvaluatedParentAnchorWorldCm = ParentTransform.TransformPosition(Constraint->Pos2);
	OutRecord.AnchorDistanceCm = FVector::Dist(OutRecord.EvaluatedParentAnchorWorldCm, OutRecord.EvaluatedChildAnchorWorldCm);
	const FQuat ChildConstraintWorldRotation =
		(ChildTransform.GetRotation() * OutRecord.AuthoredChildRefFrame).GetNormalized();
	const FQuat ParentConstraintWorldRotation =
		(ParentTransform.GetRotation() * OutRecord.AuthoredParentRefFrame).GetNormalized();
	const float ConstraintAngularErrorRadians = ChildConstraintWorldRotation.AngularDistance(ParentConstraintWorldRotation);
	OutRecord.ConstraintAngularErrorDeg = FMath::RadiansToDegrees(ConstraintAngularErrorRadians);
	return true;
}

void BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(
	const TArray<FDirectPelvisLinkForensicRecord>& Records,
	const TCHAR* ContextTag,
	bool bEmitMissingConstraintErrors)
{
	for (const FDirectPelvisLinkForensicRecord& Record : Records)
	{
		if (!Record.bConstraintFound)
		{
			if (bEmitMissingConstraintErrors)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Error,
					TEXT("[PhysAnimBalance] %s authored_data_missing link=%s physicsAsset=%s parent=%s child=%s"),
					ContextTag,
					*Record.LinkName,
					*Record.PhysicsAssetPath,
					*Record.ParentBoneName.ToString(),
					*Record.ChildBoneName.ToString());
			}
			continue;
		}

		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimBalance] %s link=%s physicsAsset=%s parentAnchorLocal=(%.2f,%.2f,%.2f) childAnchorLocal=(%.2f,%.2f,%.2f) parentSpace=%s childSpace=%s parentAnchorWorld=(%.2f,%.2f,%.2f) childAnchorWorld=(%.2f,%.2f,%.2f) anchorDistanceCm=%.2f angularErrorDeg=%.2f parentWorldQuat=%s childWorldQuat=%s parentAuthRefFrame=%s childAuthRefFrame=%s bodyOriginDistanceCm=%.2f"),
			ContextTag,
			*Record.LinkName,
			*Record.PhysicsAssetPath,
			Record.AuthoredParentAnchorLocalCm.X,
			Record.AuthoredParentAnchorLocalCm.Y,
			Record.AuthoredParentAnchorLocalCm.Z,
			Record.AuthoredChildAnchorLocalCm.X,
			Record.AuthoredChildAnchorLocalCm.Y,
			Record.AuthoredChildAnchorLocalCm.Z,
			*Record.ParentSpaceLabel,
			*Record.ChildSpaceLabel,
			Record.EvaluatedParentAnchorWorldCm.X,
			Record.EvaluatedParentAnchorWorldCm.Y,
			Record.EvaluatedParentAnchorWorldCm.Z,
			Record.EvaluatedChildAnchorWorldCm.X,
			Record.EvaluatedChildAnchorWorldCm.Y,
			Record.EvaluatedChildAnchorWorldCm.Z,
			Record.AnchorDistanceCm,
			Record.ConstraintAngularErrorDeg,
			*Record.ParentWorldRotation.ToString(),
			*Record.ChildWorldRotation.ToString(),
			*Record.AuthoredParentRefFrame.ToString(),
			*Record.AuthoredChildRefFrame.ToString(),
			Record.BodyOriginDistanceCm);
	}
}

bool FPhysAnimBalanceReadyTransition::ValidateLateValidationBaselineSnapshot(
	const FPhysAnimCertifiedHandoffSnapshot& Snapshot,
	const FPhysAnimLateValidationResult& Result,
	const FPhysAnimStabilizationSettings& Settings,
	FString& OutReason)
{
	// A valid baseline for late validation must already be 'certified' as either 
	// Outcome_AcceptRootOn (proximal+upper) or Outcome_SafeDenyUpperOnly (upper body only).
	// Outcome_Pending indicates a transient/partially-shaping topology which is too loose for a stable baseline.

	if (Result.Outcome == EBalanceLateValidationOutcome::Outcome_Pending)
	{
		if (Snapshot.ProximalSimCount >= 5 && Snapshot.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold)
		{
			// Valid baseline: upper body is being held kinematic during late validate, and will release after proof passes.
		}
		else
		{
			OutReason = Snapshot.ProximalSimCount > 0
				? TEXT("phase1_late_validate_baseline_topology_mismatch_proximal_incomplete")
				: TEXT("phase1_late_validate_baseline_topology_mismatch_upper_incomplete");
			return false;
		}
	}

	if (!Snapshot.bControlAuthoritySettled)
	{
		OutReason = TEXT("phase1_late_validate_baseline_control_authority_not_settled");
		return false;
	}

	return true;
}



bool FPhysAnimBalanceReadyTransition::BuildCertifiedHandoffSnapshot(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FPhysAnimCertifiedHandoffSnapshot& OutSnapshot, FPhysAnimLateValidationResult& OutResult, bool bUseFrozenTopology) const
{
	if (!Owner)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
	if (!PelvisBody)
	{
		return false;
	}

	TArray<FName> SimulatingBones;
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperSimCount = 0;
	bool bRootSimulating = false;

	Owner->GetSimulatingBodies(SimulatingBones);
	if (GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnimBalance] GET_SIMULATING_BODIES count=%d"), SimulatingBones.Num());
	}
	TSet<FName> SimulatingBoneSet(SimulatingBones);

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const bool bIsProximal = BalanceTransitionSets::IsProximal(BoneName);
		const bool bIsSimulating = SimulatingBoneSet.Contains(BoneName);

		if (bIsSimulating)
		{
			if (bIsProximal)
			{
				ProximalSimCount++;
			}
			else if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
			{
				DistalSimCount++;
			}
			else if (BalanceTransitionSets::IsUpperBody(BoneName))
			{
				UpperSimCount++;
			}
			continue;
		}
	}

	bRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();

	// If we have a frozen Phase 1 record, we prefer its topology for handoff classification 
	// (modes and counts) even if the live state has changed (e.g. bones starting to simulate).
	if (bHasPhase1TopologyRecord && bUseFrozenTopology)
	{
		ProximalSimCount = Phase1TopologyRecord.ProximalSimCount;
		DistalSimCount = Phase1TopologyRecord.DistalSimCount;
		UpperSimCount = Phase1TopologyRecord.UpperBodySimCount;
		bRootSimulating = Phase1TopologyRecord.bRootSimulating;

		OutSnapshot.RootOwnershipMode = Phase1TopologyRecord.RootOwnershipMode;
		OutSnapshot.ProximalOwnershipMode = Phase1TopologyRecord.ProximalOwnershipMode;
		OutSnapshot.DistalOwnershipMode = Phase1TopologyRecord.DistalOwnershipMode;
		OutSnapshot.UpperBodyOwnershipMode = Phase1TopologyRecord.UpperBodyOwnershipMode;
		OutSnapshot.bPolicySuppressed = Phase1TopologyRecord.bPolicySuppressed;
		OutSnapshot.bResetsSuppressed = Phase1TopologyRecord.bResetsSuppressed;
	}
	else
	{
		OutSnapshot.RootOwnershipMode = bRootSimulating ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		OutSnapshot.ProximalOwnershipMode = ProximalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		OutSnapshot.DistalOwnershipMode = DistalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		// UpperBodyOwnershipMode is handled below as it depends on classification.
	}

	const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = Owner->GetLastControlTargetDiagnostics();
	OutSnapshot.PolicyInfluenceAlphaAtCapture = Owner->CalculateCurrentPolicyInfluenceAlpha(Settings);
	OutSnapshot.ShellAuthorityMode = BalanceTransitionSets::GetShellAuthorityModeName(Owner->GetBalanceTransitionShellAuthorityMode());
	OutSnapshot.TopologyClass = BalanceTransitionSets::BuildCertifiedHandoffTopologyClass(
		OutSnapshot.RootOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating,
		OutSnapshot.ProximalOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating ? ProximalSimCount : 0,
		OutSnapshot.DistalOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating ? DistalSimCount : 0,
		UpperSimCount);
	OutSnapshot.SimCount = (bHasPhase1TopologyRecord && bUseFrozenTopology) ? Phase1TopologyRecord.TotalSimCount : SimulatingBones.Num();
	OutSnapshot.ProximalSimCount = ProximalSimCount;
	OutSnapshot.DistalSimCount = DistalSimCount;
	OutSnapshot.UpperBodySimCount = UpperSimCount;
	TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> DirectPelvisLinkForensics;
	DirectPelvisLinkForensics.Reserve(3);
	const auto ComputeDirectLinkErrorCm = [&](const FName BoneName)
	{
		BalanceTransitionSets::FDirectPelvisLinkForensicRecord ForensicRecord;
		BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBoneName, BoneName, ForensicRecord);
		DirectPelvisLinkForensics.Add(ForensicRecord);
		return ForensicRecord.bConstraintFound ? ForensicRecord.AnchorDistanceCm : ForensicRecord.BodyOriginDistanceCm;
	};
	OutSnapshot.PelvisThighLErrorCm = ComputeDirectLinkErrorCm(TEXT("thigh_l"));
	OutSnapshot.PelvisThighRErrorCm = ComputeDirectLinkErrorCm(TEXT("thigh_r"));
	OutSnapshot.PelvisSpine01ErrorCm = ComputeDirectLinkErrorCm(TEXT("spine_01"));
	OutSnapshot.PelvisThighLAngularErrorDeg =
		DirectPelvisLinkForensics.Num() > 0 ? DirectPelvisLinkForensics[0].ConstraintAngularErrorDeg : 0.0f;
	OutSnapshot.PelvisThighRAngularErrorDeg =
		DirectPelvisLinkForensics.Num() > 1 ? DirectPelvisLinkForensics[1].ConstraintAngularErrorDeg : 0.0f;
	OutSnapshot.PelvisSpine01AngularErrorDeg =
		DirectPelvisLinkForensics.Num() > 2 ? DirectPelvisLinkForensics[2].ConstraintAngularErrorDeg : 0.0f;
	const bool bDirectPelvisLinkAngularSatisfied =
		DirectPelvisLinkForensics.Num() == 3 &&
		DirectPelvisLinkForensics[0].bConstraintFound &&
		DirectPelvisLinkForensics[1].bConstraintFound &&
		DirectPelvisLinkForensics[2].bConstraintFound &&
		OutSnapshot.PelvisThighLAngularErrorDeg <= BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg &&
		OutSnapshot.PelvisThighRAngularErrorDeg <= BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg &&
		OutSnapshot.PelvisSpine01AngularErrorDeg <= BalanceTransitionSets::Phase2MaxPelvisSpineDirectLinkAngularErrorDeg;
	const bool bDirectPelvisSpineMarginSatisfied =
		DirectPelvisLinkForensics.Num() == 3 &&
		DirectPelvisLinkForensics[2].bConstraintFound &&
		OutSnapshot.PelvisSpine01AngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg;
	const bool bDirectPelvisThighMarginsSatisfied =
		DirectPelvisLinkForensics.Num() == 3 &&
		DirectPelvisLinkForensics[0].bConstraintFound &&
		DirectPelvisLinkForensics[1].bConstraintFound &&
		OutSnapshot.PelvisThighLAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg &&
		OutSnapshot.PelvisThighRAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg;
	const bool bDirectPelvisLinkPositionSatisfied =
		DirectPelvisLinkForensics.Num() == 3 &&
		DirectPelvisLinkForensics[0].bConstraintFound &&
		DirectPelvisLinkForensics[1].bConstraintFound &&
		DirectPelvisLinkForensics[2].bConstraintFound &&
		OutSnapshot.PelvisThighLErrorCm <= BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm &&
		OutSnapshot.PelvisThighRErrorCm <= BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm &&
		OutSnapshot.PelvisSpine01ErrorCm <= BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm;
	OutSnapshot.bRootOnDirectPelvisLinkAngularSatisfied = bDirectPelvisLinkAngularSatisfied;
	OutSnapshot.bRootOnDirectPelvisLinkGeometrySatisfied =
		DirectPelvisLinkForensics.Num() == 3 &&
		DirectPelvisLinkForensics[0].bConstraintFound &&
		DirectPelvisLinkForensics[1].bConstraintFound &&
		DirectPelvisLinkForensics[2].bConstraintFound &&
		bDirectPelvisLinkPositionSatisfied;

	const bool bRootCoupledTopologyReady = bHasPhase1TopologyRecord && bUseFrozenTopology
		? BalanceTransitionSets::IsRootCoupledReadyHandoff(
			Phase1TopologyRecord.ProximalSimCount,
			Phase1TopologyRecord.DistalSimCount,
			Phase1TopologyRecord.UpperBodySimCount,
			Phase1TopologyRecord.bRootSimulating)
		: BalanceTransitionSets::IsRootCoupledReadyHandoff(
			ProximalSimCount,
			DistalSimCount,
			UpperSimCount,
			bRootSimulating);
	const bool bUpperOnlyTopologyReady = bHasPhase1TopologyRecord && bUseFrozenTopology
		? BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(
			Phase1TopologyRecord.ProximalSimCount,
			Phase1TopologyRecord.DistalSimCount,
			Phase1TopologyRecord.UpperBodySimCount,
			Phase1TopologyRecord.bRootSimulating)
		: BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(
			ProximalSimCount,
			DistalSimCount,
			UpperSimCount,
			bRootSimulating);
	const EBalanceReadyRootOnReadinessClassification RootOnReadinessClassification =
		bRootCoupledTopologyReady
			? EBalanceReadyRootOnReadinessClassification::RootCoupledReady
			: (bUpperOnlyTopologyReady
				? EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
				: EBalanceReadyRootOnReadinessClassification::NotReady);

	if (bRootCoupledTopologyReady &&
		(!OutSnapshot.bRootOnDirectPelvisLinkGeometrySatisfied || !OutSnapshot.bRootOnDirectPelvisLinkAngularSatisfied) &&
		!bLoggedDirectPelvisLinkForensics)
	{
		TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> FailingLinkForensics;
		for (const BalanceTransitionSets::FDirectPelvisLinkForensicRecord& Record : DirectPelvisLinkForensics)
		{
			const float AngularThresholdDeg =
				Record.ChildBoneName == TEXT("spine_01")
					? BalanceTransitionSets::Phase2MaxPelvisSpineDirectLinkAngularErrorDeg
					: BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg;
			if (!Record.bConstraintFound ||
				Record.AnchorDistanceCm > BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm ||
				Record.ConstraintAngularErrorDeg > AngularThresholdDeg)
			{
				FailingLinkForensics.Add(Record);
			}
		}
		BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(
			FailingLinkForensics,
			TEXT("PHASE2_PRE_ROOT_ON_LINK_FORENSIC"),
			true);
		bLoggedDirectPelvisLinkForensics = true;
	}

	if (!(bHasPhase1TopologyRecord && bUseFrozenTopology))
	{
		OutSnapshot.UpperBodyOwnershipMode = (RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::NotReady)
			? EBalanceReadyUpperBodyOwnershipMode::None
			: EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold;
		OutSnapshot.bPolicySuppressed = ShouldSuppressPolicy();
		OutSnapshot.bResetsSuppressed = ShouldSuppressResets();
	}
	OutSnapshot.bControlAuthoritySettled = Owner->CalculateCurrentControlAuthorityAlpha(Settings) >= 1.0f - KINDA_SMALL_NUMBER;
	const int32 FinalBringUpGroupIndex = Owner->GetBringUpGroupCount() - 1;
	OutSnapshot.FinalBringUpGroupControlAuthorityAlpha = Owner->CalculateBringUpGroupControlAuthorityAlpha(FinalBringUpGroupIndex, Settings);
	OutSnapshot.bFinalBringUpGroupUnlocked = Owner->IsBringUpGroupUnlocked(FinalBringUpGroupIndex);
	OutSnapshot.bFinalBringUpGroupRampActive = Owner->IsBringUpGroupControlRampActive(FinalBringUpGroupIndex);
	OutSnapshot.BringUpStableAccumulatedSeconds = Owner->BringUpGroupStableAccumulatedSeconds;
	OutSnapshot.bBringUpWithinBodyVelocityBounds = 
		Owner->LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond <= Settings.MaxRootLinearSpeedCmPerSecond &&
		Owner->LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond <= Settings.MaxRootAngularSpeedDegPerSecond;
	OutSnapshot.bBringUpWithinRootBounds = 
		Owner->LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm <= Settings.MaxRootHeightDeltaCm &&
		Owner->LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond <= Settings.MaxRootLinearSpeedCmPerSecond &&
		Owner->LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond <= Settings.MaxRootAngularSpeedDegPerSecond;

	OutSnapshot.RootOnReadinessPolicyInfluenceRequiredAlpha = Owner->BalanceReadyPolicyInfluenceThreshold;
	OutSnapshot.RootOnReadinessPolicyInfluenceDurationSeconds =
		OutSnapshot.PolicyInfluenceAlphaAtCapture * FMath::Max(Settings.StartupRampSeconds, 0.0f);
	OutSnapshot.RootOnReadinessPolicyInfluenceRequiredSeconds =
		FMath::Max(Settings.StartupRampSeconds, UE_SMALL_NUMBER) * Owner->BalanceReadyPolicyInfluenceThreshold;
	OutSnapshot.RootOnReadinessShellHoldDurationSeconds = RootOnReadinessShellHoldAccumulatedSeconds;
	OutSnapshot.RootOnReadinessShellHoldRequiredSeconds = Settings.BalancePhase2RequiredShellHoldDuration;
	OutSnapshot.RootOnReadinessShellProofDurationSeconds = RootOnReadinessShellProofAccumulatedSeconds;
	OutSnapshot.RootOnReadinessNoCouplingProofDurationSeconds = this->RootOnReadinessNoCouplingProofAccumulatedSeconds;
	OutSnapshot.RootOnReadinessNoCouplingRequiredSeconds = Settings.BalancePhase2RequiredShellHoldDuration;
	OutSnapshot.bRootOnReadinessNoCouplingProofSatisfied =
		this->RootOnReadinessNoCouplingProofAccumulatedSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase2RequiredShellHoldDuration;
	OutSnapshot.RootOnReadinessNoCouplingPeakBodyLinearSpeed = this->RootOnReadinessNoCouplingPeakBodyLinearSpeed;
	OutSnapshot.RootOnReadinessNoCouplingPeakBodyAngularSpeed = this->RootOnReadinessNoCouplingPeakBodyAngularSpeed;
	OutSnapshot.RootOnReadinessNoCouplingWorstBone = this->RootOnReadinessNoCouplingWorstBone;
	OutSnapshot.bRootOnReadinessTiltLimitedByUprightness =
		IsPhase1TiltLimitedRootOnViability(Owner->LastPhase1PelvisCouplingRotationForensics, Settings);
	OutSnapshot.RootOnReadinessUnconstrainedTiltDeg = Owner->LastPhase1PelvisCouplingRotationForensics.UnconstrainedTiltDeg;
	OutSnapshot.RootOnReadinessUnconstrainedPelvisThighLAngularErrorDeg =
		Owner->LastPhase1PelvisCouplingRotationForensics.UnconstrainedLeftThighAngularErrorDeg;
	OutSnapshot.RootOnReadinessUnconstrainedPelvisThighRAngularErrorDeg =
		Owner->LastPhase1PelvisCouplingRotationForensics.UnconstrainedRightThighAngularErrorDeg;
	OutSnapshot.RootOnReadinessUnconstrainedPelvisSpine01AngularErrorDeg =
		Owner->LastPhase1PelvisCouplingRotationForensics.UnconstrainedSpineAngularErrorDeg;
	OutSnapshot.ShellOffsetDeltaAtCaptureCm = CachedConvergenceSnapshot.IsValid() ? CachedConvergenceSnapshot.ShellPlanarOffset : Owner->GetCurrentShellPlanarOffsetDeltaCm();
	OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond = CachedConvergenceSnapshot.IsValid() ? CachedConvergenceSnapshot.ShellPlanarVelocity : Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	OutSnapshot.ShellOffsetGrowthCm = bHasRootOnReadinessShellProofBaseline
		? FMath::Max(0.0f, OutSnapshot.ShellOffsetDeltaAtCaptureCm - RootOnReadinessShellProofStartOffsetCm)
		: 0.0f;
	OutSnapshot.ShellVelocityGrowthCmPerSecond = bHasRootOnReadinessShellProofBaseline
		? FMath::Max(0.0f, OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond - RootOnReadinessShellProofStartVelocityCmPerSecond)
		: 0.0f;

	OutSnapshot.bShellCorrectionOwnerActive = Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle;
	OutSnapshot.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame =
		Owner->WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame();
	OutSnapshot.bTransitionOwnedShellLocked = Owner->HasExplicitTransitionOwnedShellLock();
	OutSnapshot.bTransitionOwnedShellLocked = Owner->HasExplicitTransitionOwnedShellLock();
	
	const bool bExplicitReanchored = Owner->WasTransitionShellReferenceReanchored();
	
	if (GStrictPhase1Certification != 0)
	{
		OutSnapshot.bTransitionShellReferenceReanchored = bExplicitReanchored;
	}
	else
	{
		const bool bShellLockedAndIdle = OutSnapshot.bTransitionOwnedShellLocked && Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle;
		if (bShellLockedAndIdle && !bExplicitReanchored)
		{
			Audit.bUsedReanchorShortcut = true;
		}
		OutSnapshot.bTransitionShellReferenceReanchored = bExplicitReanchored || bShellLockedAndIdle;
	}

	OutSnapshot.bTransitionShellReferenceReseededAfterLock = Owner->WasTransitionShellReferenceReseededAfterLock();

	// Populate Logical Result
	OutResult.bLateValidationCompleted = bHasLateValidationProof;
	OutResult.bRootOnReadinessShellHoldSatisfied =
		OutSnapshot.RootOnReadinessShellHoldDurationSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase2RequiredShellHoldDuration;
	OutResult.bRootOnReadinessUpperOnlyShellHoldCappedByWindow =
		OutSnapshot.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold &&
		OutResult.bLateValidationCompleted &&
		!OutResult.bRootOnReadinessShellHoldSatisfied;
	OutResult.bRootOnReadinessFinalBringUpControlSettled =
		OutSnapshot.FinalBringUpGroupControlAuthorityAlpha >= 1.0f - KINDA_SMALL_NUMBER;
	OutResult.bRootOnReadinessPolicyInfluenceSettled =
		OutSnapshot.RootOnReadinessPolicyInfluenceDurationSeconds + KINDA_SMALL_NUMBER >=
		OutSnapshot.RootOnReadinessPolicyInfluenceRequiredSeconds;
	const float SignificantShellOffsetThresholdCm = 0.1f;
	const float SignificantShellVelocityThresholdCmPerSecond = 1.0f;
	
	bool bShellCorrectionActivelyAffecting = false;
	if (GStrictPhase1Certification != 0)
	{
		bShellCorrectionActivelyAffecting = OutSnapshot.bShellCorrectionOwnerActive && 
			(OutSnapshot.ShellOffsetDeltaAtCaptureCm > UE_SMALL_NUMBER || 
			 OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > UE_SMALL_NUMBER);
	}
	else
	{
		bShellCorrectionActivelyAffecting = OutSnapshot.bShellCorrectionOwnerActive && 
			(OutSnapshot.ShellOffsetDeltaAtCaptureCm > SignificantShellOffsetThresholdCm || 
			 OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > SignificantShellVelocityThresholdCmPerSecond);
		
		if (OutSnapshot.bShellCorrectionOwnerActive && !bShellCorrectionActivelyAffecting)
		{
			if (OutSnapshot.ShellOffsetDeltaAtCaptureCm > UE_SMALL_NUMBER || 
				OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > UE_SMALL_NUMBER)
			{
				Audit.bUsedShellOwnershipNarrowing = true;
			}
		}
	}

	if (Owner->bShellCorrectionStateLogged == false || GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_SHELL_CORRECTION_STATE active=%d influencingMetrics=%d staleLatch=%d"),
			OutSnapshot.bShellCorrectionOwnerActive ? 1 : 0,
			bShellCorrectionActivelyAffecting ? 1 : 0,
			(OutSnapshot.bShellCorrectionOwnerActive && !bShellCorrectionActivelyAffecting) ? 1 : 0);
		Owner->bShellCorrectionStateLogged = true;
	}

		const bool bBypassReanchor = !OutSnapshot.bShellCorrectionOwnerActive && 
			!bShellCorrectionActivelyAffecting &&
			OutSnapshot.ShellOffsetDeltaAtCaptureCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
			OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
			OutSnapshot.ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
			OutSnapshot.ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond;

		const bool bReanchorSatisfied = OutSnapshot.bTransitionShellReferenceReanchored || bBypassReanchor;

		if (bBypassReanchor && !OutSnapshot.bTransitionShellReferenceReanchored)
		{
			if (Owner->bShellCorrectionStateLogged == false)
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_SHELL_REANCHOR_BYPASSED frame=%llu phase=%d shellCorrectionActive=%d activelyAffecting=%d offset=%.2f velocity=%.2f offsetGrowth=%.2f velocityGrowth=%.2f locked=%d reanchored=%d owner=%d actor=%s component=%s"),
					GFrameCounter, static_cast<int32>(InternalPhase),
					OutSnapshot.bShellCorrectionOwnerActive ? 1 : 0, bShellCorrectionActivelyAffecting ? 1 : 0,
					OutSnapshot.ShellOffsetDeltaAtCaptureCm, OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond,
					OutSnapshot.ShellOffsetGrowthCm, OutSnapshot.ShellVelocityGrowthCmPerSecond,
					OutSnapshot.bTransitionOwnedShellLocked ? 1 : 0, OutSnapshot.bTransitionShellReferenceReanchored ? 1 : 0,
					static_cast<int32>(EBalanceReadyConditionOwner::ShellAuthorityTransfer),
					*Owner->GetOwner()->GetName(), *Owner->GetName());
				Owner->bShellCorrectionStateLogged = true;
			}
		}

	const bool bShellSafetySatisfied = 
		(OutSnapshot.RootOnReadinessShellProofDurationSeconds + KINDA_SMALL_NUMBER >=
			Settings.BalancePhase2PreRootOnShellProofRequiredSeconds || bBypassReanchor) &&
		OutSnapshot.ShellOffsetDeltaAtCaptureCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
		OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
		OutSnapshot.ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
		OutSnapshot.ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond &&
		!bShellCorrectionActivelyAffecting &&
		OutSnapshot.bTransitionOwnedShellLocked &&
		bReanchorSatisfied &&
		!OutSnapshot.bTransitionShellReferenceReseededAfterLock;

	if (Owner->bShellCorrectionStateLogged == false || GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_SHELL_SAFETY_DEBUG proof=%d complete=%d locked=%d reanchored=%d duration=%.2f/%.2f offset=%.2f/%.2f growth=%.2f/%.2f affecting=%d"),
			bShellSafetySatisfied ? 1 : 0, OutResult.bLateValidationCompleted ? 1 : 0, 
			OutSnapshot.bTransitionOwnedShellLocked ? 1 : 0, OutSnapshot.bTransitionShellReferenceReanchored ? 1 : 0, 
			OutSnapshot.RootOnReadinessShellProofDurationSeconds, Settings.BalancePhase2PreRootOnShellProofRequiredSeconds,
			OutSnapshot.ShellOffsetDeltaAtCaptureCm, Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm,
			OutSnapshot.ShellOffsetGrowthCm, Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm,
			bShellCorrectionActivelyAffecting ? 1 : 0);
		Owner->bShellCorrectionStateLogged = true;
	}

	OutResult.bPreRootOnShellSafetyProofSatisfied = bShellSafetySatisfied;
	OutResult.bRootOnReadinessNoCouplingProofSatisfied = OutSnapshot.bRootOnReadinessNoCouplingProofSatisfied;
	OutResult.bRootOnDirectPelvisLinkGeometrySatisfied = OutSnapshot.bRootOnDirectPelvisLinkGeometrySatisfied;
	OutResult.bRootOnDirectPelvisLinkAngularSatisfied = OutSnapshot.bRootOnDirectPelvisLinkAngularSatisfied;
	OutResult.PelvisThighLErrorCm = OutSnapshot.PelvisThighLErrorCm;
	OutResult.PelvisThighRErrorCm = OutSnapshot.PelvisThighRErrorCm;
	OutResult.PelvisSpine01ErrorCm = OutSnapshot.PelvisSpine01ErrorCm;
	OutResult.PelvisThighLAngularErrorDeg = OutSnapshot.PelvisThighLAngularErrorDeg;
	OutResult.PelvisThighRAngularErrorDeg = OutSnapshot.PelvisThighRAngularErrorDeg;
	OutResult.PelvisSpine01AngularErrorDeg = OutSnapshot.PelvisSpine01AngularErrorDeg;

	OutResult.bRootOnReadinessProven =
		OutResult.bRootOnReadinessShellHoldSatisfied &&
		OutResult.bRootOnReadinessFinalBringUpControlSettled &&
		OutResult.bRootOnReadinessPolicyInfluenceSettled &&
		OutResult.bPreRootOnShellSafetyProofSatisfied &&
		OutResult.bRootOnReadinessNoCouplingProofSatisfied &&
		OutResult.bRootOnDirectPelvisLinkAngularSatisfied &&
		OutResult.bRootOnDirectPelvisLinkGeometrySatisfied &&
		bDirectPelvisThighMarginsSatisfied &&
		bDirectPelvisSpineMarginSatisfied &&
		(RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady);

	OutResult.RootOnReadinessClassification = RootOnReadinessClassification;
	OutResult.RootOnReadinessGateReason = ResolveRootOnReadinessGateReason(
		RootOnReadinessClassification,
		bDirectPelvisLinkPositionSatisfied,
		OutResult.bRootOnDirectPelvisLinkAngularSatisfied,
		bDirectPelvisThighMarginsSatisfied,
		bDirectPelvisSpineMarginSatisfied,
		OutResult.bRootOnReadinessShellHoldSatisfied,
		OutResult.bRootOnReadinessFinalBringUpControlSettled,
		OutResult.bRootOnReadinessPolicyInfluenceSettled,
		OutResult.bPreRootOnShellSafetyProofSatisfied,
		OutResult.bRootOnReadinessNoCouplingProofSatisfied,
		OutSnapshot.bRootOnReadinessTiltLimitedByUprightness,
		OutSnapshot.PolicyInfluenceAlphaAtCapture);

	OutResult.Outcome = 
		(OutResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady && OutResult.bRootOnDirectPelvisLinkGeometrySatisfied)
			? EBalanceLateValidationOutcome::Outcome_AcceptRootOn
			: (OutResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
				? EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly
				: EBalanceLateValidationOutcome::Outcome_Pending);

	if (OutResult.Outcome == EBalanceLateValidationOutcome::Outcome_AcceptRootOn)
	{
		Audit.bUsedRelaxedCertification = Audit.bUsedTimeoutExtension || Audit.bUsedDwellShortcut || Audit.bUsedReanchorShortcut || Audit.bUsedShellOwnershipNarrowing;
		Audit.Topology = OutSnapshot.TopologyClass;
		Audit.ProximalSimCount = OutSnapshot.ProximalSimCount;
		Audit.DistalSimCount = OutSnapshot.DistalSimCount;
		Audit.UpperBodySimCount = OutSnapshot.UpperBodySimCount;
		Audit.bShellReanchored = OutSnapshot.bTransitionShellReferenceReanchored;
		Audit.bShellLocked = OutSnapshot.bTransitionOwnedShellLocked;
		Audit.ShellOffset = OutSnapshot.ShellOffsetDeltaAtCaptureCm;
		Audit.ShellVelocity = OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond;
	}
	else
	{
		Audit.bUsedRelaxedCertification = Audit.bUsedTimeoutExtension || Audit.bUsedDwellShortcut || Audit.bUsedReanchorShortcut || Audit.bUsedShellOwnershipNarrowing;
		Audit.Topology = OutSnapshot.TopologyClass;
		Audit.ProximalSimCount = OutSnapshot.ProximalSimCount;
		Audit.DistalSimCount = OutSnapshot.DistalSimCount;
		Audit.UpperBodySimCount = OutSnapshot.UpperBodySimCount;
		Audit.bShellReanchored = OutSnapshot.bTransitionShellReferenceReanchored;
		Audit.bShellLocked = OutSnapshot.bTransitionOwnedShellLocked;
		Audit.ShellOffset = OutSnapshot.ShellOffsetDeltaAtCaptureCm;
		Audit.ShellVelocity = OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond;
	}

	OutResult.MaxTargetDeltaDegrees = ControlTargetDiagnostics.MaxTargetDeltaDegrees;
	OutResult.MeanTargetDeltaDegrees = ControlTargetDiagnostics.MeanTargetDeltaDegrees;
	OutResult.QuietProofDurationSeconds = QuietWindowAccumulatedSeconds;
	OutResult.LateValidationSustainDurationSeconds = LateValidationAccumulatedSeconds;

	return true;
}


bool FPhysAnimBalanceReadyTransition::CaptureCertifiedHandoff(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, false))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	CertifiedHandoff = CurrentSnapshot;
	CertifiedLateValidationResult = CurrentResult;
	bHasCertifiedHandoff = true;

	if (GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_CERTIFICATION_AUDIT STRICT_AUDIT_V1 usedRelaxedCertification=%d usedTimeoutExtension=%d usedDwellShortcut=%d usedReanchorShortcut=%d usedShellOwnershipNarrowing=%d topology=%s simCount=%d proximalSimCount=%d distalSimCount=%d upperBodySimCount=%d shellReanchored=%d shellLocked=%d shellOffset=%.2f shellVelocity=%.2f"),
			Audit.bUsedRelaxedCertification ? 1 : 0,
			Audit.bUsedTimeoutExtension ? 1 : 0,
			Audit.bUsedDwellShortcut ? 1 : 0,
			Audit.bUsedReanchorShortcut ? 1 : 0,
			Audit.bUsedShellOwnershipNarrowing ? 1 : 0,
			*Audit.Topology,
			CertifiedHandoff.SimCount,
			Audit.ProximalSimCount,
			Audit.DistalSimCount,
			Audit.UpperBodySimCount,
			Audit.bShellReanchored ? 1 : 0,
			Audit.bShellLocked ? 1 : 0,
			Audit.ShellOffset,
			Audit.ShellVelocity);
	}

	OutReason.Reset();
	return true;
}


bool FPhysAnimBalanceReadyTransition::CaptureLateValidationBaseline(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, false))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationBaselineSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	CertifiedHandoff = CurrentSnapshot;
	CertifiedLateValidationResult = CurrentResult;
	bHasCertifiedHandoff = true;
	OutReason.Reset();
	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidateCertifiedHandoff(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const
{
	if (!bHasCertifiedHandoff)
	{
		OutReason = TEXT("phase2_missing_handoff_payload");
		return false;
	}

	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (CurrentSnapshot.TopologyClass != CertifiedHandoff.TopologyClass)
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	if (!ValidateRootOnReadinessSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	return true;
}



