#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

bool UPhysAnimComponent::ActivateBridgeFromReadyState(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* ActivationContext,
	FString& OutError,
	const bool bRequireLiveProofSatisfied)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Bridge activation requires a valid Physics Control component.");
		return false;
	}

	if (!ActivateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	if (bEnableLiveRuntimeEvidenceProof)
	{
		if (bRequireLiveProofSatisfied && CanEnterBalanceActiveStanding())
		{
			TransitionRuntimeState(EPhysAnimRuntimeState::BalanceActive_Standing);
			PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] WIRING_SUCCESS state=BalanceActive_Standing"));
		}
		else if (!bRequireLiveProofSatisfied)
		{
			TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
			PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] WIRING_PRE_PROOF_PHYSICS_OWNERSHIP state=BridgeActive"));
		}
		else
		{
			OutError = TEXT("Proof active but not satisfied. Activation denied.");
			PHYSANIM_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] WIRING_DENIED reason=PROOF_NOT_SATISFIED"));
			return false;
		}
	}
	else
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
	}

	LogBridgeStateSnapshot(TEXT("BeforeActivateBridgePhysicsState"));
	ActivateBridgePhysicsState(EffectiveSettings);
	LogBridgeStateSnapshot(TEXT("AfterActivateBridgePhysicsState"));
	ResetStabilizationRuntimeState();

	const double CurrentWorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		const double SettledRampStartTimeSeconds =
			CurrentWorldTimeSeconds - static_cast<double>(FMath::Max(EffectiveSettings.StartupRampSeconds, 0.0f)) - 0.1;

		BridgeStartTimeSeconds = SettledRampStartTimeSeconds;
		PolicyInfluenceRampStartTimeSeconds = SettledRampStartTimeSeconds;
		HighestUnlockedBringUpGroupIndex = GetBringUpGroupCount() - 1;

		if (BringUpGroupActivationTimeSeconds.Num() < GetBringUpGroupCount())
			BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
		if (BringUpGroupControlRampStartTimeSeconds.Num() < GetBringUpGroupCount())
			BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
		if (BringUpGroupAlphaActiveLogged.Num() < GetBringUpGroupCount())
			BringUpGroupAlphaActiveLogged.Init(false, GetBringUpGroupCount());

		for (int32 GroupIndex = 0; GroupIndex < GetBringUpGroupCount(); ++GroupIndex)
		{
			BringUpGroupActivationTimeSeconds[GroupIndex] = SettledRampStartTimeSeconds;
			BringUpGroupControlRampStartTimeSeconds[GroupIndex] = SettledRampStartTimeSeconds;
			BringUpGroupAlphaActiveLogged[GroupIndex] = 1;
		}
	}
	else
	{
		BridgeStartTimeSeconds = CurrentWorldTimeSeconds;
	}

	if (const AActor* const OwnerActor = GetOwner())
	{
		MovementSmokeStartLocation = OwnerActor->GetActorLocation();
	}
	SimulationHandoffAlpha = CalculateSimulationHandoffAlpha(EffectiveSettings);
	bLastAppliedSimulationHandoffSettled = (SimulationHandoffAlpha >= (1.0f - KINDA_SMALL_NUMBER));

	PrewarmPhysicsControlActivationPose();
	PhysicsControl->UpdateTargetCaches(0.0f);
	LogBridgeStateSnapshot(TEXT("AfterActivationUpdateTargetCaches"));
	if (!SeedControlTargetsFromCurrentPose(0.0f, OutError))
	{
		return false;
	}
	LogBridgeStateSnapshot(TEXT("AfterActivationSeedControlTargets"));
	ApplyRuntimeControlTuning(EffectiveSettings);
	LogBridgeStateSnapshot(TEXT("AfterActivationRuntimeControlTuning"));
	PhysicsControl->UpdateControls(0.0f);
	ReassertBridgeActiveStartupProofRawSimulation(TEXT("activation_prepass_updatecontrols"));
	LogBridgeStateSnapshot(TEXT("AfterActivationPrepass"));
	LogActivationSummary(EffectiveSettings, ActivationContext, true, true, SimulationHandoffAlpha);

	if (bEnableLiveRuntimeEvidenceProof && !bRequireLiveProofSatisfied)
	{
		bLiveRuntimeEvidenceProofActive = false;
		bLiveRuntimeEvidenceProofComplete = false;
		bLiveRuntimeEvidenceTerminalArtifactEmitted = false;
		LiveRuntimeEvidenceAttemptUuid.Empty();
		LiveRuntimeEvidenceStandingSeconds = 0.0f;
		LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
		LiveRuntimeEvidenceSubstepCounter = 0;
		LiveRuntimeEvidenceTerminationState = FPhysAnimRuntimeTerminationState();
		ActivatedStandingStabilityMetrics.ResetForNewAttempt();
		bActivatedStandingStabilityBaselineInitialized = false;
		bActivatedStandingPerturbationApplied = false;
		ActivatedStandingStabilityBaselineRootLocationCm = FVector::ZeroVector;
		ActivatedStandingStabilityBaselineRootTiltDeg = 0.0f;
		ActivatedStandingStabilitySupportHullAreaSumCm2 = 0.0;
		ActivatedStandingStabilityActiveSupportSideCountSum = 0.0;
		PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Startup proof restarted after bridge physics ownership began."));
	}

	PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Bridge physics activation[%s] complete."), ActivationContext);
	return true;
}


bool UPhysAnimComponent::PrewarmPhysicsControlActivationPose()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!ShouldPrewarmPhysicsControlActivationPose(
		SkeletalMesh != nullptr,
		SkeletalMesh && SkeletalMesh->LeaderPoseComponent.IsValid()))
	{
		return false;
	}

	SkeletalMesh->TickAnimation(0.0f, false);
	SkeletalMesh->RefreshBoneTransforms();
	PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Prewarmed skeletal pose for PhysicsControl activation cache."));
	return true;
}


void UPhysAnimComponent::EnterReadyForActivation(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* Context,
	bool bLogDeferredStartupSuccess)
{
	ResetStabilizationRuntimeState();
	TransitionRuntimeState(EPhysAnimRuntimeState::ReadyForActivation);
	LogBridgeStateSnapshot(Context);

	if (bLogDeferredStartupSuccess)
	{
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s DeferredActivation=true"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Bridge physics activation deferred by zero-action safe mode."));
		PHYSANIM_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
	}
}


bool UPhysAnimComponent::GatherCurrentPoseControlTargetOrientations(TMap<FName, FQuat>& OutTargetOrientations, FString& OutError) const
{
	OutTargetOrientations.Reset();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!SkeletalMesh || !PhysicsControl || !OwnerActor)
	{
		OutError = TEXT("Current-pose target gathering requires the owner, skeletal mesh component, and Physics Control component.");
		return false;
	}

	auto FindInitialControl = [OwnerActor](const FName ControlName) -> const FInitialPhysicsControl*
	{
		if (const UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
		{
			return Stage1Initializer->InitialControls.Find(ControlName);
		}

		if (const UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
		{
			return Initializer->InitialControls.Find(ControlName);
		}

		return nullptr;
	};

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		const FInitialPhysicsControl* const InitialControl = FindInitialControl(ControlName);
		if (!InitialControl)
		{
			OutError = FString::Printf(TEXT("Missing authored control definition for '%s' while gathering current-pose targets."), *ControlName.ToString());
			return false;
		}

		if (SkeletalMesh->GetBoneIndex(InitialControl->ParentBoneName) == INDEX_NONE ||
			SkeletalMesh->GetBoneIndex(InitialControl->ChildBoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(
				TEXT("Could not resolve current-pose bones for control '%s' (parent='%s', child='%s')."),
				*ControlName.ToString(),
				*InitialControl->ParentBoneName.ToString(),
				*InitialControl->ChildBoneName.ToString());
			return false;
		}

		const FQuat ParentWorldRotation = PhysicsControl->GetCachedBoneOrientation(SkeletalMesh, InitialControl->ParentBoneName).Quaternion();
		const FQuat ChildWorldRotation = PhysicsControl->GetCachedBoneOrientation(SkeletalMesh, InitialControl->ChildBoneName).Quaternion();
		OutTargetOrientations.Add(
			ControlName,
			BuildCurrentPoseControlTargetOrientation(ParentWorldRotation, ChildWorldRotation));
	}

	return true;
}


bool UPhysAnimComponent::SeedControlTargetsFromCurrentPose(float DeltaTime, FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Current-pose target seeding requires the Physics Control component.");
		return false;
	}

	TMap<FName, FQuat> CurrentPoseTargetOrientations;
	if (!GatherCurrentPoseControlTargetOrientations(CurrentPoseTargetOrientations, OutError))
	{
		return false;
	}

	PreviousControlTargetRotations.Reset();
	PolicyBlendStartControlTargetRotations.Reset();

	for (const TPair<FName, FQuat>& Pair : CurrentPoseTargetOrientations)
	{
		if (!PhysicsControl->GetControlExists(Pair.Key))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' while seeding current-pose targets."), *Pair.Key.ToString());
			return false;
		}

		PreviousControlTargetRotations.Add(Pair.Key, Pair.Value);
		PolicyBlendStartControlTargetRotations.Add(Pair.Key, Pair.Value);
		PhysicsControl->SetControlTargetOrientation(
			Pair.Key,
			Pair.Value.Rotator(),
			DeltaTime,
			true,
			false,
			true,
			false);
	}

	return true;
}

