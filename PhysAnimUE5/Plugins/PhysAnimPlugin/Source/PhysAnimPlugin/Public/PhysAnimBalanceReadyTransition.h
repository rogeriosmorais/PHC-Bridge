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
	BRT_Failed
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

struct FPhysAnimCertifiedHandoffSnapshot
{
	FString TopologyClass;
	int32 SimCount = 0;
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperBodySimCount = 0;
	EBalanceReadyUpperBodyOwnershipMode UpperBodyOwnershipMode = EBalanceReadyUpperBodyOwnershipMode::None;
	bool bPolicySuppressed = false;
	bool bControlAuthoritySettled = false;
	float MaxTargetDeltaDegrees = 0.0f;
	float MeanTargetDeltaDegrees = 0.0f;
	float QuietProofDurationSeconds = 0.0f;
	float LateValidationSustainDurationSeconds = 0.0f;
	bool bLateValidationCompleted = false;
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
	void Cancel();
	void Tick(float DeltaTime, class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings);

	bool IsActive() const { return InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive && !IsComplete(); }
	bool HasSucceeded() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded; }
	bool HasFailed() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed; }
	bool IsComplete() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded || InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed || bSafePhase2Denied; }
	bool HasActuallyStarted() const { return InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive; }
	bool HasSafePhase2Denial() const { return bSafePhase2Denied; }
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

	float GetRootBodyModifierSoftSimAlpha() const;
	float GetProximalControlSoftAlpha(FName BoneName) const;
	bool ShouldKeepBoneKinematic(FName BoneName) const;
	bool ShouldSuppressPolicyWrites(FName BoneName) const;
	float GetTransitionExtraDampingMultiplier(const struct FPhysAnimStabilizationSettings& Settings) const;
	EBalanceReadyEntryClassification ClassifyEntryState(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings) const;
	static FString ClassifyLateValidationFailureReason(bool bUpperBodyInstability, bool bSimCoverageRegressed, bool bTargetDiscontinuity);
	static bool IsFailureClassRetryable(const FString& FailureReason);
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
	bool BuildCertifiedHandoffSnapshot(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FPhysAnimCertifiedHandoffSnapshot& OutSnapshot) const;
	bool CaptureCertifiedHandoff(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings);
	bool ValidateCertifiedHandoff(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason) const;
	static FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount);
	void ResetTransitionLocalState();
	void ResetCertifiedHandoffState();
	void MarkSafePhase2Denied(const FString& Reason);
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
	float LateValidationAccumulatedSeconds = 0.0f;
	float HipQuarantineTimerSeconds = 0.0f;
	FString LastLateValidateBlockReason;
	int32 RetryCount = 0;
	int32 Phase2RetryCount = 0;
	float RetryCooldownTimerSeconds = 0.0f;
	TMap<FName, FQuat> EntryHoldRotations;
	bool bHasCertifiedHandoff = false;
	bool bHasLateValidationProof = false;
	FPhysAnimCertifiedHandoffSnapshot CertifiedHandoff;
	bool bSafePhase2Denied = false;
	FString SafePhase2DenialReason;

	FBalanceReadyTransitionDiagnostics Diagnostics;
	double LastLogTimeSeconds = -1.0;
};
