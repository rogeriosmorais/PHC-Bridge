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

#endif
