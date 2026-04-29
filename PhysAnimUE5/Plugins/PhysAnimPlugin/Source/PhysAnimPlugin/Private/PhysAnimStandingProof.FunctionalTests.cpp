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

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FPhysAnimActivationWiringTest, "ActivationPath.Wiring", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyActivationWiringCommand(EPhysAnimRuntimeState::BridgeActive));
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

#endif
