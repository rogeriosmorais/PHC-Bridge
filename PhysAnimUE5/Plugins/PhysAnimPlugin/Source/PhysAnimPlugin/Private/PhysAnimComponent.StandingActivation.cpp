#include "PhysAnimComponent.h"

#include "PhysAnimComponentPrivate.h"
#include "PhysAnimStandingActivation.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/CollisionProfile.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlRecord.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConstraintInstance.h"

bool UPhysAnimComponent::ValidateStandingActivationRecords(FString& OutFailureReason) const
{
	OutFailureReason.Reset();
	const UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!IsInGameThread() || !PhysicsControl || !Mesh)
	{
		OutFailureReason = TEXT("standing_preparation_missing_game_thread_physics_control_or_mesh");
		return false;
	}

	const TArray<FName>& BodyBones = PhysAnimBridge::GetRequiredBodyModifierBoneNames();
	const TArray<FName>& ControlBones = PhysAnimBridge::GetControlledBoneNames();
	if (BodyBones.Num() != FPhysAnimStandingActivationPlan::RequiredBodyCount ||
		ControlBones.Num() != FPhysAnimStandingActivationPlan::RequiredControlCount ||
		PhysicsControl->GetAllBodyModifierNames().Num() != FPhysAnimStandingActivationPlan::RequiredBodyCount ||
		PhysicsControl->GetAllControlNames().Num() != FPhysAnimStandingActivationPlan::RequiredControlCount)
	{
		OutFailureReason = TEXT("standing_preparation_record_count_mismatch");
		return false;
	}

	for (const FName BoneName : BodyBones)
	{
		if (!FPhysAnimPhysicsControlAccessor::GetModifierRecord(
				PhysicsControl,
				PhysAnimBridge::MakeBodyModifierName(BoneName)) ||
			!Mesh->GetBodyInstance(BoneName))
		{
			OutFailureReason = FString::Printf(TEXT("standing_preparation_missing_body_or_modifier:%s"), *BoneName.ToString());
			return false;
		}
	}
	for (const FName BoneName : ControlBones)
	{
		if (!FPhysAnimPhysicsControlAccessor::GetControlRecord(
				PhysicsControl,
				PhysAnimBridge::MakeControlName(BoneName)))
		{
			OutFailureReason = FString::Printf(TEXT("standing_preparation_missing_control:%s"), *BoneName.ToString());
			return false;
		}
	}
	return true;
}

bool UPhysAnimComponent::PrepareStandingActivation(FString& OutFailureReason)
{
	if (!ValidateStandingActivationRecords(OutFailureReason))
	{
		return false;
	}
	if (!SeedControlTargetsFromCurrentPose(0.0f, OutFailureReason))
	{
		return false;
	}

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		const FQuat* const SeededRotation = PolicyBlendStartControlTargetRotations.Find(ControlName);
		const FPhysicsControlRecord* const Record =
			FPhysAnimPhysicsControlAccessor::GetControlRecord(PhysicsControl, ControlName);
		if (!SeededRotation || !Record ||
			SeededRotation->AngularDistance(FQuat(Record->ControlTarget.TargetOrientation)) > FMath::DegreesToRadians(0.1f))
		{
			OutFailureReason = FString::Printf(TEXT("standing_preparation_target_readback_mismatch:%s"), *BoneName.ToString());
			return false;
		}
	}

	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const CharacterMovement = Character ? Character->GetCharacterMovement() : nullptr;
	UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!CharacterMovement || !Capsule)
	{
		OutFailureReason = TEXT("standing_preparation_missing_character_movement_or_capsule");
		return false;
	}
	ApplyCharacterMovementBridgeOwnership(CharacterMovement, false);
	CharacterMovement->SetUpdatedComponent(nullptr);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PendingBodyModifierCachedResetNames.Reset();
	LastBodyModifierResetRequestCount = 0;
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (FPhysicsBodyModifierRecord* const Record =
			FPhysAnimPhysicsControlAccessor::GetMutableModifierRecord(
				PhysicsControl,
				PhysAnimBridge::MakeBodyModifierName(BoneName)))
		{
			Record->bResetToCachedTarget = false;
		}
	}

	BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
	bTransitionOwnedShellReferenceReanchored = false;
	bTransitionOwnedShellReferenceReseededAfterLock = false;
	StandingActivationElapsedSeconds = 0.0f;
	bStandingFullSimulationCommitted = false;
	return true;
}

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

	FString FirstModifierMismatch;
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
	EPhysAnimStandingVariant StandingVariant = EPhysAnimStandingVariant::Normal;
#if WITH_DEV_AUTOMATION_TESTS
	StandingVariant = StandingVariantForTesting;
#endif
	const FPhysAnimStandingActivationPlan ActivationPlan = FPhysAnimStandingActivationPlan::BuildBlended(
		StandingVariant,
		Alpha,
		EffectiveSettings.AngularStrengthMultiplier,
		EffectiveSettings.AngularDampingRatioMultiplier,
		EffectiveSettings.AngularExtraDampingMultiplier * EffectiveSettings.BalanceBootstrapExtraDampingMultiplier,
		EffectiveSettings.AngularExtraDampingMultiplier * EffectiveSettings.BalanceActiveExtraDampingMultiplier);
	const FPhysAnimStandingControlPlan& ExpectedControl = ActivationPlan.Controls[0];
	FPhysicsControlMultiplier ControlMultiplier;
	ControlMultiplier.AngularStrengthMultiplier = ExpectedControl.AngularStrength;
	ControlMultiplier.AngularDampingRatioMultiplier = ExpectedControl.DampingRatio;
	ControlMultiplier.AngularExtraDampingMultiplier = ExpectedControl.ExtraDamping;
	const bool bCausalStandingPolicyMode =
		FPhysAnimStandingActivationPlan::UsesPolicyInference(StandingVariant) &&
		StandingVariant != EPhysAnimStandingVariant::ZeroActions;
	ControlMultiplier.AngularStrengthMultiplier *=
		ResolveCausalStandingPolicyStrengthFactor(
			bCausalStandingPolicyMode,
			FirstActiveStandingPolicyInferenceSnapshot.bCaptured,
			RuntimeState);
#if WITH_DEV_AUTOMATION_TESTS
	ControlMultiplier.AngularStrengthMultiplier *=
		ResolveExperimentalActiveStrengthFactorForTesting(
			ExperimentalActiveStrengthFactorForTesting,
			FirstActiveStandingPolicyInferenceSnapshot.bCaptured,
			RuntimeState);
	ControlMultiplier.MaxTorqueMultiplier =
		ShouldUseExperimentalCheckpointTorqueCeilingForRuntimeStateForTesting(
			bExperimentalCheckpointTorqueCeilingEnabledForTesting,
			FirstActiveStandingPolicyInferenceSnapshot.bCaptured,
			RuntimeState)
			? 10.0f
			: 1.0f;
	const bool bUseCheckpointForcePd =
		ShouldUseExperimentalCheckpointForcePdForRuntimeStateForTesting(
			bExperimentalCheckpointForcePdEnabledForTesting,
			FirstActiveStandingPolicyInferenceSnapshot.bCaptured,
			RuntimeState);
#endif

	for (const FName BoneName : ControlBones)
	{
		FConstraintInstance* const Constraint = Mesh->FindConstraintInstance(BoneName);
		if (!Constraint)
		{
			OutFailureReason = FString::Printf(TEXT("standing_activation_missing_constraint:%s"), *BoneName.ToString());
			return Readback;
		}
		Constraint->SetAngularVelocityDriveSLERP(ActivationPlan.bEnablePassiveConstraintVelocityDrives);

		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
#if WITH_DEV_AUTOMATION_TESTS
		if (bUseCheckpointForcePd)
		{
			FPhysicsControlData BaselineControlData;
			FPhysicsControlData CheckpointControlData;
			if (!PhysicsControl->GetControlData(ControlName, BaselineControlData) ||
				!TryBuildCheckpointForcePdControlDataForTesting(
					BoneName,
					BaselineControlData,
					CheckpointControlData))
			{
				OutFailureReason = FString::Printf(
					TEXT("standing_activation_checkpoint_force_pd_profile_failed:%s"),
					*BoneName.ToString());
				return Readback;
			}
			PhysicsControl->SetControlData(
				ControlName,
				CheckpointControlData,
				true,
				false);
			Constraint->SetAngularDriveAccelerationMode(false);
		}
#endif
		PhysicsControl->SetControlMultiplier(
			ControlName,
			ControlMultiplier,
			ExpectedControl.bEnabled,
			true,
			false);
		// SetControlMultiplier can enable a disabled control but does not disable an
		// enabled one when bEnableControl is false. Publish activation explicitly.
		PhysicsControl->SetControlEnabled(
			ControlName,
			ExpectedControl.bEnabled,
			true,
			false);
	}

	const FPhysAnimStandingPublicationDecision Decision =
		FPhysAnimStandingPublicationDecision::Build(bStandingFullSimulationCommitted, true);
	if (bCommitFullSimulation && Decision.bWriteMovementTypes)
	{
		if (!bHasSavedMeshCollisionState)
		{
			OriginalMeshCollisionProfileName = Mesh->GetCollisionProfileName();
			OriginalMeshCollisionEnabled = Mesh->GetCollisionEnabled();
			OriginalMeshPawnResponse = Mesh->GetCollisionResponseToChannel(ECC_Pawn);
			bHasSavedMeshCollisionState = true;
		}
		Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetEnablePhysicsBlending(true);
		Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
		for (int32 BodyIndex = 0; BodyIndex < BodyBones.Num(); ++BodyIndex)
		{
			const FName BoneName = BodyBones[BodyIndex];
			const FPhysAnimStandingBodyPlan& ExpectedBody = ActivationPlan.Bodies[BodyIndex];
			const EPhysicsMovementType MovementType = ExpectedBody.bSimulated
				? EPhysicsMovementType::Simulated
				: EPhysicsMovementType::Kinematic;
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, ExpectedBody.bUpdateKinematicFromSimulation, true, false);
			PhysicsControl->SetBodyModifierMovementType(ModifierName, MovementType, true, false);
			PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, ExpectedBody.PhysicsBlendWeight, true, false);
			PhysicsControl->SetBodyModifierCollisionType(ModifierName, ExpectedBody.CollisionEnabled, true, false);
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				MovementType,
				ExpectedBody.PhysicsBlendWeight,
				ExpectedBody.CollisionEnabled,
				ExpectedBody.bUpdateKinematicFromSimulation);
			FBodyInstance* const Body = Mesh->GetBodyInstance(BoneName);
			Body->SetInstanceSimulatePhysics(ExpectedBody.bSimulated, true);
			if (ExpectedBody.bSimulated)
			{
				Body->WakeInstance();
			}
		}
		PhysicsControl->UpdateControls(0.0f);
	}

	// Movement ownership changes atomically above. Collision, blend, and
	// kinematic-readback tuning remain per-tick publications so no legacy writer
	// can silently replace the standing plan after the activation frame.
	if (bStandingFullSimulationCommitted)
	{
		for (int32 BodyIndex = 0; BodyIndex < BodyBones.Num(); ++BodyIndex)
		{
			const FName BoneName = BodyBones[BodyIndex];
			const FPhysAnimStandingBodyPlan& ExpectedBody = ActivationPlan.Bodies[BodyIndex];
			const EPhysicsMovementType MovementType = ExpectedBody.bSimulated
				? EPhysicsMovementType::Simulated
				: EPhysicsMovementType::Kinematic;
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, ExpectedBody.bUpdateKinematicFromSimulation, true, false);
			PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, ExpectedBody.PhysicsBlendWeight, true, false);
			PhysicsControl->SetBodyModifierCollisionType(ModifierName, ExpectedBody.CollisionEnabled, true, false);
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				MovementType,
				ExpectedBody.PhysicsBlendWeight,
				ExpectedBody.CollisionEnabled,
				ExpectedBody.bUpdateKinematicFromSimulation);
		}
		PhysicsControl->UpdateControls(0.0f);
	}

	for (int32 BodyIndex = 0; BodyIndex < BodyBones.Num(); ++BodyIndex)
	{
		const FName BoneName = BodyBones[BodyIndex];
		const FPhysAnimStandingBodyPlan& ExpectedBody = ActivationPlan.Bodies[BodyIndex];
		const EPhysicsMovementType ExpectedMovementType = ExpectedBody.bSimulated
			? EPhysicsMovementType::Simulated
			: EPhysicsMovementType::Kinematic;
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		const FPhysicsBodyModifierRecord* const Modifier =
			FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName);
		const FBodyInstance* const Body = Mesh->GetBodyInstance(BoneName);
		const bool bModifierMatches = Modifier &&
			Modifier->BodyModifier.ModifierData.MovementType == ExpectedMovementType &&
			FMath::IsNearlyEqual(Modifier->BodyModifier.ModifierData.PhysicsBlendWeight, ExpectedBody.PhysicsBlendWeight) &&
			Modifier->BodyModifier.ModifierData.CollisionType == ExpectedBody.CollisionEnabled &&
			Modifier->BodyModifier.ModifierData.bUpdateKinematicFromSimulation == ExpectedBody.bUpdateKinematicFromSimulation;
		if (bModifierMatches)
		{
			++Readback.ModifierSimulationMatchCount;
		}
		else if (FirstModifierMismatch.IsEmpty())
		{
			FirstModifierMismatch = Modifier
				? FString::Printf(
					TEXT("%s{move=%s blend=%.3f collision=%d updateKin=%d}"),
					*BoneName.ToString(),
					GetPhysicsMovementTypeName(Modifier->BodyModifier.ModifierData.MovementType),
					Modifier->BodyModifier.ModifierData.PhysicsBlendWeight,
					static_cast<int32>(Modifier->BodyModifier.ModifierData.CollisionType),
					Modifier->BodyModifier.ModifierData.bUpdateKinematicFromSimulation ? 1 : 0)
				: FString::Printf(TEXT("%s{missing}"), *BoneName.ToString());
		}
		if (Body && Body->IsInstanceSimulatingPhysics() == ExpectedBody.bSimulated)
		{
			++Readback.RawSimulationMatchCount;
		}
	}

	FString FirstControlMismatch;
	FString FirstPassiveConstraintDriveMismatch;
	for (const FName BoneName : ControlBones)
	{
		const FConstraintInstance* const Constraint = Mesh->FindConstraintInstance(BoneName);
		if (Constraint &&
			Constraint->IsAngularVelocityDriveEnabled() == ActivationPlan.bEnablePassiveConstraintVelocityDrives)
		{
			++Readback.PassiveConstraintVelocityDriveMatchCount;
		}
		else if (FirstPassiveConstraintDriveMismatch.IsEmpty())
		{
			FirstPassiveConstraintDriveMismatch = Constraint
				? FString::Printf(
					TEXT("%s{velocity_drive_enabled=%d expected=%d}"),
					*BoneName.ToString(),
					Constraint->IsAngularVelocityDriveEnabled() ? 1 : 0,
					ActivationPlan.bEnablePassiveConstraintVelocityDrives ? 1 : 0)
				: FString::Printf(TEXT("%s{missing}"), *BoneName.ToString());
		}

		const FPhysicsControlRecord* const Control = FPhysAnimPhysicsControlAccessor::GetControlRecord(
			PhysicsControl,
			PhysAnimBridge::MakeControlName(BoneName));
		const bool bEnabledMatches = Control &&
			(Control->PhysicsControl.IsEnabled() == ExpectedControl.bEnabled);
		if (bEnabledMatches &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularStrengthMultiplier, ControlMultiplier.AngularStrengthMultiplier) &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularDampingRatioMultiplier, ControlMultiplier.AngularDampingRatioMultiplier) &&
			FMath::IsNearlyEqual(Control->PhysicsControl.ControlMultiplier.AngularExtraDampingMultiplier, ControlMultiplier.AngularExtraDampingMultiplier))
		{
			++Readback.ControlGainMatchCount;
		}
		else if (FirstControlMismatch.IsEmpty())
		{
			FirstControlMismatch = Control
				? FString::Printf(
					TEXT("%s{enabled=%d strength=%.3f damping=%.3f extra=%.3f}"),
					*BoneName.ToString(),
					Control->PhysicsControl.IsEnabled() ? 1 : 0,
					Control->PhysicsControl.ControlMultiplier.AngularStrengthMultiplier,
					Control->PhysicsControl.ControlMultiplier.AngularDampingRatioMultiplier,
					Control->PhysicsControl.ControlMultiplier.AngularExtraDampingMultiplier)
				: FString::Printf(TEXT("%s{missing}"), *BoneName.ToString());
		}
	}

	const bool bReadbackMatches =
		Readback.ModifierSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount &&
		Readback.RawSimulationMatchCount == FPhysAnimStandingActivationPlan::RequiredBodyCount &&
		Readback.ControlGainMatchCount == FPhysAnimStandingActivationPlan::RequiredControlCount &&
		Readback.PassiveConstraintVelocityDriveMatchCount == FPhysAnimStandingActivationPlan::RequiredControlCount;
	if (bCommitFullSimulation && bReadbackMatches)
	{
		bStandingFullSimulationCommitted = true;
		// The atomic standing publisher replaces the legacy per-tick tuning path, so it
		// must also publish the committed root-simulation observation consumed by target
		// dispatch. Otherwise every policy step is misclassified as a fresh sim flip.
		bLastAppliedPresentationRootSimulationEnabled = true;
	}
	Readback.bFullSimulationCommitted = bStandingFullSimulationCommitted && bReadbackMatches;
	if (!bReadbackMatches)
	{
		OutFailureReason = FString::Printf(
			TEXT("standing_activation_readback_mismatch:modifier=%d/22 raw=%d/22 controls=%d/21 passive_drives=%d/21 first_modifier=%s first_control=%s first_passive_drive=%s"),
			Readback.ModifierSimulationMatchCount,
			Readback.RawSimulationMatchCount,
			Readback.ControlGainMatchCount,
			Readback.PassiveConstraintVelocityDriveMatchCount,
			*FirstModifierMismatch,
			*FirstControlMismatch,
			*FirstPassiveConstraintDriveMismatch);
	}
	return Readback;
}
