#pragma once

#include "CoreMinimal.h"
#include "PhysAnimBridge.h"

enum class EBalanceReadyTransitionPhase : uint8
{
	BRT_Inactive,
	BRT_Phase1_Prepare,
	BRT_Phase1_LateValidate,
	BRT_Phase2_RootOn,
	BRT_Phase3_Settle,
	BRT_Succeeded,
	BRT_Failed,
	BRT_SafeDenied
};

enum class EBalanceReadyEntryClassification : uint8
{
	Preflight_Accept,
	Preflight_QueueBlock,
	Preflight_HardFailure
};

enum class EBalanceReadyUpperBodyOwnershipMode : uint8
{
	None,
	LateValidationKinematicHold
};

enum class EBalanceReadyGroupOwnershipMode : uint8
{
	Kinematic,
	Simulating
};

enum class EBalanceReadyRootOnReadinessClassification : uint8
{
	NotReady,
	RootCoupledReady,
	UpperOnlySafeDeny
};

enum class EBalanceLateValidationOutcome : uint8
{
	Outcome_Pending,
	Outcome_AcceptRootOn,
	Outcome_SafeDenyUpperOnly
};

enum class EBalanceReadyConditionOwner : uint8
{
	None,
	ExternalBridgeBringUp,
	ExternalPolicyRamp,
	Phase1TopologyShaping,
	Phase1PolicyRouting,
	Phase1UpperBodyOwnership,
	Phase1ResetSuppression,
	ShellAuthorityTransfer,
	ShellAuthorityMaintenance,
	Phase2RootOnExecution,
	Phase2TopologyEnforcement,
	TransitionRecovery
};

struct FPhysAnimLateValidationResult
{
	EBalanceLateValidationOutcome Outcome = EBalanceLateValidationOutcome::Outcome_Pending;
	EBalanceReadyRootOnReadinessClassification RootOnReadinessClassification = EBalanceReadyRootOnReadinessClassification::NotReady;
	bool bRootOnReadinessProven = false;
	bool bLateValidationCompleted = false;
	bool bRootOnReadinessShellHoldSatisfied = false;
	bool bRootOnReadinessUpperOnlyShellHoldCappedByWindow = false;
	bool bRootOnReadinessFinalBringUpControlSettled = false;
	bool bRootOnReadinessPolicyInfluenceSettled = false;
	bool bPreRootOnShellSafetyProofSatisfied = false;
	float MaxTargetDeltaDegrees = 0.0f;
	float MeanTargetDeltaDegrees = 0.0f;
	float QuietProofDurationSeconds = 0.0f;
	float LateValidationSustainDurationSeconds = 0.0f;
};

struct FPhysAnimCertifiedHandoffSnapshot
{
	FString TopologyClass;
	FString ShellAuthorityMode;
	int32 SimCount = 0;
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperBodySimCount = 0;
	EBalanceReadyGroupOwnershipMode RootOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyGroupOwnershipMode ProximalOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyGroupOwnershipMode DistalOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyUpperBodyOwnershipMode UpperBodyOwnershipMode = EBalanceReadyUpperBodyOwnershipMode::None;
	bool bPolicySuppressed = false;
	bool bResetsSuppressed = false;
	bool bControlAuthoritySettled = false;
	float FinalBringUpGroupControlAuthorityAlpha = 0.0f;
	float PolicyInfluenceAlphaAtCapture = 0.0f;
	float RootOnReadinessPolicyInfluenceRequiredAlpha = 0.0f;
	float RootOnReadinessPolicyInfluenceDurationSeconds = 0.0f;
	float RootOnReadinessPolicyInfluenceRequiredSeconds = 0.0f;
	float RootOnReadinessShellHoldDurationSeconds = 0.0f;
	float RootOnReadinessShellHoldRequiredSeconds = 0.0f;
	float RootOnReadinessShellProofDurationSeconds = 0.0f;
	float ShellOffsetDeltaAtCaptureCm = 0.0f;
	float ShellVelocityDeltaAtCaptureCmPerSecond = 0.0f;
	float ShellOffsetGrowthCm = 0.0f;
	float ShellVelocityGrowthCmPerSecond = 0.0f;
	bool bShellCorrectionOwnerActive = false;
	bool bTransitionOwnedShellLocked = false;
	bool bTransitionShellReferenceReanchored = false;
	bool bTransitionShellReferenceReseededAfterLock = false;
	bool bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = false;
};

struct FPhysAnimPhase1TopologySnapshot
{
	EBalanceReadyGroupOwnershipMode RootOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyGroupOwnershipMode ProximalOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyGroupOwnershipMode DistalOwnershipMode = EBalanceReadyGroupOwnershipMode::Kinematic;
	EBalanceReadyUpperBodyOwnershipMode UpperBodyOwnershipMode = EBalanceReadyUpperBodyOwnershipMode::None;

	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperBodySimCount = 0;
	int32 TotalSimCount = 0;

	bool bRootSimulating = false;
	bool bPolicySuppressed = false;
	bool bResetsSuppressed = false;
};

struct FPhase1AcceptedConvergenceSnapshot
{
	int64 FrameIndex = -1;
	double WorldTimeSeconds = 0.0;
	float MaxBodyLinearSpeed = 0.0f;
	float MaxBodyAngularSpeed = 0.0f;
	float RootLinearSpeed = 0.0f;
	float RootAngularSpeed = 0.0f;
	float RootTilt = 0.0f;
	float ShellPlanarOffset = 0.0f;
	float ShellPlanarVelocity = 0.0f;
	bool bIsInstabilityPrecursorActive = false;
	bool bHasPendingResets = false;
	float MaxTargetDeltaDegrees = 0.0f;
	float MeanTargetDeltaDegrees = 0.0f;
	bool bIsPelvisSimulating = false;
	FName MaxBodyLinearSpeedBone = NAME_None;
	FName MaxBodyAngularSpeedBone = NAME_None;


	bool IsValid() const { return FrameIndex >= 0; }
};

struct FBalanceReadyTransitionDiagnostics
{
	FString BlockReason;
	FString FailureReason;
	FString LastRetryDecision;
	FString Phase1TargetDiscontinuityGateSource;
	FString Phase1TargetDiscontinuityGateReason;
	FString Phase1LateValidateGateSource;
	FString Phase1LateValidateGateReason;
	FString Phase1RootOnReadinessGateReason;
	FString Phase2ShellProofGateReason;
	float RootSpeed = 0.0f;
	float RootAngularSpeed = 0.0f;
	float RootTilt = 0.0f;
	float ShellMetric = 0.0f;
	bool bSimFlipped = false;
	FTransform PelvisTransformDelta;
	FVector PelvisLinearVelPre = FVector::ZeroVector;
	FVector PelvisLinearVelPost = FVector::ZeroVector;
	FVector PelvisAngularVelPre = FVector::ZeroVector;
	FVector PelvisAngularVelPost = FVector::ZeroVector;
	
	bool bShellContributed = false;
	bool bPolicyWroteTargets = false;
	bool bResetScheduled = false;
	bool bResetApplied = false;
	bool bResetDrained = false;

	float MaxLinVelPelvis = 0.0f;
	float MaxAngVelPelvis = 0.0f;
	float MaxLinVelThighs = 0.0f;
	float MaxAngVelThighs = 0.0f;
	float MaxLinVelSpine = 0.0f;
	float MaxAngVelSpine = 0.0f;
	float MaxLinVelFeet = 0.0f;
	float MaxAngVelFeet = 0.0f;

	float PreflightPolicyAlpha = 0.0f;
	
	float BaselineRootLinVel = 0.0f;
	float BaselineRootAngVel = 0.0f;
	float BaselineShellOffset = 0.0f;
	float BaselineShellVel = 0.0f;
	FPhysAnimControlTargetDiagnostics Phase1TargetDiscontinuityGateInput;
	float Phase1TargetDiscontinuityAccumulatedSeconds = 0.0f;
	float Phase1LateValidateAccumulatedSeconds = 0.0f;
	float Phase1LateValidateBodyMotionViolationAccumulatedSeconds = 0.0f;
	FName Phase1LateValidateWorstLinearSpeedBone = NAME_None;
	FName Phase1LateValidateWorstAngularSpeedBone = NAME_None;
	float Phase1LateValidateWorstLinearSpeed = 0.0f;
	float Phase1LateValidateWorstAngularSpeed = 0.0f;
	int32 SimCountPre = 0;
	int32 SimCountPost = 0;
	int32 DistalSimCountPre = 0;
	int32 DistalSimCountPost = 0;
	int32 UpperBodySimCountPre = 0;
	int32 UpperBodySimCountPost = 0;
	float PeakMaxBodyLinearSpeed = 0.0f;
	float PeakMaxBodyAngularSpeed = 0.0f;
};

class FPhysAnimBalanceReadyTransition
{
public:
	void Start(const FString& InRequestReason, class UPhysAnimComponent* Owner);
	void Cancel(class UPhysAnimComponent* Owner);
	void Tick(float DeltaTime, class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings);

	void PushConvergenceSnapshot(const FPhase1AcceptedConvergenceSnapshot& Snapshot) { CachedConvergenceSnapshot = Snapshot; }

	bool IsActive() const { return InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive && !IsComplete(); }
	bool HasSucceeded() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded; }
	bool HasFailed() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed; }
	bool HasSafeDenied() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied; }
	bool IsComplete() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded || InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed || InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied; }

	TMap<FName, int32> DistalBoneMismatchTicks;
	TMap<FName, int32> DistalBoneConsecutiveMismatchTicks;
	TMap<FName, int32> DistalBonePersistentTicks;
	int32 DistalMismatchesTransientCount = 0;
	int32 DistalMismatchesPersistentCount = 0;
	int32 DistalMismatchesPendingCount = 0;

	/** Returns true if the transition has been started and is either running or has completed. */
	bool HasAnyInternalPhase() const { return InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive; }

	/** Returns true if the transition is currently executing logic (not inactive and not complete). */
	bool HasActuallyStarted() const { return IsActive(); }
	bool HasSafePhase2Denial() const { return HasSafeDenied(); }
	const FString& GetSafePhase2DenialReason() const { return SafePhase2DenialReason; }

	EBalanceReadyTransitionPhase GetPhase() const { return InternalPhase; }
	const FString& GetBlockReason() const { return Diagnostics.BlockReason; }
	const FString& GetFailureReason() const { return Diagnostics.FailureReason; }
	const TMap<FName, FQuat>& GetEntryHoldRotations() const { return EntryHoldRotations; }

	bool ShouldSuppressPolicy() const;
	bool ShouldSuppressShell() const;
	bool ShouldSuppressPerturbations() const;
	bool ShouldSuppressResets() const;
	bool ShouldSuppressMoveSmoke() const;
	bool IsPhase2RootAuthorityQuarantined() const { return bPhase2RootAuthorityQuarantined; }

	float GetRootBodyModifierSoftSimAlpha() const;
	float GetProximalControlSoftAlpha(FName BoneName) const;
	bool ShouldKeepBoneKinematic(FName BoneName, const struct FPhysAnimStabilizationSettings& Settings) const;
	bool ShouldSuppressPolicyWrites(FName BoneName) const;
	float GetTransitionExtraDampingMultiplier(const struct FPhysAnimStabilizationSettings& Settings) const;
	EBalanceReadyEntryClassification ClassifyEntryState(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings) const;
	static FString ClassifyLateValidationFailureReason(bool bUpperBodyInstability, bool bSimCoverageRegressed, bool bTargetDiscontinuity);
	static bool IsFailureClassRetryable(const FString& FailureReason);
	static EBalanceReadyConditionOwner ClassifyConditionOwner(const FString& Reason);
	static bool IsPhase1OwnedCondition(const FString& Reason);
	static bool IsAutomaticRetryAllowed(
		const FString& FailureReason,
		bool bRecoveryCompleted,
		bool bRecoveryChangedMaterialState,
		bool bFreshQuietProofOccurred,
		bool bCooldownElapsed,
		bool bRetryBudgetAvailable);

private:
	void SetPhase(EBalanceReadyTransitionPhase NewPhase, class UPhysAnimComponent* Owner = nullptr);
	bool EvaluateReadiness(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	bool ValidatePhase2EntryPreconditions(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	bool ValidatePhase3Continuity(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	bool BuildCertifiedHandoffSnapshot(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FPhysAnimCertifiedHandoffSnapshot& OutSnapshot, FPhysAnimLateValidationResult& OutResult, bool bUseFrozenTopology = true) const;
	bool CaptureLateValidationBaseline(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	bool CaptureCertifiedHandoff(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	bool ValidateCertifiedHandoff(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason) const;
	bool ValidateLateValidationHandoffSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const;
	bool ValidateRootOnReadinessSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const;
	bool ValidatePreRootOnShellSafetyProofSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const;
	static bool ValidateLateValidationBaselineSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	static FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount);
	void ReturnToPhase1Prepare(class UPhysAnimComponent* Owner, const FString& Reason, const TCHAR* EventName);
	void CapturePhase1TopologyRecord(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings);
	void ResetTransitionLocalState();
	void ResetCertifiedHandoffState();
	void MarkSafePhase2Denied(class UPhysAnimComponent* Owner, const FString& Reason);
	void CaptureFlipDiagnostics(class UPhysAnimComponent* Owner);

	EBalanceReadyTransitionPhase InternalPhase = EBalanceReadyTransitionPhase::BRT_Inactive;
	FString RequestReason;
	float StableHoldAccumulatedSeconds = 0.0f;
	float PhaseTimeSeconds = 0.0f;
	float TotalTransitionTimeSeconds = 0.0f;
	float TargetDiscontinuityAccumulatedSeconds = 0.0f;
	FString LastQuietBlockReason;

	bool bLastRootSimulating = false;
	bool bLastPendingResetsEmpty = true;
	bool bLatchedPelvisResetApplied = false;

	int32 QuietHandoffCount = 0;
	float QuietWindowAccumulatedSeconds = 0.0f;
	int32 ConsecutivePelvisNotSimulatingTicks = 0;
	int32 ConsecutiveBodyMotionInstabilityTicks = 0;
	float LateValidationAccumulatedSeconds = 0.0f;
	float RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
	float RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
	float RootOnReadinessShellProofStartOffsetCm = 0.0f;
	float RootOnReadinessShellProofStartVelocityCmPerSecond = 0.0f;
	bool bHasRootOnReadinessShellProofBaseline = false;
	float HipQuarantineTimerSeconds = 0.0f;
	FString LastLateValidateBlockReason;
	int32 RetryCount = 0;
	int32 Phase2RetryCount = 0;
	float RetryCooldownTimerSeconds = 0.0f;
	int32 Phase2GuardTickCount = 0;
	bool bPhase2RootAuthorityQuarantined = false;
	TMap<FName, FQuat> EntryHoldRotations;
	bool bHasCertifiedHandoff = false;
	bool bHasLateValidationProof = false;
	bool bLateValidationProofPassed = false;
	FPhysAnimCertifiedHandoffSnapshot CertifiedHandoff;
	FPhysAnimLateValidationResult CertifiedLateValidationResult;
	FPhysAnimPhase1TopologySnapshot Phase1TopologyRecord;
	bool bHasPhase1TopologyRecord = false;
	bool bHasLoggedDistalExperimentState = false;
	FString SafePhase2DenialReason;

	FPhase1AcceptedConvergenceSnapshot CachedConvergenceSnapshot;

	FBalanceReadyTransitionDiagnostics Diagnostics;
	double LastLogTimeSeconds = -1.0;
};
