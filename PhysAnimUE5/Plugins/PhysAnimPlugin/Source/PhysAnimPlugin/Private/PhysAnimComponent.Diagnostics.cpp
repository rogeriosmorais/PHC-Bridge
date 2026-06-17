#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

FPhysAnimStabilizationSettings UPhysAnimComponent::ResolveEffectiveStabilizationSettings() const
{
	FPhysAnimStabilizationSettings EffectiveSettings = StabilizationSettings;
	EffectiveSettings.ActionScale =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionScale, EffectiveSettings.ActionScale);
	EffectiveSettings.ActionClampAbs =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionClampAbs, EffectiveSettings.ActionClampAbs);
	EffectiveSettings.ActionSmoothingAlpha =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionSmoothingAlpha, EffectiveSettings.ActionSmoothingAlpha);
	EffectiveSettings.StartupRampSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimStartupRampSeconds, EffectiveSettings.StartupRampSeconds);
	EffectiveSettings.PolicyControlRateHz =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimPolicyControlRateHz,
			EffectiveSettings.PolicyControlRateHz);
	EffectiveSettings.bApplyTrainingAlignedMassScales =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedMassScales,
			EffectiveSettings.bApplyTrainingAlignedMassScales);
	EffectiveSettings.TrainingAlignedMassScaleBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedMassScaleBlend,
			EffectiveSettings.TrainingAlignedMassScaleBlend);
	EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedControlFamilyProfile,
			EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile);
	EffectiveSettings.TrainingAlignedControlFamilyProfileBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedControlFamilyProfileBlend,
			EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
	EffectiveSettings.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedLocomotionLowerLimbResponsePolicy,
			EffectiveSettings.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy);
	EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedLocomotionLowerLimbResponsePolicyBlend,
			EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend);
	EffectiveSettings.bApplyTrainingAlignedToeLimitPolicy =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedToeLimitPolicy,
			EffectiveSettings.bApplyTrainingAlignedToeLimitPolicy);
	EffectiveSettings.TrainingAlignedToeLimitPolicyBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedToeLimitPolicyBlend,
			EffectiveSettings.TrainingAlignedToeLimitPolicyBlend);
	EffectiveSettings.bApplyTrainingAlignedLowerLimbTargetRangePolicy =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedLowerLimbTargetRangePolicy,
			EffectiveSettings.bApplyTrainingAlignedLowerLimbTargetRangePolicy);
	EffectiveSettings.TrainingAlignedLowerLimbTargetRangePolicyBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedLowerLimbTargetRangePolicyBlend,
			EffectiveSettings.TrainingAlignedLowerLimbTargetRangePolicyBlend);
	EffectiveSettings.bApplyTrainingAlignedDistalLocomotionTargetPolicy =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedDistalLocomotionTargetPolicy,
			EffectiveSettings.bApplyTrainingAlignedDistalLocomotionTargetPolicy);
	EffectiveSettings.TrainingAlignedDistalLocomotionTargetPolicyBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedDistalLocomotionTargetPolicyBlend,
			EffectiveSettings.TrainingAlignedDistalLocomotionTargetPolicyBlend);
	EffectiveSettings.DistalLocomotionTargetPolicyActivationSpeedCmPerSec =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionTargetPolicyActivationSpeedCmPerSec,
			EffectiveSettings.DistalLocomotionTargetPolicyActivationSpeedCmPerSec);
	EffectiveSettings.bApplyTrainingAlignedDistalLocomotionCompositionPolicy =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedDistalLocomotionCompositionPolicy,
			EffectiveSettings.bApplyTrainingAlignedDistalLocomotionCompositionPolicy);
	EffectiveSettings.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionCompositionPolicyActivationSpeedCmPerSec,
			EffectiveSettings.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec);
	EffectiveSettings.DistalLocomotionCompositionPolicyExitSpeedCmPerSec =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionCompositionPolicyExitSpeedCmPerSec,
			EffectiveSettings.DistalLocomotionCompositionPolicyExitSpeedCmPerSec);
	EffectiveSettings.DistalLocomotionCompositionPolicyEnterHoldSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionCompositionPolicyEnterHoldSeconds,
			EffectiveSettings.DistalLocomotionCompositionPolicyEnterHoldSeconds);
	EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionCompositionPolicyIntentGraceSeconds,
			EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds);
	EffectiveSettings.DistalLocomotionCompositionPolicyExitHoldSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimDistalLocomotionCompositionPolicyExitHoldSeconds,
			EffectiveSettings.DistalLocomotionCompositionPolicyExitHoldSeconds);
	EffectiveSettings.MaxAngularStepDegreesPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxAngularStepDegPerSec,
			EffectiveSettings.MaxAngularStepDegreesPerSecond);
	EffectiveSettings.AngularStrengthMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularStrengthMultiplier,
			EffectiveSettings.AngularStrengthMultiplier);
	EffectiveSettings.AngularDampingRatioMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularDampingRatioMultiplier,
			EffectiveSettings.AngularDampingRatioMultiplier);
	EffectiveSettings.AngularExtraDampingMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularExtraDampingMultiplier,
			EffectiveSettings.AngularExtraDampingMultiplier);
	EffectiveSettings.bUseSkeletalAnimationTargets =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimUseSkeletalAnimationTargets,
			EffectiveSettings.bUseSkeletalAnimationTargets);
	EffectiveSettings.bForceZeroActions =
		PhysAnimComponentInternal::ResolveBoolOverride(PhysAnimComponentInternal::CVarPhysAnimForceZeroActions, EffectiveSettings.bForceZeroActions);
	EffectiveSettings.bLogActionDiagnostics =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimLogActionDiagnostics,
			EffectiveSettings.bLogActionDiagnostics);
	EffectiveSettings.MaxRootHeightDeltaCm =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootHeightDeltaCm,
			EffectiveSettings.MaxRootHeightDeltaCm);
	EffectiveSettings.MaxRootLinearSpeedCmPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootLinearSpeedCmPerSec,
			EffectiveSettings.MaxRootLinearSpeedCmPerSecond);
	EffectiveSettings.MaxRootAngularSpeedDegPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootAngularSpeedDegPerSec,
			EffectiveSettings.MaxRootAngularSpeedDegPerSecond);
	EffectiveSettings.InstabilityGracePeriodSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimInstabilityGracePeriodSeconds,
			EffectiveSettings.InstabilityGracePeriodSeconds);
	EffectiveSettings.bEnableInstabilityFailStop =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimEnableInstabilityFailStop,
			EffectiveSettings.bEnableInstabilityFailStop);
	EffectiveSettings.bPhase1DistalKinematicExperiment =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimPhase1DistalKinematicExperiment,
			EffectiveSettings.bPhase1DistalKinematicExperiment);
	EffectiveSettings.BalancePhase2EntryMaxRootTiltDeg = FMath::Max3(
		EffectiveSettings.BalancePhase2EntryMaxRootTiltDeg,
		BalanceQuietTiltThresholdDeg,
		25.0f);
	ApplyPresentationPerturbationStabilizationOverride(IsPresentationPerturbationOverrideActive(), EffectiveSettings);
	ApplyStabilizationStressTestRamp(
		ResolveStabilizationStressTestMultiplier(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
		EffectiveSettings);

	// Bridge-owned locomotion currently samples future MM poses, but the live current pose still comes
	// from the cached post-AnimBP pose. If CharacterMovement stays suppressed after startup, the AnimBP
	// locomotion graph never leaves idle, so the bridge ends up sliding the shell under an idle body.
	// Restore CharacterMovement as soon as policy influence comes online so the live pose cache can track
	// the same locomotion state that MM is already selecting.
	if (EffectiveSettings.bLockCharacterMovementUntilStartupReady && !EffectiveSettings.bUseSkeletalAnimationTargets)
	{
		EffectiveSettings.bRestoreCharacterMovementAfterStartupReady = true;
		EffectiveSettings.bDelayMovementUnlockUntilPolicySettled = false;
	}

	return EffectiveSettings;
}


void UPhysAnimComponent::LogBridgeStateSnapshot(const TCHAR* Context) const
{
	if (PhysAnimComponentInternal::CVarPhysAnimLogBridgeStateSnapshots.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* const CapsuleComponent = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	const UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FBodyInstance* const RootBody = SkeletalMesh ? SkeletalMesh->GetBodyInstance(RootBoneName) : nullptr;
	const FVector RootLinearVelocity = RootBody ? RootBody->GetUnrealWorldVelocity() : FVector::ZeroVector;
	const FVector RootAngularVelocity = RootBody ? FMath::RadiansToDegrees(RootBody->GetUnrealWorldAngularVelocityInRadians()) : FVector::ZeroVector;

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Snapshot[%s] state=%s bridgeOwnsPhysics=%s skeletalSim=%d skeletalBlend=%d liveControls=%d liveBodyModifiers=%d forceZero=%s controlsDesiredEnabled=%s meshProfile=%s meshCollision=%s meshPawnResponse=%s capsuleCollision=%s charMoveTick=%s movementMode=%s rootBodyValid=%s rootBodySim=%s rootLinCmPerSec=(%.1f,%.1f,%.1f) rootAngDegPerSec=(%.1f,%.1f,%.1f)"),
		Context,
		GetRuntimeStateName(RuntimeState),
		RuntimeStateOwnsBridgePhysics(RuntimeState) ? TEXT("true") : TEXT("false"),
		SkeletalMesh && SkeletalMesh->IsSimulatingPhysics() ? 1 : 0,
		0, // Removed skeletalBlend due to missing getter
		PhysicsControl ? PhysicsControl->GetAllControlNames().Num() : 0,
		PhysicsControl ? PhysicsControl->GetAllBodyModifierNames().Num() : 0,
		EffectiveSettings.bForceZeroActions ? TEXT("true") : TEXT("false"),
		EffectiveSettings.bForceZeroActions ? TEXT("false") : TEXT("true"),
		SkeletalMesh ? *SkeletalMesh->GetCollisionProfileName().ToString() : TEXT("None"),
		SkeletalMesh ? *UEnum::GetValueAsString(SkeletalMesh->GetCollisionEnabled()) : TEXT("None"),
		SkeletalMesh ? *UEnum::GetValueAsString(SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn)) : TEXT("None"),
		CapsuleComponent ? *UEnum::GetValueAsString(CapsuleComponent->GetCollisionEnabled()) : TEXT("None"),
		CharacterMovement && CharacterMovement->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
		CharacterMovement ? *UEnum::GetValueAsString(static_cast<EMovementMode>(CharacterMovement->MovementMode)) : TEXT("None"),
		RootBody && RootBody->IsValidBodyInstance() ? TEXT("true") : TEXT("false"),
		RootBody && RootBody->IsValidBodyInstance() && RootBody->IsInstanceSimulatingPhysics() ? TEXT("true") : TEXT("false"),
		RootLinearVelocity.X,
		RootLinearVelocity.Y,
		RootLinearVelocity.Z,
		RootAngularVelocity.X,
		RootAngularVelocity.Y,
		RootAngularVelocity.Z);
}


void UPhysAnimComponent::LogActivationSummary(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* Context,
	bool bCurrentPoseTargetsSeeded,
	bool bActivationPrepassCompleted,
	float SimulationHandoffProgress) const
{
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Activation[%s]: skeletalTargets=%s currentPoseTargetsSeeded=%s activationPrepassCompleted=%s simulationHandoffAlpha=%.2f"),
		Context,
		EffectiveSettings.bUseSkeletalAnimationTargets ? TEXT("true") : TEXT("false"),
		bCurrentPoseTargetsSeeded ? TEXT("true") : TEXT("false"),
		bActivationPrepassCompleted ? TEXT("true") : TEXT("false"),
		SimulationHandoffProgress);
}


void UPhysAnimComponent::LogBodyModifierTelemetrySnapshot(const TCHAR* Context) const
{
	TArray<FPhysAnimBodyInstabilitySample> BodySamples;
	if (!GatherRuntimeInstabilityBodySamples(BodySamples))
	{
		return;
	}

	FPhysAnimRuntimeInstabilityDiagnostics BodyDiagnostics;
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const FVector CurrentRootLocationCm = SkeletalMesh
		? SkeletalMesh->GetBoneLocation(PhysAnimBridge::GetRootBoneName(), EBoneSpaces::WorldSpace)
		: FVector::ZeroVector;
	const FVector ReferenceRootLocationCm = RuntimeInstabilityState.bHasReferenceRootLocation
		? RuntimeInstabilityState.ReferenceRootLocation
		: CurrentRootLocationCm;
	PhysAnimBridge::EvaluatePerBodyInstabilitySamples(
		BodySamples,
		ReferenceRootLocationCm,
		BodyDiagnostics);

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] BodyTelemetry[%s]: bodies=%d simulating=%d referenceRootZ=%.1f"),
		Context,
		BodyDiagnostics.NumBodiesConsidered,
		BodyDiagnostics.NumSimulatingBodies,
		ReferenceRootLocationCm.Z);

	for (const FPhysAnimBodyInstabilitySample& Sample : BodySamples)
	{
		const float HeightDeltaCm = FMath::Abs(Sample.Location.Z - ReferenceRootLocationCm.Z);
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] BodyTelemetry[%s] bone=%s sim=%s locZ=%.1f heightDeltaCm=%.1f linearCmPerSec=%.1f angularDegPerSec=%.1f"),
			Context,
			*Sample.BoneName.ToString(),
			Sample.bIsSimulatingPhysics ? TEXT("true") : TEXT("false"),
			Sample.Location.Z,
			HeightDeltaCm,
			Sample.LinearVelocity.Size(),
			Sample.AngularVelocity.Size());
	}
}


bool UPhysAnimComponent::IsMovementSmokeModeEnabled() const
{
	return PhysAnimComponentInternal::CVarPhysAnimMovementSmokeMode.GetValueOnGameThread() != 0;
}


void UPhysAnimComponent::ApplyMovementSmokeInput(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	LastMovementSmokeLocalIntent = FVector::ZeroVector;
	LastMovementSmokeWorldIntent = FVector::ZeroVector;
	LastMovementSmokePhaseName = NAME_None;
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;

	if (!IsMovementSmokeModeEnabled() || 
		(RuntimeState != EPhysAnimRuntimeState::BridgeActive && !bPhase1Prepare && !bPhase1LateValidate && !bPhase1RootOn && !bPhase1Settle) ||
		((bPhase1Prepare || bPhase1LateValidate || bPhase1RootOn || bPhase1Settle) && BalanceReadyTransition.ShouldSuppressMoveSmoke()))
	{
		return;
	}

	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	if (!CharacterOwner)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : BridgeStartTimeSeconds;
	const double ScriptStartTimeSeconds = PolicyInfluenceRampStartTimeSeconds >= 0.0
		? (PolicyInfluenceRampStartTimeSeconds + EffectiveSettings.StartupRampSeconds)
		: -1.0;
	if (ScriptStartTimeSeconds < 0.0 || CurrentTimeSeconds < ScriptStartTimeSeconds)
	{
		LastMovementSmokePhaseName = TEXT("WaitingForPolicy");
		LastMovementSmokeOwnerVelocityCmPerSecond = CharacterOwner->GetVelocity();
		return;
	}

	const float ScriptElapsedSeconds = static_cast<float>(CurrentTimeSeconds - ScriptStartTimeSeconds);
	const int32 NumLoops = FMath::Max(PhysAnimComponentInternal::CVarPhysAnimMovementSmokeLoopCount.GetValueOnGameThread(), 1);
	const float TotalDurationSeconds = GetMovementSmokeTotalDurationSeconds(NumLoops);
	const bool bScriptComplete = ScriptElapsedSeconds >= TotalDurationSeconds;
	const float PhaseElapsedSeconds = bScriptComplete
		? GetMovementSmokeDurationSeconds()
		: FMath::Fmod(ScriptElapsedSeconds, GetMovementSmokeDurationSeconds());
	const FVector LocalIntent = bScriptComplete
		? FVector::ZeroVector
		: ResolveMovementSmokeLocalIntent(PhaseElapsedSeconds);
	const FName PhaseName = bScriptComplete
		? TEXT("Complete")
		: ResolveMovementSmokePhaseName(PhaseElapsedSeconds);

	FRotator IntentRotation = CharacterOwner->GetActorRotation();
	if (const AController* const Controller = CharacterOwner->GetController())
	{
		IntentRotation = Controller->GetControlRotation();
	}
	IntentRotation.Pitch = 0.0f;
	IntentRotation.Roll = 0.0f;

	const FVector Forward = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::Y).GetSafeNormal2D();
	const FVector WorldIntent = ((Forward * LocalIntent.X) + (Right * LocalIntent.Y)).GetClampedToMaxSize(1.0f);

	if (!bMovementSmokeScriptStarted)
	{
		MovementSmokeStartLocation = CharacterOwner->GetActorLocation();
		bMovementSmokeScriptStarted = true;
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Movement smoke script started after policy settle."));
	}

	LastMovementSmokeLocalIntent = LocalIntent;
	LastMovementSmokeWorldIntent = WorldIntent;
	LastMovementSmokePhaseName = PhaseName;
	LastMovementSmokeOwnerVelocityCmPerSecond = CharacterOwner->GetVelocity();

	if (!WorldIntent.IsNearlyZero())
	{
		CharacterOwner->AddMovementInput(WorldIntent, 1.0f, true);
	}

	if (!bMovementSmokeCompletionLogged && bScriptComplete)
	{
		const FVector CurrentLocation = CharacterOwner->GetActorLocation();
		const float TotalDisplacementCm = FVector::Dist2D(CurrentLocation, MovementSmokeStartLocation);
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Movement smoke complete: loops=%d totalDisplacementCm=%.1f finalPhase=%s runtime=%s"),
			NumLoops,
			TotalDisplacementCm,
			*PhaseName.ToString(),
			GetRuntimeStateName(RuntimeState));
		bMovementSmokeCompletionLogged = true;
	}
}


void UPhysAnimComponent::MaybeLogRuntimeDiagnostics(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (!EffectiveSettings.bLogActionDiagnostics)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (LastRuntimeDiagnosticsLogTimeSeconds >= 0.0 &&
		(CurrentTimeSeconds - LastRuntimeDiagnosticsLogTimeSeconds) < EffectiveSettings.ActionDiagnosticsIntervalSeconds)
	{
		return;
	}

	const_cast<UPhysAnimComponent*>(this)->LastRuntimeDiagnosticsLogTimeSeconds = CurrentTimeSeconds;
	const bool bStressTestEnabled = PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0;
	const bool bStressTestActive = bStressTestEnabled && StabilizationStressTestStartTimeSeconds >= 0.0;
	const float StressTestMultiplier = ResolveStabilizationStressTestMultiplier();
	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	const float StressTestElapsedSeconds = bStressTestActive
		? static_cast<float>(FMath::Max(CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds, 0.0))
		: 0.0f;
	float StressSpineLocalDeltaCm = 0.0f;
	float StressHeadLocalDeltaCm = 0.0f;
	float StressFootLocalDeltaCm = 0.0f;
	if (bStressTestActive)
	{
		if (const AActor* const OwnerActor = GetOwner())
		{
			const FVector ActorLocation = OwnerActor->GetActorLocation();
			if (const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get())
			{
				StressSpineLocalDeltaCm = FVector::Dist(
					SkeletalMesh->GetBoneLocation(TEXT("spine_01")) - ActorLocation,
					StabilizationStressTestBaselineSpineLocalOffset);
				StressHeadLocalDeltaCm = FVector::Dist(
					SkeletalMesh->GetBoneLocation(TEXT("head")) - ActorLocation,
					StabilizationStressTestBaselineHeadLocalOffset);
				StressFootLocalDeltaCm = FMath::Max(
					FVector::Dist(
						SkeletalMesh->GetBoneLocation(TEXT("foot_l")) - ActorLocation,
						StabilizationStressTestBaselineLeftFootLocalOffset),
					FVector::Dist(
						SkeletalMesh->GetBoneLocation(TEXT("foot_r")) - ActorLocation,
						StabilizationStressTestBaselineRightFootLocalOffset));
			}
		}
	}
	float ShellPlanarOffsetDeltaCm = 0.0f;
	float ShellPlanarVelocityDeltaCmPerSecond = 0.0f;
	float ShellPlanarVelocityAlignment = 0.0f;
	if (const AActor* const OwnerActor = GetOwner())
	{
		const FVector OwnerLocation = OwnerActor->GetActorLocation();
		const FVector RootLocation = LastRuntimeInstabilityDiagnostics.RawRootLocationCm;
		if (!bHasShellCouplingReferenceRootLocalOffset)
		{
			const_cast<UPhysAnimComponent*>(this)->ShellCouplingReferenceRootLocalOffsetCm = RootLocation - OwnerLocation;
			const_cast<UPhysAnimComponent*>(this)->bHasShellCouplingReferenceRootLocalOffset = true;
		}

		ShellPlanarOffsetDeltaCm = ResolveShellCouplingPlanarOffsetDeltaCm(
			OwnerLocation,
			RootLocation,
			ShellCouplingReferenceRootLocalOffsetCm);
		const FVector EffectiveOwnerShellVelocityCmPerSecond = ResolveEffectiveShellCouplingPlanarVelocityCmPerSecond(
			OwnerActor->GetVelocity(),
			BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond,
			HasExplicitTransitionOwnedShellLock());
		ShellPlanarVelocityDeltaCmPerSecond = ResolveShellCouplingPlanarVelocityDeltaCmPerSecond(
			EffectiveOwnerShellVelocityCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RawRootLinearVelocityCmPerSecondVector);
		ShellPlanarVelocityAlignment = ResolveShellCouplingPlanarVelocityAlignment(
			EffectiveOwnerShellVelocityCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RawRootLinearVelocityCmPerSecondVector);
	}
	const PhysAnimComponentInternal::FFloatBufferSummary SelfObsSummary = PhysAnimComponentInternal::SummarizeFloatBuffer(SelfObservationBuffer);
	const PhysAnimComponentInternal::FFloatBufferSummary MimicSummary = PhysAnimComponentInternal::SummarizeFloatBuffer(MimicTargetPosesBuffer);
	const PhysAnimComponentInternal::FFloatBufferSummary TerrainBufferSummary = PhysAnimComponentInternal::SummarizeFloatBuffer(TerrainBuffer);

	if (GVerbosePhase1Forensics != 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Runtime diagnostics - policyAlpha: %.2f authorityAlpha: %.2f bringUpGroup: %d/%d resets: %d"),
			CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings),
			CalculateCurrentControlAuthorityAlpha(EffectiveSettings),
			HighestUnlockedBringUpGroupIndex + 1,
			GetBringUpGroupCount(),
			PendingBodyModifierCachedResetNames.Num());
	}
	if (GVerbosePhase2Forensics != 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Runtime diagnostics: handoffAlpha=%.2f bringUpGroup=%d/%d controlAuthorityAlpha=%.2f currentGroupControlAuthorityAlpha=%.2f policyInfluenceAlpha=%.2f policyStep[rateHz=%.1f intervalMs=%.1f updated=%s elapsedSteps=%d skipped=%d accumMs=%.1f] perturbOverride=%s stressTest[enabled=%s active=%s profile=%d sweep=%d multiplier=%.2f elapsed=%.1f firstAngSpike=%s:%.2f firstLinSpike=%s:%.2f firstInstability=%.2f localSpine=%.1f localHead=%.1f localFoot=%.1f] moveSmoke[active=%s phase=%s local=(%.1f,%.1f) world=(%.2f,%.2f) ownerVelCmPerSec=%.1f] shell[offsetDeltaCm=%.1f velDeltaCmPerSec=%.1f velAlign=%.2f] obs[selfMeanAbs=%.3f mimicMeanAbs=%.3f terrainMeanAbs=%.3f] action[rawMin=%.3f rawMax=%.3f rawMeanAbs=%.3f conditionedMeanAbs=%.3f clamped=%d] targets[policyActive=%s firstPolicyFrame=%s normal=%d held=%d total=%d maxDelta=%s:%.1fdeg meanDelta=%.1fdeg maxRawOffset=%s:%.1fdeg meanRawPolicyOffset=%.1fdeg lowerLimbLimitOccupancy=%s:%.2fx proxy=%.1fdeg mean=%.2fx] root[heightDeltaCm=%.1f linearCmPerSecond=%.1f angularDegPerSecond=%.1f unstableFor=%.2f] bodies[count=%d sim=%d maxLin=%s(%s):%.1f maxAng=%s(%s):%.1f maxHeight=%s(%s):%.1f]"),
			SimulationHandoffAlpha,
			FMath::Max(HighestUnlockedBringUpGroupIndex + 1, 0),
			GetBringUpGroupCount(),
			CalculateCurrentControlAuthorityAlpha(EffectiveSettings),
			CalculateBringUpGroupControlAuthorityAlpha(HighestUnlockedBringUpGroupIndex, EffectiveSettings),
			CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings),
			EffectiveSettings.PolicyControlRateHz,
			PolicyControlIntervalSeconds * 1000.0f,
			LastPolicyElapsedSteps > 0 ? TEXT("true") : TEXT("false"),
			LastPolicyElapsedSteps,
			PolicyControlTicksSkipped,
			PolicyUpdateAccumulatorSeconds * 1000.0f,
			IsPresentationPerturbationOverrideActive() ? TEXT("true") : TEXT("false"),
			bStressTestEnabled ? TEXT("true") : TEXT("false"),
			bStressTestActive ? TEXT("true") : TEXT("false"),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
			StressTestMultiplier,
			StressTestElapsedSeconds,
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StabilizationStressTestFirstAngularSpikeMultiplier,
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StabilizationStressTestFirstLinearSpikeMultiplier,
			StabilizationStressTestFirstInstabilityMultiplier,
			StressSpineLocalDeltaCm,
			StressHeadLocalDeltaCm,
			StressFootLocalDeltaCm,
			IsMovementSmokeModeEnabled() ? TEXT("true") : TEXT("false"),
			*LastMovementSmokePhaseName.ToString(),
			LastMovementSmokeLocalIntent.X,
			LastMovementSmokeLocalIntent.Y,
			LastMovementSmokeWorldIntent.X,
			LastMovementSmokeWorldIntent.Y,
			LastMovementSmokeOwnerVelocityCmPerSecond.Size2D(),
			ShellPlanarOffsetDeltaCm,
			ShellPlanarVelocityDeltaCmPerSecond,
			ShellPlanarVelocityAlignment,
			SelfObsSummary.MeanAbs,
			MimicSummary.MeanAbs,
			TerrainBufferSummary.MeanAbs,
			LastActionDiagnostics.RawMin,
			LastActionDiagnostics.RawMax,
			LastActionDiagnostics.RawMeanAbs,
			LastActionDiagnostics.ConditionedMeanAbs,
			LastActionDiagnostics.NumClampedActionFloats,
			LastControlTargetDiagnostics.bPolicyInfluenceActive ? TEXT("true") : TEXT("false"),
			LastControlTargetDiagnostics.bFirstPolicyEnabledFrame ? TEXT("true") : TEXT("false"),
			LastControlTargetDiagnostics.NumNormalPolicyTargetsWritten,
			LastControlTargetDiagnostics.NumHeldTargetsWritten,
			LastControlTargetDiagnostics.NumTotalTargetsWritten,
			*LastControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
			LastControlTargetDiagnostics.MaxTargetDeltaDegrees,
			LastControlTargetDiagnostics.MeanTargetDeltaDegrees,
			*LastControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
			LastControlTargetDiagnostics.MaxRawPolicyOffsetDegrees,
			LastControlTargetDiagnostics.MeanRawPolicyOffsetDegrees,
			*LastControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName.ToString(),
			LastControlTargetDiagnostics.MaxLowerLimbLimitOccupancy,
			LastControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees,
			LastControlTargetDiagnostics.MeanLowerLimbLimitOccupancy,
			LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm,
			LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond,
			LastRuntimeInstabilityDiagnostics.UnstableAccumulatedSeconds,
			LastRuntimeInstabilityDiagnostics.NumBodiesConsidered,
			LastRuntimeInstabilityDiagnostics.NumSimulatingBodies,
			*LastRuntimeInstabilityDiagnostics.MaxLinearSpeedBoneName.ToString(),
			LastRuntimeInstabilityDiagnostics.bMaxLinearSpeedBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
			LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond,
			*LastRuntimeInstabilityDiagnostics.MaxAngularSpeedBoneName.ToString(),
			LastRuntimeInstabilityDiagnostics.bMaxAngularSpeedBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
			LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond,
			*LastRuntimeInstabilityDiagnostics.MaxHeightDeltaBoneName.ToString(),
			LastRuntimeInstabilityDiagnostics.bMaxHeightDeltaBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
			LastRuntimeInstabilityDiagnostics.MaxBodyHeightDeltaCm);
	}
}



void UPhysAnimComponent::UpdateBridgeStatusIndicator(float DisplayDurationSeconds) const
{
	if (!GEngine || PhysAnimComponentInternal::CVarPhysAnimShowBridgeStatusIndicator.GetValueOnGameThread() == 0)
	{
		return;
	}

	const uint64 MessageKey = static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
	const bool bBridgeOwnsPhysics = RuntimeStateOwnsBridgePhysics(RuntimeState);
	const FString Message = BuildBridgeStatusIndicatorText(RuntimeState, bBridgeOwnsPhysics);
	const FColor Color = ResolveBridgeStatusIndicatorColor(RuntimeState, bBridgeOwnsPhysics);
	GEngine->AddOnScreenDebugMessage(MessageKey, DisplayDurationSeconds, Color, Message, false, FVector2D(1.25f, 1.25f));
}

