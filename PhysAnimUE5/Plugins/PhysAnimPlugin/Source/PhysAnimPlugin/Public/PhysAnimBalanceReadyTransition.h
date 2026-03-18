#pragma once

#include "CoreMinimal.h"
#include "PhysAnimBridge.h"

enum class EBalanceReadyTransitionPhase : uint8
{
	BRT_Inactive,
	BRT_Phase1_Prepare,
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

struct FBalanceReadyTransitionDiagnostics
{
	FString BlockReason;
	FString FailureReason;
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
	bool IsComplete() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded || InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed; }
	bool HasActuallyStarted() const { return InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive; }

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

private:
	void SetPhase(EBalanceReadyTransitionPhase NewPhase, class UPhysAnimComponent* Owner = nullptr);
	bool EvaluateReadiness(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason);
	void CaptureFlipDiagnostics(class UPhysAnimComponent* Owner);

	EBalanceReadyTransitionPhase InternalPhase = EBalanceReadyTransitionPhase::BRT_Inactive;
	FString RequestReason;
	float StableHoldAccumulatedSeconds = 0.0f;
	float PhaseTimeSeconds = 0.0f;
	float TotalTransitionTimeSeconds = 0.0f;

	bool bLastRootSimulating = false;
	bool bLastPendingResetsEmpty = true;
	bool bLatchedPelvisResetApplied = false;

	int32 QuietHandoffCount = 0;
	float QuietWindowAccumulatedSeconds = 0.0f;
	float HipQuarantineTimerSeconds = 0.0f;
	int32 RetryCount = 0;
	TMap<FName, FQuat> EntryHoldRotations;

	FBalanceReadyTransitionDiagnostics Diagnostics;
	double LastLogTimeSeconds = -1.0;
};
