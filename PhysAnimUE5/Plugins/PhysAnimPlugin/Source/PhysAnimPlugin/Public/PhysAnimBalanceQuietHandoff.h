#pragma once

#include "CoreMinimal.h"

struct FPhysAnimStabilizationSettings;
class UPhysAnimComponent;

class FPhysAnimBalanceQuietHandoff
{
public:
	void Start(const FString& InReason, UPhysAnimComponent* Owner);
	void Tick(float DeltaTime, UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& EffectiveSettings);
	void Cancel();

	bool IsActive() const { return bActive; }
	bool HasSucceeded() const { return bSucceeded; }
	bool HasFailed() const { return bFailed; }
	const FString& GetFailureReason() const { return FailureReason; }

	bool ShouldSuppressPolicy() const { return bActive && !bSucceeded && !bFailed; }
	bool ShouldSuppressShell() const { return bActive && !bSucceeded && !bFailed; }
	bool ShouldSuppressMoveSmoke() const { return bActive && !bSucceeded && !bFailed; }

private:
	enum class EPhase : uint8
	{
		Inactive,
		RequestSim,
		WaitForSettle,
		Succeeded,
		Failed,
	};

	void Fail(const FString& Reason);

	EPhase Phase = EPhase::Inactive;
	bool bActive = false;
	bool bSucceeded = false;
	bool bFailed = false;
	FString StartReason;
	FString FailureReason;
	double ElapsedSeconds = 0.0;
	double StableSeconds = 0.0;
};
