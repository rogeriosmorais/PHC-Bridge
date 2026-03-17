#pragma once

#include "CoreMinimal.h"

class UPhysAnimComponent;
struct FPhysAnimStabilizationSettings;

/**
 * Quiet handoff controller used before entering Balance Perturbation Mode.
 *
 * Goal:
 * - stop doing a hot one-frame pelvis/root sim flip inside live BridgeActive
 * - suspend policy / shell / locomotion-side effects first
 * - flip root sim in a quiet phase
 * - wait until root + shell metrics settle
 * - only then mark the transition as succeeded
 */
class FPhysAnimBalanceQuietHandoff
{
public:
	enum class EPhase : uint8
	{
		Inactive = 0,
		SuspendExternalSystems,
		FlipRootSimulation,
		WaitForQuiet,
		RestorePolicy,
		Succeeded,
		Failed,
	};

	void Start(const FString& InReason, UPhysAnimComponent* InOwner);
	void Cancel();
	void Tick(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, UPhysAnimComponent* Owner);

	bool IsActive() const;
	bool HasSucceeded() const;
	bool HasFailed() const;
	EPhase GetPhase() const;
	const FString& GetStartReason() const;

private:
	void SetPhase(EPhase NewPhase, UPhysAnimComponent* Owner, const TCHAR* Context);
	bool TickSuspendExternalSystems(UPhysAnimComponent* Owner);
	bool TickFlipRootSimulation(UPhysAnimComponent* Owner);
	bool TickWaitForQuiet(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, UPhysAnimComponent* Owner);
	bool TickRestorePolicy(float DeltaTime, UPhysAnimComponent* Owner);

private:
	EPhase Phase = EPhase::Inactive;
	FString StartReason;
	double PhaseStartTimeSeconds = -1.0;
	double LastProgressLogTimeSeconds = -1.0;
	double QuietAccumulatedSeconds = 0.0;
	bool bSystemsSuspended = false;
	bool bRootFlipRequested = false;
	bool bPolicyRestoreStarted = false;
};
