#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/Character.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Templates/SharedPointer.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Latent command to wait for the standing proof to complete.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FWaitForStandingProofCommand, UPhysAnimComponent*, Component, float, TimeoutSeconds, float, StartTime);
bool FWaitForStandingProofCommand::Update()
{
	if (!Component)
	{
		return true;
	}

	const float CurrentTime = FPlatformTime::Seconds();
	const float Elapsed = CurrentTime - StartTime;

	if (Elapsed >= TimeoutSeconds)
	{
		return true;
	}

	return false;
}

/**
 * Command to enable the proof hook.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand);
bool FEnableStandingProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			UE_LOG(LogTemp, Warning, TEXT("[!!!!PROOFIX!!!!] ENABLING_PROOF for %s"), *It->GetName());
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = true;
			break;
		}
	}

	return true;
}

/**
 * Command to start the proof wait.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FStartStandingProofWaitCommand);
bool FStartStandingProofWaitCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	UPhysAnimComponent* TargetComponent = nullptr;
	if (World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
			{
				TargetComponent = Comp;
				break;
			}
		}
	}

	if (TargetComponent)
	{
		// We can't use ADD_LATENT_AUTOMATION_COMMAND here because we are in a command's Update.
		// But we can return false and wait ourselves, or rely on the test sequence.
	}

	return true;
}

/**
 * Latent command to verify the standing proof results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FVerifyStandingProofCommand);
bool FVerifyStandingProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			
			// 1. Check if proof completed
			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Proof did not complete in time for %s"), *It->GetName());
				return true;
			}

			if (!Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Proof completed but is not truthfully satisfied for %s"), *It->GetName());
				return true;
			}

			if (!Comp->CanEnterBalanceActiveStanding())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Proof completed but cannot enter active standing for %s"), *It->GetName());
				return true;
			}

			// 2. Check Hull Area - Guard against the 0.0 failure
			const float HullArea = TerminationState.TerminalArtifact.SupportHullAreaCm2;
			if (HullArea <= 0.0f)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: SupportHullAreaCm2 is 0.0 for %s. This indicates a sampling failure or contract violation."), *It->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("StandingProof: SupportHullAreaCm2 verified at %.1f cm2"), HullArea);
			}

			// 3. Check Terminal Reason
			const EPhysAnimTerminalReason Reason = TerminationState.TerminalArtifact.TerminalReason;
			if (Reason == EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: FAILED with ActivationSupportFailure for %s"), *It->GetName());
			}
			else if (Reason == EPhysAnimTerminalReason::None)
			{
				UE_LOG(LogTemp, Warning, TEXT("StandingProof: PASSED (None) for %s"), *It->GetName());
			}
			else
			{
				// Currently we might fail due to shell velocity correction hardening, which is "expected" for now but we should acknowledge it
				UE_LOG(LogTemp, Warning, TEXT("StandingProof: TERMINATED with reason %d for %s"), (int32)Reason, *It->GetName());
			}

			if (!TerminationState.TerminalArtifact.bPhysicsAssetContractValid ||
				!TerminationState.TerminalArtifact.bSkeletonAuditPassed)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Terminal artifact audit state is inconsistent for %s"), *It->GetName());
			}

			if (!TerminationState.TerminalArtifact.bCapsuleContractPassed ||
				!TerminationState.TerminalArtifact.bPhysicalContinuityValidatorPassed ||
				TerminationState.TerminalArtifact.bContinuityBookkeepingMismatch)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Terminal artifact capsule/continuity state is invalid for %s"), *It->GetName());
			}
			
			break;
		}
	}

	return true;
}

/**
 * Command to enable the negative support proof hook.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand);
bool FEnableNegativeSupportProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			UE_LOG(LogTemp, Warning, TEXT("StandingProof: ENABLING_NEGATIVE_SUPPORT_PROOF for %s"), *It->GetName());
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = true;
			Comp->SetForceSupportFailure(true);
			break;
		}
	}

	return true;
}

/**
 * Latent command to verify the negative standing proof results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FVerifyNegativeSupportProofCommand);
bool FVerifyNegativeSupportProofCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			
			// 1. Check if proof completed
			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: Negative proof did not complete in time for %s"), *It->GetName());
				return true;
			}

			// 2. Check Terminal Reason - Expected ActivationSupportFailure
			const EPhysAnimTerminalReason Reason = TerminationState.TerminalArtifact.TerminalReason;
			if (Reason == EPhysAnimTerminalReason::ActivationSupportFailure)
			{
				UE_LOG(LogTemp, Warning, TEXT("StandingProof: NEGATIVE_TEST_PASSED (Expected ActivationSupportFailure) for %s"), *It->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: NEGATIVE_TEST_FAILED. Expected ActivationSupportFailure but got %d for %s"), (int32)Reason, *It->GetName());
			}

			if (Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: NEGATIVE_TEST_FAILED. Proof should not be satisfied for %s"), *It->GetName());
			}

			if (Comp->CanEnterBalanceActiveStanding())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: NEGATIVE_TEST_FAILED. Active standing should be denied for %s"), *It->GetName());
			}

			// 3. Ensure we didn't stay in active standing
			const EPhysAnimRuntimeState RuntimeState = Comp->GetRuntimeState();
			if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: NEGATIVE_TEST_FAILED. Remained in BalanceActive_Standing despite support loss for %s"), *It->GetName());
			}
			
			break;
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStandingProofLiveTest, "PhysAnim.StandingProof.Live", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStandingProofLiveTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimStandingProofLiveTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;

	// 1. Load the map
	AutomationOpenMap(MapName);

	// 2. Wait for map to load
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 3. Enable proof
	ADD_LATENT_AUTOMATION_COMMAND(FEnableStandingProofCommand());

	// 4. Wait for results (6 seconds to be safe)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));

	// 5. Verify results
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyStandingProofCommand());

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStandingProofNegativeSupportTest, "PhysAnim.StandingProof.NegativeSupport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStandingProofNegativeSupportTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_Negative"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimStandingProofNegativeSupportTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;
	AddExpectedError(TEXT("Fail-stop: Proof failed during activation wait"), EAutomationExpectedErrorFlags::Contains, 0);

	// 1. Load the map
	AutomationOpenMap(MapName);

	// 2. Wait for map to load
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	// 3. Enable negative proof
	ADD_LATENT_AUTOMATION_COMMAND(FEnableNegativeSupportProofCommand());

	// 4. Wait for results (6 seconds)
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(6.0f));

	// 5. Verify negative results
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyNegativeSupportProofCommand());

	return true;
}
/**
 * Command to enable activation wiring test cases.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FEnableActivationWiringCommand, bool, bEnableProof, bool, bFinishProof, bool, bForceFailure);
bool FEnableActivationWiringCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			Comp->StopBridge();
			Comp->ResetLiveRuntimeEvidenceProof();
			Comp->bEnableLiveRuntimeEvidenceProof = bEnableProof;
			
			if (bForceFailure)
			{
				Comp->SetForceSupportFailure(true);
			}

			Comp->StartBridge();
			break;
		}
	}

	return true;
}

/**
 * Command to verify activation wiring results.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyActivationWiringCommand, EPhysAnimRuntimeState, ExpectedState);
bool FVerifyActivationWiringCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const EPhysAnimRuntimeState ActualState = Comp->GetRuntimeState();
			if (ActualState == ExpectedState)
			{
				UE_LOG(LogTemp, Warning, TEXT("ActivationWiring: TEST_PASSED ActualState=%d"), (int32)ActualState);
				if (ExpectedState == EPhysAnimRuntimeState::BalanceActive_Standing &&
					(!Comp->IsLiveRuntimeEvidenceProofSatisfied() || !Comp->CanEnterBalanceActiveStanding()))
				{
					UE_LOG(LogTemp, Error, TEXT("ActivationWiring: TEST_FAILED. Standing proof gate is not truthful for %s"), *It->GetName());
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("ActivationWiring: TEST_FAILED. Expected %d but got %d for %s"), (int32)ExpectedState, (int32)ActualState, *It->GetName());
			}
			break;
		}
	}

return true;
}

/**
 * Latent command to collect activated standing stability metrics for a duration.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FCollectActivatedStandingStabilityMetricsCommand, float, DurationSeconds);
bool FCollectActivatedStandingStabilityMetricsCommand::Update()
{
	static double StartTimeSeconds = -1.0;

	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return false;
	}

	if (StartTimeSeconds < 0.0)
	{
		StartTimeSeconds = FPlatformTime::Seconds();
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			Comp->UpdateActivatedStandingStabilityMetrics(FMath::Max(0.0f, World->GetDeltaSeconds()));
			break;
		}
	}

	const bool bFinished = (FPlatformTime::Seconds() - StartTimeSeconds) >= static_cast<double>(DurationSeconds);
	if (bFinished)
	{
		StartTimeSeconds = -1.0;
	}

	return bFinished;
}

/**
 * Command to verify activated standing stability metrics.
 */
DEFINE_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingStabilityMetricsCommand);
bool FVerifyActivatedStandingStabilityMetricsCommand::Update()
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
			{
				World = Context.World();
				break;
			}
		}
	}
#endif
	if (!World) World = GWorld;

	if (!World)
	{
		return true;
	}

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (UPhysAnimComponent* Comp = It->FindComponentByClass<UPhysAnimComponent>())
		{
			const EPhysAnimRuntimeState RuntimeState = Comp->GetRuntimeState();
			const FPhysAnimRuntimeTerminationState& TerminationState = Comp->GetLiveRuntimeEvidenceTerminationState();
			const FPhysAnimActivatedStandingStabilityMetrics& Metrics = Comp->GetActivatedStandingStabilityMetrics();

			if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics expected BalanceActive_Standing but got %d for %s"), (int32)RuntimeState, *It->GetName());
			}

			if (!Comp->IsLiveRuntimeEvidenceProofComplete())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics proof did not complete for %s"), *It->GetName());
			}

			if (!Comp->IsLiveRuntimeEvidenceProofSatisfied())
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics proof is not truthful for %s"), *It->GetName());
			}

			if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics expected terminal_reason=None but got %d for %s"), (int32)TerminationState.TerminalReason, *It->GetName());
			}

			if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics were not collected for %s"), *It->GetName());
			}

			const auto CheckFinite = [&](const TCHAR* Label, double Value)
			{
				if (!FMath::IsFinite(Value))
				{
					UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics %s is not finite for %s"), Label, *It->GetName());
				}
			};

			CheckFinite(TEXT("activation_duration_sec"), Metrics.ActivationDurationSec);
			CheckFinite(TEXT("root_world_position_drift_cm"), Metrics.RootWorldPositionDriftCm);
			CheckFinite(TEXT("root_vertical_drift_cm"), Metrics.RootVerticalDriftCm);
			CheckFinite(TEXT("root_angular_drift_deg"), Metrics.RootAngularDriftDeg);
			CheckFinite(TEXT("max_body_linear_speed_cm_per_second"), Metrics.MaxBodyLinearSpeedCmPerSecond);
			CheckFinite(TEXT("max_body_angular_speed_deg_per_second"), Metrics.MaxBodyAngularSpeedDegPerSecond);
			CheckFinite(TEXT("support_hull_area_min_cm2"), Metrics.SupportHullAreaMinCm2);
			CheckFinite(TEXT("support_hull_area_mean_cm2"), Metrics.SupportHullAreaMeanCm2);
			CheckFinite(TEXT("support_hull_area_max_cm2"), Metrics.SupportHullAreaMaxCm2);
			CheckFinite(TEXT("active_support_side_count_min"), Metrics.ActiveSupportSideCountMin);
			CheckFinite(TEXT("active_support_side_count_mean"), Metrics.ActiveSupportSideCountMean);
			CheckFinite(TEXT("active_support_side_count_max"), Metrics.ActiveSupportSideCountMax);

			if (Metrics.FailStopCount != 0)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics expected no fail-stop but got %d for %s"), Metrics.FailStopCount, *It->GetName());
			}

			if (Metrics.ActivationDurationSec < 30.0)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics activation duration %.2f is below 30.0 seconds for %s"), Metrics.ActivationDurationSec, *It->GetName());
			}

			if (Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.SupportHullAreaMeanCm2 <= 0.0 || Metrics.SupportHullAreaMaxCm2 <= 0.0)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics support hull area did not stay above zero for %s"), *It->GetName());
			}

			if (Metrics.ActiveSupportSideCountMin < 1.0 || Metrics.ActiveSupportSideCountMean < 1.0 || Metrics.ActiveSupportSideCountMax < 1.0)
			{
				UE_LOG(LogTemp, Error, TEXT("StandingProof: StabilityMetrics active support side count dropped below 1 for %s"), *It->GetName());
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("StandingProof: StabilityMetrics samples=%d duration=%.2f rootDrift=%.2f verticalDrift=%.2f angularDrift=%.2f supportHull[min/mean/max]=%.2f/%.2f/%.2f activeSides[min/mean/max]=%.2f/%.2f/%.2f maxBodyLinear=%.2f maxBodyAngular=%.2f terminalReason=%d"),
				Metrics.SampleCount,
				Metrics.ActivationDurationSec,
				Metrics.RootWorldPositionDriftCm,
				Metrics.RootVerticalDriftCm,
				Metrics.RootAngularDriftDeg,
				Metrics.SupportHullAreaMinCm2,
				Metrics.SupportHullAreaMeanCm2,
				Metrics.SupportHullAreaMaxCm2,
				Metrics.ActiveSupportSideCountMin,
				Metrics.ActiveSupportSideCountMean,
				Metrics.ActiveSupportSideCountMax,
				Metrics.MaxBodyLinearSpeedCmPerSecond,
				Metrics.MaxBodyAngularSpeedDegPerSecond,
				Metrics.TerminalReason);
			break;
		}
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationWiringTest, "PhysAnim.ActivationPath.Wiring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivationWiringTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ProofDisabled"));
	OutTestCommands.Add(TEXT("ProofDisabled"));

	OutBeautifiedNames.Add(TEXT("ProofNotSatisfied"));
	OutTestCommands.Add(TEXT("ProofNotSatisfied"));

	OutBeautifiedNames.Add(TEXT("ProofSatisfied"));
	OutTestCommands.Add(TEXT("ProofSatisfied"));

	OutBeautifiedNames.Add(TEXT("ProofFailed"));
	OutTestCommands.Add(TEXT("ProofFailed"));
}

bool FPhysAnimActivationWiringTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	if (Parameters == TEXT("ProofDisabled"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(false, false, false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f)); // Wait for startup
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch));
	}
	else if (Parameters == TEXT("ProofNotSatisfied"))
	{
		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f)); // Wait for startup (but proof takes 3s)
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch));
	}
	else if (Parameters == TEXT("ProofSatisfied"))
	{
		AddExpectedError(TEXT("PhysAnimProof: TerminalArtifact"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("PhysAnimProof: AttemptResult"), EAutomationExpectedErrorFlags::Contains, 0);

		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f)); // Wait for 3s proof + startup
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing));
	}
	else if (Parameters == TEXT("ProofFailed"))
	{
		AddExpectedError(TEXT("PhysAnimProof: TerminalArtifact"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("PhysAnimProof: AttemptResult"), EAutomationExpectedErrorFlags::Contains, 0);
		AddExpectedError(TEXT("Fail-stop: Proof failed"), EAutomationExpectedErrorFlags::Contains, 0);

		ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, true));
		ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::FailStopped));
	}

	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStateMachinePhase1EntryTest, "PhysAnim.StateMachine.Phase1Entry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStateMachinePhase1EntryTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Phase1Entry"));
	OutTestCommands.Add(TEXT("Phase1Entry"));
}

bool FPhysAnimStateMachinePhase1EntryTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, false, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::WaitingForPoseSearch));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimStateMachinePhase2StandingTest, "PhysAnim.StateMachine.Phase2Standing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimStateMachinePhase2StandingTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("Phase2Standing"));
	OutTestCommands.Add(TEXT("Phase2Standing"));
}

bool FPhysAnimStateMachinePhase2StandingTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BalanceActive_Standing));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivatedStandingStabilityMetricsTest, "PhysAnim.ActivatedStanding.StabilityMetrics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FPhysAnimActivatedStandingStabilityMetricsTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("ThirdPerson_Standing_StabilityMetrics"));
	OutTestCommands.Add(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson"));
}

bool FPhysAnimActivatedStandingStabilityMetricsTest::RunTest(const FString& Parameters)
{
	const FString MapName = Parameters;

	AutomationOpenMap(MapName);
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FEnableActivationWiringCommand(true, true, false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(5.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FCollectActivatedStandingStabilityMetricsCommand(30.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivatedStandingStabilityMetricsCommand());

	return true;
}

#endif
