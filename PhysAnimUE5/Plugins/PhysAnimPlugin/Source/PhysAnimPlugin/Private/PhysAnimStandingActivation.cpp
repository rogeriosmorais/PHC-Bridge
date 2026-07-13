#include "PhysAnimStandingActivation.h"

#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"

bool FPhysAnimStandingBodyPlan::operator==(const FPhysAnimStandingBodyPlan& Other) const
{
	return bSimulated == Other.bSimulated
		&& FMath::IsNearlyEqual(PhysicsBlendWeight, Other.PhysicsBlendWeight)
		&& CollisionEnabled == Other.CollisionEnabled
		&& bUpdateKinematicFromSimulation == Other.bUpdateKinematicFromSimulation;
}

bool FPhysAnimStandingControlPlan::operator==(const FPhysAnimStandingControlPlan& Other) const
{
	return bEnabled == Other.bEnabled
		&& FMath::IsNearlyEqual(AngularStrength, Other.AngularStrength)
		&& FMath::IsNearlyEqual(DampingRatio, Other.DampingRatio)
		&& FMath::IsNearlyEqual(ExtraDamping, Other.ExtraDamping);
}

FPhysAnimStandingActivationPlan FPhysAnimStandingActivationPlan::Build(
	EPhysAnimStandingVariant Variant,
	float AngularStrength,
	float DampingRatio,
	float ExtraDamping)
{
	return BuildBlended(
		Variant,
		1.0f,
		AngularStrength,
		DampingRatio,
		ExtraDamping,
		ExtraDamping);
}

FPhysAnimStandingActivationPlan FPhysAnimStandingActivationPlan::BuildBlended(
	EPhysAnimStandingVariant Variant,
	float LinearBlendAlpha,
	float ConfiguredAngularStrength,
	float ConfiguredDampingRatio,
	float BootstrapExtraDamping,
	float StandingExtraDamping)
{
	const float Alpha = FMath::Clamp(LinearBlendAlpha, 0.0f, 1.0f);
	const bool bControlsOff = Variant == EPhysAnimStandingVariant::ControlsOff;
	const bool bDampingOnly = Variant == EPhysAnimStandingVariant::DampingOnly;
	FPhysAnimStandingActivationPlan Plan;
	Plan.Bodies.SetNum(RequiredBodyCount);
	Plan.Controls.SetNum(RequiredControlCount);
	const TArray<FName>& BodyNames = PhysAnimBridge::GetRequiredBodyModifierBoneNames();
	for (int32 BodyIndex = 0; BodyIndex < Plan.Bodies.Num() && BodyIndex < BodyNames.Num(); ++BodyIndex)
	{
		if (BodyNames[BodyIndex] == TEXT("ball_l") || BodyNames[BodyIndex] == TEXT("ball_r"))
		{
			Plan.Bodies[BodyIndex].CollisionEnabled = ECollisionEnabled::NoCollision;
		}
	}
	for (FPhysAnimStandingControlPlan& Control : Plan.Controls)
	{
		Control.bEnabled = !bControlsOff;
		Control.AngularStrength = (bControlsOff || bDampingOnly)
			? 0.0f
			: ConfiguredAngularStrength * Alpha;
		Control.DampingRatio = bControlsOff ? 0.0f : ConfiguredDampingRatio;
		Control.ExtraDamping = bControlsOff
			? 0.0f
			: FMath::Lerp(BootstrapExtraDamping, StandingExtraDamping, Alpha);
	}
	return Plan;
}

bool FPhysAnimStandingActivationPlan::UsesPolicyInference(EPhysAnimStandingVariant Variant)
{
	return Variant != EPhysAnimStandingVariant::ControlsOff &&
		Variant != EPhysAnimStandingVariant::DampingOnly &&
		Variant != EPhysAnimStandingVariant::FixedNeutralTarget;
}

bool FPhysAnimStandingActivationPlan::UsesPolicyTargetDispatch(EPhysAnimStandingVariant Variant)
{
	return Variant != EPhysAnimStandingVariant::ControlsOff &&
		Variant != EPhysAnimStandingVariant::DampingOnly &&
		Variant != EPhysAnimStandingVariant::FixedNeutralTarget;
}

bool FPhysAnimStandingActivationPlan::RequiresStandingHold(EPhysAnimStandingVariant Variant)
{
	return Variant == EPhysAnimStandingVariant::FixedNeutralTarget ||
		Variant == EPhysAnimStandingVariant::ZeroActions ||
		Variant == EPhysAnimStandingVariant::RealOnnxPolicy;
}

FPhysAnimStandingPublicationDecision FPhysAnimStandingPublicationDecision::Build(
	bool bFullSimulationAlreadyCommitted,
	bool bPublishControlGains)
{
	FPhysAnimStandingPublicationDecision Decision;
	Decision.bWriteMovementTypes = !bFullSimulationAlreadyCommitted;
	Decision.bWriteControlGains = bPublishControlGains;
	return Decision;
}

bool FPhysAnimStandingActivationPlan::operator==(const FPhysAnimStandingActivationPlan& Other) const
{
	return Bodies == Other.Bodies && Controls == Other.Controls;
}

void FPhysAnimStandingActivation::Start()
{
	Status = FPhysAnimStandingActivationStatus();
	Status.RuntimeState = EPhysAnimRuntimeState::Standing_Preparation;
}

void FPhysAnimStandingActivation::CompletePreparation(bool bPreparationValid, const FString& FailureReason)
{
	if (Status.RuntimeState != EPhysAnimRuntimeState::Standing_Preparation)
	{
		Fail(TEXT("invalid_preparation_state"));
		return;
	}
	if (!bPreparationValid)
	{
		Fail(FailureReason.IsEmpty() ? TEXT("standing_preparation_failed") : FailureReason);
		return;
	}
	Status.RuntimeState = EPhysAnimRuntimeState::Standing_FullSimulationActivation;
}

void FPhysAnimStandingActivation::CompleteFullSimulationActivation(
	const FPhysAnimStandingActivationReadback& Readback,
	const FString& FailureReason)
{
	if (Status.RuntimeState != EPhysAnimRuntimeState::Standing_FullSimulationActivation)
	{
		Fail(TEXT("invalid_full_simulation_activation_state"));
		return;
	}
	ApplyReadback(Readback);
	if (!IsReadbackValid(Readback))
	{
		Fail(FailureReason.IsEmpty() ? TEXT("standing_activation_readback_mismatch") : FailureReason);
		return;
	}
	Status.RuntimeState = EPhysAnimRuntimeState::Standing_PolicyBlend;
}

void FPhysAnimStandingActivation::TickPolicyBlend(
	float ElapsedSeconds,
	float StartupRampSeconds,
	const FPhysAnimStandingActivationReadback& Readback,
	const FString& FailureReason)
{
	if (Status.RuntimeState != EPhysAnimRuntimeState::Standing_PolicyBlend)
	{
		Fail(TEXT("invalid_policy_blend_state"));
		return;
	}
	ApplyReadback(Readback);
	if (!IsReadbackValid(Readback))
	{
		Fail(FailureReason.IsEmpty() ? TEXT("standing_policy_blend_readback_mismatch") : FailureReason);
		return;
	}
	Status.LinearBlendAlpha = StartupRampSeconds > SMALL_NUMBER
		? FMath::Clamp(ElapsedSeconds / StartupRampSeconds, 0.0f, 1.0f)
		: 1.0f;
	if (Status.LinearBlendAlpha >= 1.0f)
	{
		Status.RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
	}
}

void FPhysAnimStandingActivation::ObserveStanding(
	const FPhysAnimStandingActivationReadback& Readback,
	const FString& FailureReason)
{
	if (Status.RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		Fail(TEXT("invalid_standing_observation_state"));
		return;
	}
	ApplyReadback(Readback);
	if (!IsReadbackValid(Readback))
	{
		Fail(FailureReason.IsEmpty() ? TEXT("standing_readback_mismatch") : FailureReason);
	}
}

void FPhysAnimStandingActivation::SetObservationalEvidenceFlags(
	bool bEvidenceEnabled,
	bool bRendererInstrumentationEnabled)
{
	(void)bEvidenceEnabled;
	(void)bRendererInstrumentationEnabled;
}

bool FPhysAnimStandingActivation::IsReadbackValid(const FPhysAnimStandingActivationReadback& Readback)
{
	return Readback.bFullSimulationCommitted
		&& Readback.ModifierSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount
		&& Readback.RawSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount
		&& Readback.ControlGainMatchCount == FPhysAnimStandingActivationPlan::RequiredControlCount;
}

void FPhysAnimStandingActivation::ApplyReadback(const FPhysAnimStandingActivationReadback& Readback)
{
	Status.bFullSimulationCommitted = Readback.bFullSimulationCommitted;
	Status.ModifierSimulationMatchCount = Readback.ModifierSimulationMatchCount;
	Status.RawSimulationMatchCount = Readback.RawSimulationMatchCount;
	Status.ControlGainMatchCount = Readback.ControlGainMatchCount;
}

void FPhysAnimStandingActivation::Fail(const FString& FailureReason)
{
	Status.RuntimeState = EPhysAnimRuntimeState::FailStopped;
	Status.FailureReason = FailureReason;
	Status.bRetryRequested = false;
	Status.bKinematicDemotionRequested = false;
	Status.bResetRequested = false;
	Status.bBalanceSafeDenyPublished = false;
}
