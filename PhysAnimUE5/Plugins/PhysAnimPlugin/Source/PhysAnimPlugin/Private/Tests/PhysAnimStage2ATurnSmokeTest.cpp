#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPhysAnimBridge, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimStage2ATurnSmokeTest, "PhysAnim.PIE.Stage2A.TurnSmoke", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static constexpr float Stage2ATurnSmokeDurationSeconds = 2.0f;
static constexpr float Stage2ATurnSmokeTickSeconds = 0.05f;
static constexpr float Stage2ATurnSmokeYawDeltaPerTickDegrees = 1.0f; // Total 40 deg
static constexpr float Stage2ATurnSmokeMinAbsYawDegrees = 20.0f;
static constexpr float Stage2ATurnSmokeMaxAbsYawDegrees = 90.0f;
static constexpr float Stage2ATurnSmokeMaxTranslationCm = 120.0f;

bool FPhysAnimStage2ATurnSmokeTest::RunTest(const FString& Parameters)
{
	// Setup: Minimal transient actor/component in GWorld
	UWorld* World = GWorld;
	if (!World)
	{
		return false;
	}

	auto RunTurnPass = [&](float YawDeltaPerTick, const TCHAR* PassLabel)
	{
		AActor* TestActor = World->SpawnActor<AActor>();
		USceneComponent* Root = NewObject<USceneComponent>(TestActor);
		TestActor->SetRootComponent(Root);
		Root->RegisterComponent();

		UPhysAnimComponent* Component = NewObject<UPhysAnimComponent>(TestActor);
		Component->RegisterComponent();

		// Force the component into the implemented Stage2A-ready state
		Component->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		Component->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
		Component->BridgeShellState.bInitialized = true;
		Component->bPolicyTargetsAppliedLastFrame = true;
		Component->PolicyControlTicksExecuted = 1;
		Component->bStage2APhase3SimRootAttempted = false;
		Component->Stage2AConsecutivePolicyActiveFrames = 3;
		Component->LastPolicyControlUpdateTimeSeconds = Component->GetPhysAnimClockTime();

		// Record StartLocation and StartRotation
		const FVector StartLocation = TestActor->GetActorLocation();
		const FRotator StartRotation = TestActor->GetActorRotation();

		// Run exactly 40 iterations: 2.0 seconds / 0.05 seconds
		const int32 Iterations = FMath::RoundToInt(Stage2ATurnSmokeDurationSeconds / Stage2ATurnSmokeTickSeconds);
		for (int32 i = 0; i < Iterations; ++i)
		{
			Component->TryOpenStage2ALocomotionRequestGate(TEXT("TurnSmoke"));
			Component->TryActivateStage2ATurnIntent(Stage2ATurnSmokeTickSeconds, YawDeltaPerTick);
			
			// Mock watchdog update to keep it live
			Component->LastPolicyControlUpdateTimeSeconds = Component->GetPhysAnimClockTime();
		}

		const FVector EndLocation = TestActor->GetActorLocation();
		const FRotator EndRotation = TestActor->GetActorRotation();

		// Compute metrics
		const float SignedYawDeltaDegrees = FMath::FindDeltaAngleDegrees(StartRotation.Yaw, EndRotation.Yaw);
		const float TranslationDriftCm = FVector::Dist(StartLocation, EndLocation);

		// Assertions
		const float AbsYawDeltaDegrees = FMath::Abs(SignedYawDeltaDegrees);
		const bool bExpectLeft = YawDeltaPerTick > 0.0f;
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-01 signed yaw direction matches intent"), PassLabel), bExpectLeft ? SignedYawDeltaDegrees > 0.0f : SignedYawDeltaDegrees < 0.0f);
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-02 abs(SignedYawDeltaDegrees) >= 20.0f"), PassLabel), AbsYawDeltaDegrees >= Stage2ATurnSmokeMinAbsYawDegrees);
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-03 abs(SignedYawDeltaDegrees) <= 90.0f"), PassLabel), AbsYawDeltaDegrees <= Stage2ATurnSmokeMaxAbsYawDegrees);
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-04 TranslationDriftCm <= 120.0f"), PassLabel), TranslationDriftCm <= Stage2ATurnSmokeMaxTranslationCm);
		
		TestEqual(FString::Printf(TEXT("%s SMOKE-TURN-05 RuntimeState is LocomotionActiveShell"), PassLabel), (int32)Component->RuntimeState, (int32)EPhysAnimRuntimeState::LocomotionActiveShell);
		TestEqual(FString::Printf(TEXT("%s SMOKE-TURN-06 AuthorityState is Locomoting"), PassLabel), (int32)Component->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::Locomoting);
		TestEqual(FString::Printf(TEXT("%s SMOKE-TURN-07 TerminalState is Allowed"), PassLabel), (int32)Component->Stage2ALastLocomotionTerminalState, (int32)EStage2ALocomotionTerminalState::Allowed);

		// Telemetry assertions
		const FString ExpectedIntent = (YawDeltaPerTick > 0.0f) ? TEXT("TurnLeft") : TEXT("TurnRight");
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-08 Telemetry contains locomotion_intent=%s"), PassLabel, *ExpectedIntent), Component->LastStage2ALocomotionTelemetryLine.Contains(FString::Printf(TEXT("locomotion_intent=%s"), *ExpectedIntent)));
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-09 Telemetry contains root_mode=Stage1_KinematicRoot"), PassLabel), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("root_mode=Stage1_KinematicRoot")));
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-10 Telemetry contains phase3_simroot_attempted=false"), PassLabel), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("phase3_simroot_attempted=false")));
		TestTrue(FString::Printf(TEXT("%s SMOKE-TURN-11 Telemetry contains terminal_state=Allowed"), PassLabel), Component->LastStage2ALocomotionTelemetryLine.Contains(TEXT("terminal_state=Allowed")));

		// Emit final log line as requested
		UE_LOG(LogPhysAnimBridge, Display, TEXT("PASS_STAGE2A_TURN_%s_SMOKE duration_seconds=2.0 signed_yaw_delta_degrees=%.3f translation_cm=%.3f terminal_state=%s"), 
			PassLabel, SignedYawDeltaDegrees, TranslationDriftCm, UPhysAnimComponent::Stage2ATerminalStateToString(Component->Stage2ALastLocomotionTerminalState));

		// Cleanup
		TestActor->Destroy();
		return true;
	};

	bool bLeftOk = RunTurnPass(Stage2ATurnSmokeYawDeltaPerTickDegrees, TEXT("LEFT"));
	bool bRightOk = RunTurnPass(-Stage2ATurnSmokeYawDeltaPerTickDegrees, TEXT("RIGHT"));

	return bLeftOk && bRightOk;
}
