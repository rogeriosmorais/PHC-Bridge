#include "PhysAnimComponent.h"

#include "PhysAnimComponentPrivate.h"
#include "PhysAnimStandingActivation.h"

#include "PhysicsControlComponent.h"
#include "PhysicsControlRecord.h"
#include "PhysicsEngine/BodyInstance.h"

FPhysAnimStandingActivationReadback UPhysAnimComponent::PublishStandingPhysicsControlState(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	float LinearBlendAlpha,
	bool bCommitFullSimulation,
	FString& OutFailureReason)
{
	FPhysAnimStandingActivationReadback Readback;
	OutFailureReason.Reset();

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!IsInGameThread() || !PhysicsControl || !Mesh)
	{
		OutFailureReason = TEXT("standing_activation_missing_game_thread_physics_control_or_mesh");
		return Readback;
	}

	const TArray<FName>& BodyBones = PhysAnimBridge::GetRequiredBodyModifierBoneNames();
	const TArray<FName>& ControlBones = PhysAnimBridge::GetControlledBoneNames();
	if (BodyBones.Num() != FPhysAnimStandingActivationPlan::RequiredBodyCount ||
		ControlBones.Num() != FPhysAnimStandingActivationPlan::RequiredControlCount ||
		PhysicsControl->GetAllBodyModifierNames().Num() != FPhysAnimStandingActivationPlan::RequiredBodyCount ||
		PhysicsControl->GetAllControlNames().Num() != FPhysAnimStandingActivationPlan::RequiredControlCount)
	{
		OutFailureReason = TEXT("standing_activation_record_count_mismatch");
		return Readback;
	}

	for (const FName BoneName : BodyBones)
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName) ||
			!Mesh->GetBodyInstance(BoneName))
		{
			OutFailureReason = FString::Printf(TEXT("standing_activation_missing_body_or_modifier:%s"), *BoneName.ToString());
			return Readback;
		}
	}
	for (const FName BoneName : ControlBones)
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (!FPhysAnimPhysicsControlAccessor::GetControlRecord(PhysicsControl, ControlName))
		{
			OutFailureReason = FString::Printf(TEXT("standing_activation_missing_control:%s"), *BoneName.ToString());
			return Readback;
		}
	}

	const float Alpha = FMath::Clamp(LinearBlendAlpha, 0.0f, 1.0f);
	const float ExtraDamping = EffectiveSettings.AngularExtraDampingMultiplier * FMath::Lerp(
		EffectiveSettings.BalanceBootstrapExtraDampingMultiplier,
		EffectiveSettings.BalanceActiveExtraDampingMultiplier,
		Alpha);
	FPhysicsControlMultiplier ControlMultiplier;
	ControlMultiplier.AngularStrengthMultiplier = EffectiveSettings.AngularStrengthMultiplier * Alpha;
	ControlMultiplier.AngularDampingRatioMultiplier = EffectiveSettings.AngularDampingRatioMultiplier;
	ControlMultiplier.AngularExtraDampingMultiplier = ExtraDamping;

	for (const FName BoneName : ControlBones)
	{
		PhysicsControl->SetControlMultiplier(
			PhysAnimBridge::MakeControlName(BoneName),
			ControlMultiplier,
			true,
			true,
			false);
	}

	const FPhysAnimStandingPublicationDecision Decision =
		FPhysAnimStandingPublicationDecision::Build(bStandingFullSimulationCommitted, true);
	if (bCommitFullSimulation && Decision.bWriteMovementTypes)
	{
		for (const FName BoneName : BodyBones)
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, false, false, false);
			PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Simulated, false, true);
			PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, 1.0f, false, false);
			PhysicsControl->SetBodyModifierCollisionType(ModifierName, ECollisionEnabled::QueryAndPhysics, false, false);
			Mesh->GetBodyInstance(BoneName)->WakeInstance();
		}
		PhysicsControl->UpdateControls(0.0f);
	}

	for (const FName BoneName : BodyBones)
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		const FPhysicsBodyModifierRecord* const Modifier =
			FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName);
		const FBodyInstance* const Body = Mesh->GetBodyInstance(BoneName);
		if (Modifier &&
			Modifier->BodyModifier.ModifierData.MovementType == EPhysicsMovementType::Simulated &&
			FMath::IsNearlyEqual(Modifier->BodyModifier.ModifierData.PhysicsBlendWeight, 1.0f) &&
			Modifier->BodyModifier.ModifierData.CollisionType == ECollisionEnabled::QueryAndPhysics &&
			!Modifier->BodyModifier.ModifierData.bUpdateKinematicFromSimulation)
		{
			++Readback.ModifierSimulationMatchCount;
		}
		if (Body && Body->IsInstanceSimulatingPhysics())
		{
			++Readback.RawSimulationMatchCount;
		}
	}

	for (const FName BoneName : ControlBones)
	{
		const FPhysicsControlRecord* const Control = FPhysAnimPhysicsControlAccessor::GetControlRecord(
			PhysicsControl,
			PhysAnimBridge::MakeControlName(BoneName));
		if (Control && Control->PhysicsControl.IsEnabled() &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularStrengthMultiplier, ControlMultiplier.AngularStrengthMultiplier) &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularDampingRatioMultiplier, ControlMultiplier.AngularDampingRatioMultiplier) &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularExtraDampingMultiplier, ControlMultiplier.AngularExtraDampingMultiplier))
		{
			++Readback.ControlGainMatchCount;
		}
	}

	const bool bReadbackMatches =
		Readback.ModifierSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount &&
		Readback.RawSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount &&
		Readback.ControlGainMatchCount == FPhysAnimStandingActivationPlan::RequiredControlCount;
	if (bCommitFullSimulation && bReadbackMatches)
	{
		bStandingFullSimulationCommitted = true;
	}
	Readback.bFullSimulationCommitted = bStandingFullSimulationCommitted && bReadbackMatches;
	if (!bReadbackMatches)
	{
		OutFailureReason = FString::Printf(
			TEXT("standing_activation_readback_mismatch:modifier=%d/22 raw=%d/22 controls=%d/21"),
			Readback.ModifierSimulationMatchCount,
			Readback.RawSimulationMatchCount,
			Readback.ControlGainMatchCount);
	}
	return Readback;
}

