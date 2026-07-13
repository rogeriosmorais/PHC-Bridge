#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

enum class EPhysAnimRuntimeState : uint8;

enum class EPhysAnimStandingVariant : uint8
{
	Normal,
	ZeroActions,
	DropControlDispatch,
	ControlsOff,
	DampingOnly,
	FixedNeutralTarget,
	RealOnnxPolicy
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingBodyPlan
{
	bool bSimulated = true;
	float PhysicsBlendWeight = 1.0f;
	ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	float MaxDepenetrationVelocityCmPerSec = 200.0f;
	bool bUpdateKinematicFromSimulation = false;

	bool operator==(const FPhysAnimStandingBodyPlan& Other) const;
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingControlPlan
{
	bool bEnabled = true;
	float AngularStrength = 0.0f;
	float DampingRatio = 0.0f;
	float ExtraDamping = 0.0f;

	bool operator==(const FPhysAnimStandingControlPlan& Other) const;
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingActivationPlan
{
	static constexpr int32 RequiredBodyCount = 22;
	static constexpr int32 RequiredControlCount = 21;

	TArray<FPhysAnimStandingBodyPlan> Bodies;
	TArray<FPhysAnimStandingControlPlan> Controls;

	static FPhysAnimStandingActivationPlan Build(
		EPhysAnimStandingVariant Variant,
		float AngularStrength,
		float DampingRatio,
		float ExtraDamping);
	static FPhysAnimStandingActivationPlan BuildBlended(
		EPhysAnimStandingVariant Variant,
		float LinearBlendAlpha,
		float ConfiguredAngularStrength,
		float ConfiguredDampingRatio,
		float BootstrapExtraDamping,
		float StandingExtraDamping);
	static bool UsesPolicyInference(EPhysAnimStandingVariant Variant);
	static bool UsesPolicyTargetDispatch(EPhysAnimStandingVariant Variant);
	static bool RequiresStandingHold(EPhysAnimStandingVariant Variant);

	bool operator==(const FPhysAnimStandingActivationPlan& Other) const;
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingPublicationDecision
{
	bool bWriteMovementTypes = false;
	bool bWriteControlGains = true;

	static FPhysAnimStandingPublicationDecision Build(
		bool bFullSimulationAlreadyCommitted,
		bool bPublishControlGains);
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingActivationReadback
{
	bool bFullSimulationCommitted = false;
	int32 ModifierSimulationMatchCount = 0;
	int32 RawSimulationMatchCount = 0;
	int32 ContactCorrectionMatchCount = 0;
	int32 ControlGainMatchCount = 0;
};

struct PHYSANIMPLUGIN_API FPhysAnimStandingActivationStatus
{
	EPhysAnimRuntimeState RuntimeState = static_cast<EPhysAnimRuntimeState>(0);
	float LinearBlendAlpha = 0.0f;
	bool bFullSimulationCommitted = false;
	int32 ModifierSimulationMatchCount = 0;
	int32 RawSimulationMatchCount = 0;
	int32 ContactCorrectionMatchCount = 0;
	int32 ControlGainMatchCount = 0;
	FString FailureReason;

	bool bRetryRequested = false;
	bool bKinematicDemotionRequested = false;
	bool bResetRequested = false;
	bool bBalanceSafeDenyPublished = false;
};

class PHYSANIMPLUGIN_API FPhysAnimStandingActivation
{
public:
	void Start();
	void CompletePreparation(bool bPreparationValid, const FString& FailureReason);
	void CompleteFullSimulationActivation(
		const FPhysAnimStandingActivationReadback& Readback,
		const FString& FailureReason);
	void TickPolicyBlend(
		float ElapsedSeconds,
		float StartupRampSeconds,
		const FPhysAnimStandingActivationReadback& Readback,
		const FString& FailureReason);
	void ObserveStanding(
		const FPhysAnimStandingActivationReadback& Readback,
		const FString& FailureReason);

	void SetObservationalEvidenceFlags(bool bEvidenceEnabled, bool bRendererInstrumentationEnabled);

	const FPhysAnimStandingActivationStatus& GetStatus() const { return Status; }
	float GetTargetBlendAlpha() const { return Status.LinearBlendAlpha; }
	float GetGainBlendAlpha() const { return Status.LinearBlendAlpha; }

private:
	static bool IsReadbackValid(const FPhysAnimStandingActivationReadback& Readback);
	void ApplyReadback(const FPhysAnimStandingActivationReadback& Readback);
	void Fail(const FString& FailureReason);

	FPhysAnimStandingActivationStatus Status;
};
